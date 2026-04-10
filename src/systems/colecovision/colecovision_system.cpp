/*
 * colecovision_system.cpp — ColecoVision system implementation
 *
 * Tick loop:
 *   Z80 one T-state per tick(), VDP dot clock, SN76489 at CPU/16.
 *
 * I/O map:
 *   $A0-$BF even: VDP data read/write
 *   $A0-$BF odd:  VDP control/status
 *   $80-$9F even write: controller mode 0 select
 *   $C0-$DF even write: controller mode 1 select
 *   $E0-$FF even read:  controller data
 *   $E0-$FF odd write:  SN76489 data
 */

#include "core/cermu.hpp"
#include "systems/colecovision/colecovision_system.hpp"
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

static HardwareTraits create_coleco_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = coleco_constants::DISPLAY_WIDTH;
    traits.display.native_height   = coleco_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = coleco_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = coleco_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = coleco_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "SN76489";

    traits.timing.cpu_frequency_hz   = coleco_constants::CPU_FREQ_HZ;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = coleco_constants::TSTATES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor coleco_descriptor = {
    "ColecoVision", "ColecoVision",
    "ColecoVision — Z80A, TMS9918A, SN76489 (1982)",
    "colecovision", {"ColecoVision", "Coleco", "CV"},
    nullptr,
    create_coleco_hardware_traits(),
    nullptr,
    "Coleco", 1982, z80::ZilogZ80ATraits.display_name, SystemType::Console
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ColecoVisionSystem::ColecoVisionSystem()
    : System()
    , pins_(COLECO_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_coleco_hardware_traits();
}

ColecoVisionSystem::~ColecoVisionSystem() = default;

const SystemDescriptor& ColecoVisionSystem::get_descriptor() const {
    return coleco_descriptor;
}

bool ColecoVisionSystem::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool ColecoVisionSystem::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

bool ColecoVisionSystem::initialize() {
    log_info("ColecoVision: Initializing system\n");
    register_board(&board_);

    bind_all(board_, board_.components_, kColecoManifest);

    // Set port manifest for default peripheral attachment.
    port_manifest_       = kColecoManifest.port_slots;
    port_manifest_count_ = kColecoManifest.port_count;
    board_.apply(bus_);

    configure_bus_memory_map();

    pins_ = board_.z80.init();
    board_.psg.init();

    board_.psg.set_clock_frequency(coleco_constants::CPU_FREQ_HZ / 16);
    board_.psg.set_audio_sample_rate(audio_sample_rate_);

    if (!load_roms()) {
        log_info("ColecoVision: Warning — BIOS ROM not loaded\n");
    }

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdp.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(coleco_constants::DEFAULT_SAMPLE_RATE,
                           coleco_constants::DEFAULT_SAMPLE_RATE);
    board_.psg.set_audio_port(audio_port_.get());

    system_ready_ = true;
    log_info("ColecoVision: System initialized\n");
    return true;
}

void ColecoVisionSystem::shutdown() {
    system_ready_ = false;
}

void ColecoVisionSystem::reset() {
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    frame_tstate_counter_ = 0;
    ctrl_mode_ = 0;
    ctrl1_joystick_ = 0x7F;
    ctrl1_keypad_ = 0x0F;
}

// ============================================================================
// EXECUTION
// ============================================================================

void ColecoVisionSystem::tick() {
    // VDP tick
    bus_state_t vdp_bus = 0;
    vdp_bus = board_.vdp.tick(vdp_bus);

    if (BUS_GET_BIT(vdp_bus, BUS_IRQ_BIT) == 0)
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    else
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);

    // CPU tick
    pins_ = board_.z80.tick(pins_);

    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // PSG tick (CPU/16)
    if ((frame_tstate_counter_ & 0x0F) == 0) {
        board_.psg.tick();
    }

    frame_tstate_counter_++;
    total_cycles_++;
}

