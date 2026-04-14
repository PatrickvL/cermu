/*
 * atari8_system.cpp — Atari 400/800/XL/XE system implementation
 *
 * Tick loop:
 *   ANTIC drives the display list DMA and scanline timing.
 *   GTIA generates pixel colors and handles player/missile composition.
 *   POKEY generates audio and handles keyboard/serial.
 *   PIA handles joystick reading and memory banking (XL/XE).
 *   CPU runs at 1.79 MHz with ANTIC cycle-stealing (via WSYNC/DMA).
 *
 * I/O map (memory-mapped):
 *   $D000–$D0FF : GTIA (32 registers, mirrored)
 *   $D200–$D2FF : POKEY (16 registers, mirrored)
 *   $D300–$D3FF : PIA (4 registers, mirrored)
 *   $D400–$D4FF : ANTIC (16 registers, mirrored)
 */

#include "core/cermu.hpp"
#include "systems/atari8/atari8_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_load_helpers.hpp"
#include "core/vfs/vfs.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<Atari8Variant V>
static HardwareTraits create_atari8_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = atari8_constants::DISPLAY_WIDTH;
    traits.display.native_height   = atari8_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = atari8_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = atari8_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = gtia_palette::PALETTE_SIZE;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = atari8_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "POKEY";

    traits.timing.cpu_frequency_hz   = atari8_constants::CPU_FREQ_NTSC;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = atari8_constants::CYCLES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor a800_descriptor = {
    "Atari 800", "A800",
    "Atari 800 — 6502C, ANTIC, GTIA, POKEY (1979)",
    "atari800", {"A800", "Atari 800", "Atari800"},
    nullptr,
    create_atari8_hardware_traits<Atari8Variant::A800>(),
    nullptr,
    "Atari", 1979, "MOS 6502C", SystemType::Home
};

static SystemDescriptor a800xl_descriptor = {
    "Atari 800XL", "800XL",
    "Atari 800XL — 6502C, ANTIC, GTIA, POKEY, 64KB (1983)",
    "atari800", {"Atari 800XL", "800XL", "A800XL"},
    nullptr,
    create_atari8_hardware_traits<Atari8Variant::A800XL>(),
    nullptr,
    "Atari", 1983, "MOS 6502C", SystemType::Home
};

