#include "vic20_system.h"
#include "vic20_memory.h"
#include "vic20_chips.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/emu_key_sdl_map.h"
#include "vic20_keyboard_matrix.h" // VIC-20 keyboard matrix data
#include <cstring>
#include <cstdio>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// Include chip headers
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/video/vic/mos6560.h"
#include "../../chip/video/vic/mos6561.h"
#include "../../chip/video/vic/vic_common.h"  // For VIC_COLOR_* constants
#include "../../chip/io/mos6522.h"

// Include bus interface
#include "../../core/bus_cycle_interface.h"

// Include ROM loader
#include "../../core/storage/rom_loader.h"
#include "../../core/config/path_discovery.h"

// Shared Commodore file format loader (PRG, D64, T64, TAP, CRT, BIN)
#include "../../core/storage/commodore_file_loader.h"

// ============================================================================
// Hardware Traits Definition
// ============================================================================

static HardwareTraits create_vic20_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - VIC-20 uses MOS6560/6561 (VIC)
    // Full VIC output including borders: 63 cycles × 4 pixels = 252 pixels wide
    // Visible raster lines including borders: ~284 lines (PAL)
    traits.display.native_width = 252;       // Full VIC horizontal output
    traits.display.native_height = 284;      // Full VIC vertical output
    traits.display.visible_width = 252;
    traits.display.visible_height = 284;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 16;        // 16 colors
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // VIC-20 PAL palette (16 colors) - Hardware accurate colors
    // Get from VIC chip (ABGR format) and convert to RGB for hardware traits
    // This ensures palette consistency between VIC chip rendering and system traits
    uint32_t* vic_palette_abgr = vic_get_default_palette();
    
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
    traits.audio.sample_rate_hz = 22050;
    traits.audio.channels = 1;
    traits.audio.chip_name = "VIC 6560/6561";
    
    // Timing - PAL version (NTSC differs)
    traits.timing.cpu_frequency_hz = 1108405;   // ~1.1 MHz (PAL)
    traits.timing.video_frequency_hz = 1108405; // Same as CPU
    traits.timing.audio_sample_rate_hz = 22050;
    traits.timing.target_fps = 50;              // PAL
    traits.timing.cycles_per_frame = 22168;     // 1108405 / 50
    traits.timing.region = VideoRegion::PAL;
    
    // Memory options
    traits.memory_options.push_back({
        "Unexpanded (5KB RAM)",
        5120,   // 5KB RAM
        0,
        true
    });
    traits.memory_options.push_back({
        "3KB Expansion (8KB total)",
        8192,   // 5KB + 3KB
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
        37888,  // 5KB + 32KB
        0,
        false
    });
    
    // Region options
    traits.region_options.push_back({
        "PAL",
        VideoRegion::PAL,
        traits.timing,
        true
    });
    
    SystemTiming ntsc_timing = traits.timing;
    ntsc_timing.cpu_frequency_hz = 1022727;     // ~1.0 MHz (NTSC)
    ntsc_timing.video_frequency_hz = 1022727;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 17045;       // 1022727 / 60
    ntsc_timing.region = VideoRegion::NTSC;
    
    traits.region_options.push_back({
        "NTSC",
        VideoRegion::NTSC,
        ntsc_timing,
        false
    });
    
    return traits;
}

/** Check if load address is a VIC-20 address */
static bool is_vic20_load_address(uint16_t addr) {
    return addr == 0x1001 || addr == 0x0401 || addr == 0x1201 ||
           addr == 0x2000 || addr == 0x4000 || addr == 0x6000 || addr == 0xA000;
}

// File detection callback
static float vic20_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        // PRG files — check load address
        if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
            if (size >= 2) {
                uint16_t load_addr = data[0] | (data[1] << 8);
                if (load_addr == 0x1001) {
                    return 0.85f;  // High confidence for VIC-20 PRG
                }
                // Other common VIC-20 addresses (expansion RAM, cartridge areas)
                if (load_addr == 0x0401 || load_addr == 0x1201 || load_addr == 0x2000 ||
                    load_addr == 0x4000 || load_addr == 0x6000 || load_addr == 0xA000) {
                    return 0.7f;
                }
                // Generic PRG file - moderate confidence
                return 0.5f;
            }
        }
        // LNX files — Lynx archive; parse to inspect contained files' load addresses
        if (strcmp(ext, ".lnx") == 0 || strcmp(ext, ".LNX") == 0) {
            commodore_lynx_t lynx;
            if (commodore_lynx_open(filepath, &lynx)) {
                commodore_lynx_directory_t dir;
                if (commodore_lynx_read_directory(&lynx, &dir)) {
                    // Check first PRG entry's load address
                    for (unsigned i = 0; i < dir.file_count; i++) {
                        if (dir.entries[i].file_type == 'P' && dir.entries[i].data_length >= 2) {
                            size_t off = dir.entries[i].data_offset;
                            if (off + 1 < lynx.data_size) {
                                uint16_t addr = lynx.data[off] | ((uint16_t)lynx.data[off+1] << 8);
                                commodore_lynx_close(&lynx);
                                if (is_vic20_load_address(addr)) return 0.90f;
                                return 0.4f;
                            }
                        }
                    }
                }
                commodore_lynx_close(&lynx);
            }
            return 0.5f;  // Could not inspect
        }
        if (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0) {
            // Check TAP header to see if this is specifically a VIC-20 tape
            int platform = commodore_tap_identify_platform(filepath);
            if (platform == 1) return 0.95f;  // VIC-20 TAP
            if (platform == 0) return 0.3f;   // C64 TAP (low for VIC-20)
            return 0.5f;  // Unknown or error
        }
        if (strcmp(ext, ".d64") == 0 || strcmp(ext, ".D64") == 0) {
            // Inspect first PRG's load address to distinguish VIC-20 from C64 disks
            commodore_d64_t d64;
            if (commodore_d64_open(filepath, &d64)) {
                commodore_prg_t prg = {};
                if (commodore_d64_extract_first_prg(&d64, &prg)) {
                    float score = is_vic20_load_address(prg.load_addr) ? 0.90f : 0.4f;
                    commodore_prg_free(&prg);
                    commodore_d64_close(&d64);
                    return score;
                }
                commodore_d64_close(&d64);
            }
            return 0.5f;  // Could not inspect — moderate confidence
        }
        if (strcmp(ext, ".t64") == 0 || strcmp(ext, ".T64") == 0) {
            return 0.5f;  // T64 tape archives (usually C64 but can contain VIC-20)
        }
    }
    return 0.0f;
}

