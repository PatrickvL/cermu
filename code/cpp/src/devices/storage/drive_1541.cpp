/**
 * drive_1541.cpp - Commodore 1541 Disk Drive Implementation
 *
 * IEC-protocol-level emulation of the 1541 drive.  Handles LISTEN/TALK/OPEN/
 * CLOSE commands and reads files from D64 disk images.
 *
 * IEC PROTOCOL OVERVIEW (simplified):
 * ====================================
 * 1. Host asserts ATN and sends a command byte:
 *    - $20+device = LISTEN, $40+device = TALK
 *    - $60+sa = SECOND (set secondary address for data transfer)
 *    - $E0+sa = CLOSE
 *    - $F0+sa = OPEN
 *    - $3F = UNLISTEN, $5F = UNTALK
 * 2. Data transfer happens after ATN is released.
 * 3. Each byte is clocked via CLK/DATA handshake.
 *
 * This implementation processes commands at a higher level than the real
 * hardware, which uses the 6502 + VIA1 for bit-banged serial I/O.  The
 * advantage is speed and simplicity; the trade-off is that custom fast-loaders
 * that bypass the standard IEC protocol will not work (a future cycle-accurate
 * mode can address this).
 */

#include "drive_1541.h"
#include "../../core/device_registry.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// ============================================================================
// IEC COMMAND BYTE MASKS
// ============================================================================

static constexpr uint8_t IEC_LISTEN    = 0x20;  // + device number
static constexpr uint8_t IEC_TALK      = 0x40;  // + device number
static constexpr uint8_t IEC_UNLISTEN  = 0x3F;
static constexpr uint8_t IEC_UNTALK    = 0x5F;
static constexpr uint8_t IEC_SECOND    = 0x60;  // + secondary address
static constexpr uint8_t IEC_OPEN      = 0xF0;  // + secondary address
static constexpr uint8_t IEC_CLOSE     = 0xE0;  // + secondary address

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

Drive1541Device::Drive1541Device(uint8_t device_number)
    : device_number_(device_number)
    , iec_state_(IECState::IDLE)
    , current_command_(0)
    , current_secondary_(0)
    , addressed_(false)
    , output_signals_(0xFFFFFFFF)
    , prev_atn_(false)
    , byte_counter_(0)
    , iec_cycle_counter_(0)
    , shift_register_(0)
    , bit_counter_(-1)
    , eoi_sent_(false)
    , disk_inserted_(false)
    , drive_led_(false)
{
    name_ = "1541 Drive (#" + std::to_string(device_number) + ")";
    set_error(73, "CBM DOS V2.6 1541");
}

void Drive1541Device::reset() {
    iec_state_ = IECState::IDLE;
    current_command_ = 0;
    current_secondary_ = 0;
    addressed_ = false;
    output_signals_ = 0xFFFFFFFF;
    prev_atn_ = false;
    byte_counter_ = 0;
    iec_cycle_counter_ = 0;
    shift_register_ = 0;
    bit_counter_ = -1;
    eoi_sent_ = false;
    receive_buffer_.clear();
    drive_led_ = false;

    for (auto& ch : channels_) ch.clear();
    set_error(73, "CBM DOS V2.6 1541");

    if (port_) port_->notify_device_output_changed(output_signals_);
}

void Drive1541Device::set_device_number(uint8_t num) {
    device_number_ = num;
    name_ = "1541 Drive (#" + std::to_string(device_number_) + ")";
}

// ============================================================================
// SIGNAL I/O
// ============================================================================

uint32_t Drive1541Device::get_output_signals() const {
    return output_signals_;
}

IECBusState Drive1541Device::read_iec_bus() const {
    uint32_t signals = port_ ? port_->read_signals() : 0xFFFFFFFF;
    return {
        !(signals & (1u << ConnectorSignals::IEC_ATN)),   // Active-low: 0 = asserted
        !(signals & (1u << ConnectorSignals::IEC_CLK)),
        !(signals & (1u << ConnectorSignals::IEC_DATA)),
    };
}

