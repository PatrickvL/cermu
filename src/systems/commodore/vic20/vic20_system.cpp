#include "systems/commodore/vic20/vic20_system.hpp"
#include "systems/commodore/vic20/vic20_constants.hpp"
#include "systems/commodore/vic20/vic20_chips.hpp"
#include "core/cermu.hpp"
#include "chip/input/commodore_keyboard.hpp"
#include "core/input/emu_key_sdl_map.hpp"
#include "systems/commodore/vic20/vic20_keyboard_matrix.hpp" // VIC-20 keyboard matrix data
#include "systems/commodore/prg_content_analysis.hpp"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>

// Forward declarations
static KeyboardMapper* create_vic20_keyboard_mapper(commodore_keyboard_t* keyboard);

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// Include chip headers — CPU uses fam65xx.hpp directly for inlining
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/vic/mos6560.hpp"
#include "chip/video/vic/mos6561.hpp"
#include "chip/video/vic/vic_common.hpp"  // For VIC_COLOR_* constants
#include "chip/io/mos6522.hpp"
#include "core/chip.hpp"

// Include bus interface
#include "core/bus_cycle_interface.hpp"

// Include ROM loader
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"

// File format handlers and registry
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

// ============================================================================
// Hardware Traits Definition
// ============================================================================

static HardwareTraits create_vic20_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - VIC-20 uses MOS6560/6561 (VIC)
    // Full VIC output including borders: 63 cycles × 4 pixels = 252 pixels wide
    // Visible raster lines including borders: ~284 lines (PAL)
    traits.display.native_width = vic20_constants::DISPLAY_WIDTH;
    traits.display.native_height = vic20_constants::DISPLAY_HEIGHT;
    traits.display.visible_width = vic20_constants::DISPLAY_WIDTH;
    traits.display.visible_height = vic20_constants::DISPLAY_HEIGHT;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 16;        // 16 colors
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // VIC-20 PAL palette (16 colors) - Hardware accurate colors
    // Get from VIC chip (ABGR format) and convert to RGB for hardware traits
    // This ensures palette consistency between VIC chip rendering and system traits
    uint32_t* vic_palette_abgr = vic_base_t::get_default_palette();
    
    for (int i = 0; i < 16; i++) {
        uint32_t abgr = vic_palette_abgr[i];
        // Convert from ABGR (0xAABBGGRR) to RGB components
        uint8_t r = abgr & 0xFF;
        uint8_t g = (abgr >> 8) & 0xFF;
        uint8_t b = (abgr >> 16) & 0xFF;
        traits.display.default_palette.push_back(
            PaletteColor(r, g, b, 255)
        );
    }
    
    // Audio traits - VIC-20 has simple sound from VIC chip
    traits.audio.format = AudioFormat::MONO_8BIT;
    traits.audio.sample_rate_hz = vic20_constants::AUDIO_SAMPLE_RATE;
    traits.audio.channels = 1;
    traits.audio.chip_name = "VIC 6560/6561";
    
    // Timing - PAL version (NTSC differs)
    traits.timing.cpu_frequency_hz = vic20_constants::CPU_FREQ_PAL;
    traits.timing.video_frequency_hz = vic20_constants::CPU_FREQ_PAL;
    traits.timing.audio_sample_rate_hz = vic20_constants::AUDIO_SAMPLE_RATE;
    traits.timing.target_fps = 50;
    traits.timing.cycles_per_frame = vic20_constants::CYCLES_PER_FRAME_PAL;
    traits.timing.standard = VideoStandard::PAL;
    
    // Memory options
    traits.memory_options.push_back({
        "Unexpanded (5KB RAM)",
        5120,
        0,
        true
    });
    traits.memory_options.push_back({
        "3KB Expansion (8KB total)",
        8192,
        0,
        false
    });
    traits.memory_options.push_back({
        "8KB Expansion (13KB total)",
        13312,  // 5KB + 8KB
        0,
        false
    });
    traits.memory_options.push_back({
        "16KB Expansion (21KB total)",
        21504,  // 5KB + 16KB
        0,
        false
    });
    traits.memory_options.push_back({
        "24KB Expansion (29KB total)",
        29696,  // 5KB + 24KB
        0,
        false
    });
    traits.memory_options.push_back({
        "Full Expansion (32KB total)",
        37888,
        0,
        false
    });
    
    // Region options
    traits.video_standard_configs.push_back({
        "PAL",
        VideoStandard::PAL,
        traits.timing,
        true
    });
    
    SystemTiming ntsc_timing = traits.timing;
    ntsc_timing.cpu_frequency_hz = vic20_constants::CPU_FREQ_NTSC;
    ntsc_timing.video_frequency_hz = vic20_constants::CPU_FREQ_NTSC;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 17045;
    ntsc_timing.standard = VideoStandard::NTSC;
    
    traits.video_standard_configs.push_back({
        "NTSC",
        VideoStandard::NTSC,
        ntsc_timing,
        false
    });

    // Drive emulation mode
    CommodoreSystem::add_drive_mode_option(traits);

    return traits;
}

/** Check if load address is a VIC-20 address */
static bool is_vic20_load_address(uint16_t addr) {
    return addr == vic20_constants::BASIC_START_UNEXPANDED ||
           addr == vic20_constants::BASIC_START_3K ||
           addr == vic20_constants::BASIC_START_8K ||
           addr == 0x1000 ||                               // Default screen RAM start / ML
           addr == 0x2000 ||                               // Expansion block 1
           addr == 0x4000 ||                               // Expansion block 2
           addr == 0x6000 ||                               // Expansion block 3
           addr == 0xA000;                                 // Cartridge ROM
}

