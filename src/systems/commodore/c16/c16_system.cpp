#include "core/cermu.hpp"
#include "systems/commodore/c16/c16_system.hpp"
#include "systems/commodore/c16/c16_constants.hpp"
#include "systems/commodore/c16/c16_keyboard_matrix.hpp"
#include "core/input/emu_key_sdl_map.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_registry.hpp"
#include "core/formats/prg_format.hpp"
#include "core/formats/bin_format.hpp"
#include "core/formats/d64_format.hpp"
#include "core/formats/t64_format.hpp"
#include "core/formats/tap_format.hpp"
#include "core/formats/crt_format.hpp"
#include "core/formats/lnx_format.hpp"
#include "systems/commodore/commodore_load_helpers.hpp"
#include "devices/keyboard/commodore_keyboard_device.hpp"
#include "devices/storage/drive_1541.hpp"
#include "devices/storage/datasette_1530.hpp"
#include "systems/commodore/prg_content_analysis.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <algorithm>

using namespace ted::reg;

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// ============================================================================
// Hardware Traits Definition
// ============================================================================

template<C264SeriesVariant V>
HardwareTraits Commodore264System<V>::create_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - TED 7360
    traits.display.native_width = c16_constants::DISPLAY_WIDTH;
    traits.display.native_height = c16_constants::DISPLAY_HEIGHT;
    traits.display.visible_width = c16_constants::DISPLAY_WIDTH;
    traits.display.visible_height = c16_constants::DISPLAY_HEIGHT;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 128;      // 128 colors (luminance variations)
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // C16/Plus/4 palette (simplified - first 16 base colors)
    const uint32_t c16_colors[16] = {
        0x000000, 0xFFFFFF, 0x8E3C97, 0x72DB87,
        0x4F44D8, 0x3DAC29, 0xC94B48, 0x5DD9E8,
        0x8A4A00, 0xAC7E3C, 0xDB8B8A, 0x94B6E0,
        0x868686, 0xC9E29E, 0x5CC9B5, 0xBDBDBD
    };
    
    for (int i = 0; i < 16; i++) {
        uint32_t c = c16_colors[i];
        traits.display.default_palette.push_back(
            PaletteColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255)
        );
    }
    
    // Audio traits - TED has 2 channel sound
    traits.audio.format = AudioFormat::MONO_16BIT;
    traits.audio.sample_rate_hz = c16_constants::AUDIO_SAMPLE_RATE;
    traits.audio.channels = 1;
    traits.audio.chip_name = "TED 7360";
    
    // Timing - PAL version
    traits.timing.cpu_frequency_hz = c16_constants::CPU_FREQ_PAL;
    traits.timing.video_frequency_hz = c16_constants::CPU_FREQ_PAL;
    traits.timing.audio_sample_rate_hz = c16_constants::AUDIO_SAMPLE_RATE;
    traits.timing.target_fps = 50;
    traits.timing.cycles_per_frame = c16_constants::CYCLES_PER_FRAME_PAL;
    traits.timing.standard = VideoStandard::PAL;
    
    // Memory options — variant-specific
    if constexpr (Traits::default_ram >= c16_constants::RAM_SIZE_PLUS4) {
        traits.memory_options.push_back({
            "64KB RAM",
            c16_constants::RAM_SIZE_PLUS4,
            c16_constants::ROM_SIZE,
            true
        });
    } else {
        traits.memory_options.push_back({
            "16KB RAM",
            c16_constants::RAM_SIZE_C16,
            c16_constants::ROM_SIZE,
            true
        });
    }
    
    // Region options
    traits.video_standard_configs.push_back({
        "PAL",
        VideoStandard::PAL,
        traits.timing,
        true
    });
    
    SystemTiming ntsc_timing = traits.timing;
    ntsc_timing.cpu_frequency_hz = c16_constants::CPU_FREQ_NTSC;
    ntsc_timing.video_frequency_hz = c16_constants::CPU_FREQ_NTSC;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 14914;
    ntsc_timing.standard = VideoStandard::NTSC;
    
    traits.video_standard_configs.push_back({
        "NTSC",
        VideoStandard::NTSC,
        ntsc_timing,
        false
    });
    
    return traits;
}

// ============================================================================
// File Detection helpers (shared across all TED variants)
// ============================================================================

/**
 * Check whether a load address is characteristic of the C264 series.
 * 0x1001 is the BASIC 3.5 start (shared with VIC-20 unexpanded); other
 * addresses suggest machine-language programs targeting the C16/Plus4
 * memory map.
 */
static bool is_c264_load_address(uint16_t addr) {
    return addr == c16_constants::BASIC_START  // $1001 — BASIC 3.5 start
        || addr == 0x4000                      // Common ML origin (below ROM)
        || addr == 0x8000                      // ROM overlay / ML start
        || addr == 0xC000;                     // ML in upper RAM
}

// has_basic35_tokens() moved to shared utility: src/systems/commodore/prg_content_analysis.h

/**
 * Compute C264-series confidence from a PRG's load address and payload size.
 *
 * The key discriminator vs. VIC-20 0x1001 programs is *size*:
 * - Unexpanded VIC-20 has screen RAM at $1E00, so only ~3 KB of BASIC space.
 * - VIC-20 (fully expanded, 8K+ config) moves BASIC start to $1201.
 * - C16/Plus4 has up to $FD00 for programs starting at $1001.
 *
 * Returns a raw content-based confidence score (before filepath heuristics).
 */