void Drive1541Device::drive_iec_lines(bool data_out, bool clk_out) {
    // Active-low: to assert a line, pull it LOW (clear the bit)
    uint32_t new_signals = 0xFFFFFFFF;

    if (data_out) {
        new_signals &= ~(1u << ConnectorSignals::IEC_DATA);  // Assert DATA (pull low)
    }
    if (clk_out) {
        new_signals &= ~(1u << ConnectorSignals::IEC_CLK);   // Assert CLK (pull low)
    }

    if (new_signals != output_signals_) {
        output_signals_ = new_signals;
        if (port_) port_->notify_device_output_changed(output_signals_);
    }
}

// ============================================================================
// IEC PROTOCOL STATE MACHINE
// ============================================================================

void Drive1541Device::on_signal_change(uint32_t signal_state) {
    iec_state_machine();
}

void Drive1541Device::tick() {
    iec_cycle_counter_++;
    iec_state_machine();
}

void Drive1541Device::iec_state_machine() {
    IECBusState bus = read_iec_bus();

    // Detect ATN assertion edge (falling edge → ATN goes active)
    if (bus.atn && !prev_atn_) {
        // ATN asserted — all devices must listen for command
        iec_state_ = IECState::COMMAND;
        byte_counter_ = 0;
        receive_buffer_.clear();
        // Release DATA and CLK to indicate we're ready
        drive_iec_lines(false, false);
    }

    prev_atn_ = bus.atn;

    // In COMMAND state, wait for ATN to be released after command byte is clocked in
    // (simplified: the host will write command bytes sequentially)
}

void Drive1541Device::process_command(uint8_t command) {
    uint8_t device = command & 0x1F;
    uint8_t cmd_type = command & 0xE0;

    // Check if this command is for us
    if (cmd_type == IEC_LISTEN || cmd_type == IEC_TALK) {
        if (device != device_number_) {
            addressed_ = false;
            return;
        }
        addressed_ = true;
    }

    if (command == IEC_UNLISTEN) {
        if (iec_state_ == IECState::OPEN && !receive_buffer_.empty()) {
            // Complete the OPEN command — filename received
            std::string filename(receive_buffer_.begin(), receive_buffer_.end());
            DriveChannel& ch = channels_[current_secondary_];
            ch.clear();
            ch.filename = filename;

            if (current_secondary_ == 0) {
                // Channel 0 = default LOAD channel
                drive_led_ = true;
                if (!open_file(ch, filename)) {
                    set_error(62, "FILE NOT FOUND");
                }
                drive_led_ = false;
            } else if (current_secondary_ == 1) {
                // Channel 1 = default SAVE channel (not implemented yet)
                ch.open = true;
            } else if (current_secondary_ == 15) {
                // Channel 15 = command channel
                // Process DOS command
                ch.open = true;
                // For now, just acknowledge
            } else {
                // General file open
                drive_led_ = true;
                if (!open_file(ch, filename)) {
                    set_error(62, "FILE NOT FOUND");
                }
                drive_led_ = false;
            }
            receive_buffer_.clear();
        }
        addressed_ = false;
        iec_state_ = IECState::IDLE;
        return;
    }

    if (command == IEC_UNTALK) {
        addressed_ = false;
        iec_state_ = IECState::IDLE;
        drive_iec_lines(false, false);
        return;
    }

    if (!addressed_) return;

    // Secondary address commands
    uint8_t sa = command & 0x0F;

    if ((command & 0xF0) == (IEC_SECOND & 0xF0)) {
        // SECOND — set secondary address for data transfer
        current_secondary_ = sa;
        if (cmd_type == IEC_LISTEN || iec_state_ == IECState::LISTEN) {
            iec_state_ = IECState::LISTEN;
        } else {
            iec_state_ = IECState::TALK;
        }
        return;
    }

    if ((command & 0xF0) == (IEC_OPEN & 0xF0)) {
        current_secondary_ = sa;
        iec_state_ = IECState::OPEN;
        receive_buffer_.clear();
        return;
    }

    if ((command & 0xF0) == (IEC_CLOSE & 0xF0)) {
        current_secondary_ = sa;
        channels_[sa].clear();
        iec_state_ = IECState::IDLE;
        return;
    }

    // LISTEN / TALK without secondary = address phase
    if (cmd_type == IEC_LISTEN) {
        iec_state_ = IECState::LISTEN;
    } else if (cmd_type == IEC_TALK) {
        iec_state_ = IECState::TALK;
    }
}