// ============================================================================
// VIC-20 memory expansion helper — determines minimum memory config index
// from a PRG's load address and end address.
//
// Memory option indices (from create_vic20_hardware_traits):
//   0 = Unexpanded 5KB    ($1000-$1FFF user RAM)
//   1 = +3KB              ($0400-$0FFF added)
//   2 = +8KB              ($4000-$5FFF added, 13KB total)
//   3 = +16KB             ($2000-$3FFF + $4000-$5FFF, 21KB total)
//   4 = +24KB             (above + $6000-$7FFF, 29KB total)
//   5 = Full 32KB         (all blocks, 37KB total)
// ============================================================================
static int vic20_memory_index_for_prg(uint16_t load_addr, uint32_t end_addr) {
    int mem = 0;

    if (load_addr == vic20_constants::BASIC_START_3K) {
        mem = 1;
        if (end_addr > 0x1FFF) mem = 2;
        if (end_addr > 0x5FFF) mem = 3;
        if (end_addr > 0x7FFF) mem = 5;
    } else if (load_addr == vic20_constants::BASIC_START_8K) {
        mem = 2;
        if (end_addr > 0x5FFF) mem = 3;
        if (end_addr > 0x7FFF) mem = 5;
    } else if (load_addr == vic20_constants::BASIC_START_UNEXPANDED) {
        mem = 0;
        if (end_addr > 0x1FFF) mem = 2;
        if (end_addr > 0x5FFF) mem = 3;
        if (end_addr > 0x7FFF) mem = 5;
    } else {
        if (load_addr >= 0x0400 && load_addr < 0x1000) mem = 1;
        if (load_addr >= 0x2000 && load_addr < 0x4000) mem = 3;
        if ((load_addr >= 0x4000 && load_addr < 0x6000) ||
            (end_addr > 0x4000 && end_addr <= 0x6000))
            { if (mem < 2) mem = 2; }
        if ((load_addr >= 0x6000 && load_addr < 0x8000) ||
            (end_addr > 0x6000 && end_addr <= 0x8000))
            { if (mem < 4) mem = 4; }
        if (end_addr > 0x6000 && load_addr < 0x6000)
            { if (mem < 4) mem = 4; }
    }

    return mem;
}

// ============================================================================
// VIC-20 file probe — unified confidence + configuration detection
// ============================================================================

static SystemProbeResult vic20_probe_file(
    const format_descriptor_t* matched_format,
    const char* filepath,
    const uint8_t* data, size_t size)
{
    SystemProbeResult result{};

    if (!matched_format) return result;

    // --- PRG: confidence from load address, config from memory expansion ---
    if (matched_format == &PRG_FORMAT_DESCRIPTOR) {
        if (size >= 2) {
            uint16_t load_addr = data[0] | (data[1] << 8);
            uint32_t end_addr  = (uint32_t)load_addr + (uint32_t)(size - 2);

            if (load_addr == vic20_constants::BASIC_START_UNEXPANDED) {
                // Unexpanded VIC-20 has screen at $1E00 → ~3 KB for BASIC.
                // Programs starting at $1001 that extend far beyond that are
                // almost certainly C16/Plus4 (which shares the $1001 address).
                if (end_addr > 0x8000)
                    result.confidence = 0.30f;      // Way beyond any VIC-20 config
                else if (end_addr > 0x4000)
                    result.confidence = 0.45f;      // Exceeds expanded VIC-20 BASIC area
                else if (end_addr > 0x2000)
                    result.confidence = 0.60f;      // Larger than unexpanded can hold
                else
                    result.confidence = 0.85f;      // Fits in unexpanded VIC-20
            } else if (is_vic20_load_address(load_addr)) {
                result.confidence = 0.7f;
            } else {
                result.confidence = 0.5f;
            }

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
                    // Looks like BASIC — penalise if BASIC 3.5 tokens found
                    if ((load_addr == vic20_constants::BASIC_START_UNEXPANDED ||
                         load_addr == vic20_constants::BASIC_START_3K ||
                         load_addr == vic20_constants::BASIC_START_8K)
                        && has_basic35_tokens(payload, payload_len))
                        result.confidence *= 0.30f;  // BASIC 3.5 → not VIC-20

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
                    int vic20_hits = count_mmio_flags(mmio & MMIO_ANY_VIC20);

                    if (sys_based) {
                        // SYS-based: require narrow VIC/VIA registers
                        // (0.07 % of address space) — Color RAM ($9400-$97FF)
                        // is too broad and triggers data-as-code false positives.
                        int vic20_strong = count_mmio_flags(mmio & MMIO_VIC20_STRONG);
                        if (vic20_strong >= 2)
                            result.confidence = std::max(result.confidence, 0.92f);
                    } else {
                        // Pure ML: full classification with penalties
                        if (vic20_hits >= 2)
                            result.confidence = std::max(result.confidence, 0.92f);
                        else {
                            int c64_strong = count_mmio_flags(mmio & MMIO_C64_STRONG);
                            int c16_hits   = count_mmio_flags(mmio & MMIO_ANY_C16);
                            if (c64_strong >= 1 && vic20_hits == 0)
                                result.confidence *= 0.50f;
                            else if (c16_hits >= 1 && vic20_hits == 0)
                                result.confidence *= 0.50f;
                        }
                    }
                }
            }

            result.configuration.memory_option_index =
                vic20_memory_index_for_prg(load_addr, end_addr);
        }

    // --- LNX: inspect all contained files for VIC-20 addresses + memory ---
    } else if (matched_format == &LNX_FORMAT_DESCRIPTOR) {
        commodore_lynx_t lynx;
        if (lynx.open_mem(data, size)) {
            commodore_lynx_directory_t dir;
            if (lynx.read_directory(&dir)) {
                bool found_vic20 = false;
                bool found_any   = false;
                int  mem_index   = 0;

                for (unsigned i = 0; i < dir.file_count; i++) {
                    if (dir.entries[i].file_type == 'P' && dir.entries[i].data_length >= 2) {
                        size_t off = dir.entries[i].data_offset;
                        if (off + 1 < lynx.data_size) {
                            uint16_t addr = lynx.data[off] | ((uint16_t)lynx.data[off+1] << 8);
                            uint32_t ea   = (uint32_t)addr + (uint32_t)dir.entries[i].data_length;
                            found_any = true;
                            if (is_vic20_load_address(addr)) found_vic20 = true;

                            // Accumulate memory expansion from each entry
                            if (addr >= 0x0400 && addr < 0x1000)  { if (mem_index < 1) mem_index = 1; }
                            if (addr >= 0x2000 && addr < 0x4000)  { if (mem_index < 3) mem_index = 3; }
                            if ((addr >= 0x4000 && addr < 0x6000) || (ea > 0x4000 && ea <= 0x6000))
                                { if (mem_index < 2) mem_index = 2; }
                            if ((addr >= 0x6000 && addr < 0x8000) || (ea > 0x6000 && ea <= 0x8000))
                                { if (mem_index < 4) mem_index = 4; }
                            if (addr == vic20_constants::BASIC_START_3K) { if (mem_index < 1) mem_index = 1; }
                            if (addr == vic20_constants::BASIC_START_8K) { if (mem_index < 2) mem_index = 2; }
                            if (ea > 0x6000 && addr < 0x6000) { if (mem_index < 4) mem_index = 4; }
                        }
                    }
                }
                // Many files spanning wide ranges -> full expansion
                if (dir.file_count > 3 && mem_index >= 2) mem_index = 5;

                lynx.close();
                result.confidence = found_vic20 ? 0.90f : (found_any ? 0.4f : 0.5f);
                result.configuration.memory_option_index = mem_index;
            } else {
                lynx.close();
                result.confidence = 0.5f;
            }
        } else {
            result.confidence = 0.5f;
        }

    // --- D64: extract first PRG, check load address + size + memory ---
    } else if (matched_format == &D64_FORMAT_DESCRIPTOR) {
        commodore_d64_t d64;
        if (d64.open_mem(data, size)) {
            commodore_prg_t prg = {};
            if (d64.extract_first_prg(&prg)) {
                uint32_t end_addr = (uint32_t)prg.load_addr + (uint32_t)prg.data_size;

                if (prg.load_addr == vic20_constants::BASIC_START_UNEXPANDED) {
                    // $1001 is shared by VIC-20 (unexpanded) and C16/Plus4.
                    // Use size to estimate, but stay below alias-boost
                    // threshold (0.90) so filepath context can disambiguate.
                    if (end_addr > 0x8000)
                        result.confidence = 0.30f;
                    else if (end_addr > 0x4000)
                        result.confidence = 0.45f;
                    else if (end_addr > 0x2000)
                        result.confidence = 0.60f;
                    else
                        result.confidence = 0.80f;  // Fits unexpanded, but ambiguous with C16

                    // BASIC 3.5 tokens → definitely C16/Plus4, not VIC-20
                    if (prg.data_size > 4 && has_basic35_tokens(prg.data, prg.data_size))
                        result.confidence *= 0.30f;
                } else {
                    result.confidence = is_vic20_load_address(prg.load_addr) ? 0.90f : 0.4f;
                }

                result.configuration.memory_option_index =
                    vic20_memory_index_for_prg(prg.load_addr, end_addr);
                commodore_prg_free(&prg);
            } else {
                result.confidence = 0.5f;
            }
            d64.close();
        } else {
            result.confidence = 0.5f;
        }

    // --- TAP: platform byte distinguishes C64 / VIC-20 / C16 ---
    } else if (matched_format == &TAP_FORMAT_DESCRIPTOR) {
        int platform = commodore_tap_identify_platform_mem(data, size);
        if (platform == 1)      result.confidence = 0.95f;  // VIC-20 TAP
        else if (platform == 0) result.confidence = 0.3f;   // C64 TAP
        else                    result.confidence = 0.5f;

    // --- T64: usually C64-centric but can contain VIC-20 programs ---
    } else if (matched_format == &T64_FORMAT_DESCRIPTOR) {
        result.confidence = 0.5f;

    // --- BIN: generic binary ---
    } else if (matched_format == &BIN_FORMAT_DESCRIPTOR) {
        result.confidence = 0.3f;

    // --- CRT: VIC-20 cartridge image ---
    } else if (matched_format == &CRT_FORMAT_DESCRIPTOR) {
        if (size >= 64 && memcmp(data, "VIC20 CARTRIDGE ", 16) == 0)
            result.confidence = 1.0f;
        // C64 CRT files are not ours
        else if (size >= 64 && memcmp(data, "C64 CARTRIDGE   ", 16) == 0)
            result.confidence = 0.0f;
    }

    // =================================================================
    // =================================================================
    // Filepath heuristics — variant-specific keyword boost is now
    // handled generically by SystemRegistry::identify_system() using
    // aliases.  Only configuration hints (region) remain here.
    // =================================================================
    if (filepath) {
        std::string lower(filepath);
        for (auto& c : lower) c = static_cast<char>(tolower(c));

        // Region hint
        if (lower.find("ntsc") != std::string::npos)
            result.configuration.region_option_index = 1;   // NTSC
    }

    return result;
}

