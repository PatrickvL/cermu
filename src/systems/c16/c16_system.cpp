#include "c16_system.h"
#include "c16_keyboard_matrix.h"
#include "../../chip/input/emu_key_sdl_map.h"
#include "../../core/storage/rom_loader.h"
#include "../../core/formats/format_registry.h"
#include "../../core/formats/prg_format.h"
#include "../../core/formats/d64_format.h"
#include "../../core/formats/t64_format.h"
#include "../../core/formats/tap_format.h"
#include "../../core/formats/crt_format.h"
#include "../../core/formats/lnx_format.h"
#include "../../core/formats/commodore_load_helpers.h"
#include "../../devices/keyboard/commodore_keyboard_device.h"
// CPU is now a native ChipBase (via fam65xx_t<Traits> inheritance)
#include "../../core/chip.h"
#include "../../chip/memory/memory_chip.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// ============================================================================
// Hardware Traits Definition
// ============================================================================

template<C264SeriesVariant V>
HardwareTraits Commodore264System<V>::create_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - TED 7360
    traits.display.native_width = 320;
    traits.display.native_height = 200;
    traits.display.visible_width = 320;
    traits.display.visible_height = 200;
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
    traits.audio.sample_rate_hz = 22050;
    traits.audio.channels = 1;
    traits.audio.chip_name = "TED 7360";
    
    // Timing - PAL version
    traits.timing.cpu_frequency_hz = 886724;    // ~0.89 MHz (PAL)
    traits.timing.video_frequency_hz = 886724;  // Same as CPU
    traits.timing.audio_sample_rate_hz = 22050;
    traits.timing.target_fps = 50;              // PAL
    traits.timing.cycles_per_frame = 17734;     // 886724 / 50
    traits.timing.standard = VideoStandard::PAL;
    
    // Memory options — variant-specific
    if constexpr (Traits::default_ram >= 65536) {
        traits.memory_options.push_back({
            "64KB RAM",
            65536,
            32768,  // 32KB ROM
            true
        });
    } else {
        traits.memory_options.push_back({
            "16KB RAM",
            16384,
            32768,  // 32KB ROM
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
    ntsc_timing.cpu_frequency_hz = 894886;      // ~0.89 MHz (NTSC)
    ntsc_timing.video_frequency_hz = 894886;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 14914;       // 894886 / 60
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
// File Detection (shared across all TED variants)
// ============================================================================

template<C264SeriesVariant V>
SystemProbeResult Commodore264System<V>::probe_file_static(
    const format_descriptor_t* matched_format,
    const char* /*filepath*/,
    const uint8_t* data, size_t size)
{
    SystemProbeResult result;

    if (!matched_format) return result;

    if (matched_format == &PRG_FORMAT_DESCRIPTOR) {
        if (size >= 2) {
            uint16_t load_addr = data[0] | (data[1] << 8);
            if (load_addr == 0x1001)
                result.confidence = 0.6f;   // Moderate (could also be VIC-20)
            else
                result.confidence = 0.4f;   // Lower for generic PRG
        }

    } else if (matched_format == &TAP_FORMAT_DESCRIPTOR) {
        int platform = commodore_tap_identify_platform_mem(data, size);
        if (platform == 2)      result.confidence = 0.95f;  // C16 TAP
        else if (platform == 0) result.confidence = 0.2f;   // C64 TAP
        else                    result.confidence = 0.4f;

    } else if (matched_format == &D64_FORMAT_DESCRIPTOR) {
        result.confidence = 0.4f;

    } else if (matched_format == &T64_FORMAT_DESCRIPTOR) {
        result.confidence = 0.4f;   // Usually C64, but can contain C16

    } else if (matched_format == &LNX_FORMAT_DESCRIPTOR) {
        result.confidence = 0.4f;

    } else if (matched_format == &BIN_FORMAT_DESCRIPTOR) {
        result.confidence = 0.3f;
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

static const ConnectorDefinition c16_joy_port_1_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Joystick Port 1",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition c16_joy_port_2_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Joystick Port 2",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition c16_iec_serial_def = {
    ConnectorType::IEC_SERIAL,
    "IEC Serial Bus",
    ConnectorSignals::IEC_SERIAL_SIGNALS,
    ConnectorSignals::IEC_SERIAL_SIGNAL_COUNT,
    false,  // is_internal
    true    // is_bus — shared bus, multiple drives/printers
};

static const ConnectorDefinition c16_cassette_def = {
    ConnectorType::CASSETTE_PORT,
    "Cassette Port",
    ConnectorSignals::CASSETTE_PORT_SIGNALS,
    ConnectorSignals::CASSETTE_PORT_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition plus4_user_port_def = {
    ConnectorType::USER_PORT,
    "User Port",
    ConnectorSignals::USER_PORT_SIGNALS,
    ConnectorSignals::USER_PORT_SIGNAL_COUNT,
    false, false
};

static const SignalLine c16_expansion_signals[] = {
    { "/RESET", SignalDirection::OUTPUT, 0 },
    { "/IRQ",   SignalDirection::INPUT,  1 },
};
static const ConnectorDefinition c16_expansion_def = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port",
    c16_expansion_signals,
    2,
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
    cycles_per_frame_ = 17734;
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
        formats,
        create_hardware_traits(),
        probe_file_static
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
        ram_size_ = 16384;  // Default C16
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
    
    printf("%s: Initializing system\n", Traits::name);
    
    // Create memory chips early — storage is ready for ROM loading
    auto ram_chip = std::make_unique<MemoryChip>(
        ChipInfo{"DRAM", "Various"}, 65536, MemoryChip::RAM, &bus_state_,
        "RAM", 0x0000);
    ram_ = ram_chip.get();

    auto basic_chip = std::make_unique<MemoryChip>(
        ChipInfo{"ROM", "Commodore"}, 16384, MemoryChip::ROM, &bus_state_,
        "BASIC", 0x8000);
    basic_rom_ = basic_chip.get();

    auto kernal_chip = std::make_unique<MemoryChip>(
        ChipInfo{"ROM", "Commodore"}, 16384, MemoryChip::ROM, &bus_state_,
        "KERNAL", 0xC000);
    kernal_rom_ = kernal_chip.get();
    
    // Load ROMs using common ROM loader
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("%s: Warning - ROMs not loaded, system may not function correctly\n", Traits::name);
    }
    
    // Initialize MOS 7501 CPU — direct C++ instantiation for inlining
    cpu_ = new CSG7501();
    if (!cpu_) {
        printf("%s: Failed to create MOS 7501 CPU\n", Traits::name);
        return false;
    }
    
    // Initialize CPU and I/O port
    auto* cpu = cpu_;
    cpu->init();
    cpu->init_io_port();
    
    // Note: C16 doesn't use the io_port_mixin bank_change path.
    // Banking is handled by TED register writes.
    
    // Read reset vector from KERNAL ROM and set CPU PC
    if (roms_loaded) {
        uint16_t reset_vector = (*kernal_rom_)[0xFFFC - 0xC000] | ((*kernal_rom_)[0xFFFD - 0xC000] << 8);
        cpu->set(REG_PC, reset_vector);
        cpu->set(REG_AB, reset_vector);
        printf("%s: CPU reset vector = $%04X\n", Traits::name, reset_vector);
    }
    
    // Initialize bus state with default pin levels
    bus_state_ = C264_BUS_DEFAULT_STATE;
    
    // Initialize TED 7360 (video, sound, timers, keyboard scanning)
    {
        bool is_pal_region = (config_.region_option_index <= 0);
        ted7360_desc_t ted_desc = {};
        ted_desc.is_pal = is_pal_region;
        ted_desc.keyboard_scan = ted_keyboard_scan;
        ted_desc.keyboard_user_data = this;
        ted_desc.mem_read = ted_mem_read;
        ted_desc.mem_read_user_data = this;
        ted_ = new ted7360_t(ted_desc);
        if (ted_) {
            printf("%s: Created TED 7360 (%s)\n", Traits::name, is_pal_region ? "PAL" : "NTSC");
        } else {
            printf("%s: Warning - TED 7360 creation failed\n", Traits::name);
        }
    }
    
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
        printf("%s: Warning - keyboard matrix creation failed\n", Traits::name);
    }
    
    setup_connector_ports();

    // Register chips for the Hardware menu and debug windows
    register_chip(static_cast<ChipBase*>(cpu_),
        "MOS 7501/8501 CPU", "7501", "CPU", 0x0000);
    register_chip(ted_,
        "TED 7360 (Video/Audio/I/O)", "TED", "Video", 0xFF00);
    register_chip(std::move(ram_chip));
    register_chip(std::move(basic_chip));
    register_chip(std::move(kernal_chip));

    initialized_ = true;
    return true;
}

template<C264SeriesVariant V>
void Commodore264System<V>::shutdown() {
    printf("%s: Shutting down system\n", Traits::name);
    
    // Destroy MOS 7501 CPU
    if (cpu_) {
        delete cpu_;
        cpu_ = nullptr;
    }
    
    // Destroy TED 7360
    if (ted_) {
        delete ted_;
        ted_ = nullptr;
    }
    
    // Destroy keyboard
    if (keyboard_) {
        delete keyboard_;
        keyboard_ = nullptr;
    }
    
    initialized_ = false;
}

template<C264SeriesVariant V>
void Commodore264System<V>::reset() {
    printf("%s: Resetting system\n", Traits::name);
    
    // Reset MOS 7501 CPU — use mos7501_reset (not mos7501_init) to properly
    // reset the instruction decoder state (current_handler, half_cycle,
    // opcode_entry).  init() only reinitialises the IO port — it leaves the
    // CPU mid-instruction, which causes a segfault when emulation resumes
    // with an inconsistent pipeline.
    if (cpu_) {
        cpu_->reset(0);
        
        // Re-read reset vector from KERNAL ROM
        uint16_t reset_vector = (*kernal_rom_)[0xFFFC - 0xC000] | ((*kernal_rom_)[0xFFFD - 0xC000] << 8);
        cpu_->set(REG_PC, reset_vector);
        cpu_->set(REG_AB, reset_vector);
        printf("%s: CPU reset (PC=$%04X)\n", Traits::name, reset_vector);
    }
    
    // Reset TED 7360
    if (ted_) {
        ted_->reset();
    }
    
    // Reset bus state
    bus_state_ = C264_BUS_DEFAULT_STATE;
    
    // Reset keyboard matrix
    if (keyboard_) {
        keyboard_->reset();
    }
    
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
    
    // PHASE 1: TED PHI1
    s = ted_->tick_phi1(s);
    
    // HARDWARE WIRING: BA -> RDY (direct bit test + set/clear)
    if (BUS_GET_BIT(s, BUS_BA_BIT))
        BUS_SET_BIT(s, BUS_RDY_BIT);
    else
        BUS_CLR_BIT(s, BUS_RDY_BIT);
    
    // PHASE 2: CPU PHI2
    s = cpu_->tick<CSG7501::Phase::PHI2>(s);
    
    // PHASE 3: Memory service
    s = mem_tick(s);
    
    // PHASE 3.1: TED PHI2 delivery
    ted_->tick_phi2(s);
    
    // PHASE 4: CPU PHI1
    s = cpu_->tick<CSG7501::Phase::PHI1>(s);
    
    // Restore R/W line to read mode after CPU PHI1 has consumed write info
    BUS_SET_BIT(s, BUS_RW_BIT);
    
    bus_state_ = s;
    total_cycles_++;
}

template<C264SeriesVariant V>
void Commodore264System<V>::run_frame() {
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }

    // Tick all attached peripheral devices
    tick_peripherals();
}

// ============================================================================
// File Loading
// ============================================================================

template<C264SeriesVariant V>
bool Commodore264System<V>::load_file(const char* filepath) {
    if (!initialized_) {
        printf("%s: System not initialized, initializing now...\n", Traits::name);
        if (!initialize()) {
            printf("%s: Failed to initialize system for file loading\n", Traits::name);
            return false;
        }
    }
    
    printf("%s: Loading file: %s\n", Traits::name, filepath);

    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        printf("%s: Failed to load file: %s\n", Traits::name, result.error_msg);
        result.release();
        return false;
    }

    commodore_load_context_t ctx = {};
    ctx.system_name     = Traits::name;
    ctx.write_byte      = c16_mem_write_byte;
    ctx.write_block     = c16_mem_write_block;
    ctx.mem_read        = c16_mem_read;
    ctx.mem_ctx         = ram_->data();
    ctx.basic_params    = &COMMODORE_BASIC_C16;
    ctx.basic_start_addrs[0] = 0x1001;
    ctx.default_raw_addr = 0x4000;
    ctx.set_pc          = set_cpu_pc;
    ctx.pc_ctx          = this;

    bool success = commodore_apply_load_result(&ctx, &result, filepath);

    if (success) {
        // Set program title to bare filename
        const char* name = filepath;
        const char* sep = strrchr(filepath, '/');
        if (!sep) sep = strrchr(filepath, '\\');
        if (sep) name = sep + 1;
        program_title_ = name;
    }

    result.release();
    return success;
}

// ============================================================================
// Display
// ============================================================================

template<C264SeriesVariant V>
uint32_t* Commodore264System<V>::get_framebuffer() {
    return rgba_framebuffer_;
}

template<C264SeriesVariant V>
void Commodore264System<V>::get_display_dimensions(int* width, int* height) const {
    *width = 320;
    *height = 200;
}

template<C264SeriesVariant V>
void Commodore264System<V>::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
    
    // Set TED framebuffer for pixel output
    if (ted_) {
        ted_->set_framebuffer(buffer, width, height);
    }
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
#ifdef IMGUI_VERSION
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
#ifdef IMGUI_VERSION
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
    // Try to load C16/Plus4 ROMs from standard locations
    const char* rom_root = "data/c16/roms";  // Default ROM path
    
    // Load KERNAL ROM (16KB at $C000-$FFFF)
    const char* kernal_files[] = {
        "kernal.318004-05.bin",
        "kernal.rom",
        "318004-05.bin",
        nullptr
    };
    
    bool kernal_ok = rom_loader_load_from_root(
        rom_root, kernal_files,
        kernal_rom_->size_bytes(), kernal_rom_->data(), kernal_rom_->size_bytes()
    );
    
    if (!kernal_ok) {
        printf("%s: Failed to load KERNAL ROM\n", Traits::name);
    }
    
    // Load BASIC ROM (16KB at $8000-$BFFF)
    const char* basic_files[] = {
        "basic.318006-01.bin",
        "basic.rom",
        "318006-01.bin",
        nullptr
    };
    
    bool basic_ok = rom_loader_load_from_root(
        rom_root, basic_files,
        basic_rom_->size_bytes(), basic_rom_->data(), basic_rom_->size_bytes()
    );
    
    if (!basic_ok) {
        printf("%s: Failed to load BASIC ROM\n", Traits::name);
    }
    
    return (kernal_ok && basic_ok);
}