static const char* vic20_extensions[] = {".prg", ".tap", ".d64", ".t64", ".lnx", nullptr};

static SystemDescriptor vic20_descriptor = {
    "Commodore VIC-20",
    "VIC20",
    "Commodore VIC-20 (1980) - 5KB RAM, 22-column display",
    vic20_extensions,
    create_vic20_hardware_traits(),
    vic20_can_load_file
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
VIC20System::VIC20System()
    : EmulatedSystem()
    , memory_(nullptr)
    , cpu_(nullptr)
    , vic_(nullptr)
    , via1_(nullptr)
    , via2_(nullptr)
    , keyboard_(nullptr)
    , cycles_per_frame_(22168)
    , expansion_flags_(VIC20_EXP_NONE)
    , autostart_delay_frames_(0)
{
    hardware_traits_ = create_vic20_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    
    // Initialize bus state with pull-up resistors (all control lines HIGH = inactive)
    // VIC-20 uses same pull-up model as C64: IRQ, NMI, RES, BA, RDY, RW all pulled HIGH
    bus_.default_state = BUS_STATE(0, 0xFF, BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY | BUS_MASK_RW) | 
                         BUS_BIT(BUS_RES_BIT) | BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_NMI_BIT);
    bus_.state = bus_.default_state;
}

VIC20System::~VIC20System() {
    // Destroy CPU
    if (cpu_) {
        mos6502_destroy(cpu_);
        cpu_ = nullptr;
    }
    
    // Destroy VIC chip
    if (vic_) {
        mos6560_destroy(vic_);  // Works for both 6560 and 6561
        vic_ = nullptr;
    }
    
    // Destroy VIA chips
    if (via1_) {
        mos6522_destroy(via1_);
        via1_ = nullptr;
    }
    
    if (via2_) {
        mos6522_destroy(via2_);
        via2_ = nullptr;
    }
    
    // Destroy keyboard
    if (keyboard_) {
        commodore_keyboard_destroy(keyboard_);
        keyboard_ = nullptr;
    }
    
    // Destroy memory system
    if (memory_) {
        vic20_memory_destroy(memory_);
        memory_ = nullptr;
    }
    
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

bool VIC20System::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool VIC20System::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        const RegionOption& region = hardware_traits_.region_options[config_.region_option_index];
        cycles_per_frame_ = region.timing.cycles_per_frame;
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
        
        // Update memory system if already created
        if (memory_) {
            vic20_memory_set_expansion(memory_, expansion_flags_);
        }
        
        printf("VIC20: Expansion configuration: $%02X\n", expansion_flags_);
    }
    
    return true;
}

