/*
 * nes_system.cpp - Complete NES System Implementation
 *
 * This file implements the complete Nintendo Entertainment System with
 * hardware-accurate components and precise timing.
 */

#include "nes_system.h"
#include "ppu/nes_palette.h"
#include "nsf/nes_nsf_player.h"
#include "cartridge/mappers/mapper_nsf.h"
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

#include "nes_profiling.h"

#ifdef NES_PROFILING
NesProfileCounters g_nes_profile;
#endif

#ifdef CERMU_HAS_GUI
#include "imgui.h"
#include <SDL.h>
#endif

namespace nes_system {

// PPU implementation is now in ppu/nes_ppu.cpp
// Palette LUT is in ppu/nes_palette.h (shared header)
// Cartridge implementation is now in cartridge/nes_cartridge.cpp
// Mapper implementations are now in cartridge/mappers/ headers
// MemoryBus removed in Phase 2 — dispatch is now inline in NintendoSystem::tick()

// ============================================================================
// MAIN NES SYSTEM IMPLEMENTATION
// ============================================================================

// Hardware traits definition
static HardwareTraits create_nes_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - NES PPU
    traits.display.native_width = 256;
    traits.display.native_height = 240;
    traits.display.visible_width = 248;   // Crop left 8px (CRT overscan)
    traits.display.visible_height = 240;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 64;       // 64 colors
    traits.display.pixel_aspect_ratio = 8.0f / 7.0f;  // NTSC pixel aspect
    traits.display.has_overscan = true;
    
    // Full 64-color NES palette from NES_COLOR_TABLE (ABGR: 0xAABBGGRR)
    for (int i = 0; i < 64; i++) {
        uint32_t c = NES_COLOR_TABLE[i];
        traits.display.default_palette.push_back(
            PaletteColor(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, (c >> 24) & 0xFF)
        );
    }
    
    // Audio traits - NES APU (2A03)
    traits.audio.format = AudioFormat::MONO_16BIT;
    traits.audio.sample_rate_hz = nes_constants::AUDIO_SAMPLE_RATE;
    traits.audio.channels = 1;
    traits.audio.chip_name = "RP2A03 APU";
    
    // Timing - NTSC version
    traits.timing.cpu_frequency_hz = nes_constants::CPU_FREQ_NTSC;
    traits.timing.video_frequency_hz = 5369318;
    traits.timing.audio_sample_rate_hz = nes_constants::AUDIO_SAMPLE_RATE;
    traits.timing.target_fps = 60;
    traits.timing.cycles_per_frame = nes_constants::CYCLES_PER_FRAME_NTSC;
    traits.timing.standard = VideoStandard::NTSC;
    