static SystemDescriptor a130xe_descriptor = {
    "Atari 130XE", "130XE",
    "Atari 130XE — 6502C, ANTIC, GTIA, POKEY, 128KB banked (1985)",
    "atari800", {"Atari 130XE", "130XE", "A130XE"},
    nullptr,
    create_atari8_hardware_traits<Atari8Variant::A130XE>(),
    nullptr,
    "Atari", 1985, "MOS 6502C", SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<Atari8Variant V>
Atari8System<V>::Atari8System()
    : System()
    , pins_(ATARI8_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_atari8_hardware_traits<V>();
}

template<Atari8Variant V>
Atari8System<V>::~Atari8System() = default;

template<Atari8Variant V>
const SystemDescriptor& Atari8System<V>::get_descriptor() const {
    if constexpr (V == Atari8Variant::A800)   return a800_descriptor;
    if constexpr (V == Atari8Variant::A800XL) return a800xl_descriptor;
    return a130xe_descriptor;
}

template<Atari8Variant V>
bool Atari8System<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<Atari8Variant V>
bool Atari8System<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<Atari8Variant V>
bool Atari8System<V>::initialize() {
    const char* variant_name =
        V == Atari8Variant::A800 ? "Atari 800" :
        V == Atari8Variant::A800XL ? "Atari 800XL" : "Atari 130XE";
    log_info("Atari8: Initializing system (%s)\n", variant_name);

    register_board(&board_);
    bind_all(board_, board_.components_, kAtari8Manifest<V>);

    port_manifest_       = kAtari8Manifest<V>.port_slots;
    port_manifest_count_ = kAtari8Manifest<V>.port_count;
    board_.apply(bus_);

    configure_bus_memory_map();

    pins_ = board_.cpu.init();
    board_.antic.reset();
    board_.gtia.reset();
    board_.pokey.reset();
    board_.pia.init();

    board_.pokey.set_clock_frequency(atari8_constants::CPU_FREQ_NTSC);
    board_.pokey.set_audio_sample_rate(audio_sample_rate_);

    // Wire POKEY keyboard callbacks
    board_.pokey.keyboard_read_callback = [](void* ctx) -> uint8_t {
        auto* sys = static_cast<Atari8System*>(ctx);
        return sys->pokey_key_code_;
    };
    board_.pokey.keyboard_read_context = this;
    board_.pokey.keyboard_pressed_callback = [](void* ctx) -> bool {
        auto* sys = static_cast<Atari8System*>(ctx);
        return sys->pokey_key_pressed_;
    };
    board_.pokey.keyboard_pressed_context = this;

    if (!load_roms()) {
        log_info("Atari8: Warning — ROM(s) not loaded\n");
    }

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(atari8_constants::DEFAULT_SAMPLE_RATE,
                           atari8_constants::DEFAULT_SAMPLE_RATE);
    board_.pokey.set_audio_port(audio_port_.get());

    system_ready_ = true;
    log_info("Atari8: System initialized\n");
    return true;
}

template<Atari8Variant V>
void Atari8System<V>::shutdown() {
    system_ready_ = false;
}

template<Atari8Variant V>
void Atari8System<V>::reset() {
    board_.reset_chips();
    pins_ = board_.cpu.reset(pins_);
    frame_cycle_counter_ = 0;
}

// ============================================================================
// EXECUTION
// ============================================================================

template<Atari8Variant V>
void Atari8System<V>::tick() {
    // ANTIC tick — drives display list, DMA steal, scanline counter
    bus_state_t antic_bus = 0;
    antic_bus = board_.antic.tick(antic_bus);

    // ANTIC NMI → CPU NMI pin
    if (BUS_GET_BIT(antic_bus, BUS_NMI_BIT) == 0)
        BUS_CLR_BIT(pins_, BUS_NMI_BIT);
    else
        BUS_SET_BIT(pins_, BUS_NMI_BIT);

    // POKEY tick — sound + IRQ generation
    bus_state_t pokey_bus = 0;
    pokey_bus = board_.pokey.tick(pokey_bus);

    // CPU tick (may be halted by ANTIC WSYNC)
    if (!board_.antic.wsync_pending_) {
        pins_ = board_.cpu.template tick<MOS6502::Phase::PHI2>(pins_);
        pins_ = bus_.tick(pins_);
        pins_ = board_.cpu.template tick<MOS6502::Phase::PHI1>(pins_);
    }

    frame_cycle_counter_++;
    total_cycles_++;
}

template<Atari8Variant V>
void Atari8System<V>::run_frame() {
    if (!video_port_) return;
    uint32_t start = total_cycles_;
    uint32_t cpf = atari8_constants::CYCLES_PER_FRAME_NTSC;
    while (total_cycles_ - start < cpf) {
        tick();
    }
}

// ============================================================================
// BUS / FILE
// ============================================================================

template<Atari8Variant V>
void Atari8System<V>::configure_bus_memory_map() {
    board_.apply(bus_);
}

template<Atari8Variant V>
bool Atari8System<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    uint8_t* cart = board_.cart.data();
    if (!cart) return false;

    // Atari 8-bit cartridges: 8KB or 16KB ROM, loaded at $8000 or $A000
    if (!load_raw_rom_mirrored(filepath, cart, 0x4000, 0x8000,
                               get_descriptor().name, program_title_))
        return false;

    reset();
    return true;
}

// ============================================================================
// AUDIO
// ============================================================================

template<Atari8Variant V>
uint32_t Atari8System<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return board_.pokey.audio_read(buffer, max_samples);
}

template<Atari8Variant V>
void Atari8System<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    board_.pokey.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT — console keys + joystick (via PIA and GTIA)
// ============================================================================