// ============================================================================
// Auto-detect optimal configuration from file contents
// ============================================================================
SystemConfiguration VIC20System::detect_optimal_configuration(
    const char* filepath, const uint8_t* data, size_t size) {

    // Start from the base-class defaults
    SystemConfiguration config = EmulatedSystem::detect_optimal_configuration(filepath, data, size);

    if (!filepath) return config;

    // Determine the PRG load address and data size.
    // For raw PRG files we can read the two-byte header directly.
    // For container formats (D64, T64) we use the commodore file loader
    // to extract the first PRG and inspect its load address.
    uint16_t load_addr = 0;
    uint32_t end_addr  = 0;
    bool     have_prg  = false;

    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
#ifdef _MSC_VER
    bool is_prg = ext && (_stricmp(ext, ".prg") == 0);
    bool is_lnx = ext && (_stricmp(ext, ".lnx") == 0);
#else
    bool is_prg = ext && (strcasecmp(ext, ".prg") == 0);
    bool is_lnx = ext && (strcasecmp(ext, ".lnx") == 0);
#endif

    // LNX archives need special handling: inspect ALL contained files
    // to determine the maximum memory expansion needed.
    if (is_lnx) {
        commodore_load_result_t result = {};
        if (commodore_load_file(filepath, &result) && result.type == COMMODORE_LOAD_LNX) {
            int mem_index = 0;
            for (int f = 0; f < result.lynx_file_count; f++) {
                const commodore_prg_t* prg = &result.lynx_files[f];
                uint16_t la = prg->load_addr;
                uint32_t ea = (uint32_t)la + (uint32_t)prg->data_size;

                // Determine minimum expansion for this file
                if (la >= 0x0400 && la < 0x1000) { if (mem_index < 1) mem_index = 1; }
                if (la >= 0x2000 && la < 0x4000) { if (mem_index < 3) mem_index = 3; }
                if ((la >= 0x4000 && la < 0x6000) || (ea > 0x4000 && ea <= 0x6000))
                    { if (mem_index < 2) mem_index = 2; }
                if ((la >= 0x6000 && la < 0x8000) || (ea > 0x6000 && ea <= 0x8000))
                    { if (mem_index < 4) mem_index = 4; }
                if (la == 0x0401) { if (mem_index < 1) mem_index = 1; }
                if (la == 0x1201) { if (mem_index < 2) mem_index = 2; }
                // If any file writes above $6000, need 24KB+
                if (ea > 0x6000 && la < 0x6000) { if (mem_index < 4) mem_index = 4; }
            }
            // For safety, if multiple files span wide address ranges, use full expansion
            if (result.lynx_file_count > 3 && mem_index >= 2) {
                mem_index = 5;  // Full 32KB expansion
            }
            config.memory_option_index = mem_index;
            printf("VIC20: LNX auto-detected memory config: %s (%d files)\n",
                   hardware_traits_.memory_options[mem_index].name, result.lynx_file_count);
            commodore_load_result_free(&result);
            return config;
        }
        commodore_load_result_free(&result);
        return config;
    }

    if (is_prg && data && size >= 2) {
        // Fast path: raw PRG — load address is first two bytes
        load_addr = data[0] | (data[1] << 8);
        end_addr  = (uint32_t)load_addr + (uint32_t)(size - 2);
        have_prg  = true;
    } else {
        // Container formats (D64, T64, etc.) — extract first PRG via loader
        commodore_load_result_t result = {};
        if (commodore_load_file(filepath, &result)) {
            if ((result.type == COMMODORE_LOAD_D64 ||
                 result.type == COMMODORE_LOAD_T64 ||
                 result.type == COMMODORE_LOAD_PRG) &&
                result.prg.data_size > 0) {
                load_addr = result.prg.load_addr;
                end_addr  = (uint32_t)load_addr + (uint32_t)result.prg.data_size;
                have_prg  = true;
            }
            commodore_load_result_free(&result);
        }
    }

    if (!have_prg) return config;

    // ---- Determine minimum memory configuration from load address ----
    // Memory option indices (from create_vic20_hardware_traits):
    //   0 = Unexpanded 5KB    ($1000-$1FFF user RAM)
    //   1 = +3KB              ($0400-$0FFF added)
    //   2 = +8KB              ($4000-$5FFF added, 13KB total)
    //   3 = +16KB             ($2000-$3FFF + $4000-$5FFF, 21KB total)
    //   4 = +24KB             (above + $6000-$7FFF, 29KB total)
    //   5 = Full 32KB         (all blocks, 37KB total)
    int mem_index = 0;

    if (load_addr == 0x0401) {
        // 3KB-expanded BASIC start address
        mem_index = 1;
        if (end_addr > 0x1FFF) mem_index = 2;
        if (end_addr > 0x5FFF) mem_index = 3;
        if (end_addr > 0x7FFF) mem_index = 5;
    } else if (load_addr == 0x1201) {
        // 8KB+ expanded BASIC start address
        mem_index = 2;
        if (end_addr > 0x5FFF) mem_index = 3;
        if (end_addr > 0x7FFF) mem_index = 5;
    } else if (load_addr == 0x1001) {
        // Standard unexpanded BASIC
        mem_index = 0;
        // If the program overflows the 4KB user area, enable expansion
        if (end_addr > 0x1FFF) mem_index = 2;
        if (end_addr > 0x5FFF) mem_index = 3;
        if (end_addr > 0x7FFF) mem_index = 5;
    } else {
        // Machine-language program — check which expansion blocks are needed
        if (load_addr >= 0x0400 && load_addr < 0x1000) {
            // Block 0 ($0400-$0FFF) — needs at least 3KB expansion
            mem_index = 1;
        }
        if (load_addr >= 0x2000 && load_addr < 0x4000) {
            // Block 2 ($2000-$3FFF) — needs 16KB config (includes block 2)
            mem_index = 3;
        }
        if ((load_addr >= 0x4000 && load_addr < 0x6000) ||
            (end_addr > 0x4000 && end_addr <= 0x6000)) {
            // Block 3 ($4000-$5FFF) — needs at least 8KB config
            if (mem_index < 2) mem_index = 2;
        }
        if ((load_addr >= 0x6000 && load_addr < 0x8000) ||
            (end_addr > 0x6000 && end_addr <= 0x8000)) {
            // Block 5 ($6000-$7FFF) — needs 24KB config
            if (mem_index < 4) mem_index = 4;
        }
        // If data spans multiple blocks, pick the highest needed
        if (end_addr > 0x6000 && load_addr < 0x6000) {
            if (mem_index < 4) mem_index = 4;
        }
    }

    config.memory_option_index = mem_index;
    printf("VIC20: Auto-detected memory config: %s (load=$%04X end=$%04X)\n",
           hardware_traits_.memory_options[mem_index].name,
           load_addr, (uint16_t)(end_addr & 0xFFFF));

    return config;
}

