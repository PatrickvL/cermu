/*
 * nes_system.cpp - Complete NES System Implementation
 *
 * This file implements the complete Nintendo Entertainment System with
 * hardware-accurate components and precise timing.
 */

#include "nes_system.h"
#include "nsf/nes_nsf_player.h"
#include "nsf/nes_nsf_cartridge.h"
#include "../../core/formats/nsf_format.h"
#include "../../core/formats/ines_format.h"
#include "../../core/vfs/vfs.h"
// CPU is now a native ChipBase (via fam65xx_t<Traits> inheritance)
#include "../../core/chip.h"
#include "../../chip/input/cd4021.h"
#include "../../chip/memory/memory_chip.h"
#include "../../devices/input/nes_standard_controller.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

#ifdef IMGUI_VERSION
#include "imgui.h"
#include <SDL.h>
#endif

namespace nes_system {

// PPU implementation is now in ppu/nes_ppu.cpp
// Palette LUT is now private to nes_ppu.cpp
// Cartridge implementation is now in cartridge/nes_cartridge.cpp
// Mapper implementations are now in cartridge/mappers/ headers
// MemoryBus removed in Phase 2 — dispatch is now inline in NintendoSystem::clock()

// ============================================================================
// MAIN NES SYSTEM IMPLEMENTATION
// ============================================================================

// Hardware traits definition
static HardwareTraits create_nes_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - NES PPU
    traits.display.native_width = 256;
    traits.display.native_height = 240;
    traits.display.visible_width = 256;
    traits.display.visible_height = 240;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 64;       // 64 colors
    traits.display.pixel_aspect_ratio = 8.0f / 7.0f;  // NTSC pixel aspect
    traits.display.has_overscan = true;
    
    // NES palette (simplified - first 16 colors)
    const uint32_t nes_colors[16] = {
        0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC,
        0x940084, 0xA80020, 0xA81000, 0x881400,
        0x503000, 0x007800, 0x006800, 0x005800,
        0x004058, 0x000000, 0x000000, 0x000000
    };
    
    for (int i = 0; i < 16; i++) {
        uint32_t c = nes_colors[i];
        traits.display.default_palette.push_back(
            PaletteColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255)
        );
    }
    
    // Audio traits - NES APU (2A03)
    traits.audio.format = AudioFormat::MONO_16BIT;
    traits.audio.sample_rate_hz = 44100;
    traits.audio.channels = 1;
    traits.audio.chip_name = "RP2A03 APU";
    
    // Timing - NTSC version
    traits.timing.cpu_frequency_hz = 1789773;   // ~1.79 MHz
    traits.timing.video_frequency_hz = 5369318; // PPU is 3x CPU
    traits.timing.audio_sample_rate_hz = 44100;
    traits.timing.target_fps = 60;
    traits.timing.cycles_per_frame = 29829;     // 1789773 / 60
    traits.timing.standard = VideoStandard::NTSC;
    
    // Memory options (NES has fixed 2KB RAM)
    traits.memory_options.push_back({
        "2KB RAM (Standard)",
        2048,
        0,
        true
    });
    
    // Region options
    traits.video_standard_configs.push_back({
        "NTSC",
        VideoStandard::NTSC,
        traits.timing,
        true
    });
    
    SystemTiming pal_timing = traits.timing;
    pal_timing.cpu_frequency_hz = 1662607;      // ~1.66 MHz (PAL)
    pal_timing.video_frequency_hz = 4987821;    // PPU is 3x CPU
    pal_timing.target_fps = 50;
    pal_timing.cycles_per_frame = 33252;        // 1662607 / 50
    pal_timing.standard = VideoStandard::PAL;
    
    traits.video_standard_configs.push_back({
        "PAL",
        VideoStandard::PAL,
        pal_timing,
        false
    });
    
    return traits;
}

// ============================================================================
// NES file probe — unified confidence + configuration detection
// ============================================================================

static SystemProbeResult nes_probe_file(
    const format_descriptor_t* matched_format,
    const char* /*filepath*/,
    const uint8_t* data, size_t size)
{
    SystemProbeResult result;

    if (!matched_format) return result;

    if (matched_format == &INES_FORMAT_DESCRIPTOR) {
        // iNES ROM
        if (size >= 16 && data[0] == 'N' && data[1] == 'E' &&
            data[2] == 'S' && data[3] == 0x1A) {
            result.confidence = 1.0f;

            // Detect PAL/NTSC from iNES header
            bool is_ines2 = ((data[7] & 0x0C) == 0x08);
            if (is_ines2) {
                // iNES 2.0: byte 12, bits 0-1
                uint8_t timing = data[12] & 0x03;
                if (timing == 1)
                    result.configuration.region_option_index = 1;  // PAL
            } else {
                // iNES 1.0: byte 9, bit 0
                if (data[9] & 0x01)
                    result.configuration.region_option_index = 1;  // PAL
            }
        } else {
            result.confidence = 0.9f;  // Extension match, no header
        }

    } else if (matched_format == &NSF_FORMAT_DESCRIPTOR) {
        // NSF music file
        if (size >= 128 && data[0] == 'N' && data[1] == 'E' &&
            data[2] == 'S' && data[3] == 'M' && data[4] == 0x1A) {
            result.confidence = 1.0f;
            // NSF byte 0x7A: PAL/NTSC flags
            // bit 0: 0=NTSC, 1=PAL; bit 1: dual-compatible
            if (size > 0x7A && (data[0x7A] & 0x01) && !(data[0x7A] & 0x02))
                result.configuration.region_option_index = 1;  // PAL-only
        } else {
            result.confidence = 0.9f;
        }
    }

    return result;
}