/** Formats the VIC-20 can load — used by SystemDescriptor and file dialogs. */
static const format_descriptor_t* const vic20_formats[] = {
    &PRG_FORMAT_DESCRIPTOR, &TAP_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR,
    &T64_FORMAT_DESCRIPTOR, &LNX_FORMAT_DESCRIPTOR, &CRT_FORMAT_DESCRIPTOR,
    &BIN_FORMAT_DESCRIPTOR,
    nullptr
};

static SystemDescriptor vic20_descriptor = {
    "Commodore VIC-20",
    "VIC20",
    "Commodore VIC-20 (1980) - 5KB RAM, 22-column display",
    "vic20",
    {"VIC20", "VIC-20", "VIC 20", "VIC_20"},
    vic20_formats,
    create_vic20_hardware_traits(),
    vic20_probe_file,
    "Commodore", 1980, fam65xx::MOS6502Traits.display_name, SystemType::Home
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
VIC20System::VIC20System()
    : CommodoreSystem()
    , expansion_flags_(VIC20_EXP_NONE)
{
    cycles_per_frame_ = vic20_constants::CYCLES_PER_FRAME_PAL;
    hardware_traits_ = create_vic20_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    
    // Initialize bus state with pull-up resistors (all control lines HIGH = inactive)
    bus_.default_state = VIC20_BUS_DEFAULT_STATE;
    bus_.state = bus_.default_state;
}

VIC20System::~VIC20System() {
    // Destroy keyboard (not part of chip manifest)
    if (keyboard_) {
        delete keyboard_;
        keyboard_ = nullptr;
    }
    
    // CPU, VIC, VIA1, VIA2, and memory chips are all owned by board_
    // and cleaned up automatically via its owned_chips_ vector.
}

// ============================================================================
// System Identification
// ============================================================================

const SystemDescriptor& VIC20System::get_descriptor() const {
    return vic20_descriptor;
}

// ============================================================================
// Configuration Management
// ============================================================================

bool VIC20System::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        const VideoStandardConfig& std_cfg = hardware_traits_.video_standard_configs[config_.region_option_index];
        cycles_per_frame_ = std_cfg.timing.cycles_per_frame;
    }
    
    // Apply memory configuration
    if (config_.memory_option_index >= 0 &&
        config_.memory_option_index < static_cast<int>(hardware_traits_.memory_options.size())) {
        
        // Map memory option index to expansion flags
        switch (config_.memory_option_index) {
            case 0:  // Unexpanded
                expansion_flags_ = VIC20_EXP_NONE;
                break;
            case 1:  // 3KB expansion
                expansion_flags_ = VIC20_EXP_3K;
                break;
            case 2:  // 8KB expansion
                expansion_flags_ = VIC20_EXP_8K;
                break;
            case 3:  // 16KB expansion
                expansion_flags_ = VIC20_EXP_16K;
                break;
            case 4:  // 24KB expansion
                expansion_flags_ = VIC20_EXP_24K;
                break;
            case 5:  // Full expansion
                expansion_flags_ = VIC20_EXP_FULL;
                break;
            default:
                expansion_flags_ = VIC20_EXP_NONE;
                break;
        }
        
        // Update memory system if already initialized
        if (initialized_) {
            setup_expansion_map();
        }
        
        log_info("VIC20: Expansion configuration: $%02X\n", expansion_flags_);
    }
    
    return true;
}