static float c264_prg_confidence(uint16_t load_addr, size_t prg_data_size) {
    uint32_t end_addr = (uint32_t)load_addr + (uint32_t)prg_data_size;

    if (load_addr == c16_constants::BASIC_START) {          // $1001
        if (end_addr > 0x8000)  return 0.95f;              // Way beyond any VIC-20 config
        if (end_addr > 0x4000)  return 0.90f;              // Exceeds VIC-20 expanded BASIC area
        if (end_addr > 0x2000)  return 0.80f;              // Exceeds unexpanded VIC-20
        return 0.55f;                                       // Small: ambiguous C16 vs VIC-20
    }
    if (load_addr == 0x4000 || load_addr == 0xC000 || load_addr == 0x8000)
        return 0.60f;                                       // Plausible ML address
    return 0.40f;                                           // No strong C264 signal
}

// ============================================================================
// File Detection (shared across all TED variants)
// ============================================================================

template<C264SeriesVariant V>
SystemProbeResult Commodore264System<V>::probe_file_static(
    const format_descriptor_t* matched_format,
    const char* filepath,
    const uint8_t* data, size_t size)
{
    SystemProbeResult result{};

    if (!matched_format) return result;

    // ----- PRG: load address + program size + BASIC 3.5 token scan -----
    if (matched_format == &PRG_FORMAT_DESCRIPTOR) {
        if (size >= 2) {
            uint16_t load_addr = data[0] | (data[1] << 8);
            result.confidence = c264_prg_confidence(load_addr, size - 2);

            // Content analysis: BASIC version + MMIO references
            if (size > 6) {
                const uint8_t* payload = data + 2;
                size_t payload_len = size - 2;
                float basic_conf = basic_program_confidence(payload, payload_len, load_addr);

                // Determine what region to scan for MMIO references
                const uint8_t* scan_data = nullptr;
                size_t scan_len = 0;
                bool sys_based = false;

                if (basic_conf >= 0.5f) {
                    // Looks like BASIC — BASIC 3.5 tokens are a near-definitive
                    // C264 marker
                    if (load_addr == c16_constants::BASIC_START
                        && has_basic35_tokens(payload, payload_len))
                        result.confidence = std::max(result.confidence, 0.95f);

                    // BASIC stub: extract SYS target, scan ML from there.
                    // Limit to first 512 bytes — init code touches MMIO early;
                    // data segments further out cause false positives.
                    uint16_t sys_addr = extract_sys_address(payload, payload_len, load_addr);
                    if (sys_addr != 0 && sys_addr >= load_addr) {
                        uint32_t ml_off = (uint32_t)sys_addr - (uint32_t)load_addr;
                        if (ml_off < payload_len) {
                            scan_data = payload + ml_off;
                            size_t remaining = payload_len - ml_off;
                            scan_len  = remaining < 512 ? remaining : 512;
                            sys_based = true;
                        }
                    }
                } else {
                    // Pure machine language — scan everything
                    scan_data = payload;
                    scan_len  = payload_len;
                }

                if (scan_data && scan_len > 8) {
                    uint32_t mmio = scan_6502_mmio_references(scan_data, scan_len);
                    int c16_hits = count_mmio_flags(mmio & MMIO_ANY_C16);

                    // TED $FF00-$FF1F is a very narrow range (0.05 % of the
                    // address space) so even SYS-based scans are reliable.
                    // Suppress boost if multiple strong VIC-20 MMIO hits
                    // are present — a program hitting both $9000-$912F (VIC/VIA)
                    // and $FFxx is likely VIC-20 with coincidental TED match.
                    // A single VIC-20 hit is too weak a signal (0.02 % range).
                    int vic20_strong = count_mmio_flags(mmio & MMIO_VIC20_STRONG);
                    if (c16_hits >= 1 && vic20_strong < 2)
                        result.confidence = std::max(result.confidence, 0.92f);

                    // Cross-system penalties only for pure ML — SYS-based
                    // scans misinterpret data segments as instructions,
                    // producing false SID/CIA/VIA hits.
                    if (!sys_based) {
                        int c64_strong = count_mmio_flags(mmio & MMIO_C64_STRONG);
                        int vic20_hits = count_mmio_flags(mmio & MMIO_ANY_VIC20);
                        if (c64_strong >= 1 && c16_hits == 0)
                            result.confidence *= 0.50f;
                        else if (vic20_hits >= 2 && c16_hits == 0)
                            result.confidence *= 0.50f;
                    }
                }
            }

            // Memory-capacity gate: penalise if the program overshoots this
            // variant's RAM.  Plus/4 (64 KB) can run anything that fits;
            // C16/C116 (16 KB) cannot run programs ending above $4000.
            uint32_t end_addr = (uint32_t)load_addr + (uint32_t)(size - 2);
            if constexpr (Traits::default_ram < c16_constants::RAM_SIZE_PLUS4) {
                if (end_addr > Traits::default_ram)
                    result.confidence *= 0.50f;     // Doesn't fit in 16 KB
            } else {
                // Plus/4: give a small extra nudge when >16 KB is needed
                if (end_addr > c16_constants::RAM_SIZE_C16)
                    result.confidence = std::max(result.confidence,
                                                 result.confidence + 0.02f);
            }
        }

    // ----- TAP: platform byte in header is definitive -----
    } else if (matched_format == &TAP_FORMAT_DESCRIPTOR) {
        int platform = commodore_tap_identify_platform_mem(data, size);
        if (platform == 2)      result.confidence = 0.95f;  // C16 TAP
        else if (platform == 0) result.confidence = 0.2f;   // C64 TAP
        else                    result.confidence = 0.4f;

    // ----- D64: extract first PRG, apply address + size analysis -----
    } else if (matched_format == &D64_FORMAT_DESCRIPTOR) {
        commodore_d64_t d64;
        if (d64.open_mem(data, size)) {
            commodore_prg_t prg = {};
            if (d64.extract_first_prg(&prg)) {
                result.confidence = c264_prg_confidence(prg.load_addr, prg.data_size);

                if (prg.load_addr == c16_constants::BASIC_START && prg.data_size > 4)
                    if (has_basic35_tokens(prg.data, prg.data_size))
                        result.confidence = std::max(result.confidence, 0.95f);

                commodore_prg_free(&prg);
            } else {
                result.confidence = 0.45f;  // Valid D64, no PRG extracted
            }
            d64.close();
        } else {
            result.confidence = 0.35f;
        }

    // ----- T64: extract first PRG, apply same heuristics -----
    } else if (matched_format == &T64_FORMAT_DESCRIPTOR) {
        commodore_t64_t t64;
        if (t64.open_mem(data, size)) {
            commodore_prg_t prg = {};
            if (t64.extract_first_prg(&prg)) {
                result.confidence = c264_prg_confidence(prg.load_addr, prg.data_size);

                if (prg.load_addr == c16_constants::BASIC_START && prg.data_size > 4)
                    if (has_basic35_tokens(prg.data, prg.data_size))
                        result.confidence = std::max(result.confidence, 0.95f);

                commodore_prg_free(&prg);
            } else {
                result.confidence = 0.35f;
            }
            t64.close();
        } else {
            result.confidence = 0.35f;
        }

    // ----- LNX: inspect contained files for C264 addresses -----
    } else if (matched_format == &LNX_FORMAT_DESCRIPTOR) {
        commodore_lynx_t lynx;
        if (lynx.open_mem(data, size)) {
            commodore_lynx_directory_t dir;
            if (lynx.read_directory(&dir)) {
                bool found_c264 = false;
                bool found_any  = false;
                for (unsigned i = 0; i < dir.file_count; i++) {
                    if (dir.entries[i].file_type == 'P' && dir.entries[i].data_length >= 2) {
                        size_t off = dir.entries[i].data_offset;
                        if (off + 1 < lynx.data_size) {
                            uint16_t addr = lynx.data[off] | ((uint16_t)lynx.data[off + 1] << 8);
                            found_any = true;
                            if (is_c264_load_address(addr)) found_c264 = true;
                        }
                    }
                }
                lynx.close();
                result.confidence = found_c264 ? 0.85f : (found_any ? 0.45f : 0.50f);
            } else {
                lynx.close();
                result.confidence = 0.40f;
            }
        } else {
            result.confidence = 0.40f;
        }

    // ----- BIN: generic binary -----
    } else if (matched_format == &BIN_FORMAT_DESCRIPTOR) {
        result.confidence = 0.3f;
    }

    // =================================================================
    // =================================================================
    // Filepath heuristics — variant-specific alias boost is now
    // handled generically by SystemRegistry::identify_system() using
    // aliases.  Only family-level and configuration hints remain.
    // =================================================================
    if (filepath) {
        std::string lower(filepath);
        for (auto& c : lower) c = static_cast<char>(tolower(c));

        // General TED/C264 family signals — weaker than variant-specific
        // keywords (which are applied at 0.90 by the registry).  These
        // ensure that any C264-family file in a shared directory (e.g.
        // "c264/" or "264 series/") still gets a reasonable score.
        if (lower.find("plus4")  != std::string::npos ||
            lower.find("plus/4") != std::string::npos ||
            lower.find("plus-4") != std::string::npos ||
            lower.find("c16")    != std::string::npos ||
            lower.find("c116")   != std::string::npos ||
            lower.find("c264")   != std::string::npos ||
            lower.find("264 series") != std::string::npos) {
            result.confidence = std::max(result.confidence, 0.75f);
        }

        // Region hint
        if (lower.find("ntsc") != std::string::npos)
            result.configuration.region_option_index = 1;   // NTSC
    }

    return result;
}

