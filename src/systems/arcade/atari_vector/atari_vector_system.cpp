/*
 * atari_vector_system.cpp — Atari vector arcade system implementation
 *
 * Shared implementation for Asteroids and Lunar Lander.
 *
 * Tick loop (per CPU cycle at 1.512 MHz):
 *   1. CPU PHI2 → address + data on bus
 *   2. Address decode:
 *      - $0000-$03FF → Work RAM (via MemoryBus)
 *      - $2000-$2FFF → I/O reads (manual dispatch)
 *      - $3000-$3FFF → I/O writes (manual dispatch)
 *      - $4000-$47FF → Vector RAM (via MemoryBus)
 *      - $5000-$57FF → Vector ROM (via MemoryBus)
 *      - $6000/$6800+ → Program ROM (via MemoryBus)
 *      - Unmapped → open bus
 *   3. CPU PHI1
 *   4. DVG tick (runs concurrently)
 *   5. NMI timer (fires every ~6048 CPU cycles ≈ 250 Hz)
 *
 * ROM file format:
 *   Raw binary ROM dumps.  The system expects the full program ROM
 *   concatenated with vector ROM if separate.
 */

#include "systems/arcade/atari_vector/atari_vector_system.hpp"
#include "core/system_registry.hpp"
#include "core/vfs/vfs.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

namespace atv = atari_vector_constants;

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<AtariVectorVariant V>
static HardwareTraits create_vector_hardware_traits() {
    using Traits = AtariVectorTraits<V>;

    HardwareTraits ht = {};

    // Display
    ht.display.native_width    = atv::DISPLAY_WIDTH;
    ht.display.native_height   = atv::DISPLAY_HEIGHT;
    ht.display.visible_width   = atv::DISPLAY_WIDTH;
    ht.display.visible_height  = atv::DISPLAY_HEIGHT;
    ht.display.format          = FramebufferFormat::RGBA8888;
    ht.display.palette_size    = 0;  // Vector display — no palette
    ht.display.pixel_aspect_ratio = 1.0f;
    ht.display.has_overscan    = false;

    // Audio
    ht.audio.format             = AudioFormat::CUSTOM;
    ht.audio.sample_rate_hz     = atv::DEFAULT_SAMPLE_RATE;
    ht.audio.channels           = 1;
    ht.audio.chip_name          = "Discrete";

    // Timing
    ht.timing.cpu_frequency_hz   = atv::CPU_FREQ_HZ;
    ht.timing.video_frequency_hz = atv::CPU_FREQ_HZ;
    ht.timing.audio_sample_rate_hz = atv::DEFAULT_SAMPLE_RATE;
    ht.timing.target_fps         = atv::TARGET_FPS;
    ht.timing.cycles_per_frame   = atv::CYCLES_PER_FRAME;
    ht.timing.standard           = VideoStandard::CUSTOM;

    return ht;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor asteroids_descriptor = {
    "Asteroids",
    "Asteroids",
    "Atari Asteroids (1979) — 6502 CPU, DVG vector display",
    "asteroids",
    {"Asteroids", "ASTEROIDS"},
    nullptr,
    create_vector_hardware_traits<AtariVectorVariant::ASTEROIDS>(),
    [](const format_descriptor_t*, const char* filepath,
       const uint8_t*, size_t size) -> SystemProbeResult {
        SystemProbeResult result = { 0.0f, {} };
        const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
        if (!ext) return result;
        // Match common Asteroids ROM extensions
        if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
            // Asteroids ROMs are typically 6 KB (program) + 2 KB (vector) = 8 KB
            // or raw 6 KB program ROM
            if (size == 6144 || size == 8192)
                result.confidence = 0.3f;
        }
        return result;
    }
};