// ============================================================================
// System Lifecycle
// ============================================================================
bool VIC20System::initialize() {
    if (initialized_) return true;
    
    log_info("VIC20: Initializing system\n");
    register_board(&board_);
    
    // ── Bind all value-typed chips, then factory-create memory chips ─────
    bind_all(board_, board_.components_, kVIC20Manifest);

    // Set port manifest for default peripheral attachment.
    port_manifest_       = kVIC20Manifest.port_slots;
    port_manifest_count_ = kVIC20Manifest.port_count;

    // ── NTSC: swap the PAL VIC for a factory-created MOS 6560 ────────────
    const bool is_ntsc = (config_.region_option_index > 0);
    if (is_ntsc) {
        board_.unbind_slot(kVIC20_VicSlot);
        board_.override_factory(kVIC20_VicSlot, resolve_slot_factory<mos6560_t>());
        board_.override_label(kVIC20_VicSlot, "MOS 6560 (NTSC)");
    }

    board_.create_chips(&bus_.state);

    // Page tables + MMIO are configured later by setup_expansion_map()
    // which calls apply() with condition-aware expansion block filtering.

    // ── Retrieve polymorphic VIC pointer ─────────────────────────────────
    vic_ = board_.chip_as<vic_base_t>(kVIC20_VicSlot);
    
    // Initialize Color RAM to cyan (color 3) for proper text visibility
    memset(board_.colorram.memory, VIC_COLOR_CYAN, 1024);

    // Initialize load callback pointers (ram0 through blk3 are contiguous
    // in Board flat memory, so ram0.data() serves as base for $0000-$7FFF).
    load_mem_ctx_ = { board_.ram0.data(), board_.cart.data() };
    
    // Load ROMs into ROMChip buffers
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        log_info("VIC20: Warning - ROMs not loaded, system may not function correctly\n");
    }
    
    // ── Post-creation wiring: init/reset/callbacks ──────────────────────
    
    // CPU
    board_.cpu.init();
    board_.cpu.reset();
    
    // VIC — PAL uses value-typed board_.vic, NTSC uses heap-created mos6560_t.
    // Both accessed uniformly via vic_ (vic_base_t*).
    if (!vic_) {
        log_info("VIC20: Failed to create VIC chip\n");
        return false;
    }
    if (is_ntsc) {
        static_cast<mos6560_t*>(vic_)->init();
    } else {
        board_.vic.init();
    }
    log_info("VIC20: Created %s VIC chip\n",
           is_ntsc ? "MOS6560 (NTSC)" : "MOS6561 (PAL)");
    
    // Set up VIC memory callbacks for accessing video and character memory
    vic_->set_memory_callbacks(
        VIC20System::vic_mem_read,      // Memory read callback
        this,                            // User data (VIC20System instance)
        VIC20System::vic_color_read,    // Color RAM read callback
        this);                           // User data for color RAM
    
    // VIA chips (MOS6522) — value-typed in Chips
    // VIC-20 hardware: VIA1 ($9110) → NMI line, VIA2 ($9120) → IRQ line
    // VIA2 Timer 1 is the system heartbeat (jiffy clock, keyboard scan, cursor blink)
    board_.via1.reset();
    board_.via1.interrupt_bit = BUS_NMI_BIT;

    board_.via2.reset();
    board_.via2.interrupt_bit = BUS_IRQ_BIT;

    // I/O decoder — wire to VIC and VIAs
    board_.io_dec.wire(vic_, &board_.via1, &board_.via2);
    
    // Create keyboard matrix and connect to VIA2
    // VIC-20 keyboard: VIA2 Port B selects columns, VIA2 Port A reads rows
    keyboard_ = new commodore_keyboard_t();
    if (!keyboard_->init(&vic20_keyboard_config)) {
        delete keyboard_;
        keyboard_ = nullptr;
    }
    if (keyboard_) {
        // Create the layered keyboard mapper for character-based input
        keyboard_mapper_.reset(create_vic20_keyboard_mapper(keyboard_));
        build_petscii_map();
        
        // Register port read callbacks for keyboard matrix scanning
        // Port A reads rows, Port B reads columns (reverse scanning)
        board_.via2.set_port_a_read_callback(vic20_via2_port_a_read, this);
        board_.via2.set_port_b_read_callback(vic20_via2_port_b_read, this);
        log_info("VIC20: Keyboard connected to VIA2 via callbacks\n");
    } else {
        log_info("VIC20: Warning: Could not create keyboard\n");
    }
    
    // Register all manifest-created chips for the Hardware menu and debug windows
    register_bus_chips(board_);
    
    // Set up page pointers for current expansion and ROM banking
    setup_expansion_map();

    // Wire VIC chip to video output port
    video_port_ = std::make_unique<CompositeVideoPort>();
    vic_->set_video_out(&video_port_->output());
    video_port_->bind_display(nullptr, nullptr,
                              vic20_constants::DISPLAY_WIDTH, 8);
    video_port_->bind_frame_output(&last_frame_data_);

    // Wire VIC chip to audio port (decimates chip-rate audio to host sample rate)
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(vic_->clock_frequency, vic20_constants::AUDIO_SAMPLE_RATE);
    vic_->set_audio_port(audio_port_.get());

    // Initialize cycle-accurate drive subsystem (reads drive_mode from config)
    init_drive_subsystem();

    initialized_ = true;
    return true;
}