// ============================================================================
// DISK IMAGE MANAGEMENT
// ============================================================================

bool Drive1541Device::insert_disk(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("1541: Cannot open disk image '%s'\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Validate D64 size
    if (size != DRIVE_D64_STD_SIZE && size != DRIVE_D64_STD_SIZE_ERR &&
        size != DRIVE_D64_EXT_SIZE && size != DRIVE_D64_EXT_SIZE_ERR) {
        printf("1541: Invalid D64 size %ld for '%s'\n", size, filepath);
        fclose(f);
        return false;
    }

    disk_image_.resize(static_cast<size_t>(size));
    if (fread(disk_image_.data(), 1, static_cast<size_t>(size), f) != static_cast<size_t>(size)) {
        printf("1541: Failed to read disk image '%s'\n", filepath);
        fclose(f);
        disk_image_.clear();
        return false;
    }
    fclose(f);

    disk_path_ = filepath;
    disk_inserted_ = true;
    set_error(0, "OK");

    // Auto-add to fliplist (convenient for multi-disc games)
    fliplist_add(filepath);

    printf("1541: Disk inserted: '%s' (%ld bytes)\n", filepath, size);
    return true;
}

void Drive1541Device::eject_disk() {
    disk_image_.clear();
    disk_path_.clear();
    disk_inserted_ = false;
    for (auto& ch : channels_) ch.clear();
    set_error(74, "DRIVE NOT READY");
    printf("1541: Disk ejected\n");
}

bool Drive1541Device::swap_disk(const char* filepath) {
    // Swap = replace image without resetting drive state.
    // Only the disc media changes — IEC protocol state, uploaded fastloader
    // code, and VIA state are all preserved (like physically swapping a floppy).

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("1541: Cannot open disk image '%s' for swap\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size != DRIVE_D64_STD_SIZE && size != DRIVE_D64_STD_SIZE_ERR &&
        size != DRIVE_D64_EXT_SIZE && size != DRIVE_D64_EXT_SIZE_ERR) {
        printf("1541: Invalid D64 size %ld for swap '%s'\n", size, filepath);
        fclose(f);
        return false;
    }

    disk_image_.resize(static_cast<size_t>(size));
    if (fread(disk_image_.data(), 1, static_cast<size_t>(size), f) != static_cast<size_t>(size)) {
        printf("1541: Failed to read disk image '%s' for swap\n", filepath);
        fclose(f);
        disk_image_.clear();
        disk_inserted_ = false;
        return false;
    }
    fclose(f);

    // Invalidate open channels — file references are stale after swap
    for (auto& ch : channels_) ch.clear();

    disk_path_ = filepath;
    disk_inserted_ = true;
    set_error(0, "OK");

    printf("1541: Disk swapped: '%s' (%ld bytes)\n", filepath, size);
    return true;
}

// ============================================================================
// DISC FLIPLIST
// ============================================================================

void Drive1541Device::fliplist_add(const char* filepath) {
    std::string path(filepath);

    // Avoid duplicates
    for (const auto& entry : fliplist_) {
        if (entry == path) return;
    }
    fliplist_.push_back(std::move(path));

    // If this is the currently inserted disc, update index
    if (disk_inserted_ && disk_path_ == filepath) {
        fliplist_index_ = static_cast<int>(fliplist_.size()) - 1;
    }
    printf("1541: Fliplist add '%s' (total: %zu)\n", filepath,
           fliplist_.size());
}

void Drive1541Device::fliplist_remove(int index) {
    if (index < 0 || index >= static_cast<int>(fliplist_.size())) return;

    fliplist_.erase(fliplist_.begin() + index);

    // Adjust current index
    if (fliplist_.empty()) {
        fliplist_index_ = -1;
    } else if (fliplist_index_ >= static_cast<int>(fliplist_.size())) {
        fliplist_index_ = static_cast<int>(fliplist_.size()) - 1;
    }
}

void Drive1541Device::fliplist_clear() {
    fliplist_.clear();
    fliplist_index_ = -1;
}

bool Drive1541Device::flip_next() {
    if (fliplist_.empty()) return false;

    fliplist_index_++;
    if (fliplist_index_ >= static_cast<int>(fliplist_.size())) {
        fliplist_index_ = 0;  // Wrap around
    }

    return swap_disk(fliplist_[fliplist_index_].c_str());
}

bool Drive1541Device::flip_prev() {
    if (fliplist_.empty()) return false;

    fliplist_index_--;
    if (fliplist_index_ < 0) {
        fliplist_index_ = static_cast<int>(fliplist_.size()) - 1;  // Wrap
    }

    return swap_disk(fliplist_[fliplist_index_].c_str());
}

// ============================================================================
// D64 LOW-LEVEL ACCESS
// ============================================================================

uint8_t Drive1541Device::sectors_per_track(uint8_t track) const {
    // Standard D64 sector counts per track zone
    if (track >= 1 && track <= 17) return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 40) return 17;
    return 0;
}