// ============================================================================
// Commodore Load Helper Callbacks (non-template free functions)
// ============================================================================

static uint8_t c16_mem_read(void* ctx, uint16_t addr) {
    return static_cast<uint8_t*>(ctx)[addr];
}

static void c16_mem_write_byte(void* ctx, uint16_t addr, uint8_t val) {
    static_cast<uint8_t*>(ctx)[addr] = val;
}

static void c16_mem_write_block(void* ctx, uint16_t addr,
                                const uint8_t* data, size_t len) {
    memcpy(&static_cast<uint8_t*>(ctx)[addr], data, len);
}

// ============================================================================
// Connector Definitions (non-template file-scope statics)
// ============================================================================

static const PortDefinition c16_joy_port_1_def = {
    PortType::CONTROL_PORT_DB9,
    "Joystick Port 1",
    PortSignals::CONTROL_PORT_SIGNALS,
    PortSignals::CONTROL_PORT_SIGNAL_COUNT,
    false, false
};

static const PortDefinition c16_joy_port_2_def = {
    PortType::CONTROL_PORT_DB9,
    "Joystick Port 2",
    PortSignals::CONTROL_PORT_SIGNALS,
    PortSignals::CONTROL_PORT_SIGNAL_COUNT,
    false, false
};

static const PortDefinition c16_iec_serial_def = {
    PortType::IEC_SERIAL,
    "IEC Serial Bus",
    PortSignals::IEC_SERIAL_SIGNALS,
    PortSignals::IEC_SERIAL_SIGNAL_COUNT,
    false,  // is_internal
    true    // is_bus — shared bus, multiple drives/printers
};

static const PortDefinition c16_cassette_def = {
    PortType::CASSETTE_PORT,
    "Cassette Port",
    PortSignals::CASSETTE_PORT_SIGNALS,
    PortSignals::CASSETTE_PORT_SIGNAL_COUNT,
    false, false
};