void VIC20System::shutdown() {
    log_info("VIC20: Shutting down system\n");
    System::shutdown();
}

void VIC20System::reset() {
    log_info("VIC20: Resetting system\n");
    
    // Reset all manifest chips (VIC, VIA1, VIA2; RAM/ROM/CPU are no-op)
    board_.reset_chips();
    
    // Re-establish VIC callbacks (reset clears them)
    if (vic_) {
        vic_->set_memory_callbacks(
            VIC20System::vic_mem_read, this,
            VIC20System::vic_color_read, this);
    }
    
    // Clear RAM (zero page, stack, main RAM $0000-$7FFF) but preserve ROMs
    // ram0 through blk3 are contiguous in Board flat memory.
    memset(board_.ram0.data(), 0, 0x8000);            // $0000-$7FFF: all RAM
    // Reinitialize Color RAM to default cyan
    memset(board_.colorram.memory, VIC_COLOR_CYAN, 1024);
    
    // Clear the framebuffer to black
    if (rgba_framebuffer_ && rgba_width_ > 0 && rgba_height_ > 0) {
        memset(rgba_framebuffer_, 0, (size_t)rgba_width_ * rgba_height_ * sizeof(uint32_t));
    }
    
    // Reset CPU last (so it picks up clean bus state)
    board_.cpu.reset();
    
    // Reset bus state
    bus_.state = bus_.default_state;
    
    total_cycles_ = 0;

    // Reset deferred loading state
    reset_load_state();

    // Reset cycle-accurate drive subsystem
    reset_drive_subsystem();
}

// ============================================================================
// Execution
// ============================================================================

// Unified bus dispatch: I/O region handled manually, everything else through MemoryBus.
// mem_tick / io_tick removed — CS-tick architecture:
//   resolve() → service() handles RAM/ROM/Color RAM via page table,
//   board_.io_dec.tick() dispatches VIC/VIA1/VIA2 via 74LS138 decode.

void VIC20System::tick() {
    // Start with clean bus state (pull-up resistors)
    bus_state_t s = bus_.default_state;

    // Preserve address and data from previous cycle
    BUS_SET_ADDR(s, BUS_GET_ADDR(bus_.state));
    BUS_SET_DATA(s, BUS_GET_DATA(bus_.state));

    // =========================================================================
    // PHASE 1: VIC CHIP TICKING
    // VIC-20's VIC chip runs continuously, generating video and handling DMA
    // =========================================================================
    s = vic_->tick(s);

    // =========================================================================
    // PHASE 2: VIA CHIPS TICKING (BEFORE CPU PHI2)
    // VIA chips handle I/O and timing, must tick before CPU to set interrupt lines
    // =========================================================================
    s = board_.via1.tick(s);
    s = board_.via2.tick(s);

    // =========================================================================
    // PHASE 3: CPU TICKING (PHI2 phase - sets up memory access)
    // CPU executes instruction and puts address/control on bus
    // =========================================================================
    auto& cpu = board_.cpu;
    s = cpu.tick<MOS6502::Phase::PHI2>(s);

    // =========================================================================
    // PHASE 4: Address decode + flat-mem service + MMIO self-dispatch
    // resolve() sets CS from page table, service() handles RAM/ROM/Color RAM,
    // board_.io_dec.tick() dispatches VIC/VIA1/VIA2 via 74LS138 decode.
    // =========================================================================
    s = mem_bus_.resolve(s);
    s = mem_bus_.service(s);
    s = board_.io_dec.tick(s);

    // NMI edge detection — sample after bus dispatch (post-dispatch state)
    cpu.sample_nmi_pin(s);

    // =========================================================================
    // PHASE 5: CPU TICKING (PHI1 phase - completes cycle)
    // CPU prepares next instruction fetch
    // =========================================================================
    s = cpu.tick<MOS6502::Phase::PHI1>(s);

    // =========================================================================
    // PHASE 5.5: Cycle-accurate IEC drive integration
    //
    // VIC-20 IEC bus wiring (from VICE vic20iec.c):
    //   VIA1 PA7     = ATN OUT   (bit 7 HIGH → ATN asserted → line LOW)
    //   VIA2 CA2     = CLK OUT   (PCR bits 3:1 = 110 → LOW, 111 → HIGH)
    //   VIA2 CB2     = DATA OUT  (PCR bits 7:5 = 110 → LOW, 111 → HIGH)
    //   VIA1 PA0     = CLK IN    (bit 0 HIGH → bus CLK is LOW)
    //   VIA1 PA1     = DATA IN   (bit 1 HIGH → bus DATA is LOW)
    // =========================================================================
    if (is_drive_cycle_accurate()) {
        // Read host VIA output → IEC bus
        uint8_t via1_pa_out = board_.via1.port_a.output();
        uint8_t via2_pcr    = board_.via2.pcr;

        // ATN OUT: VIA1 PA7 HIGH = ATN asserted (transistor inverts → line LOW)
        bool atn_released = !(via1_pa_out & 0x80);

        // CLK OUT: VIA2 CA2 manual output (PCR bits 3:1)
        //   110 (0x0C) = manual LOW = CLK asserted (transistor inverts)
        //   111 (0x0E) = manual HIGH = CLK released
        bool clk_released = ((via2_pcr & 0x0E) == 0x0E);

        // DATA OUT: VIA2 CB2 manual output (PCR bits 7:5)
        //   110 (0xC0) = manual LOW = DATA asserted
        //   111 (0xE0) = manual HIGH = DATA released
        bool data_released = ((via2_pcr & 0xE0) == 0xE0);

        drive_subsystem_.sync_host_to_iec_signals(atn_released, clk_released, data_released);
        drive_subsystem_.advance_drives(total_cycles_);

        // Read IEC bus → VIA1 PA input pins (bits 0-1)
        bool bus_clk_released, bus_data_released;
        drive_subsystem_.sync_iec_to_host_signals(bus_clk_released, bus_data_released);

        // VIA1 PA0 = CLK IN: bus CLK LOW → PA0 HIGH (inverted by hardware)
        // VIA1 PA1 = DATA IN: bus DATA LOW → PA1 HIGH
        uint8_t pa_in = board_.via1.port_a_pins_;
        pa_in = (pa_in & 0xFC)
              | (bus_clk_released  ? 0 : 0x01)   // CLK LOW → PA0 HIGH
              | (bus_data_released ? 0 : 0x02);   // DATA LOW → PA1 HIGH
        board_.via1.port_a_pins_ = pa_in;
    }

    // Restore R/W line to read mode after CPU PHI1 has consumed write info.
    // Maintains invariant: BUS_MASK_RW is always set outside the CPU write window.
    BUS_SET_BIT(s, BUS_RW_BIT);

    // Update bus state
    bus_.state = s;
    total_cycles_++;
}