// ============================================================================
// System Lifecycle
// ============================================================================
bool VIC20System::initialize() {
    printf("VIC20: Initializing system\n");
    
    // Create the memory banking system
    memory_ = vic20_memory_create(expansion_flags_, false);  // No cartridge by default
    if (!memory_) {
        printf("VIC20: Failed to create memory system\n");
        return false;
    }
    
    // Attach system to memory
    vic20_memory_attach_system(memory_, this);
    
    // Initialize Color RAM to cyan (color 3) for proper text visibility
    // This matches the VIC-20 boot screen: cyan text on blue background
    uint8_t* colorram = vic20_memory_get_colorram_ptr(memory_);
    if (colorram) {
        memset(colorram, VIC_COLOR_CYAN, 1024);
    }
    
    // Load ROMs into memory system
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("VIC20: Warning - ROMs not loaded, system may not function correctly\n");
    }
    
    // Create CPU (MOS6502) with memory callbacks
    cpu_ = mos6502_create();
    if (!cpu_) {
        printf("VIC20: Failed to create MOS6502 CPU\n");
        return false;
    }
    
    // Create enhanced descriptor with memory callbacks
    fam65xx_chip_descriptor_t* cpu_desc = mos6502_create_descriptor(
        cpu_read,
        cpu_write,
        this  // user_data points to this VIC20System instance
    );
    
    if (cpu_desc) {
        mos6502_init_enhanced(cpu_, cpu_desc);
        mos6502_destroy_descriptor(cpu_desc);
    }
    
    // Reset CPU to initialize state
    mos6502_reset(cpu_, 0);
    
    // Manually load the reset vector since automatic reset doesn't work with callback-only CPU
    // KERNAL is at $E000-$FFFF (8KB), reset vector is at $FFFC-$FFFD
    uint8_t* kernal_ptr = vic20_memory_get_rom_ptr(memory_, VIC20_BASE_KERNAL);
    if (kernal_ptr) {
        uint16_t reset_vector = kernal_ptr[0xFFFC - 0xE000] | (kernal_ptr[0xFFFD - 0xE000] << 8);
        mos6502_set_pc(cpu_, reset_vector);
        printf("VIC20: CPU reset complete - PC = $%04X\n", mos6502_get_pc(cpu_));
    }
    
    // Create VIC chip (MOS6560 PAL - default, TODO: support NTSC 6561)
    vic_ = (mos6560_t*)mos6560_create(&mos6560_descriptor);
    if (!vic_) {
        printf("VIC20: Failed to create VIC chip\n");
        return false;
    }
    
    // Store VIC chip pointer in memory system for I/O handling
    memory_->vic_chip = vic_;
    
    // Set up VIC memory callbacks for accessing video and character memory
    vic_set_memory_callbacks(&vic_->base,
        VIC20System::vic_mem_read,      // Memory read callback
        this,                            // User data (VIC20System instance)
        VIC20System::vic_color_read,    // Color RAM read callback
        this);                           // User data for color RAM
    
    // Create VIA chips (MOS6522)
    // VIC-20 hardware: VIA1 ($9110) → NMI line, VIA2 ($9120) → IRQ line
    // VIA2 Timer 1 is the system heartbeat (jiffy clock, keyboard scan, cursor blink)
    via1_ = (mos6522_t*)mos6522_create(&mos6522_descriptor);
    if (via1_) {
        via1_->interrupt_line = BUS_MASK_NMI;
        memory_->via1_chip = via1_;
    } else {
        printf("VIC20: Failed to create VIA1\n");
    }
    
    via2_ = (mos6522_t*)mos6522_create(&mos6522_descriptor);
    if (via2_) {
        via2_->interrupt_line = BUS_MASK_IRQ;
        memory_->via2_chip = via2_;
    } else {
        printf("VIC20: Warning: VIA2 not created (optional)\n");
    }
    
    // Create keyboard matrix and connect to VIA2
    // VIC-20 keyboard: VIA2 Port B selects columns, VIA2 Port A reads rows
    keyboard_ = commodore_keyboard_create(&vic20_keyboard_config);
    if (keyboard_) {
        // Create the layered keyboard mapper for character-based input
        keyboard_mapper_.reset(create_vic20_keyboard_mapper(keyboard_));
        
        if (via2_) {
            // Register port read callbacks for keyboard matrix scanning
            // Port A reads rows, Port B reads columns (reverse scanning)
            mos6522_set_port_a_read_callback(via2_, vic20_via2_port_a_read, this);
            mos6522_set_port_b_read_callback(via2_, vic20_via2_port_b_read, this);
            printf("VIC20: Keyboard connected to VIA2 via callbacks\n");
        } else {
            printf("VIC20: Warning: Could not connect keyboard to VIA2\n");
        }
    } else {
        printf("VIC20: Warning: Could not create keyboard\n");
    }
    
    // Initialize I/O handlers now that all chips are created
    vic20_memory_init_io_handlers(memory_);
    
    return true;
}

void VIC20System::shutdown() {
    printf("VIC20: Shutting down system\n");
}

void VIC20System::reset() {
    printf("VIC20: Resetting system\n");
    
    // Reset VIC chip (clears registers, video state, audio state)
    if (vic_) {
        mos6560_reset(vic_);
        // Re-establish memory callbacks (vic_system_reset clears them)
        vic_set_memory_callbacks(&vic_->base,
            VIC20System::vic_mem_read, this,
            VIC20System::vic_color_read, this);
        // Re-establish framebuffer pointer
        if (rgba_framebuffer_) {
            mos6560_set_framebuffer(vic_, rgba_framebuffer_, rgba_width_, rgba_height_);
        }
    }
    
    // Reset VIA chips (clears timers, interrupt flags, port registers)
    // interrupt_line is preserved by mos6522_reset — it's hardware wiring, not state
    if (via1_) {
        mos6522_reset(via1_);
    }
    if (via2_) {
        mos6522_reset(via2_);
    }
    
    // Clear RAM (zero page, stack, main RAM $0000-$7FFF) but preserve ROMs
    if (memory_ && memory_->buffer) {
        memset(memory_->buffer, 0, 0x8000);           // $0000-$7FFF: all RAM
        // Reinitialize Color RAM to default cyan
        memset(memory_->buffer + VIC20_BASE_COLOR_RAM, VIC_COLOR_CYAN, 1024);
    }
    
    // Clear the framebuffer to black
    if (rgba_framebuffer_ && rgba_width_ > 0 && rgba_height_ > 0) {
        memset(rgba_framebuffer_, 0, (size_t)rgba_width_ * rgba_height_ * sizeof(uint32_t));
    }
    
    // Reset CPU last (so it picks up clean bus state)
    if (cpu_) {
        mos6502_reset(cpu_, 0);
        
        // Reload reset vector
        uint8_t* kernal_ptr = vic20_memory_get_rom_ptr(memory_, VIC20_BASE_KERNAL);
        if (kernal_ptr) {
            uint16_t reset_vector = kernal_ptr[0xFFFC - 0xE000] | (kernal_ptr[0xFFFD - 0xE000] << 8);
            mos6502_set_pc(cpu_, reset_vector);
        }
    }
    
    // Reset bus state
    bus_.state = bus_.default_state;
    
    total_cycles_ = 0;
    autostart_delay_frames_ = 0;
    // Don't clear pending_filepath_ here — reset() is called by the GUI
    // *before* load_file(), so clearing would lose the deferred load.
}