static SystemDescriptor lunar_lander_descriptor = {
    "Lunar Lander",
    "LunarLander",
    "Atari Lunar Lander (1979) — 6502 CPU, DVG vector display",
    "lunar_lander",
    {"LunarLander", "Lunar Lander", "LUNARLANDER"},
    nullptr,
    create_vector_hardware_traits<AtariVectorVariant::LUNAR_LANDER>(),
    [](const format_descriptor_t*, const char* filepath,
       const uint8_t*, size_t size) -> SystemProbeResult {
        SystemProbeResult result = { 0.0f, {} };
        const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
        if (!ext) return result;
        if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
            // Lunar Lander: 8 KB program + 2 KB vector = 10 KB, or 8 KB program alone
            if (size == 8192 || size == 10240)
                result.confidence = 0.3f;
        }
        return result;
    }
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<AtariVectorVariant V>
AtariVectorSystem<V>::AtariVectorSystem()
    : System()
    , pins_(MOS6502::default_bus_state())
    , nmi_counter_(atv::NMI_PERIOD_CYCLES)
{
    hardware_traits_ = create_vector_hardware_traits<V>();
    current_palette_ = hardware_traits_.display.default_palette;
}

template<AtariVectorVariant V>
AtariVectorSystem<V>::~AtariVectorSystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<>
const SystemDescriptor& AsteroidsSystem::get_descriptor() const {
    return asteroids_descriptor;
}