void VIC20System::run_frame() {
    if (!video_port_) return;

    // Check if a deferred file load is waiting for BASIC to reach READY
    check_deferred_load();

    // Per-frame keyboard injection (one key per frame, matrix level)
    tick_key_injection();

    // Run until the video chip drives FrameEnd into the output.
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }

    video_port_->swap_frame();

    // Tick all attached peripheral devices (datasette, drive, etc.)
    tick_peripherals();
}

// ============================================================================
// CommodoreSystem Loading Hooks -- VIC-20 specific
// ============================================================================

static uint8_t vic20_mem_read_for_load(void* ctx, uint16_t addr) {
    auto* m = static_cast<VIC20System::MemLoadCtx*>(ctx);
    if (addr < 0x8000) return m->ram_base[addr];
    if (addr >= 0xA000 && addr < 0xC000) return m->cart_base[addr - 0xA000];
    return 0xFF;
}

static void vic20_mem_write_byte_cb(void* ctx, uint16_t addr, uint8_t val) {
    auto* m = static_cast<VIC20System::MemLoadCtx*>(ctx);
    if (addr < 0x8000) m->ram_base[addr] = val;
    else if (addr >= 0xA000 && addr < 0xC000) m->cart_base[addr - 0xA000] = val;
}

bool VIC20System::is_basic_ready() const {
    const uint8_t* ram = board_.ram0.data();

    // BASIC warm-start vector at $0302/$0303 = $C474
    if (ram[0x0302] != vic20_constants::BASIC_WARMSTART_LO ||
        ram[0x0303] != vic20_constants::BASIC_WARMSTART_HI)
        return false;

    // Keyboard buffer must be empty (no in-flight characters)
    if (ram[vic20_constants::KBD_BUFFER_COUNT] != 0)
        return false;

    // First boot: wait until BASIC's NEW has run (VARTAB $2D != 0)
    if (!boot_completed_ && ram[0x002D] == 0)
        return false;

    return true;
}

commodore_load_context_t VIC20System::build_load_context() {
    commodore_load_context_t ctx = {};
    ctx.system_name     = "VIC20";
    ctx.write_byte      = vic20_mem_write_byte_cb;
    ctx.write_block     = nullptr;  // VIC-20 uses banked memory, no memcpy
    ctx.mem_read        = vic20_mem_read_for_load;
    ctx.mem_ctx         = &load_mem_ctx_;
    ctx.basic_params    = &COMMODORE_BASIC_VIC20;
    ctx.basic_start_addrs[0] = vic20_constants::BASIC_START_3K;
    ctx.basic_start_addrs[1] = vic20_constants::BASIC_START_UNEXPANDED;
    ctx.basic_start_addrs[2] = vic20_constants::BASIC_START_8K;
    ctx.default_raw_addr = 0xA000;
    ctx.set_pc          = nullptr;  // VIC-20 uses keyboard buffer injection
    ctx.try_sys_from_filename = true;
    ctx.inject_keys     = [](void* sys_ctx, const char* str) {
        static_cast<CommodoreSystem*>(sys_ctx)->inject_keys(str);
    };
    ctx.keys_ctx        = this;
    return ctx;
}

// ============================================================================
// CRT Cartridge Loading
// ============================================================================

bool VIC20System::on_file_parsed(format_load_result_t& result,
                                 const char* filepath) {
    (void)filepath;

    // For CRT files: set program title from the cartridge name
    if (result.format && strcmp(result.format->name, "CRT") == 0 &&
        result.type == FORMAT_LOAD_METADATA &&
        result.metadata_size >= sizeof(commodore_crt_header_t)) {
        const auto* hdr = reinterpret_cast<const commodore_crt_header_t*>(result.metadata);
        if (hdr->name[0]) {
            program_title_ = hdr->name;
        }
    }

    return true;
}

/// Callback context for CRT CHIP packet loading into VIC-20 chip buffers.
struct vic20_crt_load_ctx {
    uint8_t* ram_base;   // board_.ram0.data() — contiguous for $0000-$7FFF
    uint8_t* cart_base;  // board_.cart.data() — for $A000-$BFFF
    int chips_loaded;
};

/// CHIP packet callback — loads ROM data into the RAM buffer.
static bool vic20_crt_chip_loader(const commodore_crt_chip_t* chip,
                                  const uint8_t* rom_data,
                                  void* user_data) {
    auto* ctx = static_cast<vic20_crt_load_ctx*>(user_data);

    log_info("VIC20: CRT CHIP bank=%u type=%u addr=$%04X size=%u\n",
           chip->bank_number, chip->chip_type,
           chip->load_address, chip->rom_size);

    // Validate address range
    if (chip->load_address + chip->rom_size > 65536) {
        log_info("VIC20: CHIP bank %u address out of range\n", chip->bank_number);
        return false;
    }

    // Route ROM data to the correct chip buffer:
    // $A000-$BFFF → cart chip, $0000-$7FFF → contiguous RAM from ram0
    uint8_t* dest;
    if (chip->load_address >= 0xA000 && chip->load_address < 0xC000)
        dest = ctx->cart_base + (chip->load_address - 0xA000);
    else if (chip->load_address < 0x8000)
        dest = ctx->ram_base + chip->load_address;
    else {
        log_info("VIC20: CHIP bank %u at unexpected address $%04X\n",
               chip->bank_number, chip->load_address);
        return false;
    }

    memcpy(dest, rom_data, chip->rom_size);
    log_info("VIC20: Loaded %uKB ROM at $%04X\n", chip->rom_size / 1024, chip->load_address);

    ctx->chips_loaded++;
    return true;  // Continue iterating
}

