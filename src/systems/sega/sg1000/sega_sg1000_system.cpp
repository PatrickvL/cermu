/*
 * sega_sg1000_system.cpp — Sega SG-1000 / SC-3000 system implementation
 *
 * Tick loop:
 *   Z80 one T-state per tick(), VDP dot clock, SN76489 at CPU/16.
 *
 * I/O map:
 *   $7E write: SN76489 data
 *   $BE read/write: VDP data
 *   $BF read: VDP status; write: VDP control
 *   $DC read: I/O port A (joystick 1)
 *   $DD read: I/O port B (joystick 2 + misc)
 */

#include "systems/sega/sg1000/sega_sg1000_system.hpp"
#include "core/system_registry.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<SG1000Variant V>
static HardwareTraits create_sg1000_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = sg1000_constants::DISPLAY_WIDTH;
    traits.display.native_height   = sg1000_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = sg1000_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = sg1000_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = sg1000_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "SN76489";

    traits.timing.cpu_frequency_hz   = sg1000_constants::CPU_FREQ_HZ;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = sg1000_constants::TSTATES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor sg1000_descriptor = {
    "Sega SG-1000", "SG-1000",
    "Sega SG-1000 — Z80A, TMS9918A, SN76489 (1983)",
    "sega_sg1000", {"SG1000", "SG-1000"},
    nullptr,
    create_sg1000_hardware_traits<SG1000Variant::SG1000>(),
    nullptr,
    "Sega", 1983, "Z80A", SystemType::Console
};

static SystemDescriptor sc3000_descriptor = {
    "Sega SC-3000", "SC-3000",
    "Sega SC-3000 — Z80A, TMS9918A, SN76489, keyboard (1983)",
    "sega_sg1000", {"SC3000", "SC-3000"},
    nullptr,
    create_sg1000_hardware_traits<SG1000Variant::SC3000>(),
    nullptr,
    "Sega", 1983, "Z80A", SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<SG1000Variant V>
SegaSG1000System<V>::SegaSG1000System()
    : System()
    , pins_(SG1000_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_sg1000_hardware_traits<V>();
}

template<SG1000Variant V>
SegaSG1000System<V>::~SegaSG1000System() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<SG1000Variant V>
const SystemDescriptor& SegaSG1000System<V>::get_descriptor() const {
    if constexpr (V == SG1000Variant::SG1000) return sg1000_descriptor;
    else return sc3000_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<SG1000Variant V>
bool SegaSG1000System<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<SG1000Variant V>
bool SegaSG1000System<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<SG1000Variant V>
bool SegaSG1000System<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    board_.bind_chip(board_.template find_index<TMS9918A>(), &board_.video());
    board_.create_chips(&pins_);
    board_.apply(bus_);

    configure_bus_memory_map();

    pins_ = board_.cpu().init();
    board_.sound().init();

    board_.sound().set_clock_frequency(sg1000_constants::CPU_FREQ_HZ / 16);
    board_.sound().set_audio_sample_rate(audio_sample_rate_);

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.video().set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(sg1000_constants::DEFAULT_SAMPLE_RATE,
                           sg1000_constants::DEFAULT_SAMPLE_RATE);
    board_.sound().set_audio_port(audio_port_.get());

    system_ready_ = true;
    printf("%s: System initialized (RAM: %dB)\n", Traits::name, Traits::ram_size);
    return true;
}

template<SG1000Variant V>
void SegaSG1000System<V>::shutdown() {
    system_ready_ = false;
}

template<SG1000Variant V>
void SegaSG1000System<V>::reset() {
    board_.reset_chips();
    pins_ = board_.cpu().reset(pins_);
    frame_tstate_counter_ = 0;
    joypad_state_ = 0xFF;
}

// ============================================================================
// EXECUTION
// ============================================================================

template<SG1000Variant V>
void SegaSG1000System<V>::tick() {

    // VDP tick
    bus_state_t vdp_bus = 0;
    vdp_bus = board_.video().tick(vdp_bus);

    // VDP interrupt → Z80 INT
    if (BUS_GET_BIT(vdp_bus, BUS_IRQ_BIT) == 0)
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    else
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);

    // CPU tick
    pins_ = board_.cpu().tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // PSG tick (CPU/16)
    if ((frame_tstate_counter_ & 0x0F) == 0) {
        board_.sound().tick();
    }

    frame_tstate_counter_++;
    total_cycles_++;
}

template<SG1000Variant V>
void SegaSG1000System<V>::run_frame() {
    if (!video_port_) return;
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<SG1000Variant V>
void SegaSG1000System<V>::configure_bus_memory_map() {
    board_.apply(bus_);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<SG1000Variant V>
bus_state_t SegaSG1000System<V>::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // SN76489 write ($7E-$7F area, active on even)
    if (!is_read && (port & 0xFE) == sg1000_constants::PSG_PORT) {
        board_.sound().write(BUS_GET_DATA(pins));
        return pins;
    }

    // VDP data port ($BE)
    if (port == sg1000_constants::VDP_DATA_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 0);  // port 0 = data
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9918A::port_read(&board_.video(), vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9918A::port_write(&board_.video(), vdp_bus);
        }
        return pins;
    }

    // VDP control port ($BF)
    if (port == sg1000_constants::VDP_CTRL_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 1);  // port 1 = control/status
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9918A::port_read(&board_.video(), vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9918A::port_write(&board_.video(), vdp_bus);
        }
        return pins;
    }

    // I/O port A ($DC) — joystick 1
    if (is_read && port == sg1000_constants::IO_PORT_A) {
        BUS_SET_DATA(pins, joypad_state_);
        return pins;
    }

    // I/O port B ($DD) — joystick 2
    if (is_read && port == sg1000_constants::IO_PORT_B) {
        BUS_SET_DATA(pins, 0xFF);
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<SG1000Variant V>
bool SegaSG1000System<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;
    // TODO: Load .sg cartridge ROM
    printf("%s: File loading not yet implemented: %s\n", Traits::name, filepath);
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<SG1000Variant V>
uint32_t SegaSG1000System<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return board_.sound().audio_read(buffer, max_samples);
}

template<SG1000Variant V>
void SegaSG1000System<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    board_.sound().set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

template<SG1000Variant V>
void SegaSG1000System<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // SG-1000 joypad: Up/Down/Left/Right/TL(fire1)/TR(fire2)
    // Bits 0-5 active-low in joypad_state_ (bit 0=Up, 1=Down, 2=Left, 3=Right, 4=TL, 5=TR)
    struct JoyMapping { SDL_Keycode sdl_key; int bit; };
    static constexpr JoyMapping mappings[] = {
        { SDLK_UP,    0 }, { SDLK_DOWN,  1 },
        { SDLK_LEFT,  2 }, { SDLK_RIGHT, 3 },
        { SDLK_z,     4 }, // TL (fire 1)
        { SDLK_x,     5 }, // TR (fire 2)
    };

    for (const auto& m : mappings) {
        if (m.sdl_key == key) {
            if (pressed)
                joypad_state_ &= ~(1 << m.bit);
            else
                joypad_state_ |= (1 << m.bit);
        }
    }
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class SegaSG1000System<SG1000Variant::SG1000>;
template class SegaSG1000System<SG1000Variant::SC3000>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(sg1000_descriptor, [] {
    return std::make_unique<SegaSG1000System<SG1000Variant::SG1000>>();
});

REGISTER_SYSTEM(sc3000_descriptor, [] {
    return std::make_unique<SegaSG1000System<SG1000Variant::SC3000>>();
});