// ============================================================================
// Execution
// ============================================================================

bus_state_t VIC20System::mem_tick(bus_state_t s) {
    // Use the new memory banking system
    return vic20_memory_cpu_tick(memory_, s);
}

void VIC20System::tick() {
    // Proper PHI1/PHI2 timing following C64 pattern
    // VIC-20 has simpler fixed memory mapping without PLA
    
    // Start with clean bus state (pull-up resistors)
    bus_state_t s = bus_.default_state;
    
    // Preserve address and data from previous cycle
    BUS_SET_ADDR(s, BUS_GET_ADDR(bus_.state));
    BUS_SET_DATA(s, BUS_GET_DATA(bus_.state));
    
    // =========================================================================
    // PHASE 1: VIC CHIP TICKING
    // VIC-20's VIC chip runs continuously, generating video and handling DMA
    // =========================================================================
    if (vic_) {
        s = mos6560_tick(vic_, s);
    }
    
    // =========================================================================
    // PHASE 2: VIA CHIPS TICKING (BEFORE CPU PHI2)
    // VIA chips handle I/O and timing, must tick before CPU to set interrupt lines
    // =========================================================================
    if (via1_) {
        s = mos6522_tick(via1_, s);
    }
    if (via2_) {
        s = mos6522_tick(via2_, s);
    }
    
    // =========================================================================
    // PHASE 3: CPU TICKING (PHI2 phase - sets up memory access)
    // CPU executes instruction and puts address/control on bus
    // =========================================================================
    if (cpu_) {
        s = mos6502_tick_phi2(cpu_, s);
    }
    
    // =========================================================================
    // PHASE 4: MEMORY SERVICE PHASE
    // Service memory access set up by CPU during PHI2
    // This is CRITICAL - memory access happens BETWEEN PHI2 and PHI1
    // so data is ready for CPU to complete the cycle
    // =========================================================================
    s = mem_tick(s);
    
    // =========================================================================
    // PHASE 5: CPU TICKING (PHI1 phase - completes cycle)
    // CPU prepares next instruction fetch
    // =========================================================================
    if (cpu_) {
        s = mos6502_tick_phi1(cpu_, s);
    }
    
    // Update bus state
    bus_.state = s;
    total_cycles_++;
}

void VIC20System::run_frame() {
    // Deferred autostart: load file into memory and inject RUN command
    // after the KERNAL boot sequence completes and BASIC is at READY.
    if (autostart_delay_frames_ > 0) {
        if (--autostart_delay_frames_ == 0 && !pending_filepath_.empty()) {
            if (load_file_into_memory(pending_filepath_.c_str())) {
                printf("VIC20: Deferred load complete\n");
            } else {
                printf("VIC20: Deferred load FAILED for: %s\n", pending_filepath_.c_str());
            }
            pending_filepath_.clear();
        }
    }

    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }
}

// ============================================================================
// File Loading
// ============================================================================

bool VIC20System::load_file(const char* filepath) {
    if (!memory_ || !cpu_) {
        printf("VIC20: System not initialized, initializing now...\n");
        if (!initialize()) {
            printf("VIC20: Failed to initialize system for file loading\n");
            return false;
        }
    }

    printf("VIC20: Scheduling deferred load: %s\n", filepath);

    // Store the filepath for deferred loading.  We cannot load into
    // system memory immediately because the KERNAL boot sequence
    // ($FD22) clears zero-page — wiping BASIC pointers ($2B-$32) and
    // keyboard buffer count ($C6) — and BASIC init reinitializes the
    // program area.  Instead, run_frame() calls load_file_into_memory()
    // after enough frames for boot to reach the READY prompt.
    pending_filepath_ = filepath;
    autostart_delay_frames_ = 120;  // ~2 seconds at 60fps
    return true;
}