bool VIC20System::pre_apply_pending_load() {
    if (!pending_load_.active) return false;

    // Only handle CRT files
    const auto& result = pending_load_.result;
    if (!result.format || strcmp(result.format->name, "CRT") != 0)
        return false;

    if (result.metadata_size < sizeof(commodore_crt_header_t)) return false;

    const auto* hdr = reinterpret_cast<const commodore_crt_header_t*>(result.metadata);

    // Verify this is a VIC-20 CRT
    if (commodore_crt_machine(hdr->signature) != CRT_MACHINE_VIC20) {
        log_info("VIC20: CRT file is not a VIC-20 cartridge (signature: %.16s)\n",
               hdr->signature);
        return false;
    }

    log_info("VIC20: Loading CRT cartridge \"%s\" (hw_type=%u)\n",
           hdr->name, hdr->hardware_type);

    // Re-read the raw file to iterate CHIP packets
    // (the format handler only stores the header as metadata)
    size_t file_size = 0;
    uint8_t* file_data = format_read_entire_file(
        pending_load_.filepath.c_str(), &file_size);
    if (!file_data) {
        log_info("VIC20: Failed to re-read CRT file: %s\n",
               pending_load_.filepath.c_str());
        return false;
    }

    // Enable cartridge in the memory system
    cartridge_present_ = true;
    setup_cartridge_pages(true);

    // Iterate CHIP packets and load ROM data into chip buffers
    vic20_crt_load_ctx load_ctx = { board_.ram0.data(), board_.cart.data(), 0 };
    int chip_count = commodore_crt_iterate_chips(
        file_data, file_size, hdr,
        vic20_crt_chip_loader, &load_ctx);

    free(file_data);

    if (chip_count <= 0) {
        log_info("VIC20: No valid CHIP packets found in CRT file\n");
        // Revert cartridge state
        cartridge_present_ = false;
        setup_cartridge_pages(false);
        return false;
    }

    log_info("VIC20: Loaded %d CHIP packet(s) — resetting CPU for cartridge boot\n",
           chip_count);

    // Reset the system so the CPU picks up the new RESET vector
    // from the cartridge ROM (typically at $A000 with autostart header)
    reset();

    // Mark load as handled — skip the standard BASIC injection path
    boot_completed_ = true;
    return true;
}

// ============================================================================
// Display
// ============================================================================


// ============================================================================
// Input
// ============================================================================

// ============================================================================
// GUI Integration
// ============================================================================

void VIC20System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    (void)0;  // System-specific items go here
#endif
}

// ============================================================================
// Chip Registration — populate registered_chips_ for Hardware menu + debug
// ============================================================================

// ── Expansion map ────────────────────────────────────────────────────────
// Called at init and when expansion_flags_ changes.
// Expansion blocks use condition tags on their manifest slots — apply()
// maps only those whose condition bit is set in expansion_flags_.
// The $9800-$9FFF I/O expansion range is always unmapped (floating bus).
void VIC20System::setup_expansion_map() {
    // Condition callback: condition tag is the expansion flag bit;
    // context points to the current expansion_flags_ byte.
    auto exp_condition = [](uint16_t cond, const void* ctx) -> bool {
        return (*static_cast<const uint8_t*>(ctx) & cond) != 0;
    };
    board_.apply(mem_bus_, 0, exp_condition, &expansion_flags_);

    // $9800-$9FFF (pages 38-39): expansion I/O, always unmapped (floating bus)
    mem_bus_.map_no_chip_selected(0, 38, 2);

    // Cartridge ROM at $A000-$BFFF: handled via setup_cartridge_pages
    setup_cartridge_pages(cartridge_present_);

    log_info("VIC20: Expansion map configured (flags=$%02X, cart=%s)\n",
           expansion_flags_, cartridge_present_ ? "yes" : "no");
}

// ── Cartridge page protection ────────────────────────────────────────────
// When cartridge present: reads come from RAM buffer (where cart data was loaded),
// writes are blocked (write pages → no chip selected).
// When absent: $A000-$BFFF is unmapped (no chip selected for both read and write).
void VIC20System::setup_cartridge_pages(bool present) {
    using CId = typename Bus::ChipId;
    // 1 KB pages (matches PgBits=10 in ManifestBusSpec)
    constexpr size_t kPgBits = 10;
    constexpr size_t kCartBase = kVIC20Manifest.base_id(kVIC20_CartSlot, kPgBits);
    // $A000 >> 10 = page 40, $BFFF = 8 pages
    constexpr size_t kCartPage = 0xA000 >> kPgBits;   // 40
    constexpr size_t kCartPages = 0x2000 >> kPgBits;  // 8

    if (present) {
        // Reads from cart chip buffer (cartridge data loaded there), writes blocked
        mem_bus_.fill_read_pages(0, kCartPage, kCartPages, CId(kCartBase));
        mem_bus_.map_write_no_chip_selected(0, kCartPage, kCartPages);
    } else {
        // Unmapped: floating bus on read, writes ignored
        mem_bus_.map_no_chip_selected(0, kCartPage, kCartPages);
    }
}

void VIC20System::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    ImGui::Text("VIC-20 Configuration");
    ImGui::Separator();
    
    // Memory configuration
    ImGui::Text("Memory Expansion:");
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
// State
// ============================================================================

uint32_t VIC20System::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    // AudioPort path: VIC drives audio_port_ per cycle via IIR-filtered float stream.
    // AudioPort decimates to host rate and writes to its lock-free ring.
    if (audio_port_) {
        return static_cast<uint32_t>(audio_port_->read_samples(buffer, static_cast<int>(max_samples)));
    }

    // Legacy path: read uint8_t from VIC internal ring, convert to float
    if (!vic_) return 0;

    uint32_t avail = vic_->audio_available();
    uint32_t to_read = avail < max_samples ? avail : max_samples;
    if (to_read == 0) return 0;

    // Stack-local scratch to avoid heap allocation on the audio thread
    uint8_t tmp[512];
    uint32_t written = 0;
    while (written < to_read) {
        uint32_t chunk = to_read - written;
        if (chunk > sizeof(tmp)) chunk = sizeof(tmp);
        uint32_t n = vic_->audio_read(tmp, chunk);
        if (n == 0) break;
        for (uint32_t i = 0; i < n; i++) {
            // 128 = silence  →  0.0f ;  0 = -1.0f ;  255 = ~+1.0f
            buffer[written + i] = (tmp[i] - 128) / 128.0f;
        }
        written += n;
    }
    return written;
}