static const PortDefinition plus4_user_port_def = {
    PortType::USER_PORT,
    "User Port",
    PortSignals::USER_PORT_SIGNALS,
    PortSignals::USER_PORT_SIGNAL_COUNT,
    false, false
};

static const SignalLine c16_expansion_signals[] = {
    { "/RESET", SignalDirection::OUTPUT, 0 },
    { "/IRQ",   SignalDirection::INPUT,  1 },
};
static const PortDefinition c16_expansion_def = {
    PortType::EXPANSION_PORT,
    "Expansion Port",
    c16_expansion_signals,
    2,
    false, false
};

// A/V output — DIN-8 connector carries composite video + audio
static const PortDefinition c16_video_out_def = {
    PortType::VIDEO_COMPOSITE,
    "Video Out",
    nullptr, 0,
    false, false
};

static const PortDefinition c16_audio_out_def = {
    PortType::AUDIO_MONO,
    "Audio Out",
    nullptr, 0,
    false, false
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

template<C264SeriesVariant V>
Commodore264System<V>::Commodore264System()
    : CommodoreSystem()
    , cpu_(nullptr)
    , ted_(nullptr)
    , bus_state_(0)
    , initialized_(false)
{
    cycles_per_frame_ = c16_constants::CYCLES_PER_FRAME_PAL;
    hardware_traits_ = create_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    
    // Each variant has exactly one memory option (index 0)
    config_.memory_option_index = 0;
}

template<C264SeriesVariant V>
Commodore264System<V>::~Commodore264System() {
    shutdown();
}

// ============================================================================
// System Identification
// ============================================================================

template<C264SeriesVariant V>
const SystemDescriptor& Commodore264System<V>::static_descriptor() {
    static const format_descriptor_t* const formats[] = {
        &PRG_FORMAT_DESCRIPTOR, &TAP_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR,
        &T64_FORMAT_DESCRIPTOR, &LNX_FORMAT_DESCRIPTOR, &BIN_FORMAT_DESCRIPTOR,
        nullptr
    };
    static const SystemDescriptor desc = {
        Traits::full_name,
        Traits::short_id,
        Traits::description,
        Traits::data_folder,
        Traits::get_aliases(),
        formats,
        create_hardware_traits(),
        probe_file_static,
        "Commodore", 1984, fam65xx::CSG7501Traits.display_name, SystemType::Home
    };
    return desc;
}

template<C264SeriesVariant V>
const SystemDescriptor& Commodore264System<V>::get_descriptor() const {
    return static_descriptor();
}

// ============================================================================
// Configuration Management
// ============================================================================

template<C264SeriesVariant V>
bool Commodore264System<V>::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        const VideoStandardConfig& std_cfg = hardware_traits_.video_standard_configs[config_.region_option_index];
        cycles_per_frame_ = std_cfg.timing.cycles_per_frame;
        cached_target_fps_ = std_cfg.timing.target_fps;
    }
    
    // Cache RAM size from memory options (avoids vector lookup every cycle in mem_tick)
    if (config_.memory_option_index >= 0 &&
        config_.memory_option_index < static_cast<int>(hardware_traits_.memory_options.size())) {
        ram_size_ = hardware_traits_.memory_options[config_.memory_option_index].ram_size;
    } else {
        ram_size_ = c16_constants::RAM_SIZE_C16;  // Default C16
    }
    
    // Reconfigure page pointers for new RAM size / banking state
    if (initialized_) {
        setup_ram_mirroring();
        build_banking_snapshots();
        apply_cpu_banking();
        apply_ted_video_banking();
    }
    
    return true;
}

// ============================================================================
// System Lifecycle
// ============================================================================