// ============================================================================
// BUS MEMORY SERVICE
// ============================================================================

template<C264SeriesVariant V>
bus_state_t Commodore264System<V>::mem_tick(bus_state_t s) {
    uint16_t addr = BUS_GET_ADDR(s);

    // Use cached RAM size (updated in apply_configuration)
    const size_t ram_size = ram_size_;

    if (BUS_GET_BIT(s, BUS_RW_BIT)) {
        // ---- Read cycle ----
        uint8_t data = 0xFF;

        // TED registers at $FF00-$FF3F (always visible)
        if (addr >= 0xFF00 && addr <= 0xFF3F) {
            if (ted_) {
                bus_state_t ted_state = 0;
                BUS_SET_ADDR(ted_state, addr);
                ted_state = ted_->registers_read(ted_state);
                data = BUS_GET_DATA(ted_state);
            }
        }
        // RAM (0x0000-size based on configuration)
        else if (addr < ram_size) {
            data = (*ram_)[addr];
        }
        // BASIC ROM (0x8000-0xBFFF = 16KB)
        else if (addr >= 0x8000 && addr < 0xC000) {
            bool rom_visible = ted_ ? ted_->rom_enabled : true;
            data = rom_visible ? (*basic_rom_)[addr - 0x8000] : (*ram_)[addr];
        }
        // KERNAL ROM (0xC000-0xFFFF = 16KB)
        else if (addr >= 0xC000) {
            bool rom_visible = ted_ ? ted_->rom_enabled : true;
            data = rom_visible ? (*kernal_rom_)[addr - 0xC000] : (*ram_)[addr];
        }

        BUS_SET_DATA(s, data);
    } else {
        // ---- Write cycle ----
        uint8_t data = BUS_GET_DATA(s);

        // TED registers at $FF00-$FF3F (always writable)
        if (addr >= 0xFF00 && addr <= 0xFF3F) {
            if (ted_) {
                bus_state_t ted_state = 0;
                BUS_SET_ADDR(ted_state, addr);
                BUS_SET_DATA(ted_state, data);
                ted_->registers_write(ted_state);
            }
        }
        // Writes always go to RAM (ROM is read-only, writes pass through)
        else if (addr < ram_size) {
            (*ram_)[addr] = data;
        }
        // For 64KB models, RAM extends to $FFFF (excluding TED registers)
        else if (ram_size >= 65536 && addr < 0xFF00) {
            (*ram_)[addr] = data;
        }
    }

    return s;
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
    
    // Active-low: 0 bits select columns, 0 bits in result = key pressed.
    uint8_t result = 0xFF;
    for (int col = 0; col < 8; col++) {
        if (!(column & (1 << col))) {
            result &= sys->keyboard_->row_open_contacts[col];
        }
    }
    return result;
}