// ============================================================================
// NintendoSystem static descriptor (function-local static, same pattern as TED)
// ============================================================================

template<NintendoVariant V>
const SystemDescriptor& NintendoSystem<V>::static_descriptor() {
    static const format_descriptor_t* const formats[] = {
        &INES_FORMAT_DESCRIPTOR, &NSF_FORMAT_DESCRIPTOR, nullptr
    };
    static const SystemDescriptor desc = {
        Traits::full_name,
        Traits::short_id,
        Traits::description,
        formats,
        create_nes_hardware_traits(),
        nes_probe_file
    };
    return desc;
}

template<NintendoVariant V>
NintendoSystem<V>::NintendoSystem()
    : EmulatedSystem()
    , cpu_(nullptr)
    , pins_(0)
    , is_pal_(false)
    , system_ready_(false)
    , cycles_per_frame_(29829)
    , initialized_(false)
    , audio_sample_rate_(44100)
    , audio_sample_counter_(0)
    , residual_time_(0.0)
{
    hardware_traits_ = create_nes_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
}

template<NintendoVariant V>
NintendoSystem<V>::~NintendoSystem() {
    shutdown();
}

template<NintendoVariant V>
const SystemDescriptor& NintendoSystem<V>::get_descriptor() const {
    return static_descriptor();
}

template<NintendoVariant V>
bool NintendoSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    
    // Check if region changed and update cached target FPS
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        cached_target_fps_ = hardware_traits_.video_standard_configs[config_.region_option_index].timing.target_fps;
        bool new_is_pal = (hardware_traits_.video_standard_configs[config_.region_option_index].standard == VideoStandard::PAL);
        if (new_is_pal != is_pal_) {
            is_pal_ = new_is_pal;
            // Need to recreate system with new region
            if (initialized_) {
                shutdown();
                initialize();
            }
        }
    }
    
    return true;
}

template<NintendoVariant V>
bool NintendoSystem<V>::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        const VideoStandardConfig& std_cfg = hardware_traits_.video_standard_configs[config_.region_option_index];
        cycles_per_frame_ = std_cfg.timing.cycles_per_frame;
    }
    
    return true;
}

template<NintendoVariant V>
bool NintendoSystem<V>::initialize() {
    if (initialized_) {
        return true;
    }
    
    printf("%s: Initializing system (%s)\n", Traits::name,
           is_pal_ ? "PAL" : "NTSC");
    
    // Create CPU with integrated APU
    cpu_ = new RICOH_2A03();
    if (!cpu_) {
        printf("%s: Failed to create CPU\n", Traits::name);
        return false;
    }
    
    // Initialize CPU
    cpu_->init();
    
    // Set APU region
    cpu_->set_apu_region(is_pal_);
    
    // Create PPU
    ppu_ = std::make_shared<PPU>(is_pal_);
    
    // Initialize page-pointer bus
    bus_.init();

    // Connect bus to PPU for page-pointer VRAM access
    ppu_->connect_bus(&bus_);
    
    setup_audio_timing();
    setup_connector_ports();

    // Register chips for the Hardware menu and debug windows
    register_nes_chips();

    initialized_ = true;
    
    return true;
}

template<NintendoVariant V>
void NintendoSystem<V>::shutdown() {
    // Save battery-backed SRAM before shutdown
    if (cartridge_ && cartridge_->battery_backed) {
        cartridge_->save_sram(cartridge_->sram_path_for_rom(cartridge_->get_rom_filepath()));
    }
    if (cpu_) {
        printf("%s: Shutting down system\n", Traits::name);
        delete cpu_;
        cpu_ = nullptr;
    }
    initialized_ = false;
    system_ready_ = false;
}

template<NintendoVariant V>
void NintendoSystem<V>::reset() {
    if (!cpu_) return;
    
    printf("%s: Resetting system\n", Traits::name);
    
    pins_ = NES_BUS_DEFAULT_STATE;
    cpu_->reset(pins_);
    
    if (ppu_) {
        ppu_->reset();
    }
    
    bus_.reset();
    
    if (cartridge_) {
        cartridge_->reset();
        cartridge_->update_bank_map(&bus_, ppu_->vram.data());
    }
    
    // Read reset vector from $FFFC/$FFFD and set CPU PC
    {
        uint8_t lo = peek_memory(0xFFFC);
        uint8_t hi = peek_memory(0xFFFD);
        uint16_t reset_vector = lo | (hi << 8);
        cpu_->set(REG_PC, reset_vector);
        cpu_->set(REG_AB, reset_vector);
        printf("%s: Reset vector $%04X\n", Traits::name, reset_vector);
    }
    
    total_cycles_ = 0;
    residual_time_ = 0.0;
    audio_sample_counter_ = 0;
}

template<NintendoVariant V>
void NintendoSystem<V>::tick() {
    if (!cpu_) return;
    
    clock();
    // total_cycles_ is updated in clock()
}