template<Atari8Variant V>
void Atari8System<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Console keys (active-low in GTIA CONSOL register)
    switch (key) {
        case SDLK_F2:  // Start
            if (pressed) board_.gtia.console_keys_ &= ~0x01;
            else         board_.gtia.console_keys_ |=  0x01;
            break;
        case SDLK_F3:  // Select
            if (pressed) board_.gtia.console_keys_ &= ~0x02;
            else         board_.gtia.console_keys_ |=  0x02;
            break;
        case SDLK_F4:  // Option
            if (pressed) board_.gtia.console_keys_ &= ~0x04;
            else         board_.gtia.console_keys_ |=  0x04;
            break;
        default: break;
    }

    // POKEY keyboard: SDL keycode → Atari scancode (KBCODE register)
    // Atari scancodes: bits [5:0] = key, bit 6 = CTRL, bit 7 = SHIFT
    // We map the base key; CTRL/SHIFT modify the code.
    uint8_t scancode = 0xFF;
    switch (key) {
        case SDLK_l:         scancode = 0x00; break;
        case SDLK_j:         scancode = 0x01; break;
        case SDLK_SEMICOLON: scancode = 0x02; break;
        case SDLK_k:         scancode = 0x05; break;
        case SDLK_EQUALS:    scancode = 0x06; break;  // +/= key
        case SDLK_BACKQUOTE: scancode = 0x07; break;  // * key
        case SDLK_o:         scancode = 0x08; break;
        case SDLK_p:         scancode = 0x0A; break;
        case SDLK_u:         scancode = 0x0B; break;
        case SDLK_RETURN:    scancode = 0x0C; break;
        case SDLK_i:         scancode = 0x0D; break;
        case SDLK_MINUS:     scancode = 0x0E; break;
        case SDLK_v:         scancode = 0x10; break;
        case SDLK_c:         scancode = 0x12; break;
        case SDLK_b:         scancode = 0x15; break;
        case SDLK_x:         scancode = 0x16; break;
        case SDLK_z:         scancode = 0x17; break;
        case SDLK_4:         scancode = 0x18; break;
        case SDLK_3:         scancode = 0x1A; break;
        case SDLK_6:         scancode = 0x1B; break;
        case SDLK_ESCAPE:    scancode = 0x1C; break;
        case SDLK_5:         scancode = 0x1D; break;
        case SDLK_2:         scancode = 0x1E; break;
        case SDLK_1:         scancode = 0x1F; break;
        case SDLK_COMMA:     scancode = 0x20; break;
        case SDLK_SPACE:     scancode = 0x21; break;
        case SDLK_PERIOD:    scancode = 0x22; break;
        case SDLK_n:         scancode = 0x23; break;
        case SDLK_m:         scancode = 0x25; break;
        case SDLK_SLASH:     scancode = 0x26; break;
        case SDLK_r:         scancode = 0x28; break;
        case SDLK_e:         scancode = 0x2A; break;
        case SDLK_y:         scancode = 0x2B; break;
        case SDLK_TAB:       scancode = 0x2C; break;
        case SDLK_t:         scancode = 0x2D; break;
        case SDLK_w:         scancode = 0x2E; break;
        case SDLK_q:         scancode = 0x2F; break;
        case SDLK_9:         scancode = 0x30; break;
        case SDLK_0:         scancode = 0x32; break;
        case SDLK_7:         scancode = 0x33; break;
        case SDLK_BACKSPACE: scancode = 0x34; break;  // DELETE key
        case SDLK_8:         scancode = 0x35; break;
        case SDLK_LESS:      scancode = 0x36; break;  // < key
        case SDLK_GREATER:   scancode = 0x37; break;  // > key
        case SDLK_f:         scancode = 0x38; break;
        case SDLK_h:         scancode = 0x39; break;
        case SDLK_d:         scancode = 0x3A; break;
        case SDLK_g:         scancode = 0x3D; break;
        case SDLK_s:         scancode = 0x3E; break;
        case SDLK_a:         scancode = 0x3F; break;
        case SDLK_CAPSLOCK:  scancode = 0x3C; break;
        default: break;
    }

    if (scancode != 0xFF) {
        if (pressed) {
            pokey_key_code_ = scancode;
            pokey_key_pressed_ = true;
        } else {
            pokey_key_pressed_ = false;
        }
    }
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<Atari8Variant V>
bool Atari8System<V>::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("atari800", rom_root, sizeof(rom_root))) {
        log_info("Atari8: Could not find ROM root folder\n");
        return false;
    }
    return board_.load_roms(rom_root, "Atari 8-bit");
}

// ============================================================================
// TEMPLATE INSTANTIATION
// ============================================================================

template class Atari8System<Atari8Variant::A800>;
template class Atari8System<Atari8Variant::A800XL>;
template class Atari8System<Atari8Variant::A130XE>;

REGISTER_SYSTEM(a800_descriptor, [] {
    return std::make_unique<Atari8System<Atari8Variant::A800>>();
});
REGISTER_SYSTEM(a800xl_descriptor, [] {
    return std::make_unique<Atari8System<Atari8Variant::A800XL>>();
});
REGISTER_SYSTEM(a130xe_descriptor, [] {
    return std::make_unique<Atari8System<Atari8Variant::A130XE>>();
});