bool VIC20System::load_file_into_memory(const char* filepath) {
    printf("VIC20: Loading file into memory: %s\n", filepath);

    commodore_load_result_t result = {};
    if (!commodore_load_file(filepath, &result)) {
        printf("VIC20: Failed to load file: %s\n", result.error_msg);
        commodore_load_result_free(&result);
        return false;
    }

    bool success = false;

    switch (result.type) {
        case COMMODORE_LOAD_PRG:
        case COMMODORE_LOAD_D64:
        case COMMODORE_LOAD_T64: {
            const commodore_prg_t* prg = &result.prg;

            printf("VIC20: Loading %s: $%04X-$%04X (%zu bytes)\n",
                   commodore_load_type_name(result.type),
                   prg->load_addr, prg->end_addr, prg->data_size);

            if (prg->data_size == 0 || (uint32_t)prg->load_addr + prg->data_size > 0x10000) {
                printf("VIC20: Invalid PRG address range: $%04X-$%04X\n",
                       prg->load_addr, prg->end_addr);
                break;
            }

            // Write program data directly into VIC-20 memory
            for (size_t i = 0; i < prg->data_size; i++) {
                vic20_memory_write_byte(memory_,
                                        (uint16_t)(prg->load_addr + i),
                                        prg->data[i]);
            }

            // Set BASIC pointers and inject RUN command for BASIC programs.
            // VIC-20 BASIC start addresses vary by memory expansion:
            //   $0401 = 3KB expansion
            //   $1001 = unexpanded (5KB)
            //   $1201 = 8KB+ expansion
            bool is_basic = (prg->load_addr == 0x0401 ||
                             prg->load_addr == 0x1001 ||
                             prg->load_addr == 0x1201);

            if (is_basic) {
                uint16_t end_addr = prg->end_addr;
                // TXTTAB ($2B/$2C) = start of BASIC text
                vic20_memory_write_byte(memory_, 0x2B, (uint8_t)(prg->load_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x2C, (uint8_t)(prg->load_addr >> 8));
                // VARTAB ($2D/$2E) = end of BASIC text (start of variables)
                vic20_memory_write_byte(memory_, 0x2D, (uint8_t)(end_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x2E, (uint8_t)(end_addr >> 8));
                // ARYTAB ($2F/$30) = start of arrays
                vic20_memory_write_byte(memory_, 0x2F, (uint8_t)(end_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x30, (uint8_t)(end_addr >> 8));
                // STREND ($31/$32) = end of arrays
                vic20_memory_write_byte(memory_, 0x31, (uint8_t)(end_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x32, (uint8_t)(end_addr >> 8));

                // Inject "RUN\r" into the KERNAL keyboard buffer
                const char* run_cmd = "RUN\r";
                int len = (int)strlen(run_cmd);
                for (int i = 0; i < len; i++) {
                    vic20_memory_write_byte(memory_, 0x0277 + i, (uint8_t)run_cmd[i]);
                }
                vic20_memory_write_byte(memory_, 0x00C6, (uint8_t)len);
                printf("VIC20: Set BASIC pointers and injected RUN command\n");
            } else {
                // Machine language program — try to extract SYS address
                // from the filename (e.g. "rl-test_SYS4352.prg" → SYS4352)
                const char* basename = filepath;
                const char* sep = strrchr(filepath, '/');
                if (sep) basename = sep + 1;

                int sys_addr = -1;
                for (const char* p = basename; *p; p++) {
                    if ((p[0] == 'S' || p[0] == 's') &&
                        (p[1] == 'Y' || p[1] == 'y') &&
                        (p[2] == 'S' || p[2] == 's') &&
                        p[3] >= '0' && p[3] <= '9') {
                        sys_addr = atoi(p + 3);
                        break;
                    }
                }

                if (sys_addr >= 0 && sys_addr <= 65535) {
                    char cmd[16];
                    int len = snprintf(cmd, sizeof(cmd), "SYS%d\r", sys_addr);
                    if (len > 0 && len <= 10) {  // VIC-20 keyboard buffer = 10 bytes
                        for (int i = 0; i < len; i++) {
                            vic20_memory_write_byte(memory_, 0x0277 + i, (uint8_t)cmd[i]);
                        }
                        vic20_memory_write_byte(memory_, 0x00C6, (uint8_t)len);
                        printf("VIC20: Injected auto-start: SYS%d\n", sys_addr);
                    }
                }
            }

            success = true;
            break;
        }

        case COMMODORE_LOAD_TAP: {
            printf("VIC20: TAP file detected (platform=%u, version=%u)\n",
                   result.tap_header.platform, result.tap_header.version);
            printf("VIC20: TAP tape emulation not yet implemented\n");
            success = false;
            break;
        }

        case COMMODORE_LOAD_CRT: {
            printf("VIC20: CRT cartridge: \"%s\" (type=%u)\n",
                   result.crt_header.name, result.crt_header.hardware_type);
            printf("VIC20: CRT cartridge loading not yet implemented\n");
            success = false;
            break;
        }

        case COMMODORE_LOAD_BIN: {
            const uint16_t default_addr = 0xA000;
            const commodore_prg_t* prg = &result.prg;
            
            printf("VIC20: Loading BIN at default $%04X (%zu bytes)\n",
                   default_addr, prg->data_size);

            if (default_addr + prg->data_size > 0x10000) {
                printf("VIC20: BIN too large for address space\n");
                break;
            }

            for (size_t i = 0; i < prg->data_size; i++) {
                vic20_memory_write_byte(memory_,
                                        (uint16_t)(default_addr + i),
                                        prg->data[i]);
            }
            success = true;
            break;
        }

        case COMMODORE_LOAD_LNX: {
            // Lynx archive — load ALL extracted PRG files into memory
            printf("VIC20: Loading LNX archive with %d files\n", result.lynx_file_count);

            bool any_basic = false;
            uint16_t basic_load_addr = 0;
            uint16_t basic_end_addr = 0;

            for (int f = 0; f < result.lynx_file_count; f++) {
                const commodore_prg_t* prg = &result.lynx_files[f];

                if (prg->data_size == 0 || (uint32_t)prg->load_addr + prg->data_size > 0x10000) {
                    printf("VIC20: LNX file %d: Invalid address range $%04X-$%04X, skipping\n",
                           f, prg->load_addr, prg->end_addr);
                    continue;
                }

                printf("VIC20: LNX file %d: $%04X-$%04X (%zu bytes)\n",
                       f, prg->load_addr, prg->end_addr, prg->data_size);

                for (size_t i = 0; i < prg->data_size; i++) {
                    vic20_memory_write_byte(memory_,
                                            (uint16_t)(prg->load_addr + i),
                                            prg->data[i]);
                }

                // Track if any file is a BASIC program (for auto-run)
                if (prg->load_addr == 0x0401 || prg->load_addr == 0x1001 ||
                    prg->load_addr == 0x1201) {
                    any_basic = true;
                    basic_load_addr = prg->load_addr;
                    basic_end_addr = prg->end_addr;
                }
            }

            if (any_basic) {
                // Set BASIC pointers for the BASIC program
                vic20_memory_write_byte(memory_, 0x2B, (uint8_t)(basic_load_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x2C, (uint8_t)(basic_load_addr >> 8));
                vic20_memory_write_byte(memory_, 0x2D, (uint8_t)(basic_end_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x2E, (uint8_t)(basic_end_addr >> 8));
                vic20_memory_write_byte(memory_, 0x2F, (uint8_t)(basic_end_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x30, (uint8_t)(basic_end_addr >> 8));
                vic20_memory_write_byte(memory_, 0x31, (uint8_t)(basic_end_addr & 0xFF));
                vic20_memory_write_byte(memory_, 0x32, (uint8_t)(basic_end_addr >> 8));

                const char* run_cmd = "RUN\r";
                int len = (int)strlen(run_cmd);
                for (int i = 0; i < len; i++) {
                    vic20_memory_write_byte(memory_, 0x0277 + i, (uint8_t)run_cmd[i]);
                }
                vic20_memory_write_byte(memory_, 0x00C6, (uint8_t)len);
                printf("VIC20: LNX: Set BASIC pointers ($%04X-$%04X) and injected RUN\n",
                       basic_load_addr, basic_end_addr);
            }

            success = result.lynx_file_count > 0;
            break;
        }

        default:
            printf("VIC20: Unsupported load result type: %d\n", result.type);
            break;
    }

    commodore_load_result_free(&result);
    return success;
}

// ============================================================================
// Display
// ============================================================================

uint32_t* VIC20System::get_framebuffer() {
    return rgba_framebuffer_;
}

void VIC20System::get_display_dimensions(int* width, int* height) const {
    *width = 252;   // Full VIC horizontal output (63 cycles × 4 pixels)
    *height = 284;  // Full VIC vertical output including borders
}

void VIC20System::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
    
    // Update VIC chip with new framebuffer (critical for display!)
    if (vic_ && buffer) {
        printf("VIC20: Setting framebuffer on VIC chip: %dx%d buffer=%p\n", width, height, (void*)buffer);
        mos6560_set_framebuffer(vic_, buffer, width, height);
    } else {
        printf("VIC20: Warning - cannot set framebuffer (vic_=%p buffer=%p)\n", (void*)vic_, (void*)buffer);
    }
}

// ============================================================================
// Input
// ============================================================================

void VIC20System::handle_keyboard_event(SDL_Keycode key, bool pressed) {
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
                commodore_keyboard_key_down(keyboard_, ek, false);
            } else {
                commodore_keyboard_key_up(keyboard_, ek, false);
            }
        }
    }
}

void VIC20System::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, scancode, mod, repeat);
        } else {
            keyboard_mapper_->process_key_up(key, scancode, mod);
        }
    } else if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