template<NintendoVariant V>
void NintendoSystem<V>::run_frame() {
    if (!system_ready_ || !ppu_) return;

    // Sync peripheral device button state into legacy controllers
    // (bus reads $4016/$4017 still use the Controller shift registers)
    for (int p = 0; p < 2 && p < static_cast<int>(connector_ports_.size()); p++) {
        auto* dev = connector_ports_[p]->get_attached_device();
        if (auto* pad = dynamic_cast<NesStandardController*>(dev)) {
            uint8_t state = pad->get_button_state();
            for (int b = 0; b < 8; b++) {
                auto btn = static_cast<Controller::Button>(1 << b);
                controllers_[p].set_button_state(btn, (state >> b) & 1);
            }
        }
    }

    ppu_->frame_complete = false;
    while (!ppu_->frame_complete) {
        clock();
    }

    // Copy PPU screen into the GUI-provided framebuffer so the emu
    // thread's snapshot sees the rendered frame.
    get_framebuffer();

    // Tick all attached peripheral devices
    tick_peripherals();
}

template<NintendoVariant V>
bool NintendoSystem<V>::load_file(const char* filepath) {
    if (!cpu_) {
        if (!initialize()) {
            return false;
        }
    }
    
    printf("%s: Loading file: %s\n", Traits::name, filepath);

    // =========================================================================
    // Single VFS read — reuse buffer for format detection and loading
    // =========================================================================
    size_t file_size = 0;
    uint8_t* file_data = vfs_read_file(filepath, &file_size);
    if (!file_data || file_size == 0) {
        printf("%s: Failed to read file: %s\n", Traits::name, filepath);
        free(file_data);
        return false;
    }

    // Check for NSF by extension or header magic
    std::string ext_str = vfs_extension(filepath);
    const char* ext = ext_str.empty() ? nullptr : ext_str.c_str();
    bool is_nsf = (ext && (strcasecmp(ext, ".nsf") == 0));

    if (!is_nsf && file_size >= 5) {
        if (file_data[0] == 'N' && file_data[1] == 'E' && file_data[2] == 'S' &&
            file_data[3] == 'M' && file_data[4] == 0x1A) {
            is_nsf = true;
        }
    }

    // =========================================================================
    // NSF FILE — parse from buffer, then launch NSF player
    // =========================================================================
    if (is_nsf) {
        // Parse NSF header
        nsf_header_t header;
        if (!nsf_parse_header(file_data, file_size, &header)) {
            printf("%s: Invalid NSF header\n", Traits::name);
            free(file_data);
            return false;
        }

        // Extract payload
        const uint8_t* payload = file_data + 128;
        size_t payload_size = file_size - 128;

        program_data_t prog = {};
        prog.data = const_cast<uint8_t*>(payload);  // Temporary, won't be freed
        prog.data_size = payload_size;
        prog.load_addr = header.load_addr;

        // Compute 0-based subtune index from 1-based start_song
        uint16_t subtune = header.start_song;
        if (subtune > 0) subtune--;

        // Launch NSF player
        bus_.reset();
        nsf_cartridge_ = NsfPlayer::apply_load(
            cpu_, ppu_.get(), bus_.cpu_ram,
            &header, &prog, subtune, is_pal_);

        if (!nsf_cartridge_) {
            printf("%s: Failed to apply NSF load\n", Traits::name);
            free(file_data);
            return false;
        }

        // Connect NSF cartridge to system
        cartridge_ = nsf_cartridge_;
        ppu_->connect_cartridge(cartridge_);
        cartridge_->update_bank_map(&bus_, ppu_->vram.data());

        // Save state for subtune switching
        active_nsf_header_ = header;
        active_nsf_data_.assign(payload, payload + payload_size);
        active_nsf_subtune_ = subtune;
        nsf_player_active_ = true;
        system_ready_ = true;

        // Set program title from NSF header (strings are already UTF-8
        // after parsing — Latin-1→UTF-8 conversion happens in nsf_parse_header).
        program_title_ = header.name;
        if (header.artist[0]) {
            program_title_ += " - ";
            program_title_ += header.artist;
        }

        printf("%s: NSF player active — \"%s\" by %s\n",
               Traits::name, header.name, header.artist);
        free(file_data);
        return true;
    }

    // =========================================================================
    // STANDARD PATH — iNES ROM cartridge (load from already-read buffer)
    // =========================================================================
    nsf_player_active_ = false;
    nsf_cartridge_.reset();
    
    try {
        cartridge_ = std::make_shared<Cartridge>();
        if (!cartridge_->load_from_buffer(file_data, file_size, filepath)) {
            printf("%s: Failed to parse cartridge data\n", Traits::name);
            free(file_data);
            return false;
        }
        free(file_data);

        // Connect cartridge to PPU and set up page-pointer bank maps
        ppu_->connect_cartridge(cartridge_);
        cartridge_->update_bank_map(&bus_, ppu_->vram.data());
        
        // Reset system with new cartridge
        reset();
        system_ready_ = true;

        // Set program title to bare filename (VFS-aware)
        std::string fname = vfs_filename(filepath);
        program_title_ = fname.empty() ? filepath : fname;
        
        printf("%s: Cartridge loaded successfully\n", Traits::name);
        return true;
    } catch (const std::exception& e) {
        printf("%s: Failed to load cartridge: %s\n", Traits::name, e.what());
        return false;
    }
}

