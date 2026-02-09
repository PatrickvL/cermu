#include "vic20_system.h"
#include "vic20_memory.h"
#include "vic20_chips.h"
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

// File detection callback
static float vic20_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
            // PRG files with VIC-20 load address (0x1001)
            if (size >= 2) {
                uint16_t load_addr = data[0] | (data[1] << 8);
                if (load_addr == 0x1001) {
                    return 0.85f;  // High confidence for VIC-20 PRG
                }
                // Generic PRG file - moderate confidence
                return 0.5f;
            }
        }
        if (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0) {
            return 0.7f;  // TAP files
        }
        if (strcmp(ext, ".d64") == 0 || strcmp(ext, ".D64") == 0) {
            return 0.6f;  // Disk images
        }
    }
    return 0.0f;
}

static const char* vic20_extensions[] = {".prg", ".tap", ".d64", nullptr};

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
    , cycles_per_frame_(22168)
    , expansion_flags_(VIC20_EXP_NONE)
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
    
    // Initialize I/O handlers now that all chips are created
    vic20_memory_init_io_handlers(memory_);
    
    return true;
}

void VIC20System::shutdown() {
    printf("VIC20: Shutting down system\n");
}

void VIC20System::reset() {
    printf("VIC20: Resetting system\n");
    if (cpu_) {
        mos6502_reset(cpu_, 0);
        
        // Reload reset vector
        uint8_t* kernal_ptr = vic20_memory_get_rom_ptr(memory_, VIC20_BASE_KERNAL);
        if (kernal_ptr) {
            uint16_t reset_vector = kernal_ptr[0xFFFC - 0xE000] | (kernal_ptr[0xFFFD - 0xE000] << 8);
            mos6502_set_pc(cpu_, reset_vector);
        }
    }
    total_cycles_ = 0;
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
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }
}

// ============================================================================
// File Loading
// ============================================================================

bool VIC20System::load_file(const char* filepath) {
    printf("VIC20: Loading file: %s\n", filepath);
    
    // Determine file type
    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        printf("VIC20: Unknown file type (no extension)\n");
        return false;
    }
    
    if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
        // TODO: Implement PRG loading
        printf("VIC20: PRG file loading not yet implemented\n");
        return false;
    }
    
    printf("VIC20: Unsupported file type: %s\n", ext);
    return false;
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

void VIC20System::handle_keyboard_event(int key, bool pressed) {
    // TODO: Implement keyboard matrix
    (void)key;
    (void)pressed;
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