// ============================================================================
// TED MEMORY READ CALLBACK
// ============================================================================

template<C264SeriesVariant V>
uint8_t Commodore264System<V>::ted_mem_read(void* user_data, uint16_t address) {
    auto* sys = static_cast<Commodore264System<V>*>(user_data);
    return (*sys->ram_)[address & 0xFFFF];
}

// ============================================================================
// LOAD HELPER — set CPU PC
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::set_cpu_pc(void* user_data, uint16_t addr) {
    auto* sys = static_cast<Commodore264System<V>*>(user_data);
    if (sys->cpu_) {
        sys->cpu_->set(REG_PC, addr);
        sys->cpu_->set(REG_AB, addr);
        sys->cpu_->transition_to_fetch();
        printf("%s: PC set to $%04X\n", Traits::name, addr);
    }
}

// ============================================================================
// CONNECTOR PORT SETUP
// ============================================================================

template<C264SeriesVariant V>
void Commodore264System<V>::setup_connector_ports() {
    connector_ports_.clear();

    // Port 0 — Joystick Port 1
    add_connector_port(c16_joy_port_1_def, 1);

    // Port 1 — Joystick Port 2
    add_connector_port(c16_joy_port_2_def, 2);

    // Port 2 — IEC Serial Bus
    add_connector_port(c16_iec_serial_def, 0);

    // Port 3 — Cassette Port
    add_connector_port(c16_cassette_def, 0);

    // Port 4 — User Port (Plus/4 only)
    if constexpr (Traits::has_user_port) {
        add_connector_port(plus4_user_port_def, 0);
    }

    // Port 5 — Expansion Port (cartridge slot)
    add_connector_port(c16_expansion_def, 0);

    // Internal Keyboard (always attached)
    static const ConnectorDefinition c16_keyboard_def = {
        ConnectorType::CUSTOM, "Keyboard", nullptr, 0, true, false
    };
    int kb_port = add_connector_port(c16_keyboard_def, 0);

    // Attach internal keyboard device
    auto kb_device = std::make_unique<CommodoreKeyboardDevice>(keyboard_);
    auto* kb_raw = kb_device.get();
    connector_ports_[kb_port]->attach_device(kb_raw);
    owned_devices_.push_back(std::move(kb_device));

    // Default: attach joystick to Joystick Port 1
    attach_device_to_port(0, "joystick");

    printf("%s: Created %zu connector ports\n",
           Traits::name, connector_ports_.size());
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