template<NintendoVariant V>
uint32_t* NintendoSystem<V>::get_framebuffer() {
    if (!ppu_ || !rgba_framebuffer_) return rgba_framebuffer_;
    
    // Get NES screen buffer and copy to our framebuffer
    const std::vector<uint32_t>& nes_screen = ppu_->get_screen();
    if (!nes_screen.empty() && rgba_framebuffer_) {
        // NES screen is 256x240, copy directly
        memcpy(rgba_framebuffer_, nes_screen.data(), 256 * 240 * sizeof(uint32_t));
    }
    
    return rgba_framebuffer_;
}

template<NintendoVariant V>
void NintendoSystem<V>::get_display_dimensions(int* width, int* height) const {
    *width = 256;
    *height = 240;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
}

template<NintendoVariant V>
void NintendoSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
#ifdef IMGUI_VERSION
    // NES has no system keyboard — all keyboard input flows through
    // the attached NesStandardController peripheral devices via
    // process_sdl_event_for_devices().  Nothing to do here.
    (void)key;
    (void)pressed;
#else
    (void)key;
    (void)pressed;
#endif
}

template<NintendoVariant V>
void NintendoSystem<V>::handle_controller_event(int controller, int button, bool pressed) {
    if (controller < 0 || controller >= 2) return;
    
    if (pressed) {
        press_button(controller, static_cast<Controller::Button>(button));
    } else {
        release_button(controller, static_cast<Controller::Button>(button));
    }
}

// =============================================================================
// NSF Player — Extended keyboard handler with subtune selection
// =============================================================================