template<C264SeriesVariant V>
bool Commodore264System<V>::initialize() {
    if (initialized_) {
        return true;
    }
    
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);
    
    // Initialize TED 7360 (video, sound, timers, keyboard scanning)
    // TED is a value member of board_.chips() — configure via two-phase init.
    {
        bool is_pal_region = (config_.region_option_index <= 0);
        ted7360_desc_t ted_desc = {};
        ted_desc.is_pal = is_pal_region;
        ted_desc.keyboard_scan = ted_keyboard_scan;
        ted_desc.keyboard_user_data = this;
        ted_desc.mem_read = ted_mem_read;
        ted_desc.mem_read_user_data = this;
        ted_desc.banking_change = ted_banking_changed;
        ted_desc.banking_change_user_data = this;
        board_.ted.init(ted_desc);
        ted_ = &board_.ted;
        log_info("%s: Created TED 7360 (%s)\n", Traits::name, is_pal_region ? "PAL" : "NTSC");
        // Initialize sound subsystem: TED master clock is 2× CPU clock
        uint32_t ted_clock = is_pal_region ? TED_PAL_CLOCK_HZ : TED_NTSC_CLOCK_HZ;
        ted_->audio_reset(ted_clock, c16_constants::AUDIO_SAMPLE_RATE);
    }

    // Bind value-typed chips from Chips, then factory-create remaining
    {   size_t slot_idx_ = 0;
        C264_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_)
    }
    board_.create_chips(&bus_state_);
    board_.apply(bus_);

    // Convenience pointers for direct buffer access (ROM loading, KERNAL checks, etc.)
    ram_        = &board_.ram;
    basic_rom_  = &board_.basic_rom;
    kernal_rom_ = &board_.kernal_rom;
    cpu_        = &board_.csg7501;
    pio1_       = &board_.pio1;
    pio2_       = &board_.pio2;
    io_dec_     = &board_.io_dec;

    // Wire I/O decoder: connect PIO chips and ROM bank change callback.
    // The 74LS139 + 74LS175 inside the I/O decoder handle sub-page dispatch
    // and ROM bank latching respectively.
    io_dec_->wire(pio1_, pio2_);
    io_dec_->set_bank_change_callback([](void* ctx) {
        static_cast<Commodore264System<V>*>(ctx)->apply_cpu_banking();
    }, this);

    // Page $FD is always I/O — unmatched addresses return open bus.
    // The MaskedSubTable base was captured from the underlying ROM/RAM page
    // by apply(); override it to no-chip (open bus reads, writes dropped).
    bus_.set_masked_base(0, c264_sub::kIoPage,
                         Bus::PT::kNoChipSelected,
                         Bus::PT::kNoChipSelectedWrite);
    
    // Load ROMs using common ROM loader
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        log_info("%s: Warning - ROMs not loaded, system may not function correctly\n", Traits::name);
    }
    
    // Initialize MOS 7501 CPU — owned by board_, retrieved via chip_as
    if (!cpu_) {
        log_info("%s: Failed to get MOS 7501 CPU from board\n", Traits::name);
        return false;
    }
    
    // Initialize CPU and I/O port
    board_.csg7501.init();
    cpu_->init_io_port(0x00, 0x00, 0xFF);  // C16: DDR=0 (all inputs), data=0, pins=0xFF (all high)

    // Reset the CPU to start the hardware-accurate RESET sequence.
    // The deferred hijack fetches $FFFC/$FFFD through the bus on first tick.
    board_.csg7501.reset();
    
    // Note: C16 doesn't use the io_port_mixin bank_change path.
    // Banking is handled by TED register writes.
    
    // Initialize bus state with default pin levels
    bus_state_ = C264_BUS_DEFAULT_STATE;
    
    // Create keyboard matrix (8x8, scanned by TED)
    keyboard_ = new commodore_keyboard_t();
    if (!keyboard_->init(&c16_keyboard_config)) {
        delete keyboard_;
        keyboard_ = nullptr;
    }
    if (keyboard_) {
        // Create the layered keyboard mapper for character-based input
        keyboard_mapper_.reset(create_c16_keyboard_mapper(keyboard_));
    } else {
        log_info("%s: Warning - keyboard matrix creation failed\n", Traits::name);
    }
    
    setup_ports();

    // Register chips for the Hardware menu and debug windows
    register_bus_chips(board_);

    // Set up page pointers for current RAM size and ROM banking state
    setup_ram_mirroring();

    // Pre-compute banking mode snapshots for both viewers
    build_banking_snapshots();

    // Apply initial banking state (rom_enabled=true after TED reset)
    apply_cpu_banking();
    apply_ted_video_banking();

    // Wire TED to composite video output port
    video_port_ = std::make_unique<CompositeVideoPort>();
    ted_->set_video_out(&video_port_->output());
    video_port_->bind_display(nullptr, nullptr,
                              c16_constants::DISPLAY_WIDTH, 0);
    video_port_->bind_frame_output(&last_frame_data_);

    // Wire TED to audio port (decimates TED master-clock-rate audio to host sample rate)
    audio_port_ = std::make_unique<AudioPort>();
    uint32_t ted_clock = ted_->timing.is_pal ? TED_PAL_CLOCK_HZ : TED_NTSC_CLOCK_HZ;
    audio_port_->configure(ted_clock, c16_constants::AUDIO_SAMPLE_RATE);
    ted_->set_audio_port(audio_port_.get());

    initialized_ = true;
    return true;
}

template<C264SeriesVariant V>
void Commodore264System<V>::shutdown() {
    log_info("%s: Shutting down system\n", Traits::name);
    
    // Destroy keyboard
    if (keyboard_) {
        delete keyboard_;
        keyboard_ = nullptr;
    }

    initialized_ = false;

    System::shutdown();
}

template<C264SeriesVariant V>
void Commodore264System<V>::reset() {
    log_info("%s: Resetting system\n", Traits::name);
    
    // Reset all manifest chips (CPU, TED, PIO1, PIO2, ROM bank select; RAM/ROM are no-op)
    board_.reset_chips();
    
    // Reset bus state
    bus_state_ = C264_BUS_DEFAULT_STATE;
    
    // Reset keyboard matrix
    if (keyboard_) {
        keyboard_->reset();
    }
    
    // Clear the memory locations that is_basic_ready() checks, so stale
    // values from the previous session don't cause premature detection.
    // KERNAL boot will set these properly: RAMTAS clears zero page
    // (including $2D), the vector copy writes $0302/$0303, and NEW sets
    // VARTAB ($2D) to TXTTAB+2.
    if (ram_) {
        ram_->data()[0x0302] = 0;
        ram_->data()[0x0303] = 0;
        ram_->data()[0x002D] = 0;
        // Clear the keyboard buffer count so is_basic_ready() doesn't
        // get stuck waiting for a stale non-zero $EF left by a
        // previously running program.
        ram_->data()[c16_constants::KBD_BUFFER_COUNT] = 0;
    }

    // Reset debug cart state (not real hardware, not part of board_.reset_chips())
    debug_cart_written_ = false;
    debug_cart_value_ = 0;

    // Sync page tables to post-reset banking state (rom_enabled=true, video_romsel=0)
    apply_cpu_banking();
    apply_ted_video_banking();

    // Reset deferred loading state
    reset_load_state();
    
    total_cycles_ = 0;
}