template<>
const SystemDescriptor& LunarLanderSystem::get_descriptor() const {
    return lunar_lander_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::initialize() {
    using Traits = AtariVectorTraits<V>;
    printf("%s: Initializing system\n", Traits::NAME);

    register_board(&board_);

    // Factory-create all chips from the manifest
    board_.create_chips(&pins_);
    cpu_      = board_.template cpu<MOS6502>();
    vec_ram_  = board_.template find<RAMChip>(1);   // 2nd RAMChip = vector RAM
    vec_rom_  = board_.template find<ROMChip>(0);   // 1st ROMChip = vector ROM
    prog_rom_ = board_.template find<ROMChip>(1);   // 2nd ROMChip = program ROM

    // Wire MemoryBus page tables
    board_.apply(bus_);

    // Initialize CPU
    board_.cpu_chip()->init();
    board_.cpu_chip()->reset();

    // Initialize DVG
    dvg_.init();

    // Register all manifest-created chips for the Hardware menu
    register_bus_chips(board_);

    // Register DVG as a non-bus chip for the Hardware menu
    register_chip(&dvg_, "DVG", "DVG", "Video");

    // Video port — VectorVideoPort for signal-based rendering
    video_port_ = std::make_unique<VectorVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    // Wire DVG to the video stream
    dvg_.set_stream(&video_port_->stream());

    // Wire DVG vector memory — will be set properly after ROM load
    // (vector RAM + vector ROM concatenated into a contiguous 4 KB window)

    // Audio port (basic — discrete sound, not chip-driven)
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(atv::DEFAULT_SAMPLE_RATE, atv::DEFAULT_SAMPLE_RATE);

    // Default DIP switches (factory defaults)
    dsw1_ = 0x00;
    dsw2_ = 0x00;

    printf("%s: System initialized\n", Traits::NAME);
    return true;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::shutdown() {
    printf("%s: Shutting down\n", AtariVectorTraits<V>::NAME);
    System::shutdown();
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::reset() {
    printf("%s: Reset\n", AtariVectorTraits<V>::NAME);

    board_.reset_chips();

    if (board_.cpu_chip()) {
        board_.cpu_chip()->reset();
    }

    pins_ = MOS6502::default_bus_state();
    total_cycles_ = 0;

    dvg_.reset();
    nmi_counter_ = atv::NMI_PERIOD_CYCLES;
    nmi_pending_ = false;

    in0_ = 0xFF;
    in1_ = 0xFF;
    in2_ = 0xFF;
    thrust_ = 0x00;
    snd_latch_ = 0x00;
}

// ============================================================================
// EXECUTION
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::tick() {
    // CPU tick
    tick_cpu();

    // DVG tick — runs at the same frequency as the CPU
    dvg_.tick();

    // NMI timer
    if (nmi_counter_ > 0) {
        --nmi_counter_;
    } else {
        nmi_counter_ = atv::NMI_PERIOD_CYCLES;
        nmi_pending_ = true;
    }

    // Drive NMI pin
    if (nmi_pending_) {
        BUS_CLR_BIT(pins_, BUS_NMI_BIT);  // NMI is active-low
    } else {
        BUS_SET_BIT(pins_, BUS_NMI_BIT);
    }

    total_cycles_++;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::run_frame() {
    if (!video_port_) return;

    if (!system_ready_) {
        // No ROM loaded — emit random test vectors so the display isn't blank
        dvg_.generate_test_pattern();
        video_port_->swap_frame();
        return;
    }

    // Run one frame's worth of CPU cycles
    for (uint32_t i = 0; i < atv::CYCLES_PER_FRAME; ++i) {
        tick();
    }

    // Swap frame — produces FrameData with SignalType::Vector for the GPU
    video_port_->swap_frame();

    // Tick peripherals
    tick_peripherals();
}

// ============================================================================
// CPU TICK
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::tick_cpu() {
    if (!cpu_) return;

    pins_ = cpu_->tick<MOS6502::Phase::PHI2>(pins_);

    uint16_t addr = BUS_GET_ADDR(pins_);
    bool is_write = !BUS_GET_BIT(pins_, BUS_RW_BIT);

    // I/O region: $2000-$3FFF — manual dispatch (not on MemoryBus)
    if (addr >= 0x2000 && addr < 0x4000) {
        if (is_write) {
            pins_ = io_write(addr, BUS_GET_DATA(pins_), pins_);
        } else {
            pins_ = io_read(addr, pins_);
        }
    } else {
        // All other addresses: RAM, vector RAM/ROM, program ROM via MemoryBus
        pins_ = bus_.tick(pins_);
    }

    pins_ = cpu_->tick<MOS6502::Phase::PHI1>(pins_);
    cpu_->sample_nmi_pin(pins_);
}

// ============================================================================
// I/O READ
// ============================================================================

template<AtariVectorVariant V>
bus_state_t AtariVectorSystem<V>::io_read(uint16_t addr, bus_state_t pins) {
    uint8_t data = 0x00;

    if constexpr (V == AtariVectorVariant::ASTEROIDS) {
        // Asteroids I/O reads
        switch (addr & 0x2C01) {
            case 0x2000:  // IN0: coins, self-test, DVG halt
                data = in0_;
                // Inject DVG HALT status into bit 5
                if (dvg_.is_halted())
                    data |= atv::AST_IN0_HALT;
                else
                    data &= ~atv::AST_IN0_HALT;
                // 3 KHz clock on bit 0 — toggle based on cycle count
                if ((total_cycles_ / (atv::CPU_FREQ_HZ / 6000)) & 1)
                    data |= atv::AST_IN0_CLOCK;
                else
                    data &= ~atv::AST_IN0_CLOCK;
                break;
            case 0x2001:  // IN1: player 1 controls
                data = in1_;
                break;
            case 0x2400:  // (Asteroids doesn't use $2400)
                data = 0xFF;
                break;
            case 0x2800:  // DSW1
                // DIP switches are active-low; each D0-D7 maps to one switch
                data = dsw1_;
                break;
            case 0x2801:  // DSW2
                data = dsw2_;
                break;
            case 0x2C00:  // DVG halt status (bit 7)
                data = dvg_.is_halted() ? 0x80 : 0x00;
                break;
            default:
                data = 0xFF;
                break;
        }
    } else {
        // Lunar Lander I/O reads
        switch (addr & 0x2C01) {
            case 0x2000:  // IN0: coins, self-test, DVG halt, start/select
                data = in0_;
                if (dvg_.is_halted())
                    data |= atv::LL_IN0_HALT;
                else
                    data &= ~atv::LL_IN0_HALT;
                break;
            case 0x2001:  // IN1: game select, abort
                data = in1_;
                break;
            case 0x2400:  // Thrust lever ADC (4-bit value)
                data = thrust_;
                break;
            case 0x2800:  // DSW1
                data = dsw1_;
                break;
            case 0x2801:  // DSW2
                data = dsw2_;
                break;
            case 0x2C00:  // DVG halt status (bit 7)
                data = dvg_.is_halted() ? 0x80 : 0x00;
                break;
            default:
                data = 0xFF;
                break;
        }
    }

    BUS_SET_DATA(pins, data);
    return pins;
}

// ============================================================================
// I/O WRITE
// ============================================================================

template<AtariVectorVariant V>
bus_state_t AtariVectorSystem<V>::io_write(uint16_t addr, uint8_t data, bus_state_t pins) {
    // I/O writes are decode-by-address — the upper address bits select the register.
    // The data byte on the bus is sometimes ignored (trigger-only writes).

    uint16_t reg = addr & 0x3E00;

    switch (reg) {
        case atv::VGGO_ADDR: {
            // $3000 — VGGO: Start DVG vector state machine
            // DVG addresses $0000-$07FF = vector RAM, $0800-$0FFF = vector ROM.
            // Board allocates flat_mem_ contiguously in slot order, so
            // vector RAM (slot 1) is immediately followed by vector ROM (slot 2).
            if (vec_ram_) {
                dvg_.set_vector_memory(vec_ram_->data(),
                                       atv::VECRAM_SIZE + atv::VECROM_SIZE);
            }
            dvg_.trigger_go();
            break;
        }

        case atv::VGRST_ADDR:
            // $3200 — VGRST: Reset DVG
            dvg_.trigger_reset();
            break;

        case atv::WDCLR_ADDR:
            // $3400 — WD CLR: Watchdog clear (no-op in emulation)
            break;

        case atv::SND_BASE_ADDR:
            // $3600 — Sound triggers (game-specific discrete circuits)
            snd_latch_ = data;
            break;

        case 0x3800:
        case 0x3A00:
            // $3800-$3BFF — Additional sound / output latches
            // Game-specific; store for potential audio modeling
            break;

        case atv::COIN_CTR_ADDR:
            // $3C00 — Coin counter (no-op in emulation)
            break;

        case atv::NMI_ACK_ADDR:
            // $3E00 — NMI acknowledge: clears the NMI flip-flop
            nmi_pending_ = false;
            BUS_SET_BIT(pins, BUS_NMI_BIT);  // De-assert NMI (active-low: set = inactive)
            break;

        default:
            break;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::load_file(const char* filepath) {
    using Traits = AtariVectorTraits<V>;

    bool cold_boot = system_ready_;

    if (!cpu_) {
        if (!initialize()) return false;
    }

    printf("%s: Loading file: %s\n", Traits::NAME, filepath);

    // Read the ROM file
    size_t file_size = 0;
    uint8_t* file_data = vfs_read_file(filepath, &file_size);
    if (!file_data) {
        printf("%s: Failed to open file: %s\n", Traits::NAME, filepath);
        return false;
    }

    if (file_size == 0) {
        printf("%s: Empty file\n", Traits::NAME);
        free(file_data);
        return false;
    }

    printf("%s: ROM file is %zu bytes\n", Traits::NAME, file_size);

    // Determine ROM layout:
    // The file may contain:
    //   a) Just the program ROM
    //   b) Program ROM + vector ROM concatenated
    //   c) A combined ROM image with everything

    if (file_size >= Traits::PROGROM_ACTUAL + atv::VECROM_SIZE) {
        // File contains both program ROM and vector ROM
        // Layout: program ROM first, then vector ROM at the end.
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, Traits::PROGROM_ACTUAL);
        }
        if (vec_rom_) {
            std::memcpy(vec_rom_->data(), file_data + Traits::PROGROM_ACTUAL,
                        atv::VECROM_SIZE);
        }
    } else if (file_size >= Traits::PROGROM_ACTUAL) {
        // Just the program ROM — vector ROM must be loaded separately
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, Traits::PROGROM_ACTUAL);
        }
    } else {
        // Unknown size — load as much as fits into program ROM
        size_t to_copy = std::min(file_size, static_cast<size_t>(Traits::PROGROM_ACTUAL));
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, to_copy);
        }
    }

    free(file_data);

    // Set program title from filename
    const char* name = strrchr(filepath, '/');
    if (!name) name = strrchr(filepath, '\\');
    program_title_ = name ? (name + 1) : filepath;

    // Wire DVG to vector memory (vector RAM + vector ROM as contiguous view)
    if (vec_ram_) {
        dvg_.set_vector_memory(vec_ram_->data(), atv::VECRAM_SIZE + atv::VECROM_SIZE);
    }

    system_ready_ = true;
    reset();

    if (cold_boot && cpu_) {
        cpu_->set(REG_A, 0);
        cpu_->set(REG_X, 0);
        cpu_->set(REG_Y, 0);
    }

    printf("%s: ROM loaded, system ready\n", Traits::NAME);
    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::get_display_dimensions(int* width, int* height) const {
    if (width)  *width  = atv::DISPLAY_WIDTH;
    if (height) *height = atv::DISPLAY_HEIGHT;
}

// ============================================================================
// AUDIO
// ============================================================================

template<AtariVectorVariant V>
uint32_t AtariVectorSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    // Discrete sound — for now, output silence.
    // Future: model discrete sound circuits as CPU-driven DAC samples.
    if (buffer && max_samples > 0) {
        std::memset(buffer, 0, max_samples * sizeof(float));
    }
    return max_samples;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = sample_rate_hz;
}