template<NintendoVariant V>
void NintendoSystem<V>::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode,
                                          uint16_t mod, bool pressed, bool repeat) {
    // NSF player subtune selection — intercept before controller mapping
    if (nsf_player_active_ && pressed && !repeat) {
        if (handle_nsf_player_key(key)) return;
    }

    // Fall through to standard keyboard handling
    if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

// =============================================================================
// NSF Player — Subtune selection via keyboard
// =============================================================================
//
// Digits 1-9:  select subtune 1-9 directly (0-based index 0-8)
// Digit 0:     select subtune 10 (0-based index 9)
// Right arrow:  next subtune (wraps from last → first)
// Left arrow:   previous subtune (wraps from first → last)
// ESC:          exit application
// =============================================================================

template<NintendoVariant V>
bool NintendoSystem<V>::handle_nsf_player_key(SDL_Keycode key) {
    if (!cpu_ || active_nsf_header_.num_songs == 0) return false;

    const uint16_t num_songs = active_nsf_header_.num_songs;
    int new_subtune = -1;

    // Digit keys: 1→subtune 1, ..., 9→subtune 9, 0→subtune 10
    if (key >= SDLK_0 && key <= SDLK_9) {
        int digit = (key == SDLK_0) ? 10 : (key - SDLK_0);
        if (digit <= num_songs) {
            new_subtune = digit - 1;  // Convert to 0-based
        }
    }
    // Cursor right = next subtune (with wrapping)
    else if (key == SDLK_RIGHT) {
        new_subtune = (active_nsf_subtune_ + 1) % num_songs;
    }
    // Cursor left = previous subtune (with wrapping)
    else if (key == SDLK_LEFT) {
        new_subtune = (active_nsf_subtune_ == 0) ? (num_songs - 1)
                                                   : (active_nsf_subtune_ - 1);
    }
    // ESC = exit application while in NSF player mode
    else if (key == SDLK_ESCAPE) {
        request_quit();
        return true;
    }

    if (new_subtune < 0) return false;
    if (static_cast<uint16_t>(new_subtune) == active_nsf_subtune_) return true;

    active_nsf_subtune_ = static_cast<uint16_t>(new_subtune);
    NsfPlayer::switch_subtune(cpu_, ppu_.get(), bus_.cpu_ram,
                            nsf_cartridge_.get(), &active_nsf_header_,
                            active_nsf_data_.data(), active_nsf_data_.size(),
                            active_nsf_subtune_, is_pal_);
    return true;
}

template<NintendoVariant V>
void NintendoSystem<V>::render_system_menu_items() {
#ifdef IMGUI_VERSION
    char reset_label[32];
    snprintf(reset_label, sizeof(reset_label), "Reset %s", Traits::name);
    if (ImGui::MenuItem(reset_label)) {
        reset();
    }
    
    if (ImGui::MenuItem("Eject Cartridge", nullptr, false, is_cartridge_loaded())) {
        eject_cartridge();
    }
#endif
}

template<NintendoVariant V>
const char* NintendoSystem<V>::get_mode_label() const {
    return nsf_player_active_ ? "NSF Player" : nullptr;
}

template<NintendoVariant V>
std::string NintendoSystem<V>::get_subtitle_info() const {
    if (!nsf_player_active_ || active_nsf_header_.num_songs <= 1) return {};
    return "[" + std::to_string(active_nsf_subtune_ + 1) + "/" +
           std::to_string(active_nsf_header_.num_songs) + "]";
}

// ============================================================================
// Chip Registration — populate registered_chips_ for Hardware menu + debug
// ============================================================================

template<NintendoVariant V>
void NintendoSystem<V>::register_nes_chips() {
    auto* cpu = cpu_;

    // CPU (Ricoh 2A03) — native ChipBase, registered directly
    register_chip(static_cast<ChipBase*>(cpu_),
        "Ricoh 2A03 (6502 + APU)", "2A03", "CPU", 0x0000);

    // PPU (Ricoh 2C02) — native ChipBase, registered directly
    register_chip(ppu_.get(),
        "Ricoh 2C02 PPU", "PPU", "Video", 0x2000);

    // APU (built into 2A03) — native ChipBase, registered directly
    register_chip(cpu_->get_apu(),
        "APU (built-in 2A03)", "APU", "Audio", 0x4000);

    // RAM — MemoryChip with layout rendering
    auto ram = std::make_unique<MemoryChip>(
        ChipInfo{"SRAM", "Various"}, 2048, MemoryChip::SRAM, &pins_,
        "RAM", 0x0000);
    ram->bind(bus_.cpu_ram);  // Point at unified bus RAM for live debug view
    register_chip(std::move(ram));

    // CIRAM (2KB nametable VRAM on NES motherboard)
    auto ciram = std::make_unique<MemoryChip>(
        ChipInfo{"SRAM", "Various"}, 2048, MemoryChip::SRAM, &pins_,
        "CIRAM", 0x2000);
    ciram->bind(ppu_->vram.data());  // Point at PPU's CIRAM for live debug view
    register_chip(std::move(ciram));

    // Cartridge — now a proper ChipBase subclass
    if (cartridge_) {
        register_chip(cartridge_.get(),
            "Cartridge", "Cart", "Memory", 0x4020);
    }

    // Controller shift registers (CD4021 × 2)
    auto ctrl1 = std::make_unique<CD4021>();
    register_chip(std::move(ctrl1), "Controller 1 SR", "CTRL1", "Input", 0x4016);

    auto ctrl2 = std::make_unique<CD4021>();
    register_chip(std::move(ctrl2), "Controller 2 SR", "CTRL2", "Input", 0x4017);
}

template<NintendoVariant V>
void NintendoSystem<V>::render_configuration_ui() {
#ifdef IMGUI_VERSION
    ImGui::Text("%s Configuration", Traits::name);
    ImGui::Separator();
    
    // Region configuration
    ImGui::Text("Video Region:");
    for (size_t i = 0; i < hardware_traits_.video_standard_configs.size(); i++) {
        bool selected = (config_.region_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.video_standard_configs[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.region_option_index = static_cast<int>(i);
            set_configuration(new_config);
            apply_configuration();
        }
    }
    
    ImGui::Separator();
    
    // Cartridge info
    if (is_cartridge_loaded()) {
        ImGui::Text("Cartridge: Loaded");
    } else {
        ImGui::TextDisabled("Cartridge: None");
    }
#endif
}

template<NintendoVariant V>
void NintendoSystem<V>::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

template<NintendoVariant V>
uint32_t NintendoSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    uint32_t avail = static_cast<uint32_t>(audio_buffer_.size());
    uint32_t to_copy = avail < max_samples ? avail : max_samples;
    if (to_copy > 0) {
        memcpy(buffer, audio_buffer_.data(), to_copy * sizeof(float));
        // Remove consumed samples (shift remainder to front)
        audio_buffer_.erase(audio_buffer_.begin(),
                            audio_buffer_.begin() + to_copy);
    }
    return to_copy;
}

template<NintendoVariant V>
void NintendoSystem<V>::setup_audio_timing() {
    uint32_t cpu_freq = is_pal_ ? nes_constants::CPU_FREQ_PAL : nes_constants::CPU_FREQ_NTSC;
    audio_samples_per_frame_ = (audio_sample_rate_ * (is_pal_ ? 50 : 60)) / (is_pal_ ? 50 : 60);
}

template<NintendoVariant V>
void NintendoSystem<V>::eject_cartridge() {
    // Save battery-backed SRAM before ejecting
    if (cartridge_ && cartridge_->battery_backed) {
        cartridge_->save_sram(cartridge_->sram_path_for_rom(cartridge_->get_rom_filepath()));
    }
    cartridge_.reset();
    bus_.init();  // Clear page pointers and RAM
    if (ppu_) {
        ppu_->connect_cartridge(nullptr);
    }
    system_ready_ = false;
}

template<NintendoVariant V>
void NintendoSystem<V>::clock() {
    // ====================================================================
    // PPU tick (runs at 3× CPU clock)
    // ====================================================================
    if (ppu_) {
        ppu_->clock();
    }

    // ====================================================================
    // OAM DMA controller — stalls CPU while transferring 256 bytes
    // ====================================================================
    if (unlikely(bus_.dma_transfer)) {
        if (bus_.dma_dummy) {
            if (bus_.system_clock_counter % 2 == 1) {
                bus_.dma_dummy = false;
            }
        } else {
            if (bus_.system_clock_counter % 2 == 0) {
                // DMA read from CPU address space
                uint16_t dma_src = (bus_.dma_page << 8) | bus_.dma_addr;
                bus_.dma_data = bus_.cpu_read(dma_src);
            } else {
                ppu_->oam[bus_.dma_addr] = bus_.dma_data;
                bus_.dma_addr++;
                if (bus_.dma_addr == 0x00) {
                    bus_.dma_transfer = false;
                    bus_.dma_dummy = true;
                }
            }
        }
        bus_.system_clock_counter++;
        total_cycles_++;
        return;
    }

    bus_.system_clock_counter++;

    // ====================================================================
    // CPU tick — one PHI2/PHI1 cycle every 3 PPU ticks
    // ====================================================================
    if (bus_.system_clock_counter % 3 != 0) {
        total_cycles_++;
        return;
    }

    // PHI2: CPU drives address bus and R/W signal
    pins_ = cpu_->tick<RICOH_2A03::Phase::PHI2>(pins_);

    const uint16_t addr = BUS_GET_ADDR(pins_);
    const bool is_read = BUS_GET_BIT(pins_, BUS_RW_BIT);

    // ====================================================================
    // CPU bus dispatch — page-pointer fast path with I/O fallback
    // ====================================================================

    if (is_read) {
        // ---- READ ----
        if (addr < 0x2000) {
            // Fast path: WRAM ($0000-$1FFF) — 2KB mirrored
            BUS_SET_DATA(pins_, bus_.cpu_ram[addr & 0x07FF]);
        } else {
            const uint8_t page = addr >> 12;
            const uint8_t* rp = bus_.cpu_read_page[page];
            if (likely(rp != nullptr)) {
                // Page pointer hit: PRG-ROM, PRG-RAM, or expansion
                BUS_SET_DATA(pins_, rp[addr & 0x0FFF]);
            } else if (page <= 3) {
                // PPU registers ($2000-$3FFF, mirrored every 8 bytes)
                if (ppu_) {
                    pins_ = ppu_->cpu_bus_tick(pins_);
                }
            } else if (page == 4) {
                // APU/IO registers ($4000-$4FFF)
                if (addr == 0x4016) {
                    BUS_SET_DATA(pins_, controllers_[0].read());
                } else if (addr == 0x4017) {
                    BUS_SET_DATA(pins_, controllers_[1].read());
                }
                // Other APU reads ($4015 etc.) handled by CPU PHI1
            } else {
                // Unmapped expansion or cartridge I/O — fall through to
                // cartridge cpu_bus_tick for NSF and unusual mappers
                if (cartridge_) {
                    bool handled = false;
                    pins_ = cartridge_->cpu_bus_tick(pins_, handled);
                }
            }
        }
    } else {
        // ---- WRITE ----
        const uint8_t data = BUS_GET_DATA(pins_);

        if (addr < 0x2000) {
            // Fast path: WRAM ($0000-$1FFF) — 2KB mirrored
            bus_.cpu_ram[addr & 0x07FF] = data;
        } else {
            const uint8_t page = addr >> 12;
            uint8_t* wp = bus_.cpu_write_page[page];
            if (wp != nullptr) {
                // Page pointer hit: PRG-RAM or expansion write
                wp[addr & 0x0FFF] = data;
            } else if (page <= 3) {
                // PPU registers ($2000-$3FFF, mirrored every 8 bytes)
                if (ppu_) {
                    pins_ = ppu_->cpu_bus_tick(pins_);
                }
            } else if (page == 4) {
                // APU/IO registers ($4000-$4FFF)
                if (addr == 0x4014) {
                    // OAM DMA trigger
                    bus_.dma_page = data;
                    bus_.dma_addr = 0x00;
                    bus_.dma_transfer = true;
                } else if (addr == 0x4016) {
                    controllers_[0].write(data);
                    controllers_[1].write(data);
                }
                // Other APU writes ($4000-$4013, $4015, $4017) handled by CPU PHI1
            } else if (page >= 8) {
                // ROM region writes → mapper register dispatch
                if (cartridge_) {
                    if (cartridge_->handle_mapper_write(addr, data)) {
                        cartridge_->update_bank_map(&bus_, ppu_->vram.data());
                    } else {
                        // Mapper didn't claim — fall through to cartridge
                        // bus tick (needed for NsfCartridge self-modifying writes)
                        bool handled = false;
                        pins_ = cartridge_->cpu_bus_tick(pins_, handled);
                    }
                }
            } else {
                // Expansion writes ($5000-$7FFF) not covered by page pointers
                // Fall through to cartridge cpu_bus_tick for NSF etc.
                if (cartridge_) {
                    bool handled = false;
                    pins_ = cartridge_->cpu_bus_tick(pins_, handled);
                }
            }
        }
    }

    // ====================================================================
    // Interrupt wire handling
    // ====================================================================

    // NMI from PPU — edge-sensitive (active low)
    if (ppu_->get_nmi()) {
        BUS_CLR_BIT(pins_, BUS_NMI_BIT);
    } else {
        BUS_SET_BIT(pins_, BUS_NMI_BIT);
    }

    // IRQ — level-sensitive (active low)
    bool irq_asserted = false;
    if (cartridge_ && cartridge_->irq_state()) {
        irq_asserted = true;
        cartridge_->irq_clear();
    }
    if (cpu_->apu_irq()) {
        irq_asserted = true;
    }
    if (irq_asserted) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    } else {
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);
    }

    // PHI1: CPU internal operations (including APU clock)
    pins_ = cpu_->tick<RICOH_2A03::Phase::PHI1>(pins_);

    // ====================================================================
    // Audio sample generation
    // ====================================================================
    if (audio_sample_counter_ == 0) {
        float sample = cpu_->generate_audio_sample();
        audio_buffer_.push_back(sample);
    }
    audio_sample_counter_ = (audio_sample_counter_ + 1) % (is_pal_ ? 33 : 37);

    total_cycles_++;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_controller_state(int controller, uint8_t state) {
    if (controller < 0 || controller >= 2) return;
    
    // Set individual buttons based on state
    for (int i = 0; i < 8; i++) {
        bool pressed = (state >> i) & 1;
        Controller::Button button = static_cast<Controller::Button>(1 << i);
        controllers_[controller].set_button_state(button, pressed);
    }
}