uint32_t Drive1541Device::track_sector_to_offset(uint8_t track, uint8_t sector) const {
    if (track == 0 || track > 40) return 0;

    uint32_t offset = 0;
    for (uint8_t t = 1; t < track; t++) {
        offset += static_cast<uint32_t>(sectors_per_track(t)) * 256;
    }
    offset += static_cast<uint32_t>(sector) * 256;
    return offset;
}

bool Drive1541Device::read_sector(uint8_t track, uint8_t sector, uint8_t* buffer) {
    if (!disk_inserted_ || disk_image_.empty()) return false;
    if (sector >= sectors_per_track(track)) return false;

    uint32_t offset = track_sector_to_offset(track, sector);
    if (offset + 256 > disk_image_.size()) return false;

    memcpy(buffer, disk_image_.data() + offset, 256);
    return true;
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

void Drive1541Device::set_error(int code, const char* message, int track, int sector) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%02d,%s,%02d,%02d\r", code, message, track, sector);
    error_message_ = buf;

    // Update error channel (channel 15)
    DriveChannel& ch = channels_[15];
    ch.buffer.assign(error_message_.begin(), error_message_.end());
    ch.position = 0;
    ch.eof = false;
    ch.open = true;
}

bool Drive1541Device::open_file(DriveChannel& channel, const std::string& filename) {
    if (!disk_inserted_) {
        set_error(74, "DRIVE NOT READY");
        return false;
    }

    // Special case: "$" loads the directory listing
    if (filename == "$" || filename.empty()) {
        load_directory(channel);
        return true;
    }

    // Parse filename: strip leading zeros, handle wildcards etc.
    // For now, simple exact match against directory entries.
    std::string search_name = filename;

    // Strip file type suffix (e.g., ",P" or ",S")
    auto comma = search_name.find(',');
    if (comma != std::string::npos) {
        search_name = search_name.substr(0, comma);
    }

    // Read directory to find the file
    uint8_t sector_buf[256];
    uint8_t dir_track = DIR_TRACK;
    uint8_t dir_sector = DIR_SECTOR;

    while (dir_track != 0) {
        if (!read_sector(dir_track, dir_sector, sector_buf)) {
            set_error(21, "READ ERROR", dir_track, dir_sector);
            return false;
        }

        // Process 8 directory entries per sector (32 bytes each)
        for (int entry = 0; entry < 8; entry++) {
            uint8_t* e = sector_buf + entry * 32;
            uint8_t file_type = e[2] & 0x07;

            if (file_type == 0) continue;  // Deleted/scratched

            // Extract 16-byte filename (PETSCII, padded with $A0)
            char entry_name[17];
            for (int i = 0; i < 16; i++) {
                uint8_t ch = e[5 + i];
                if (ch == 0xA0) { entry_name[i] = '\0'; break; }
                entry_name[i] = static_cast<char>(ch);
                entry_name[i + 1] = '\0';
            }

            // Simple match (case-insensitive for ASCII range)
            bool match = false;
            if (search_name == "*") {
                // Wildcard: match first PRG file
                match = (file_type == 2);  // PRG
            } else {
                // Compare PETSCII (upper-case) with search name
                std::string entry_str(entry_name);
                std::string upper_search = search_name;
                // Convert both to upper case for comparison
                for (auto& c : entry_str) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
                for (auto& c : upper_search) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
                match = (entry_str == upper_search);
            }

            if (match) {
                // Found the file — read all its data sectors
                uint8_t file_track = e[3];
                uint8_t file_sector = e[4];
                uint16_t file_blocks = e[30] | (e[31] << 8);

                channel.buffer.clear();
                channel.position = 0;
                channel.eof = false;

                while (file_track != 0) {
                    uint8_t file_buf[256];
                    if (!read_sector(file_track, file_sector, file_buf)) {
                        set_error(21, "READ ERROR", file_track, file_sector);
                        return false;
                    }

                    uint8_t next_track = file_buf[0];
                    uint8_t next_sector = file_buf[1];

                    if (next_track == 0) {
                        // Last sector: next_sector = number of valid bytes
                        channel.buffer.insert(channel.buffer.end(),
                                              file_buf + 2, file_buf + 1 + next_sector);
                    } else {
                        channel.buffer.insert(channel.buffer.end(),
                                              file_buf + 2, file_buf + 256);
                    }

                    file_track = next_track;
                    file_sector = next_sector;
                }

                channel.open = true;
                set_error(0, "OK");
                printf("1541: Opened '%s' (%zu bytes, %d blocks)\n",
                       entry_name, channel.buffer.size(), file_blocks);
                return true;
            }
        }

        // Follow chain to next directory sector
        dir_track = sector_buf[0];
        dir_sector = sector_buf[1];
    }

    // File not found
    return false;
}