// ============================================================================
// Execution
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::tick() {
    bus_state_t s = bus_state_;
    
    // =========================================================================
    // UNIFIED TIMING MODEL (matching C64 phi1/phi2 pattern)
    // =========================================================================
    
    // Start each cycle with pull-up resistors on signal lines.
    // Carry forward only address and data from previous cycle;
    // IRQ, NMI, BA, AEC, RDY are re-derived from chip outputs each tick.
    {
        bus_state_t def = C264_BUS_DEFAULT_STATE;
        BUS_SET_ADDR(def, BUS_GET_ADDR(s));
        BUS_SET_DATA(def, BUS_GET_DATA(s));
        s = def;
    }
    
    // PHASE 1: TED PHI1
    s = ted_->tick_phi1(s);
    
    // HARDWARE WIRING: BA -> RDY (direct bit test + set/clear)
    if (BUS_GET_BIT(s, BUS_BA_BIT))
        BUS_SET_BIT(s, BUS_RDY_BIT);
    else
        BUS_CLR_BIT(s, BUS_RDY_BIT);
    
    // PHASE 2: CPU PHI2
    s = cpu_->tick<CSG7501::Phase::PHI2>(s);
    
    // PHASE 3: Address decode + flat-mem service + MMIO self-dispatch
    s = bus_.resolve(s);
    s = bus_.service(s);
    s = io_dec_->tick(s);

    // Debug cart capture ($FDCF) — VICE test convention, not real hardware.
    if (unlikely(debug_cart_enabled_ && !BUS_GET_BIT(s, BUS_RW_BIT)
                 && BUS_GET_ADDR(s) == 0xFDCF)) {
        debug_cart_value_ = BUS_GET_DATA(s);
        debug_cart_written_ = true;
    }

    // NMI edge detection — sample after bus dispatch (post-dispatch state)
    cpu_->sample_nmi_pin(s);
    
    // PHASE 3.1: TED PHI2 — CS register dispatch + DMA data delivery
    s = ted_->tick_phi2(s);
    
    // PHASE 4: CPU PHI1
    s = cpu_->tick<CSG7501::Phase::PHI1>(s);
    
    // Restore R/W line to read mode after CPU PHI1 has consumed write info
    BUS_SET_BIT(s, BUS_RW_BIT);
    
    bus_state_ = s;
    total_cycles_++;
}

template<C264SeriesVariant V>
void Commodore264System<V>::run_frame() {
    if (!video_port_) return;

    // Run until the video chip drives FrameEnd into the output.
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }

    video_port_->swap_frame();

    // Check deferred load once per frame (only active during boot)
    check_deferred_load();

    // Tick all attached peripheral devices
    tick_peripherals();
}

// ============================================================================
// CommodoreSystem virtual hook implementations — C16/Plus4
// ============================================================================

template<C264SeriesVariant V>
bool Commodore264System<V>::is_basic_ready() const {
    if (!initialized_ || !ram_) return false;

    const uint8_t* ram = ram_->data();

    // BASIC 3.5 warm-start vector at $0302/$0303 = $8712.
    // Also check VARTAB ($2D) on first boot to ensure NEW has run.
    if (ram[0x0302] != c16_constants::BASIC_WARMSTART_LO ||
        ram[0x0303] != c16_constants::BASIC_WARMSTART_HI)
        return false;

    if (ram[c16_constants::KBD_BUFFER_COUNT] != 0)
        return false;

    if (!boot_completed_ && ram[0x002D] == 0)
        return false;

    return true;
}

template<C264SeriesVariant V>
commodore_load_context_t Commodore264System<V>::build_load_context() {
    commodore_load_context_t ctx = {};
    ctx.system_name     = Traits::name;
    ctx.write_byte      = c16_mem_write_byte;
    ctx.write_block     = c16_mem_write_block;
    ctx.mem_read        = c16_mem_read;
    ctx.mem_ctx         = ram_->data();
    ctx.basic_params    = &COMMODORE_BASIC_C16;
    ctx.basic_start_addrs[0] = c16_constants::BASIC_START;
    ctx.default_raw_addr = 0x4000;
    ctx.set_pc          = set_cpu_pc;
    ctx.pc_ctx          = this;
    // inject_keys is left as nullptr — commodore_load_helpers uses the
    // default $0277/$C6, but C16 needs $0527/$EF.  We set the custom
    // callback so commodore_apply_load_result uses our inject_keys override.
    ctx.inject_keys     = [](void* kctx, const char* str) {
        auto* ram = static_cast<uint8_t*>(kctx);
        int len = static_cast<int>(strlen(str));
        if (len > c16_constants::KBD_BUFFER_SIZE)
            len = c16_constants::KBD_BUFFER_SIZE;
        for (int i = 0; i < len; i++) {
            ram[c16_constants::KBD_BUFFER_BASE + i] = static_cast<uint8_t>(str[i]);
        }
        ram[c16_constants::KBD_BUFFER_COUNT] = static_cast<uint8_t>(len);
    };
    ctx.keys_ctx        = ram_->data();
    return ctx;
}

template<C264SeriesVariant V>
void Commodore264System<V>::inject_keys(const char* str) {
    if (!ram_) return;
    auto* ram = ram_->data();
    int len = static_cast<int>(strlen(str));
    if (len > c16_constants::KBD_BUFFER_SIZE)
        len = c16_constants::KBD_BUFFER_SIZE;
    for (int i = 0; i < len; i++) {
        ram[c16_constants::KBD_BUFFER_BASE + i] = static_cast<uint8_t>(str[i]);
    }
    ram[c16_constants::KBD_BUFFER_COUNT] = static_cast<uint8_t>(len);
}

// ============================================================================
// Display
// ============================================================================


// ============================================================================
// Audio
// ============================================================================