    // Memory options (NES has fixed 2KB RAM)
    traits.memory_options.push_back({
        "2KB RAM (Standard)",
        nes_constants::CPU_RAM_SIZE,
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
    pal_timing.cpu_frequency_hz = nes_constants::CPU_FREQ_PAL;
    pal_timing.video_frequency_hz = 4987821;
    pal_timing.target_fps = 50;
    pal_timing.cycles_per_frame = 33252;
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
    , cycles_per_frame_(nes_constants::CYCLES_PER_FRAME_NTSC)
    , initialized_(false)
    , audio_sample_rate_(nes_constants::AUDIO_SAMPLE_RATE)
    , audio_sample_counter_(37)  // NTSC default; reset() will recalculate
    , audio_sample_period_(37)   // NTSC default
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
        // Sync PRG-RAM from unified buffer back to cartridge vector for save
        if (bus_.prg_ram && bus_.prg_ram_size > 0 && !cartridge_->prg_ram.empty()) {
            std::memcpy(cartridge_->prg_ram.data(), bus_.prg_ram,
                        std::min(static_cast<size_t>(bus_.prg_ram_size),
                                 cartridge_->prg_ram.size()));
        }
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

    // Reset DMA + clock state
    dma_page_ = 0;
    dma_addr_ = 0;
    dma_data_ = 0;
    dma_transfer_ = false;
    dma_dummy_ = true;
    system_clock_counter_ = 0;
    cpu_div_ = 0;
    dma_odd_cycle_ = false;

    if (cartridge_) {
        cartridge_->reset();
        cartridge_->update_bank_map(&bus_, bus_.ciram);
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
    audio_sample_period_ = is_pal_ ? 33 : 37;
    audio_sample_counter_ = audio_sample_period_;

    // Flush any buffered audio samples so old game audio doesn't bleed
    // into the new cartridge.  The ring buffer in the GUI layer is reset
    // separately when the emulation thread restarts.
    audio_buffer_.clear();
}

template<NintendoVariant V>
void NintendoSystem<V>::run_frame() {
    if (!system_ready_ || !ppu_) return;

    ppu_->frame_complete = false;
    while (!ppu_->frame_complete) {
        tick();
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
    bool is_nsf = (ext && (cermu_strcasecmp(ext, ".nsf") == 0));

    if (!is_nsf && file_size >= 5) {
        if (file_data[0] == 'N' && file_data[1] == 'E' && file_data[2] == 'S' &&
            file_data[3] == 'M' && file_data[4] == 0x1A) {
            is_nsf = true;
        }
    }

    // =========================================================================
    // NSF FILE — parse from buffer, then launch NSF player via standard
    // Cartridge + MapperNsf (same pipeline as iNES ROM loading).
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

        // Compute 0-based subtune index from 1-based start_song
        uint16_t subtune = header.start_song;
        if (subtune > 0) subtune--;

        // Detect bankswitching
        bool bankswitched = false;
        for (int i = 0; i < 8; i++) {
            if (header.bankswitch[i] != 0) { bankswitched = true; break; }
        }

        // ---- Build a standard Cartridge ----
        cartridge_ = std::make_shared<Cartridge>();
        cartridge_->mapper_id = MapperNsf::NSF_MAPPER_ID;
        cartridge_->mirror_mode = Mirror::HORIZONTAL;

        // Populate PRG-ROM: for bankswitched NSFs, round up to 4KB pages.
        // For non-bankswitched, pad from load_addr to $FFFF.
        if (bankswitched) {
            size_t num_banks = (payload_size + 0x0FFF) / 0x1000;
            cartridge_->prg_memory.resize(num_banks * 0x1000, 0);
            std::memcpy(cartridge_->prg_memory.data(), payload, payload_size);
        } else {
            size_t rom_size = 0x10000 - header.load_addr;
            cartridge_->prg_memory.resize(rom_size, 0);
            std::memcpy(cartridge_->prg_memory.data(), payload, payload_size);
        }
        cartridge_->prg_banks = static_cast<uint8_t>(
            (cartridge_->prg_memory.size() + 0x3FFF) / 0x4000);

        // CHR-RAM (8KB for font tiles — no CHR-ROM)
        cartridge_->chr_memory.clear();
        cartridge_->chr_banks = 0;

        // PRG-RAM (8KB work RAM at $6000-$7FFF)
        cartridge_->prg_ram.resize(nes_constants::INES_PRG_RAM_DEFAULT, 0);

        // Create MapperNsf and assign to cartridge
        auto nsf_mapper = std::make_unique<MapperNsf>(
            bankswitched, header.bankswitch, header.load_addr);

        // Assign mapper via Cartridge's internal mechanism — we need to
        // set it up the same way load_from_buffer does.  Since Cartridge
        // doesn't have a public set_mapper(), we use update_bank_map
        // after init_unified_buffer has been called and memory pointers
        // reference the unified buffer copy.

        // ---- Allocate unified buffer ----
        ppu_->connect_cartridge(cartridge_.get());
        bus_.init_unified_buffer(
            cartridge_->prg_memory.data(), cartridge_->prg_memory.size(),
            nullptr, 0,            // no CHR-ROM data
            true,                  // CHR is RAM
            cartridge_->prg_ram.data(), cartridge_->prg_ram.size());

        // Give mapper pointers into the unified buffer copy
        nsf_mapper->set_memory_pointers(
            bus_.prg_rom_ptr, bus_.prg_rom_size,
            bus_.chr_data_ptr, bus_.chr_data_size,
            true,  // CHR is RAM
            bus_.prg_ram, bus_.prg_ram_size);
        nsf_mapper->set_header_mirror(Mirror::HORIZONTAL);

        // Install mapper into cartridge (access via friend-like helper)
        cartridge_->install_mapper(std::move(nsf_mapper));

        // Re-connect PPU to bus (ciram pointer may have changed)
        ppu_->connect_bus(&bus_);
        cartridge_->update_bank_map(&bus_, bus_.ciram);

        // ---- Write vectors and 6502 stubs ----
        printf("NES NSF: Loading \"%s\" by %s\n", header.name, header.artist);
        printf("NES NSF: load=$%04X init=$%04X play=$%04X songs=%d start=%d\n",
               header.load_addr, header.init_addr, header.play_addr,
               header.num_songs, header.start_song);

        NsfPlayer::setup_cpu(
            cpu_, bus_.cpu_ram,
            bus_.prg_rom_ptr, bus_.prg_rom_size,
            bankswitched, header.bankswitch, header.load_addr,
            &header, subtune, is_pal_);

        // Re-update bank map after vectors are written to ROM
        cartridge_->update_bank_map(&bus_, bus_.ciram);

        NsfPlayer::write_info_page(ppu_.get(), &header, subtune);

        // Save state for subtune switching
        active_nsf_header_ = header;
        active_nsf_data_.assign(payload, payload + payload_size);
        active_nsf_subtune_ = subtune;
        nsf_bankswitched_ = bankswitched;
        nsf_player_active_ = true;
        system_ready_ = true;

        // Set program title from NSF header
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
    
    try {
        cartridge_ = std::make_shared<Cartridge>();
        if (!cartridge_->load_from_buffer(file_data, file_size, filepath)) {
            printf("%s: Failed to parse cartridge data\n", Traits::name);
            free(file_data);
            return false;
        }
        free(file_data);

        // Connect cartridge to PPU and set up page-pointer bank maps
        ppu_->connect_cartridge(cartridge_.get());

        // Allocate unified buffer with cartridge ROM/RAM data
        bus_.init_unified_buffer(
            cartridge_->prg_memory.data(), cartridge_->prg_memory.size(),
            cartridge_->chr_memory.data(), cartridge_->chr_memory.size(),
            cartridge_->chr_memory.size() == 0 ||
                (cartridge_->get_mapper() && cartridge_->get_mapper()->chr_is_ram()),
            cartridge_->prg_ram.data(), cartridge_->prg_ram.size());

        // Re-set mapper memory pointers to reference the unified buffer copy
        // so that bank configs return pointers the bus can convert to block numbers.
        if (auto* m = cartridge_->get_mapper()) {
            m->set_memory_pointers(
                bus_.prg_rom_ptr, bus_.prg_rom_size,
                bus_.chr_data_ptr, bus_.chr_data_size,
                bus_.chr_is_ram,
                bus_.prg_ram, bus_.prg_ram_size);
        }

        // Re-connect PPU to bus (ciram pointer may have changed)
        ppu_->connect_bus(&bus_);

        cartridge_->update_bank_map(&bus_, bus_.ciram);
        
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
    
    // Copy PPU screen → framebuffer, cropping the left 8 pixels.
    // The PPU renders full 256px internally; visible output is 248px
    // (pixels 8–255).  On real hardware the left column is hidden by
    // CRT overscan and often contains scroll-seam artifacts.
    const std::vector<uint32_t>& nes_screen = ppu_->get_screen();
    if (!nes_screen.empty() && rgba_framebuffer_) {
        constexpr int SRC_W  = 256;
        constexpr int CROP_L = 8;     // pixels to crop from left
        constexpr int DST_W  = SRC_W - CROP_L;  // 248
        constexpr int H      = 240;
        for (int y = 0; y < H; ++y) {
            memcpy(&rgba_framebuffer_[y * DST_W],
                   &nes_screen[y * SRC_W + CROP_L],
                   DST_W * sizeof(uint32_t));
        }
    }
    
    return rgba_framebuffer_;
}

template<NintendoVariant V>
void NintendoSystem<V>::get_display_dimensions(int* width, int* height) const {
    *width = 248;   // 256 - 8 left overscan
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
#ifdef CERMU_HAS_GUI
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
    if (controller < 0 || controller >= static_cast<int>(connector_ports_.size())) return;

    auto* dev = connector_ports_[controller]->get_attached_device();
    if (auto* pad = dynamic_cast<NesStandardController*>(dev)) {
        pad->set_button_state(static_cast<NesStandardController::Button>(button), pressed);
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
                            bus_.prg_rom_ptr, bus_.prg_rom_size,
                            cartridge_->get_mapper(),
                            &active_nsf_header_,
                            active_nsf_data_.data(), active_nsf_data_.size(),
                            active_nsf_subtune_, is_pal_);
    // Re-update bank map after subtune switch resets bank regs
    cartridge_->update_bank_map(&bus_, bus_.ciram);
    return true;
}

template<NintendoVariant V>
void NintendoSystem<V>::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
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
        ChipInfo{"SRAM", "Various"}, nes_constants::CPU_RAM_SIZE, MemoryChip::SRAM, &pins_,
        "RAM", 0x0000);
    ram->bind(bus_.cpu_ram);  // Point at unified bus RAM for live debug view
    register_chip(std::move(ram));

    // CIRAM (2KB nametable VRAM on NES motherboard)
    auto ciram = std::make_unique<MemoryChip>(
        ChipInfo{"SRAM", "Various"}, 2048, MemoryChip::SRAM, &pins_,
        "CIRAM", 0x2000);
    ciram->bind(bus_.ciram);  // Point at unified buffer CIRAM for live debug view
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
#ifdef CERMU_HAS_GUI
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
void NintendoSystem<V>::tick() {
    if (!cpu_) return;
    
    NES_PROF_TICK();

    // ====================================================================
    // PPU tick (runs at 3× CPU clock)
    // ppu_ is guaranteed valid during frame execution (run_frame gates it)
    // ====================================================================
    NES_PROF_START(ppu);
    ppu_->bus_snapshot_ = ppu_->clock(ppu_->bus_snapshot_);
    NES_PROF_END(ppu_clock_cycles, ppu);

    // ====================================================================
    // OAM DMA controller — stalls CPU while transferring 256 bytes.
    // DMA operates at CPU clock rate: one read or write per CPU cycle.
    // Only advance DMA state on the CPU clock edge (every 3 PPU cycles).
    // ====================================================================
    if (unlikely(dma_transfer_)) {
        NES_PROF_START(dma);
        if (cpu_div_ != 0) {
            // Intermediate PPU cycle — just tick and wait for CPU edge
            cpu_div_--;
        } else {
            // CPU cycle boundary — advance DMA state machine
            cpu_div_ = 2;
            if (dma_dummy_) {
                if (dma_odd_cycle_) {
                    dma_dummy_ = false;
                }
            } else {
                if (!dma_odd_cycle_) {
                    // DMA read from CPU address space
                    uint16_t dma_src = (dma_page_ << 8) | dma_addr_;
                    dma_data_ = bus_.cpu_read(dma_src);
                } else {
                    ppu_->oam[dma_addr_] = dma_data_;
                    dma_addr_++;
                    if (dma_addr_ == 0x00) {
                        dma_transfer_ = false;
                        dma_dummy_ = true;
                    }
                }
            }
            dma_odd_cycle_ = !dma_odd_cycle_;
        }
        system_clock_counter_++;
        total_cycles_++;
        NES_PROF_END(dma_cycles, dma);
        return;
    }

    system_clock_counter_++;

    // ====================================================================
    // CPU tick — one PHI2/PHI1 cycle every 3 PPU ticks
    // ====================================================================
    if (cpu_div_ != 0) {
        cpu_div_--;
        total_cycles_++;
        return;
    }
    cpu_div_ = 2;  // Reset countdown (next CPU tick in 3 PPU cycles)

    NES_PROF_CPU_TICK();

    // Transfer PPU /NMI onto CPU bus BEFORE PHI2, so the CPU's
    // edge-detect flip-flop samples the current NMI level.
    //
    // NMI pulse preservation DISABLED — the CPU's edge-detect latch
    // (nmi_edge_latch) handles persistence.  Once latched, the NMI fires
    // was giving iteration 05 of blargg 06-suppression an extra NMI LOW
    // cycle, making NMI fire when it shouldn't.
    pins_ = PPU_CPU_BITMIX(pins_, ppu_->bus_snapshot_);

    // PHI2: CPU drives address bus and R/W signal
    NES_PROF_START(phi2);
    pins_ = cpu_->tick<RICOH_2A03::Phase::PHI2>(pins_);
    NES_PROF_END(cpu_phi2_cycles, phi2);

    const uint16_t addr = BUS_GET_ADDR(pins_);
    const bool is_read = BUS_GET_BIT(pins_, BUS_RW_BIT);

    // ====================================================================
    // CPU bus dispatch — page-pointer fast path with I/O fallback
    // ====================================================================
    NES_PROF_START(bus);

    if (is_read) {
        // ---- READ ----
        {
            const uint16_t block = bus_.cpu_read_block[addr >> 12];
            if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
                // Block hit: WRAM, PRG-ROM, PRG-RAM, or expansion
                BUS_SET_DATA(pins_, bus_.cpu_block_read(block, addr));
            } else if (block == nes_bus::BLOCK_PPU_REGS) {
                // PPU registers ($2000-$3FFF, mirrored every 8 bytes)
                auto [cpu_result, ppu_result] = ppu_->service_cpu_bus(
                    pins_, ppu_->bus_snapshot_);
                pins_ = cpu_result;
                ppu_->bus_snapshot_ = ppu_result;
            } else if (block == nes_bus::BLOCK_APU_IO) {
                // APU/IO registers ($4000-$4FFF)
                if (addr == 0x4016 || addr == 0x4017) {
                    // Controller read via ConnectorPort signal protocol.
                    // Read D0 from device, then pulse CLK to shift next bit.
                    const int p = addr & 1;  // 0 for $4016, 1 for $4017
                    uint8_t result = 0;
                    if (p < static_cast<int>(connector_ports_.size())) {
                        auto* dev = connector_ports_[p]->get_attached_device();
                        if (dev) {
                            uint32_t sigs = dev->get_output_signals();
                            // D0 is active-low: bit clear = button pressed → result bit 0 = 1
                            if (!(sigs & (1u << ConnectorSignals::NESControllerBit::NES_D0)))
                                result = 1;
                        }
                        // Pulse CLK high then low to advance shift register
                        const uint32_t clk_mask = 1u << ConnectorSignals::NESControllerBit::NES_CLK;
                        connector_ports_[p]->write_system_signals(clk_mask, clk_mask);
                        connector_ports_[p]->write_system_signals(clk_mask, 0);
                    }
                    BUS_SET_DATA(pins_, result);
                }
                // Other APU reads ($4015 etc.) handled by CPU PHI1
            } else {
                // Unmapped expansion or cartridge I/O — open bus
            }
        }
    } else {
        // ---- WRITE ----
        const uint8_t data = BUS_GET_DATA(pins_);

        {
            const uint16_t block = bus_.cpu_write_block[addr >> 12];
            if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
                // Block hit: WRAM, PRG-RAM, or expansion write
                bus_.cpu_block_write(block, addr, data);
            } else if (block == nes_bus::BLOCK_PPU_REGS) {
                // PPU registers ($2000-$3FFF, mirrored every 8 bytes)
                auto [cpu_result, ppu_result] = ppu_->service_cpu_bus(
                    pins_, ppu_->bus_snapshot_);
                pins_ = cpu_result;
                ppu_->bus_snapshot_ = ppu_result;
            } else if (block == nes_bus::BLOCK_APU_IO) {
                // APU/IO registers ($4000-$4FFF)
                if (addr == 0x4014) {
                    // OAM DMA trigger
                    dma_page_ = data;
                    dma_addr_ = 0x00;
                    dma_transfer_ = true;
                } else if (addr == 0x4016) {
                    // Drive LATCH signal on both controller ports.
                    // Bit 0 of data: 1 = LATCH high, 0 = LATCH low.
                    const uint32_t latch_mask = 1u << ConnectorSignals::NESControllerBit::NES_LATCH;
                    const uint32_t latch_val  = (data & 1) ? latch_mask : 0;
                    for (size_t cp = 0; cp < 2 && cp < connector_ports_.size(); cp++)
                        connector_ports_[cp]->write_system_signals(latch_mask, latch_val);
                }
                // Other APU writes ($4000-$4013, $4015, $4017) handled by CPU PHI1
            } else {
                // BLOCK_OPEN_BUS — mapper register write ($5xxx expansion
                // or $8000+ PRG space).  Delegates to mapper->register_write.
                if (cartridge_) {
                    if (cartridge_->handle_mapper_write(addr, data)) {
                        cartridge_->update_bank_map(&bus_, bus_.ciram);
                    }
                }
            }
        }
    }

    // ====================================================================
    // Interrupt wire handling — post bus-dispatch update
    // ====================================================================
    NES_PROF_END(bus_dispatch_cycles, bus);

    NES_PROF_START(irq);

    // Re-transfer PPU /NMI after bus dispatch — service_cpu_bus() may have
    // changed NMI state ($2002 read clears VBL, $2000 write toggles enable).
    pins_ = PPU_CPU_BITMIX(pins_, ppu_->bus_snapshot_);

    // Sample NMI pin AFTER bus dispatch so the CPU sees the post-operation
    // pin state.  End of PHI2 and start of PHI1 are the same clock edge;
    // sampling here is equivalent to sampling at the end of PHI2.
    // The 1-cycle-before-acting delay is inherent: edge latched at end of
    // cycle N → process_interrupt_detection at PHI2 of N+1 sees it.
    cpu_->sample_nmi_pin(pins_);

    // IRQ — level-sensitive (active low)
    // Mapper IRQ (e.g. MMC3 scanline counter) stays asserted until the game
    // explicitly acknowledges it by writing to the appropriate mapper register
    // ($E000 for MMC3).  Do NOT auto-clear — the line must remain low so the
    // CPU's level-sensitive detection can sample it reliably.
    bool irq_asserted = false;
    if (cartridge_ && cartridge_->irq_state()) {
        irq_asserted = true;
    }
    if (cpu_->apu_irq()) {
        irq_asserted = true;
    }
    if (irq_asserted) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    } else {
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);
    }
    NES_PROF_END(irq_nmi_cycles, irq);