void VIC20System::handle_text_input(const char* text) {
    if (keyboard_mapper_) {
        keyboard_mapper_->process_text_input(text);
    }
}

void VIC20System::release_all_keys() {
    if (keyboard_mapper_) {
        keyboard_mapper_->release_all();
    }
}

// ============================================================================
// GUI Integration
// ============================================================================

void VIC20System::render_system_menu_items() {
#ifdef IMGUI_VERSION
    if (ImGui::MenuItem("Reset VIC-20")) {
        reset();
    }
#endif
}

void VIC20System::render_configuration_ui() {
#ifdef IMGUI_VERSION
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
    for (size_t i = 0; i < hardware_traits_.region_options.size(); i++) {
        bool selected = (config_.region_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.region_options[i].name, selected)) {
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

uint32_t VIC20System::get_target_fps() const {
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        return hardware_traits_.region_options[config_.region_option_index].timing.target_fps;
    }
    return 50;  // Default PAL
}

// ============================================================================
// Emulation Control
// ============================================================================

void VIC20System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

uint32_t VIC20System::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!vic_ || max_samples == 0 || !buffer) return 0;

    // Read unsigned-8-bit samples from VIC ring buffer and convert to float
    uint32_t avail = vic_audio_available(&vic_->base);
    uint32_t to_read = avail < max_samples ? avail : max_samples;
    if (to_read == 0) return 0;

    // Stack-local scratch to avoid heap allocation on the audio thread
    uint8_t tmp[512];
    uint32_t written = 0;
    while (written < to_read) {
        uint32_t chunk = to_read - written;
        if (chunk > sizeof(tmp)) chunk = sizeof(tmp);
        uint32_t n = vic_audio_read(&vic_->base, tmp, chunk);
        if (n == 0) break;
        for (uint32_t i = 0; i < n; i++) {
            // 128 = silence  →  0.0f ;  0 = -1.0f ;  255 = ~+1.0f
            buffer[written + i] = (tmp[i] - 128) / 128.0f;
        }
        written += n;
    }
    return written;
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
    if (!sys || !sys->keyboard_ || !sys->via2_) return 0xFF;

    // Port A reads rows based on which columns are selected via Port B
    // Get Port B output (column select) - only bits with DDR=1 are driven
    uint8_t port_b_output = sys->via2_->port_b_data & sys->via2_->port_b_ddr;
    uint8_t column_select = ~port_b_output;  // Active-LOW: 0 = selected

    uint8_t row_state = 0xFF;  // Default: all rows open (no keys pressed)
    for (int col = 0; col < 8; col++) {
        if (column_select & (1 << col)) {
            // This column is selected - AND in the row contacts
            row_state &= sys->keyboard_->row_open_contacts[col];
        }
    }
    return row_state;
}