void Drive1541Device::load_directory(DriveChannel& channel) {
    if (!disk_inserted_) return;

    channel.buffer.clear();
    channel.position = 0;
    channel.eof = false;

    // Build BASIC-format directory listing (as the C64 expects it)
    // Start address: $0401 (or $0801 — we use $0401 as VICE does)
    channel.buffer.push_back(0x01);  // Load address low
    channel.buffer.push_back(0x04);  // Load address high

    // Read BAM sector for disk name
    uint8_t bam[256];
    if (!read_sector(BAM_TRACK, BAM_SECTOR, bam)) return;

    // First line: disk name (line number = 0)
    // BASIC line: [next ptr lo] [next ptr hi] [line# lo] [line# hi] [data...] [0x00]
    auto add_line = [&](uint16_t line_number, const char* text) {
        // Placeholder next-line pointer (fixed up later or 0)
        size_t ptr_pos = channel.buffer.size();
        channel.buffer.push_back(0x01);  // Next line ptr low (placeholder)
        channel.buffer.push_back(0x04);  // Next line ptr high (placeholder)

        channel.buffer.push_back(static_cast<uint8_t>(line_number & 0xFF));
        channel.buffer.push_back(static_cast<uint8_t>((line_number >> 8) & 0xFF));

        // Copy text bytes
        while (*text) {
            channel.buffer.push_back(static_cast<uint8_t>(*text++));
        }
        channel.buffer.push_back(0x00);  // End of BASIC line

        // Fix up next-line pointer
        uint16_t next_addr = static_cast<uint16_t>(0x0401 + channel.buffer.size() - 2);
        channel.buffer[ptr_pos] = static_cast<uint8_t>(next_addr & 0xFF);
        channel.buffer[ptr_pos + 1] = static_cast<uint8_t>((next_addr >> 8) & 0xFF);
    };

    // Disk name line
    char disk_name_line[40];
    char disk_name[17] = {};
    for (int i = 0; i < 16; i++) {
        uint8_t ch = bam[0x90 + i];
        disk_name[i] = (ch == 0xA0) ? ' ' : static_cast<char>(ch);
    }
    char disk_id[6] = {};
    for (int i = 0; i < 5; i++) {
        uint8_t ch = bam[0xA2 + i];
        disk_id[i] = (ch == 0xA0) ? ' ' : static_cast<char>(ch);
    }
    snprintf(disk_name_line, sizeof(disk_name_line), "\x12\"%-16s\" %s", disk_name, disk_id);
    add_line(0, disk_name_line);

    // Read directory entries
    uint8_t dir_track = DIR_TRACK;
    uint8_t dir_sector = DIR_SECTOR;
    uint8_t sector_buf[256];

    while (dir_track != 0) {
        if (!read_sector(dir_track, dir_sector, sector_buf)) break;

        for (int entry = 0; entry < 8; entry++) {
            uint8_t* e = sector_buf + entry * 32;
            uint8_t file_type = e[2] & 0x07;
            if (file_type == 0) continue;

            char fname[17] = {};
            for (int i = 0; i < 16; i++) {
                uint8_t ch = e[5 + i];
                fname[i] = (ch == 0xA0) ? ' ' : static_cast<char>(ch);
            }

            uint16_t blocks = e[30] | (e[31] << 8);

            const char* type_str = "???";
            switch (file_type) {
                case 1: type_str = "SEQ"; break;
                case 2: type_str = "PRG"; break;
                case 3: type_str = "USR"; break;
                case 4: type_str = "REL"; break;
            }
            bool locked = (e[2] & 0x40) != 0;
            bool closed = (e[2] & 0x80) != 0;

            char line[48];
            snprintf(line, sizeof(line), "   \"%-16s\" %s%s",
                     fname, type_str, locked ? "<" : (closed ? " " : "*"));
            add_line(blocks, line);
        }

        dir_track = sector_buf[0];
        dir_sector = sector_buf[1];
    }

    // "blocks free" line
    // Count free blocks from BAM
    int free_blocks = 0;
    for (int t = 1; t <= 35; t++) {
        if (t == 18) continue;  // Skip directory track
        int bam_offset = 4 * t;
        if (bam_offset < 256) {
            free_blocks += bam[bam_offset];
        }
    }

    char free_line[32];
    snprintf(free_line, sizeof(free_line), "BLOCKS FREE.");
    add_line(static_cast<uint16_t>(free_blocks), free_line);

    // End-of-program marker
    channel.buffer.push_back(0x00);
    channel.buffer.push_back(0x00);

    channel.open = true;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void Drive1541Device::render_device_ui() {
    ImGui::Text("Device #%d", device_number_);

    if (disk_inserted_) {
        // Show just the filename
        const char* fname = disk_path_.c_str();
        const char* sep = strrchr(fname, '/');
        if (!sep) sep = strrchr(fname, '\\');
        ImGui::Text("Disk: %s", sep ? sep + 1 : fname);

        if (drive_led_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "[LED]");
        }

        if (ImGui::Button("Eject")) eject_disk();

        // Fliplist navigation (same line if space permits)
        if (!fliplist_.empty() && fliplist_.size() > 1) {
            ImGui::SameLine();
            if (ImGui::Button("<")) flip_prev();
            ImGui::SameLine();
            ImGui::Text("%d/%d", fliplist_index_ + 1,
                        static_cast<int>(fliplist_.size()));
            ImGui::SameLine();
            if (ImGui::Button(">")) flip_next();
        }
    } else {
        ImGui::TextDisabled("No disk inserted");
    }

    // Fliplist display (collapsible)
    if (!fliplist_.empty()) {
        if (ImGui::TreeNode("Fliplist")) {
            int remove_idx = -1;
            for (int i = 0; i < static_cast<int>(fliplist_.size()); i++) {
                ImGui::PushID(i);

                bool is_current = (i == fliplist_index_);

                // Quick-swap button
                if (is_current) {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), ">");
                } else {
                    if (ImGui::SmallButton(">")) {
                        fliplist_index_ = i;
                        swap_disk(fliplist_[i].c_str());
                    }
                }
                ImGui::SameLine();

                // Show filename
                const char* path = fliplist_[i].c_str();
                const char* sep = strrchr(path, '/');
                if (!sep) sep = strrchr(path, '\\');
                ImGui::Text("%s", sep ? sep + 1 : path);

                // Remove button
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) {
                    remove_idx = i;
                }

                ImGui::PopID();
            }

            if (remove_idx >= 0) fliplist_remove(remove_idx);

            if (ImGui::SmallButton("Clear All")) fliplist_clear();

            ImGui::TreePop();
        }
    }

    ImGui::TextWrapped("Status: %s", error_message_.c_str());
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor drive_1541_descriptor = {
    "1541",
    "1541 Disk Drive",
    "Commodore 1541 floppy disk drive — connects via IEC serial bus, reads D64 images",
    ConnectorType::IEC_SERIAL,
    true   // Bus device: multiple can share the IEC bus
};

REGISTER_DEVICE(drive_1541_descriptor, []() {
    return std::make_unique<Drive1541Device>(8);
})