template<C264SeriesVariant V>
uint32_t Commodore264System<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    // AudioPort path: TED drives audio_port_ per TED clock via IIR-filtered float
    if (audio_port_) {
        return static_cast<uint32_t>(audio_port_->read_samples(buffer, static_cast<int>(max_samples)));
    }

    // Legacy path: read float samples from TED's internal ring buffer
    if (!ted_) return 0;
    uint32_t avail = ted_->audio_available();
    uint32_t to_read = (avail < max_samples) ? avail : max_samples;
    if (to_read == 0) return 0;
    return ted_->audio_read(buffer, to_read);
}

template<C264SeriesVariant V>
void Commodore264System<V>::set_audio_sample_rate(int sample_rate_hz) {
    if (!ted_ || sample_rate_hz <= 0) return;

    bool is_pal = (config_.region_option_index <= 0);
    uint32_t ted_clock = is_pal ? TED_PAL_CLOCK_HZ : TED_NTSC_CLOCK_HZ;

    // Reconfigure AudioPort decimation for the negotiated host sample rate
    if (audio_port_) {
        audio_port_->configure(ted_clock, static_cast<uint32_t>(sample_rate_hz));
    }

    // Also update legacy path (TED internal ring buffer downsample ratio)
    ted_->audio_reset(ted_clock, static_cast<uint32_t>(sample_rate_hz));
}

// ============================================================================
// Input
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, SDL_SCANCODE_UNKNOWN, 0, false);
        } else {
            keyboard_mapper_->process_key_up(key, SDL_SCANCODE_UNKNOWN, 0);
        }
    } else if (keyboard_) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                keyboard_->key_down(ek, false);
            } else {
                keyboard_->key_up(ek, false);
            }
        }
    }
}

// ============================================================================
// GUI Integration
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    char reset_label[32];
    snprintf(reset_label, sizeof(reset_label), "Reset %s", Traits::name);
    if (ImGui::MenuItem(reset_label)) {
        reset();
    }
#endif
}

// ============================================================================
// Chip Registration — now done inline in initialize()
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    ImGui::Text("%s Configuration", Traits::name);
    ImGui::Separator();
    
    // Memory configuration
    ImGui::Text("System Model:");
    for (size_t i = 0; i < hardware_traits_.memory_options.size(); i++) {
        bool selected = (config_.memory_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.memory_options[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.memory_option_index = static_cast<int>(i);
            set_configuration(new_config);
            apply_configuration();
        }
    }
    
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
#endif
}

// ============================================================================
// Private Helper Methods
// ============================================================================

template<C264SeriesVariant V>
bool Commodore264System<V>::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("c16", rom_root, sizeof(rom_root))) {
        log_info("%s: ROM root not found — cannot load ROMs\n", Traits::name);
        return false;
    }
    return board_.load_roms(rom_root, Traits::name);
}

// ── RAM mirroring ────────────────────────────────────────────────────────
// Called after apply() and when ram_size_ changes.
// For 16KB models, every 16KB of address space mirrors the same RAM.
// For 64KB models, the apply() default mapping is already correct.
//
// Mirroring is handled by the chip_info_ address mask: a 16KB model uses
// mask 0x3FFF so all accesses wrap to the first 16KB of the 64KB buffer.
template<C264SeriesVariant V>
void Commodore264System<V>::setup_ram_mirroring() {
    constexpr size_t kRamBase = kC264Chips.base_id(kC264Chips.find<RAMChip>(), 8);

    // Update chip_info_ address mask to reflect actual RAM size.
    // 16KB: mask = 0x3FFF → hardware address mirroring.
    // 64KB: mask = 0xFFFF → full buffer (already set by apply()).
    const auto& ci = bus_.chip_info(kRamBase);
    bus_.set_chip_info(kRamBase, ci.base,
                       static_cast<typename Bus::MaskT>(ram_size_ - 1));
}

// ── Banking mode snapshots ───────────────────────────────────────────────
// Delegates to the generic overlay snapshot builder which derives all
// banking modes from the manifest's overlay_group tags.  Viewer 0 (CPU)
// uses the state left by apply() (with MMIO sentinels on $FD/$FF).
// Viewer 1 (TED video) is set up as pure memory (no sub-tables) by the
// builder.  The I/O page ($FD) base must be set to open bus before calling
// the builder so it is preserved across all snapshots.
template<C264SeriesVariant V>
void Commodore264System<V>::build_banking_snapshots() {
    board_.build_overlay_snapshots(bus_, kNumViewers, snapshots_);
}

// ── Apply CPU banking ────────────────────────────────────────────────────
// Loads the pre-computed snapshot for the current rom_enabled state.
// Sub-table bases (TED page ROM/RAM fallthrough) are baked into each
// snapshot, so a single load_snapshot covers everything.
template<C264SeriesVariant V>
void Commodore264System<V>::apply_cpu_banking() {
    bool rom_on = ted_ && ted_->rom_enabled;
    bus_.load_snapshot(c264_viewer::kCpu, snapshots_[c264_viewer::kCpu][rom_on ? 1 : 0]);
}

// ── Apply TED video banking ──────────────────────────────────────────────
// Loads the pre-computed snapshot for the current video_romsel state.
// No sub-tables to update — viewer 1 is pure page table.
template<C264SeriesVariant V>
void Commodore264System<V>::apply_ted_video_banking() {
    bool romsel = ted_ && (ted_->regs_[MEM_CTRL] & 0x04) != 0;
    bus_.load_snapshot(c264_viewer::kTedVideo,
                       snapshots_[c264_viewer::kTedVideo][romsel ? 1 : 0]);
}

// ============================================================================
// MOS 7501 I/O PORT CALLBACKS
// ============================================================================

template<C264SeriesVariant V>
uint8_t Commodore264System<V>::io_port_in(void* user_data) {
    (void)user_data;
    // Stub: all input lines HIGH (no external devices connected yet)
    return 0x5F;
}