    // PHI1: CPU internal operations (including APU clock)
    NES_PROF_START(phi1);
    pins_ = cpu_->tick<RICOH_2A03::Phase::PHI1>(pins_);

    // ====================================================================
    // Audio sample generation
    // ====================================================================
    if (--audio_sample_counter_ == 0) {
        audio_sample_counter_ = audio_sample_period_;
        float sample = cpu_->generate_audio_sample();
        audio_buffer_.push_back(sample);
    }

    // ====================================================================
    // DMC DMA — service sample fetch requests from the APU's DMC channel.
    // On real hardware this steals 1-4 CPU cycles; for now we do an
    // instantaneous read to get the DMC functionally working.
    // ====================================================================
    if (unlikely(cpu_->apu_needs_dma())) {
        uint16_t dmc_addr = cpu_->apu_dma_address();
        uint8_t sample = bus_.cpu_read(dmc_addr);
        cpu_->apu_load_dma_sample(sample);
    }

    NES_PROF_END(cpu_phi1_cycles, phi1);

    total_cycles_++;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_controller_state(int controller, uint8_t state) {
    if (controller < 0 || controller >= static_cast<int>(connector_ports_.size())) return;

    auto* dev = connector_ports_[controller]->get_attached_device();
    if (auto* pad = dynamic_cast<NesStandardController*>(dev)) {
        for (int i = 0; i < 8; i++) {
            pad->set_button_state(
                static_cast<NesStandardController::Button>(1 << i),
                (state >> i) & 1);
        }
    }
}

template<NintendoVariant V>
const std::vector<uint32_t>& NintendoSystem<V>::get_screen() const {
    static std::vector<uint32_t> empty_screen;
    return ppu_ ? ppu_->get_screen() : empty_screen;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_audio_sample_rate(uint32_t rate) {
    audio_sample_rate_ = rate;
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
    f.write(reinterpret_cast<const char*>(bus_.ciram), nes_bus::CIRAM_SIZE);
    f.write(reinterpret_cast<const char*>(ppu_->oam.data()), ppu_->oam.size());
    f.write(reinterpret_cast<const char*>(ppu_->palette.data()), ppu_->palette.size());
    f.write(reinterpret_cast<const char*>(&ppu_->internal), sizeof(ppu_->internal));
    int16_t sl = ppu_->scanline; f.write(reinterpret_cast<const char*>(&sl), sizeof(sl));
    uint16_t cy = ppu_->cycle;   f.write(reinterpret_cast<const char*>(&cy), sizeof(cy));
    uint64_t fc = ppu_->frame_count; f.write(reinterpret_cast<const char*>(&fc), sizeof(fc));

    // Bus state -- CPU RAM
    f.write(reinterpret_cast<const char*>(bus_.cpu_ram), nes_bus::WRAM_SIZE);
    f.write(reinterpret_cast<const char*>(&dma_page_), 1);
    f.write(reinterpret_cast<const char*>(&dma_addr_), 1);
    f.write(reinterpret_cast<const char*>(&dma_data_), 1);
    uint8_t dma_flags = (dma_transfer_ ? 1 : 0) | (dma_dummy_ ? 2 : 0);
    f.write(reinterpret_cast<const char*>(&dma_flags), 1);
    f.write(reinterpret_cast<const char*>(&system_clock_counter_), sizeof(system_clock_counter_));

    // PRG RAM (if present -- saved from unified buffer)
    if (bus_.prg_ram && bus_.prg_ram_size > 0) {
        uint32_t ram_size = bus_.prg_ram_size;
        f.write(reinterpret_cast<const char*>(&ram_size), sizeof(ram_size));
        f.write(reinterpret_cast<const char*>(bus_.prg_ram), ram_size);
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
    f.read(reinterpret_cast<char*>(bus_.ciram), nes_bus::CIRAM_SIZE);
    f.read(reinterpret_cast<char*>(ppu_->oam.data()), ppu_->oam.size());
    f.read(reinterpret_cast<char*>(ppu_->palette.data()), ppu_->palette.size());
    f.read(reinterpret_cast<char*>(&ppu_->internal), sizeof(ppu_->internal));
    int16_t sl; f.read(reinterpret_cast<char*>(&sl), sizeof(sl)); ppu_->scanline = sl;
    uint16_t cy; f.read(reinterpret_cast<char*>(&cy), sizeof(cy)); ppu_->cycle = cy;
    uint64_t fc; f.read(reinterpret_cast<char*>(&fc), sizeof(fc)); ppu_->frame_count = fc;

    // Bus state
    f.read(reinterpret_cast<char*>(bus_.cpu_ram), nes_bus::WRAM_SIZE);
    f.read(reinterpret_cast<char*>(&dma_page_), 1);
    f.read(reinterpret_cast<char*>(&dma_addr_), 1);
    f.read(reinterpret_cast<char*>(&dma_data_), 1);
    uint8_t dma_flags; f.read(reinterpret_cast<char*>(&dma_flags), 1);
    dma_transfer_ = (dma_flags & 1) != 0;
    dma_dummy_ = (dma_flags & 2) != 0;
    f.read(reinterpret_cast<char*>(&system_clock_counter_), sizeof(system_clock_counter_));

    // Derive fast-path dividers from restored system_clock_counter
    cpu_div_ = static_cast<uint8_t>(system_clock_counter_ % 3);
    dma_odd_cycle_ = (system_clock_counter_ & 1) != 0;

    // PRG RAM
    uint32_t ram_size = 0;
    f.read(reinterpret_cast<char*>(&ram_size), sizeof(ram_size));
    if (ram_size > 0 && bus_.prg_ram && bus_.prg_ram_size >= ram_size) {
        f.read(reinterpret_cast<char*>(bus_.prg_ram), ram_size);
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
    // keyboard.  auto_assign_controller_keymaps() (called at the end of
    // attach_default_peripherals) picks WASD for P1, IJKL for P2 via the
    // generic collision-scoring framework.
    attach_default_peripherals();
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
    // Block dispatch first — consistent with the tick loop's read path.
    uint16_t block = bus_.cpu_read_block[addr >> 12];
    if (block < nes_bus::BLOCK_SENTINEL_MIN) {
        return bus_.cpu_block_read(block, addr);
    }

    // PPU registers ($2000-$3FFF): side-effect-free peek
    if (block == nes_bus::BLOCK_PPU_REGS && ppu_) {
        return ppu_->cpu_peek(addr);
    }

    return 0;
}

template<NintendoVariant V>
void NintendoSystem<V>::poke_memory(uint16_t addr, uint8_t value) {
    // All writable ranges: block dispatch (WRAM, PRG-RAM, expansion)
    uint16_t block = bus_.cpu_write_block[addr >> 12];
    if (block < nes_bus::BLOCK_SENTINEL_MIN) {
        bus_.cpu_block_write(block, addr, value);
    }
}

template<NintendoVariant V>
uint8_t NintendoSystem<V>::peek_ppu_memory(uint16_t addr) const {
    addr &= 0x3FFF;

    // Palette RAM ($3F00-$3F1F, mirrors above $3F20)
    if (addr >= 0x3F00 && ppu_) {
        return ppu_->palette[PPU::pal_mirror_[addr & 0x1F]] & 0x3F;
    }

    // CHR + nametable via block dispatch
    uint16_t block = bus_.ppu_read_block[addr >> nes_bus::PPU_PAGE_SHIFT];
    if (block < nes_bus::BLOCK_SENTINEL_MIN) {
        return bus_.ppu_block_read(block, addr);
    }
    return 0;
}

template<NintendoVariant V>
uint16_t NintendoSystem<V>::get_cpu_pc() const {
    if (!cpu_) return 0;
    return static_cast<uint16_t>(cpu_->get(REG_PC));
}

template<NintendoVariant V>
void NintendoSystem<V>::set_cpu_pc(uint16_t addr) {
    if (cpu_) cpu_->set(REG_PC, addr);
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