void VIC20System::set_audio_sample_rate(int sample_rate_hz) {
    if (sample_rate_hz <= 0) return;
    uint32_t rate = static_cast<uint32_t>(sample_rate_hz);

    // Reconfigure AudioPort decimation for the negotiated host sample rate
    if (audio_port_ && vic_) {
        audio_port_->configure(vic_->clock_frequency, rate);
    }

    // Also update legacy path (VIC internal ring buffer downsample ratio)
    if (vic_) {
        vic_->audio_reset(vic_->clock_frequency, rate);
    }
}

// ============================================================================
// VIA2 Port Read Callbacks - Keyboard Matrix Scanning
// ============================================================================
// VIC-20 keyboard wiring:
//   VIA2 Port B = column select (output, active LOW)
//   VIA2 Port A = row read (input, LOW = key pressed)
//
// When software writes to Port B to select columns, then reads Port A to get
// which rows have pressed keys in those columns. Reverse scanning also works:
// write Port A to select rows, read Port B to get columns.

uint8_t VIC20System::vic20_via2_port_a_read(void* context, uint8_t port_a_output) {
    VIC20System* sys = static_cast<VIC20System*>(context);
    if (!sys || !sys->keyboard_) return 0xFF;

    // Port A reads rows based on which columns are selected via Port B
    // Get Port B output (column select) - only bits with DDR=1 are driven
    uint8_t port_b_output = sys->board_.via2.port_b.output();
    uint8_t column_select = ~port_b_output;  // Active-LOW: 0 = selected

    uint8_t row_state = 0xFF;  // Default: all rows open (no keys pressed)
    for (int col = 0; col < 8; col++) {
        if (column_select & (1 << col)) {
            // This column is selected - AND in the row contacts
            row_state &= static_cast<uint8_t>(sys->keyboard_->row_open_contacts[col]);
        }
    }
    return row_state;
}

uint8_t VIC20System::vic20_via2_port_b_read(void* context, uint8_t port_b_output) {
    VIC20System* sys = static_cast<VIC20System*>(context);
    if (!sys || !sys->keyboard_) return 0xFF;

    // Port B reads columns based on which rows are selected via Port A
    // (Reverse scanning direction)
    uint8_t port_a_output = sys->board_.via2.port_a.output();
    uint8_t row_select = ~port_a_output;  // Active-LOW: 0 = selected

    uint8_t col_state = 0xFF;  // Default: all columns open (no keys pressed)
    for (int row = 0; row < 8; row++) {
        if (row_select & (1 << row)) {
            // This row is selected - AND in the column contacts
            // Note: col_open_contacts is uint16_t to support >8 row matrices,
            // but for the VIC-20's 8×8 matrix only the lower 8 bits are meaningful.
            col_state &= (uint8_t)sys->keyboard_->col_open_contacts[row];
        }
    }
    return col_state;
}

// ============================================================================
// Private Helper Methods - VIC Memory Callbacks
// ============================================================================

// VIC chip memory read — 14-bit address space (16 KB window)
// VA13=0 ($0000-$1FFF): Character ROM (4KB mirrored)
// VA13=1 ($2000-$3FFF): CPU RAM $0000-$1FFF
uint8_t VIC20System::vic_mem_read(void* user_data, uint16_t addr) {
    VIC20System* sys = static_cast<VIC20System*>(user_data);
    if (!sys) return 0xFF;

    if (addr < 0x2000) {
        // Character ROM — 4KB at $8000 in ROM chip, mirrored to 8KB via 0x0FFF mask
        return sys->board_.charrom.data()[addr & 0x0FFF];
    } else {
        // RAM — VIC sees CPU $0000-$1FFF
        // ram0/blk0/ram1 are contiguous in Board flat memory, so
        // ram0.data() serves as base pointer for the whole $0000-$1FFF range.
        return sys->board_.ram0.data()[addr & 0x1FFF];
    }
}

// Color RAM read — 10-bit address (1 KB), 4-bit wide (MOS 2114)
uint8_t VIC20System::vic_color_read(void* user_data, uint16_t addr) {
    VIC20System* sys = static_cast<VIC20System*>(user_data);
    if (!sys) return 0x0F;

    return sys->board_.colorram.memory[addr & 0x03FF] & 0x0F;
}

// ============================================================================
// ROM Loading
// ============================================================================

bool VIC20System::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("vic20", rom_root, sizeof(rom_root))) {
        log_info("VIC20: ROM root directory not found\n");
        return false;
    }
    return board_.load_roms(rom_root, "VIC20");
}

// ============================================================================
// Keyboard mapper factory
// ============================================================================

static KeyboardMapper* create_vic20_keyboard_mapper(commodore_keyboard_t* keyboard) {
    KeyboardMapper* mapper = new KeyboardMapper();
    mapper->set_guest_keyboard(keyboard);
    mapper->build_character_map_from_matrix(&vic20_keyboard_config);

    mapper->register_default_synthetic_mappings();

    // Commodore-specific character mappings (£, ↑, ←, π) are now handled
    // automatically by the PETSCII decode tables + petscii_to_host_char().

    auto& sdl_map = EmuKeySDLMap::instance();
    sdl_map.clear_system_mappings();
    sdl_map.register_candidates(EMUKEY_CBM_RESTORE, {SDL_SCANCODE_SYSREQ, SDL_SCANCODE_GRAVE});
    sdl_map.register_candidates(EMUKEY_CBM_POUND,   {SDL_SCANCODE_NONUSHASH});
    // Both host ALTs → CBM key (more accessible than Super/LGUI on Linux)
    sdl_map.register_candidates(EMUKEY_CBM_COMMODORE, {SDL_SCANCODE_LALT, SDL_SCANCODE_RALT}, true);

    return mapper;
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(vic20_descriptor, []() {
    return std::make_unique<VIC20System>();
})