uint8_t VIC20System::vic20_via2_port_b_read(void* context, uint8_t port_b_output) {
    VIC20System* sys = static_cast<VIC20System*>(context);
    if (!sys || !sys->keyboard_ || !sys->via2_) return 0xFF;

    // Port B reads columns based on which rows are selected via Port A
    // (Reverse scanning direction)
    uint8_t port_a_output = sys->via2_->port_a_data & sys->via2_->port_a_ddr;
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
// Private Helper Methods - CPU Memory Callbacks
// ============================================================================

uint8_t VIC20System::cpu_read(void* user_data, uint32_t addr, uint8_t bus_state_param) {
    VIC20System* sys = static_cast<VIC20System*>(user_data);
    (void)bus_state_param;
    
    if (!sys || !sys->memory_) return 0xFF;
    
    return vic20_memory_read_byte(sys->memory_, addr & 0xFFFF);
}

void VIC20System::cpu_write(void* user_data, uint32_t addr, uint8_t data) {
    VIC20System* sys = static_cast<VIC20System*>(user_data);
    
    if (!sys || !sys->memory_) return;
    
    vic20_memory_write_byte(sys->memory_, addr & 0xFFFF, data);
}

// ============================================================================
// Private Helper Methods - VIC Memory Callbacks
// ============================================================================

uint8_t VIC20System::vic_mem_read(void* user_data, uint16_t addr) {
    VIC20System* sys = static_cast<VIC20System*>(user_data);
    
    if (!sys || !sys->memory_) return 0xFF;
    
    return vic20_memory_vic_read(sys->memory_, addr);
}

uint8_t VIC20System::vic_color_read(void* user_data, uint16_t addr) {
    VIC20System* sys = static_cast<VIC20System*>(user_data);
    
    if (!sys || !sys->memory_) return 0x0F;
    
    return vic20_memory_color_read(sys->memory_, addr);
}

// ============================================================================
// ROM Loading
// ============================================================================

bool VIC20System::load_roms() {
    if (!memory_) {
        printf("VIC20: Cannot load ROMs - memory system not initialized\n");
        return false;
    }
    
    // Discover ROM root path for VIC-20 system
    char rom_root[1024];
    bool rom_root_found = system_config_discover_rom_root("vic20", rom_root, sizeof(rom_root));
    
    if (!rom_root_found) {
        printf("VIC20: ROM root directory not found\n");
        return false;
    }
    
    printf("VIC20: ROM root discovered: %s\n", rom_root);
    
    // Temporary buffers for ROM loading
    uint8_t char_buf[4096];
    uint8_t basic_buf[8192];
    uint8_t kernal_buf[8192];
    
    // Load Character ROM (4KB at $8000-$8FFF)
    const char* char_files[] = {
        "characters.901460-03.bin",
        "chargen.rom",
        "901460-03.bin",
        nullptr
    };
    
    bool char_ok = rom_loader_load_from_root(
        rom_root, char_files,
        sizeof(char_buf), char_buf, sizeof(char_buf)
    );
    
    if (char_ok) {
        vic20_memory_load_rom(memory_, VIC20_BASE_CHARROM, char_buf, sizeof(char_buf));
    } else {
        printf("VIC20: Failed to load Character ROM\n");
    }
    
    // Load BASIC ROM (8KB at $C000-$DFFF)
    const char* basic_files[] = {
        "basic.901486-01.bin",
        "basic.rom",
        "901486-01.bin",
        nullptr
    };
    
    bool basic_ok = rom_loader_load_from_root(
        rom_root, basic_files,
        sizeof(basic_buf), basic_buf, sizeof(basic_buf)
    );
    
    if (basic_ok) {
        vic20_memory_load_rom(memory_, VIC20_BASE_BASIC, basic_buf, sizeof(basic_buf));
    } else {
        printf("VIC20: Failed to load BASIC ROM\n");
    }
    
    // Load KERNAL ROM (8KB at $E000-$FFFF)
    const char* kernal_files[] = {
        "kernal.901486-07.bin",
        "kernal.rom",
        "901486-07.bin",
        nullptr
    };
    
    bool kernal_ok = rom_loader_load_from_root(
        rom_root, kernal_files,
        sizeof(kernal_buf), kernal_buf, sizeof(kernal_buf)
    );
    
    if (kernal_ok) {
        vic20_memory_load_rom(memory_, VIC20_BASE_KERNAL, kernal_buf, sizeof(kernal_buf));
    } else {
        printf("VIC20: Failed to load KERNAL ROM\n");
    }
    
    return (kernal_ok && basic_ok && char_ok);
}

void VIC20System::memory_init(const rom_config_t* rom_config) {
    // Reload ROMs if configuration provided
    if (rom_config) {
        reload_roms(rom_config);
    }
}

bool VIC20System::reload_roms(const rom_config_t* rom_config) {
    // Reload ROMs using provided configuration
    (void)rom_config;  // TODO: Use rom_config paths if provided
    return load_roms();
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(vic20_descriptor, []() {
    return std::make_unique<VIC20System>();
})