// ============================================================================
// INPUT
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if constexpr (V == AtariVectorVariant::ASTEROIDS) {
        // Asteroids controls:
        //   Arrow keys = rotate left/right, thrust
        //   Space = fire
        //   H = hyperspace
        //   1/2 = 1P/2P start (active on coin IN0)
        //   5 = coin
        switch (key) {
            case SDLK_LEFT:
                if (pressed) in1_ &= ~atv::AST_IN1_ROT_LEFT;
                else         in1_ |=  atv::AST_IN1_ROT_LEFT;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ &= ~atv::AST_IN1_ROT_RIGHT;
                else         in1_ |=  atv::AST_IN1_ROT_RIGHT;
                break;
            case SDLK_UP:
                if (pressed) in1_ &= ~atv::AST_IN1_THRUST;
                else         in1_ |=  atv::AST_IN1_THRUST;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ &= ~atv::AST_IN1_FIRE;
                else         in1_ |=  atv::AST_IN1_FIRE;
                break;
            case SDLK_h:
                if (pressed) in1_ &= ~atv::AST_IN1_HYPERSPACE;
                else         in1_ |=  atv::AST_IN1_HYPERSPACE;
                break;
            case SDLK_1:
                // 1P start — directly on IN1 bit 7 is 2P start;
                // Asteroids doesn't have a dedicated 1P start bit — it uses
                // hyperspace or a separate mechanism.  Map to 2P start for now.
                if (pressed) in1_ &= ~atv::AST_IN1_2P_START;
                else         in1_ |=  atv::AST_IN1_2P_START;
                break;
            case SDLK_5:
                // Coin insert
                if (pressed) in0_ &= ~atv::AST_IN0_COIN_C;
                else         in0_ |=  atv::AST_IN0_COIN_C;
                break;
            default:
                break;
        }
    } else {
        // Lunar Lander controls:
        //   Up/Down = thrust (adjusts ADC value)
        //   Left/Right = rotate
        //   Space = abort
        //   1 = start
        //   5 = coin
        switch (key) {
            case SDLK_UP:
                if (pressed) {
                    thrust_ = std::min(255, thrust_ + 32);
                }
                break;
            case SDLK_DOWN:
                if (pressed) {
                    thrust_ = std::max(0, thrust_ - 32);
                }
                break;
            case SDLK_SPACE:
                // Abort button
                if (pressed) in1_ &= ~0x01;
                else         in1_ |=  0x01;
                break;
            case SDLK_1:
                if (pressed) in0_ &= ~atv::LL_IN0_START;
                else         in0_ |=  atv::LL_IN0_START;
                break;
            case SDLK_5:
                if (pressed) in0_ &= ~atv::LL_IN0_COIN;
                else         in0_ |=  atv::LL_IN0_COIN;
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// GUI
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    // Future: DIP switch editor, display options
#endif
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    // Future: DIP switch configuration, phosphor color selection
#endif
}

// ============================================================================
// SPEED CONTROL
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATION
// ============================================================================

template class AtariVectorSystem<AtariVectorVariant::ASTEROIDS>;
template class AtariVectorSystem<AtariVectorVariant::LUNAR_LANDER>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(asteroids_descriptor, [] { return std::make_unique<AsteroidsSystem>(); });
REGISTER_SYSTEM(lunar_lander_descriptor, [] { return std::make_unique<LunarLanderSystem>(); });