template<C264SeriesVariant V>
void Commodore264System<V>::io_port_out(uint8_t data, void* user_data) {
    (void)data;
    (void)user_data;
    // Stub: ignore output for now
    // TODO: Handle cassette motor (bit 0), serial bus SRQ/DATA/CLK/ATN (bits 1-4),
    //       cassette sense (bit 6). Bit 5 absent (mask 0x5F).
}

// ============================================================================
// TED KEYBOARD SCAN CALLBACK
// ============================================================================

template<C264SeriesVariant V>
uint8_t Commodore264System<V>::ted_keyboard_scan(void* user_data, uint8_t column) {
    auto* sys = static_cast<Commodore264System<V>*>(user_data);
    if (!sys->keyboard_) return 0xFF;

    // On real hardware, PIO2 ($FD30) selects which keyboard rows to drive
    // (active-low).  The value written to $FF08 (the 'column' parameter)
    // only controls joystick port selection — the keyboard row select comes
    // from PIO2.  See VICE ted-mem.c ted08_store() for reference.
    uint8_t row_select = sys->pio2_ ? sys->pio2_->output_latch : 0xFF;
    uint8_t result = 0xFF;
    for (int row = 0; row < 8; row++) {
        if (!(row_select & (1 << row))) {
            result &= static_cast<uint8_t>(sys->keyboard_->row_open_contacts[row]);
        }
    }
    return result;
}

// ============================================================================
// TED MEMORY READ CALLBACK
// ============================================================================

// TED video memory reads go through viewer 1 (pre-computed page table).
// The active snapshot (snapshots_[kTedVideo][0 or 1]) is selected by the
// banking_change callback when $FF12 bit 2 (video_romsel) changes.
template<C264SeriesVariant V>
uint8_t Commodore264System<V>::ted_mem_read(void* user_data, uint16_t address) {
    auto* sys = static_cast<Commodore264System<V>*>(user_data);
    return sys->bus_.peek_byte(address, c264_viewer::kTedVideo);
}

// ============================================================================
// TED BANKING CHANGE CALLBACK
// ============================================================================

// Called by TED when rom_enabled or video_romsel changes.
// Loads the appropriate pre-computed snapshot for the affected viewer.
template<C264SeriesVariant V>
void Commodore264System<V>::ted_banking_changed(void* user_data, uint8_t changes) {
    auto* sys = static_cast<Commodore264System<V>*>(user_data);
    if (changes & TED_BANK_ROM_LATCH)
        sys->apply_cpu_banking();
    if (changes & TED_BANK_VIDEO_ROMSEL)
        sys->apply_ted_video_banking();
}

// ============================================================================
// LOAD HELPER — set CPU PC
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::set_cpu_pc(void* user_data, uint16_t addr) {
    auto* sys = static_cast<Commodore264System<V>*>(user_data);
    if (sys->cpu_) {
        sys->cpu_->set(PC, addr);
        sys->cpu_->set(AB, addr);
        sys->cpu_->transition_to_fetch();
        log_info("%s: PC set to $%04X\n", Traits::name, addr);
    }
}

// ============================================================================
// CONNECTOR PORT SETUP
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::setup_ports() {

    // Port 0 — Joystick Port 1
    add_port(c16_joy_port_1_def, 1);

    // Port 1 — Joystick Port 2
    add_port(c16_joy_port_2_def, 2);

    // Port 2 — IEC Serial Bus
    add_port(c16_iec_serial_def, 0);

    // Port 3 — Cassette Port
    add_port(c16_cassette_def, 0);

    // Port 4 — User Port (Plus/4 only)
    if constexpr (Traits::has_user_port) {
        add_port(plus4_user_port_def, 0);
    }

    // Port 5 — Expansion Port (cartridge slot)
    add_port(c16_expansion_def, 0);

    // Video/Audio output ports
    add_port(c16_video_out_def, 0);
    add_port(c16_audio_out_def, 0);

    // Internal Keyboard (always attached)
    static const PortDefinition c16_keyboard_def = {
        PortType::CUSTOM, "Keyboard", nullptr, 0, true, false
    };
    int kb_port = add_port(c16_keyboard_def, 0);

    // Attach internal keyboard device
    auto kb_device = std::make_unique<CommodoreKeyboardDevice>(keyboard_);
    auto* kb_raw = kb_device.get();
    get_port(kb_port)->attach_device(kb_raw);
    owned_devices_.push_back(std::move(kb_device));

    log_info("%s: Created %zu ports\n",
           Traits::name, get_ports().size());
}

template<C264SeriesVariant V>
std::vector<System::DefaultPeripheral>
Commodore264System<V>::get_default_peripherals() const {
    // Video port index depends on variant (Plus/4 has user port at index 4)
    int video_idx = Traits::has_user_port ? 6 : 5;
    return {
        { 0, "joystick"  },  // Joystick Port 1
        { 1, "joystick"  },  // Joystick Port 2
        { 2, "1541"      },  // IEC Serial Bus — 1541 disk drive
        { 3, "datasette" },  // Cassette Port  — datasette (1530)
        { video_idx, "crt_tv" },  // Video Out — Color TV
    };
}

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

template class Commodore264System<C264SeriesVariant::C16>;
template class Commodore264System<C264SeriesVariant::C116>;
template class Commodore264System<C264SeriesVariant::PLUS4>;

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(C116System::static_descriptor(), []() {
    return std::make_unique<C116System>();
})

REGISTER_SYSTEM(C16System::static_descriptor(), []() {
    return std::make_unique<C16System>();
})

REGISTER_SYSTEM(Plus4System::static_descriptor(), []() {
    return std::make_unique<Plus4System>();
})