void ColecoVisionSystem::run_frame() {
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

void ColecoVisionSystem::configure_bus_memory_map() {
    board_.apply(bus_);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

bus_state_t ColecoVisionSystem::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // VDP data port (even addresses $A0-$BE)
    if ((port & 0xE1) == 0xA0) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 0);  // port 0 = data
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9918A::port_read(&board_.vdp, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9918A::port_write(&board_.vdp, vdp_bus);
        }
        return pins;
    }

    // VDP control port (odd addresses $A1-$BF)
    if ((port & 0xE1) == 0xA1) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 1);  // port 1 = control/status
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9918A::port_read(&board_.vdp, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9918A::port_write(&board_.vdp, vdp_bus);
        }
        return pins;
    }

    // Controller mode select
    if (!is_read && (port & 0xE1) == 0x80) {
        ctrl_mode_ = 0;  // Joystick mode
        return pins;
    }
    if (!is_read && (port & 0xE1) == 0xC0) {
        ctrl_mode_ = 1;  // Keypad mode
        return pins;
    }

    // Controller read (even addresses $E0-$FE)
    if (is_read && (port & 0xE1) == 0xE0) {
        if (ctrl_mode_ == 0)
            BUS_SET_DATA(pins, ctrl1_joystick_);
        else
            BUS_SET_DATA(pins, ctrl1_keypad_);
        return pins;
    }

    // SN76489 write (odd addresses $E1-$FF)
    if (!is_read && (port & 0xE1) == 0xE1) {
        board_.psg.write(BUS_GET_DATA(pins));
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

bool ColecoVisionSystem::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    uint8_t* cart = board_.cart.data();
    if (!cart) return false;

    // ColecoVision cartridges are raw ROM images, up to 32 KB.
    // Smaller ROMs are mirrored within the 32 KB window ($8000-$FFFF).
    if (!load_raw_rom_mirrored(filepath, cart, 0x8000, 0x8000,
                               get_descriptor().name, program_title_))
        return false;

    reset();
    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

uint32_t ColecoVisionSystem::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return board_.psg.audio_read(buffer, max_samples);
}

void ColecoVisionSystem::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    board_.psg.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

void ColecoVisionSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // ColecoVision controller: joystick + 2 fire buttons + 12-key keypad
    // Joystick mode (ctrl_mode_=0): 
    //   bit 0=Up, 1=Right, 2=Down, 3=Left, 6=Fire1(L), ~bit6=Fire2(R) via separate pin
    struct JoyMapping { SDL_Keycode sdl_key; int bit; };
    static constexpr JoyMapping joy_mappings[] = {
        { SDLK_UP,    0 }, { SDLK_RIGHT, 1 },
        { SDLK_DOWN,  2 }, { SDLK_LEFT,  3 },
        { SDLK_z,     6 }, // Fire 1 (left side button)
    };

    for (const auto& m : joy_mappings) {
        if (m.sdl_key == key) {
            if (pressed) ctrl1_joystick_ &= ~(1 << m.bit);
            else         ctrl1_joystick_ |= (1 << m.bit);
        }
    }

    // Fire 2 (right side button) — active-low on bit 7
    if (key == SDLK_x) {
        if (pressed) ctrl1_joystick_ &= ~(1 << 7);
        else         ctrl1_joystick_ |= (1 << 7);
    }

    // 12-key keypad (active when ctrl_mode_=1, active-low)
    // Keypad encoding in bits [3:0] of ctrl1_keypad_
    //   0=$0E, 1=$0D, 2=$07, 3=$0C, 4=$02, 5=$03, 6=$0E (dup), 7=$05,
    //   8=$01, 9=$0B, *=$09, #=$06
    struct KeypadMapping { SDL_Keycode sdl_key; uint8_t code; };
    static constexpr KeypadMapping kp_mappings[] = {
        { SDLK_KP_0, 0x05 }, { SDLK_KP_1, 0x02 }, { SDLK_KP_2, 0x08 },
        { SDLK_KP_3, 0x03 }, { SDLK_KP_4, 0x0D }, { SDLK_KP_5, 0x0C },
        { SDLK_KP_6, 0x01 }, { SDLK_KP_7, 0x0A }, { SDLK_KP_8, 0x0E },
        { SDLK_KP_9, 0x09 }, { SDLK_KP_MULTIPLY, 0x06 }, { SDLK_KP_ENTER, 0x0B },
    };

    for (const auto& m : kp_mappings) {
        if (m.sdl_key == key) {
            if (pressed) ctrl1_keypad_ = m.code;
            else         ctrl1_keypad_ = 0x0F;  // No key
        }
    }
}

// ============================================================================
// ROM LOADING
// ============================================================================

bool ColecoVisionSystem::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("colecovision", rom_root, sizeof(rom_root))) {
        log_info("ColecoVision: Could not find ROM root folder\n");
        return false;
    }
    return board_.load_roms(rom_root, "ColecoVision");
}

REGISTER_SYSTEM(coleco_descriptor, [] {
    return std::make_unique<ColecoVisionSystem>();
});