template<NintendoVariant V>
void NintendoSystem<V>::press_button(int controller, Controller::Button button) {
    if (controller < 0 || controller >= 2) return;
    controllers_[controller].set_button_state(button, true);
}

template<NintendoVariant V>
void NintendoSystem<V>::release_button(int controller, Controller::Button button) {
    if (controller < 0 || controller >= 2) return;
    controllers_[controller].set_button_state(button, false);
}

template<NintendoVariant V>
const std::vector<uint32_t>& NintendoSystem<V>::get_screen() const {
    static std::vector<uint32_t> empty_screen;
    return ppu_ ? ppu_->get_screen() : empty_screen;
}

template<NintendoVariant V>
const std::vector<uint32_t>& NintendoSystem<V>::get_pattern_table(int table, uint8_t palette) const {
    static std::vector<uint32_t> empty_table;
    return ppu_ ? ppu_->get_pattern_table(table, palette) : empty_table;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_audio_sample_rate(uint32_t rate) {
    audio_sample_rate_ = rate;
    setup_audio_timing();
}

template<NintendoVariant V>
bool NintendoSystem<V>::save_state(const std::string& filename) const {
    if (!cpu_ || !ppu_) return false;

    std::ofstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;

    // Magic + version
    const char magic[4] = {'C','S','S','1'};  // cermu save state v1
    f.write(magic, 4);

    // CPU state — save pins and the full register file
    f.write(reinterpret_cast<const char*>(&pins_), sizeof(pins_));

    // PPU state
    f.write(reinterpret_cast<const char*>(&ppu_->regs), sizeof(ppu_->regs));
    f.write(reinterpret_cast<const char*>(ppu_->vram.data()), ppu_->vram.size());
    f.write(reinterpret_cast<const char*>(ppu_->oam.data()), ppu_->oam.size());
    f.write(reinterpret_cast<const char*>(ppu_->palette.data()), ppu_->palette.size());
    f.write(reinterpret_cast<const char*>(&ppu_->internal), sizeof(ppu_->internal));
    int16_t sl = ppu_->scanline; f.write(reinterpret_cast<const char*>(&sl), sizeof(sl));
    uint16_t cy = ppu_->cycle;   f.write(reinterpret_cast<const char*>(&cy), sizeof(cy));
    uint64_t fc = ppu_->frame_count; f.write(reinterpret_cast<const char*>(&fc), sizeof(fc));

    // Bus state — CPU RAM
    f.write(reinterpret_cast<const char*>(bus_.cpu_ram), sizeof(bus_.cpu_ram));
    f.write(reinterpret_cast<const char*>(&bus_.dma_page), 1);
    f.write(reinterpret_cast<const char*>(&bus_.dma_addr), 1);
    f.write(reinterpret_cast<const char*>(&bus_.dma_data), 1);
    uint8_t dma_flags = (bus_.dma_transfer ? 1 : 0) | (bus_.dma_dummy ? 2 : 0);
    f.write(reinterpret_cast<const char*>(&dma_flags), 1);
    f.write(reinterpret_cast<const char*>(&bus_.system_clock_counter), sizeof(bus_.system_clock_counter));

    // PRG RAM (if present)
    if (cartridge_ && !cartridge_->prg_ram.empty()) {
        uint32_t ram_size = static_cast<uint32_t>(cartridge_->prg_ram.size());
        f.write(reinterpret_cast<const char*>(&ram_size), sizeof(ram_size));
        f.write(reinterpret_cast<const char*>(cartridge_->prg_ram.data()), ram_size);
    } else {
        uint32_t zero = 0;
        f.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
    }

    printf("%s: Saved state to %s\n", Traits::name, filename.c_str());
    return true;
}

template<NintendoVariant V>
bool NintendoSystem<V>::load_state(const std::string& filename) {
    if (!cpu_ || !ppu_) return false;

    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;

    char magic[4];
    f.read(magic, 4);
    if (magic[0] != 'C' || magic[1] != 'S' || magic[2] != 'S' || magic[3] != '1') {
        printf("%s: Invalid save state file\n", Traits::name);
        return false;
    }

    // CPU pins
    f.read(reinterpret_cast<char*>(&pins_), sizeof(pins_));

    // PPU state
    f.read(reinterpret_cast<char*>(&ppu_->regs), sizeof(ppu_->regs));
    f.read(reinterpret_cast<char*>(ppu_->vram.data()), ppu_->vram.size());
    f.read(reinterpret_cast<char*>(ppu_->oam.data()), ppu_->oam.size());
    f.read(reinterpret_cast<char*>(ppu_->palette.data()), ppu_->palette.size());
    f.read(reinterpret_cast<char*>(&ppu_->internal), sizeof(ppu_->internal));
    int16_t sl; f.read(reinterpret_cast<char*>(&sl), sizeof(sl)); ppu_->scanline = sl;
    uint16_t cy; f.read(reinterpret_cast<char*>(&cy), sizeof(cy)); ppu_->cycle = cy;
    uint64_t fc; f.read(reinterpret_cast<char*>(&fc), sizeof(fc)); ppu_->frame_count = fc;

    // Bus state
    f.read(reinterpret_cast<char*>(bus_.cpu_ram), sizeof(bus_.cpu_ram));
    f.read(reinterpret_cast<char*>(&bus_.dma_page), 1);
    f.read(reinterpret_cast<char*>(&bus_.dma_addr), 1);
    f.read(reinterpret_cast<char*>(&bus_.dma_data), 1);
    uint8_t dma_flags; f.read(reinterpret_cast<char*>(&dma_flags), 1);
    bus_.dma_transfer = (dma_flags & 1) != 0;
    bus_.dma_dummy = (dma_flags & 2) != 0;
    f.read(reinterpret_cast<char*>(&bus_.system_clock_counter), sizeof(bus_.system_clock_counter));

    // PRG RAM
    uint32_t ram_size = 0;
    f.read(reinterpret_cast<char*>(&ram_size), sizeof(ram_size));
    if (ram_size > 0 && cartridge_ && cartridge_->prg_ram.size() >= ram_size) {
        f.read(reinterpret_cast<char*>(cartridge_->prg_ram.data()), ram_size);
    }

    printf("%s: Loaded state from %s\n", Traits::name, filename.c_str());
    return true;
}

template<NintendoVariant V>
void NintendoSystem<V>::power_cycle() {
    eject_cartridge();
    reset();
}

// ============================================================================
// CONNECTOR PORT SETUP — NES
// ============================================================================
// NES has: 2× front controller ports (7-pin) and 1× bottom expansion port (48-pin).
// Controller ports use a serial shift-register protocol (LATCH + CLK + D0).
// Connector definitions are now in src/connectors/nes_connectors.h (shared).
#include "../../connectors/nes_connectors.h"

template<NintendoVariant V>
void NintendoSystem<V>::setup_connector_ports() {
    connector_ports_.clear();

    if constexpr (Traits::is_famicom) {
        // Famicom: hardwired controllers, 15-pin expansion port
        add_connector_port(NesConnectors::FC_CONTROLLER_1, 1);
        add_connector_port(NesConnectors::FC_CONTROLLER_2, 2);
        add_connector_port(NesConnectors::FC_EXPANSION, 0);
        printf("%s: Created %zu connector ports\n", Traits::name, connector_ports_.size());
    } else {
        // NES: removable controller ports, bottom expansion
        add_connector_port(NesConnectors::NES_CONTROLLER_1, 1);
        add_connector_port(NesConnectors::NES_CONTROLLER_2, 2);
        add_connector_port(NesConnectors::NES_EXPANSION, 0);
        printf("%s: Created %zu connector ports\n", Traits::name, connector_ports_.size());
    }

    // Attach default peripherals declared by get_default_peripherals().
    // auto_bind_host_inputs() (called from attach_device_to_port) assigns
    // connected host gamepads first; if none are available it falls back to
    // keyboard.
    attach_default_peripherals();

    // Set per-player keyboard maps — WASD for P1, IJKL for P2.
    // (Only effective when binding.type == KEYBOARD.)
    for (int p = 0; p < 2 && p < static_cast<int>(connector_ports_.size()); p++) {
        auto* dev = connector_ports_[p]->get_attached_device();
        if (auto* pad = dynamic_cast<NesStandardController*>(dev)) {
            pad->set_key_map(p == 0 ? nes_keymap_wasd() : nes_keymap_ijkl());
        }
    }
}

template<NintendoVariant V>
std::vector<EmulatedSystem::DefaultPeripheral>
NintendoSystem<V>::get_default_peripherals() const {
    return {
        { 0, "nes_gamepad" },   // Controller Port 1
        { 1, "nes_gamepad" },   // Controller Port 2
    };
}

// ============================================================================
// Debug / Test harness helpers
// ============================================================================

template<NintendoVariant V>
uint8_t NintendoSystem<V>::peek_memory(uint16_t addr) const {
    // $0000-$1FFF: CPU RAM (mirrored every 2KB)
    if (addr < 0x2000) {
        return bus_.cpu_ram[addr & 0x07FF];
    }

    // $2000-$3FFF: PPU registers (read-only peek)
    if (addr >= 0x2000 && addr <= 0x3FFF && ppu_) {
        return ppu_->cpu_peek(addr);
    }

    // $4000-$5FFF: APU/IO — no side-effect-free peek available
    // Try page pointers for $6000+
    if (addr >= 0x6000) {
        const uint8_t* rp = bus_.cpu_read_page[addr >> 12];
        if (rp != nullptr) {
            return rp[addr & 0x0FFF];
        }
    }

    // Fall through to cartridge for unmapped ranges
    if (addr >= 0x6000 && cartridge_) {
        return cartridge_->peek(addr);
    }

    return 0;
}

template<NintendoVariant V>
uint16_t NintendoSystem<V>::get_cpu_pc() const {
    if (!cpu_) return 0;
    return static_cast<uint16_t>(cpu_->get(REG_PC));
}

} // namespace nes_system

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

template class nes_system::NintendoSystem<nes_system::NintendoVariant::NES>;
template class nes_system::NintendoSystem<nes_system::NintendoVariant::FAMICOM>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(nes_system::NESSystem::static_descriptor(), []() {
    return std::make_unique<nes_system::NESSystem>();
})

REGISTER_SYSTEM(nes_system::FamicomSystem::static_descriptor(), []() {
    return std::make_unique<nes_system::FamicomSystem>();
})