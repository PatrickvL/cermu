#include "vic20_system.h"
#include "vic20_constants.h"
#include "vic20_memory.h"
#include "vic20_chips.h"
#include "../../core/cermu.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/emu_key_sdl_map.h"
#include "vic20_keyboard_matrix.h" // VIC-20 keyboard matrix data
#include <cstring>
#include <cstdio>

#ifdef CERMU_HAS_GUI
#include "imgui.h"
#endif

// Include chip headers — CPU uses fam65xx.hpp directly for inlining
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/video/vic/mos6560.h"
#include "../../chip/video/vic/mos6561.h"
#include "../../chip/video/vic/vic_common.h"  // For VIC_COLOR_* constants
#include "../../chip/io/mos6522.h"
#include "../../core/chip.h"
#include "../../chip/memory/memory_chip.h"

// Include bus interface
#include "../../core/bus_cycle_interface.h"

// Include ROM loader
#include "../../core/storage/rom_loader.h"
#include "../../core/config/path_discovery.h"

// File format handlers and registry
#include "../../core/formats/format_registry.h"
#include "../../core/formats/prg_format.h"
#include "../../core/formats/d64_format.h"
#include "../../core/formats/t64_format.h"
#include "../../core/formats/tap_format.h"
#include "../../core/formats/crt_format.h"
#include "../../core/formats/lnx_format.h"
#include "../../core/formats/commodore_load_helpers.h"
#include "../../devices/keyboard/commodore_keyboard_device.h"

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
    
    return traits;
}

/** Check if load address is a VIC-20 address */
static bool is_vic20_load_address(uint16_t addr) {
    return addr == vic20_constants::BASIC_START_UNEXPANDED ||
           addr == vic20_constants::BASIC_START_3K ||
           addr == vic20_constants::BASIC_START_8K ||
           addr == vic20_constants::BLK1_START ||
           addr == vic20_constants::BLK2_START ||
           addr == vic20_constants::BLK3_START ||
           addr == vic20_constants::BLK5_START;
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
        if (load_addr >= vic20_constants::BLK0_START && load_addr < vic20_constants::BLK0_END) mem = 1;
        if (load_addr >= vic20_constants::BLK1_START && load_addr < vic20_constants::BLK1_END) mem = 3;
        if ((load_addr >= vic20_constants::BLK2_START && load_addr < vic20_constants::BLK2_END) ||
            (end_addr > vic20_constants::BLK2_START && end_addr <= vic20_constants::BLK2_END))
            { if (mem < 2) mem = 2; }
        if ((load_addr >= vic20_constants::BLK3_START && load_addr < vic20_constants::BLK3_END) ||
            (end_addr > vic20_constants::BLK3_START && end_addr <= vic20_constants::BLK3_END))
            { if (mem < 4) mem = 4; }
        if (end_addr > vic20_constants::BLK3_START && load_addr < vic20_constants::BLK3_START)
            { if (mem < 4) mem = 4; }
    }

    return mem;
}

// ============================================================================
// VIC-20 file probe — unified confidence + configuration detection
// ============================================================================

static SystemProbeResult vic20_probe_file(
    const format_descriptor_t* matched_format,
    const char* /*filepath*/,
    const uint8_t* data, size_t size)
{
    SystemProbeResult result;

    if (!matched_format) return result;

    // --- PRG: confidence from load address, config from memory expansion ---
    if (matched_format == &PRG_FORMAT_DESCRIPTOR) {
        if (size >= 2) {
            uint16_t load_addr = data[0] | (data[1] << 8);
            uint32_t end_addr  = (uint32_t)load_addr + (uint32_t)(size - 2);

            if (load_addr == vic20_constants::BASIC_START_UNEXPANDED)
                result.confidence = 0.85f;
            else if (is_vic20_load_address(load_addr))
                result.confidence = 0.7f;
            else
                result.confidence = 0.5f;

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
                            if (addr >= vic20_constants::BLK0_START && addr < vic20_constants::BLK0_END)  { if (mem_index < 1) mem_index = 1; }
                            if (addr >= vic20_constants::BLK1_START && addr < vic20_constants::BLK1_END)  { if (mem_index < 3) mem_index = 3; }
                            if ((addr >= vic20_constants::BLK2_START && addr < vic20_constants::BLK2_END) || (ea > vic20_constants::BLK2_START && ea <= vic20_constants::BLK2_END))
                                { if (mem_index < 2) mem_index = 2; }
                            if ((addr >= vic20_constants::BLK3_START && addr < vic20_constants::BLK3_END) || (ea > vic20_constants::BLK3_START && ea <= vic20_constants::BLK3_END))
                                { if (mem_index < 4) mem_index = 4; }
                            if (addr == vic20_constants::BASIC_START_3K) { if (mem_index < 1) mem_index = 1; }
                            if (addr == vic20_constants::BASIC_START_8K) { if (mem_index < 2) mem_index = 2; }
                            if (ea > vic20_constants::BLK3_START && addr < vic20_constants::BLK3_START) { if (mem_index < 4) mem_index = 4; }
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

    // --- D64: extract first PRG, check load address + memory ---
    } else if (matched_format == &D64_FORMAT_DESCRIPTOR) {
        commodore_d64_t d64;
        if (d64.open_mem(data, size)) {
            commodore_prg_t prg = {};
            if (d64.extract_first_prg(&prg)) {
                uint32_t end_addr = (uint32_t)prg.load_addr + (uint32_t)prg.data_size;
                result.confidence = is_vic20_load_address(prg.load_addr) ? 0.90f : 0.4f;
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
    }

    return result;
}

/** Formats the VIC-20 can load — used by SystemDescriptor and file dialogs. */
static const format_descriptor_t* const vic20_formats[] = {
    &PRG_FORMAT_DESCRIPTOR, &TAP_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR,
    &T64_FORMAT_DESCRIPTOR, &LNX_FORMAT_DESCRIPTOR, &BIN_FORMAT_DESCRIPTOR,
    nullptr
};

static SystemDescriptor vic20_descriptor = {
    "Commodore VIC-20",
    "VIC20",
    "Commodore VIC-20 (1980) - 5KB RAM, 22-column display",
    vic20_formats,
    create_vic20_hardware_traits(),
    vic20_probe_file
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
VIC20System::VIC20System()
    : CommodoreSystem()
    , memory_(nullptr)
    , cpu_(nullptr)
    , vic_(nullptr)
    , via1_(nullptr)
    , via2_(nullptr)
    , expansion_flags_(VIC20_EXP_NONE)
    , autostart_delay_frames_(0)
{
    cycles_per_frame_ = vic20_constants::CYCLES_PER_FRAME_PAL;
    hardware_traits_ = create_vic20_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    
    // Initialize bus state with pull-up resistors (all control lines HIGH = inactive)
    bus_.default_state = VIC20_BUS_DEFAULT_STATE;
    bus_.state = bus_.default_state;
}

VIC20System::~VIC20System() {
    // Destroy CPU
    if (cpu_) {
        delete cpu_;
        cpu_ = nullptr;
    }
    
    // Destroy VIC chip — virtual destructor dispatches correctly
    if (vic_) {
        delete vic_;
        vic_ = nullptr;
    }
    
    // Destroy VIA chips
    if (via1_) {
        delete via1_;
        via1_ = nullptr;
    }
    
    if (via2_) {
        delete via2_;
        via2_ = nullptr;
    }
    
    // Destroy keyboard
    if (keyboard_) {
        delete keyboard_;
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
        
        // Update memory system if already created
        if (memory_) {
            vic20_memory_set_expansion(memory_, expansion_flags_);
        }
        
        printf("VIC20: Expansion configuration: $%02X\n", expansion_flags_);
    }
    
    return true;
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
    
    // Create CPU (MOS6502) — direct C++ instantiation for inlining
    cpu_ = new MOS6502();
    if (!cpu_) {
        printf("VIC20: Failed to create MOS6502 CPU\n");
        return false;
    }
    
    // Initialize CPU (descriptor-free — memory I/O is handled via bus_state_t pins)
    cpu_->init();
    
    // Reset CPU to initialize state
    cpu_->reset(0);
    
    // Manually load the reset vector since automatic reset doesn't work with callback-only CPU
    // KERNAL is at $E000-$FFFF (8KB), reset vector is at $FFFC-$FFFD
    uint8_t* kernal_ptr = vic20_memory_get_rom_ptr(memory_, VIC20_BASE_KERNAL);
    if (kernal_ptr) {
        uint16_t reset_vector = kernal_ptr[0xFFFC - 0xE000] | (kernal_ptr[0xFFFD - 0xE000] << 8);
        cpu_->set(REG_PC, reset_vector);
        printf("VIC20: CPU reset complete - PC = $%04X\n", (unsigned)cpu_->get(REG_PC));
    }
    
    // Create VIC chip — region-aware: MOS6561 for PAL, MOS6560 for NTSC
    bool is_pal_region = (config_.region_option_index <= 0);
    if (is_pal_region) {
        auto* pal_vic = new mos6561_t();
        pal_vic->init();
        vic_ = pal_vic;
        printf("VIC20: Created MOS6561 (PAL) VIC chip\n");
    } else {
        auto* ntsc_vic = new mos6560_t();
        ntsc_vic->init();
        vic_ = ntsc_vic;
        printf("VIC20: Created MOS6560 (NTSC) VIC chip\n");
    }
    if (!vic_) {
        printf("VIC20: Failed to create VIC chip\n");
        return false;
    }
    
    // Store VIC chip pointer in memory system for I/O handling
    memory_->vic_chip = vic_;
    
    // Set up VIC memory callbacks for accessing video and character memory
    vic_->set_memory_callbacks(
        VIC20System::vic_mem_read,      // Memory read callback
        this,                            // User data (VIC20System instance)
        VIC20System::vic_color_read,    // Color RAM read callback
        this);                           // User data for color RAM
    
    // Create VIA chips (MOS6522)
    // VIC-20 hardware: VIA1 ($9110) → NMI line, VIA2 ($9120) → IRQ line
    // VIA2 Timer 1 is the system heartbeat (jiffy clock, keyboard scan, cursor blink)
    via1_ = new mos6522_t();
    via1_->reset();
    if (via1_) {
        via1_->interrupt_bit = BUS_NMI_BIT;
        memory_->via1_chip = via1_;
    } else {
        printf("VIC20: Failed to create VIA1\n");
    }
    
    via2_ = new mos6522_t();
    via2_->reset();
    if (via2_) {
        via2_->interrupt_bit = BUS_IRQ_BIT;
        memory_->via2_chip = via2_;
    } else {
        printf("VIC20: Warning: VIA2 not created (optional)\n");
    }
    
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
        
        if (via2_) {
            // Register port read callbacks for keyboard matrix scanning
            // Port A reads rows, Port B reads columns (reverse scanning)
            via2_->set_port_a_read_callback(vic20_via2_port_a_read, this);
            via2_->set_port_b_read_callback(vic20_via2_port_b_read, this);
            printf("VIC20: Keyboard connected to VIA2 via callbacks\n");
        } else {
            printf("VIC20: Warning: Could not connect keyboard to VIA2\n");
        }
    } else {
        printf("VIC20: Warning: Could not create keyboard\n");
    }
    
    // Initialize I/O handlers now that all chips are created
    vic20_memory_init_io_handlers(memory_);
    
    // Setup connector ports (generic framework from EmulatedSystem)
    setup_connector_ports();

    // Register chips for the Hardware menu and debug windows
    register_vic20_chips();
    
    return true;
}

void VIC20System::shutdown() {
    printf("VIC20: Shutting down system\n");
}

void VIC20System::reset() {
    printf("VIC20: Resetting system\n");
    
    // Reset VIC chip (clears registers, video state, audio state)
    if (vic_) {
        vic_->reset();
        // Re-establish memory callbacks (reset clears them)
        vic_->set_memory_callbacks(
            VIC20System::vic_mem_read, this,
            VIC20System::vic_color_read, this);
        // Re-establish framebuffer pointer
        if (rgba_framebuffer_) {
            vic_->set_framebuffer(rgba_framebuffer_, rgba_width_, rgba_height_);
        }
    }
    
    // Reset VIA chips (clears timers, interrupt flags, port registers)
    // interrupt_bit is preserved by mos6522_reset — it's hardware wiring, not state
    if (via1_) {
        via1_->reset();
    }
    if (via2_) {
        via2_->reset();
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
        auto* cpu = cpu_;
        cpu->reset(0);
        
        // Reload reset vector
        uint8_t* kernal_ptr = vic20_memory_get_rom_ptr(memory_, VIC20_BASE_KERNAL);
        if (kernal_ptr) {
            uint16_t reset_vector = kernal_ptr[0xFFFC - 0xE000] | (kernal_ptr[0xFFFD - 0xE000] << 8);
            cpu->set(REG_PC, reset_vector);
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
    s = vic_->tick(s);
    
    // =========================================================================
    // PHASE 2: VIA CHIPS TICKING (BEFORE CPU PHI2)
    // VIA chips handle I/O and timing, must tick before CPU to set interrupt lines
    // =========================================================================
    s = via1_->tick(s);
    s = via2_->tick(s);
    
    // =========================================================================
    // PHASE 3: CPU TICKING (PHI2 phase - sets up memory access)
    // CPU executes instruction and puts address/control on bus
    // =========================================================================
    auto* cpu = cpu_;
    s = cpu->tick<MOS6502::Phase::PHI2>(s);
    
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
    s = cpu->tick<MOS6502::Phase::PHI1>(s);
    
    // Restore R/W line to read mode after CPU PHI1 has consumed write info.
    // Maintains invariant: BUS_MASK_RW is always set outside the CPU write window.
    BUS_SET_BIT(s, BUS_RW_BIT);

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

    // Tick all attached peripheral devices (datasette, drive, etc.)
    tick_peripherals();
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

    // Set program title to bare filename
    const char* name = filepath;
    const char* sep = strrchr(filepath, '/');
    if (!sep) sep = strrchr(filepath, '\\');
    if (sep) name = sep + 1;
    program_title_ = name;

    return true;
}

// ============================================================================
// Commodore Load Helper Callbacks -- VIC-20 specific
// ============================================================================

static uint8_t vic20_mem_read_for_load(void* ctx, uint16_t addr) {
    vic20_memory_t* mem = static_cast<vic20_memory_t*>(ctx);
    return vic20_memory_read_byte(mem, addr);
}

static void vic20_mem_write_byte_cb(void* ctx, uint16_t addr, uint8_t val) {
    vic20_memory_t* mem = static_cast<vic20_memory_t*>(ctx);
    vic20_memory_write_byte(mem, addr, val);
}

bool VIC20System::load_file_into_memory(const char* filepath) {
    printf("VIC20: Loading file into memory: %s\n", filepath);

    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        printf("VIC20: Failed to load file: %s\n", result.error_msg);
        result.release();
        return false;
    }

    commodore_load_context_t ctx = {};
    ctx.system_name     = "VIC20";
    ctx.write_byte      = vic20_mem_write_byte_cb;
    ctx.write_block     = nullptr;  // VIC-20 uses banked memory, no memcpy
    ctx.mem_read        = vic20_mem_read_for_load;
    ctx.mem_ctx         = memory_;
    ctx.basic_params    = &COMMODORE_BASIC_VIC20;
    ctx.basic_start_addrs[0] = vic20_constants::BASIC_START_3K;
    ctx.basic_start_addrs[1] = vic20_constants::BASIC_START_UNEXPANDED;
    ctx.basic_start_addrs[2] = vic20_constants::BASIC_START_8K;
    ctx.default_raw_addr = vic20_constants::BLK5_START;
    ctx.set_pc          = nullptr;  // VIC-20 uses keyboard buffer injection
    ctx.try_sys_from_filename = true;

    bool success = commodore_apply_load_result(&ctx, &result, filepath);

    result.release();
    return success;
}

// ============================================================================
// Display
// ============================================================================

uint32_t* VIC20System::get_framebuffer() {
    return rgba_framebuffer_;
}

void VIC20System::get_display_dimensions(int* width, int* height) const {
    *width = vic20_constants::DISPLAY_WIDTH;
    *height = vic20_constants::DISPLAY_HEIGHT;
}

void VIC20System::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
    
    // Update VIC chip with new framebuffer (critical for display!)
    if (vic_ && buffer) {
        printf("VIC20: Setting framebuffer on VIC chip: %dx%d buffer=%p\n", width, height, (void*)buffer);
        vic_->set_framebuffer(buffer, width, height);
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

void VIC20System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset VIC-20")) {
        reset();
    }
#endif
}

// ============================================================================
// Chip Registration — populate registered_chips_ for Hardware menu + debug
// ============================================================================

void VIC20System::register_vic20_chips() {
    auto* cpu = cpu_;
    auto* vic = vic_;
    auto* via1 = via1_;
    auto* via2 = via2_;

    // CPU — native ChipBase, registered directly
    register_chip(static_cast<ChipBase*>(cpu_),
        "MOS 6502 CPU", "6502", "CPU", 0x0000);

    // VIC — native ChipBase, registered directly
    register_chip(vic,
        "VIC (MOS 6560/6561)", "VIC", "Video", 0x9000);

    // VIA 1 — native ChipBase, registered directly
    register_chip(via1,
        "VIA 1 (MOS 6522)", "VIA 1", "I/O", 0x9110);

    // VIA 2 — native ChipBase, registered directly
    register_chip(via2,
        "VIA 2 (MOS 6522)", "VIA 2", "I/O", 0x9120);

    // RAM — MemoryChip with layout rendering
    register_chip(std::make_unique<MemoryChip>(
        ChipInfo{"DRAM", "Various"}, 32768, MemoryChip::RAM, &bus_.state,
        "RAM", 0x0000));

    // Character ROM
    register_chip(std::make_unique<MemoryChip>(
        ChipInfo{"MOS 901460-03", "Commodore"}, 4096, MemoryChip::ROM, &bus_.state,
        "CHARROM", 0x8000));

    // BASIC ROM
    register_chip(std::make_unique<MemoryChip>(
        ChipInfo{"MOS 901486-01", "Commodore"}, 8192, MemoryChip::ROM, &bus_.state,
        "BASIC", 0xC000));

    // KERNAL ROM
    register_chip(std::make_unique<MemoryChip>(
        ChipInfo{"MOS 901486-07", "Commodore"}, 8192, MemoryChip::ROM, &bus_.state,
        "KERNAL", 0xE000));
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
    if (!vic_ || max_samples == 0 || !buffer) return 0;

    // Read unsigned-8-bit samples from VIC ring buffer and convert to float
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
    uint8_t port_b_output = sys->via2_->port_b.output();
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
    uint8_t port_a_output = sys->via2_->port_a.output();
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
// CONNECTOR PORT SETUP — VIC-20
// ============================================================================
// VIC-20 has: 1× Control Port (DB-9), IEC Serial Bus, Cassette Port,
// User Port, and Expansion Port (cartridge slot).

static const ConnectorDefinition vic20_control_port_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Control Port",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition vic20_iec_serial_def = {
    ConnectorType::IEC_SERIAL,
    "IEC Serial Bus",
    ConnectorSignals::IEC_SERIAL_SIGNALS,
    ConnectorSignals::IEC_SERIAL_SIGNAL_COUNT,
    false,  // is_internal
    true    // is_bus — shared bus, multiple drives/printers
};

static const ConnectorDefinition vic20_cassette_def = {
    ConnectorType::CASSETTE_PORT,
    "Cassette Port",
    ConnectorSignals::CASSETTE_PORT_SIGNALS,
    ConnectorSignals::CASSETTE_PORT_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition vic20_user_port_def = {
    ConnectorType::USER_PORT,
    "User Port",
    ConnectorSignals::USER_PORT_SIGNALS,
    ConnectorSignals::USER_PORT_SIGNAL_COUNT,
    false, false
};

static const SignalLine vic20_expansion_signals[] = {
    { "RESET", SignalDirection::OUTPUT, 0 },
};
static const ConnectorDefinition vic20_expansion_def = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port",
    vic20_expansion_signals,
    1,
    false, false
};

void VIC20System::setup_connector_ports() {
    connector_ports_.clear();

    // Port 0 — Control Port (joystick/paddles/lightpen)
    add_connector_port(vic20_control_port_def, 1);

    // Port 1 — IEC Serial Bus (disk drive, printer)
    add_connector_port(vic20_iec_serial_def, 0);

    // Port 2 — Cassette Port (datasette)
    add_connector_port(vic20_cassette_def, 0);

    // Port 3 — User Port (modems, RS-232, custom peripherals)
    add_connector_port(vic20_user_port_def, 0);

    // Port 4 — Expansion Port (cartridge)
    add_connector_port(vic20_expansion_def, 0);

    // Port 5 — Internal Keyboard (always attached)
    static const ConnectorDefinition vic20_keyboard_def = {
        ConnectorType::CUSTOM, "Keyboard", nullptr, 0, true, false
    };
    int kb_port = add_connector_port(vic20_keyboard_def, 0);

    // Attach internal keyboard device
    auto kb_device = std::make_unique<CommodoreKeyboardDevice>(keyboard_);
    auto* kb_raw = kb_device.get();
    connector_ports_[kb_port]->attach_device(kb_raw);
    owned_devices_.push_back(std::move(kb_device));

    // Default: attach peripheral devices via declarative list
    attach_default_peripherals();

    printf("VIC20: Created %zu connector ports\n", connector_ports_.size());
}

std::vector<EmulatedSystem::DefaultPeripheral>
VIC20System::get_default_peripherals() const {
    return {
        { 0, "joystick"  },  // Control Port — joystick
        { 1, "1541"      },  // IEC Serial Bus — 1541 disk drive
        { 2, "datasette" },  // Cassette Port — datasette (1530)
    };
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
