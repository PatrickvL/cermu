/*
 * sega_sms_system.cpp — Sega Master System system implementation
 *
 * Tick loop:
 *   Z80 one T-state per tick(), VDP dot clock, SN76489 at CPU/16.
 *
 * I/O map:
 *   $3E write: Memory control
 *   $3F write: I/O port control
 *   $7E read:  V-counter; write: SN76489
 *   $7F read:  H-counter; write: SN76489
 *   $BE read/write: VDP data
 *   $BF read: VDP status; write: VDP control
 *   $DC read: I/O port A (joypad 1)
 *   $DD read: I/O port B (joypad 2 + misc)
 *
 * Mapper:
 *   Writes to $FFFC-$FFFF control cartridge ROM banking.
 */

#include "core/cermu.hpp"
#include "systems/sega/sms/sega_sms_system.hpp"
#include "core/system_registry.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

static HardwareTraits create_sms_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = sms_constants::DISPLAY_WIDTH;
    traits.display.native_height   = sms_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = sms_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = sms_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 64;  // 32-entry CRAM × 2 (sprites+bg)

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = sms_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "SN76489 (Sega PSG)";

    traits.timing.cpu_frequency_hz   = sms_constants::CPU_FREQ_HZ_NTSC;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = sms_constants::TSTATES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor sms_descriptor = {
    "Sega Master System", "SMS",
    "Sega Master System — Z80A, 315-5124 VDP, SN76489 PSG (1986)",
    "sega_sms", {"SMS", "MasterSystem", "SegaMasterSystem", "Mark III", "MarkIII"},
    nullptr,
    create_sms_hardware_traits(),
    nullptr,
    "Sega", 1986, z80::ZilogZ80ATraits.display_name, SystemType::Console
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

SegaSMSSystem::SegaSMSSystem()
    : System()
    , pins_(SMS_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_sms_hardware_traits();
}

SegaSMSSystem::~SegaSMSSystem() = default;

const SystemDescriptor& SegaSMSSystem::get_descriptor() const {
    return sms_descriptor;
}

bool SegaSMSSystem::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool SegaSMSSystem::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

bool SegaSMSSystem::initialize() {
    log_info("Sega Master System: Initializing system\n");
    register_board(&board_);

    bind_all(board_, board_.components_, kSMSManifest);

    // Set port manifest for default peripheral attachment.
    port_manifest_       = kSMSManifest.port_slots;
    port_manifest_count_ = kSMSManifest.port_count;
    board_.apply(bus_);

    configure_bus_memory_map();

    pins_ = board_.z80.init();
    board_.psg.init();

    board_.psg.set_clock_frequency(sms_constants::CPU_FREQ_HZ_NTSC / 16);
    board_.psg.set_audio_sample_rate(audio_sample_rate_);

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdp.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(sms_constants::DEFAULT_SAMPLE_RATE,
                           sms_constants::DEFAULT_SAMPLE_RATE);
    board_.psg.set_audio_port(audio_port_.get());

    system_ready_ = true;
    log_info("Sega Master System: System initialized\n");
    return true;
}

void SegaSMSSystem::shutdown() {
    system_ready_ = false;
}

void SegaSMSSystem::reset() {
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    frame_tstate_counter_ = 0;
    mapper_ctrl_ = 0;
    mapper_bank_[0] = 0; mapper_bank_[1] = 1; mapper_bank_[2] = 2;
    joypad1_state_ = 0xFF;
    joypad2_state_ = 0xFF;
    configure_bus_memory_map();
}

// ============================================================================
// EXECUTION
// ============================================================================

void SegaSMSSystem::tick() {

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

        // Check for mapper register writes
        if (!BUS_GET_BIT(pins_, BUS_RW_BIT)) {
            uint16_t addr = BUS_GET_ADDR(pins_);
            if (addr >= sms_constants::MAPPER_CTRL && addr <= sms_constants::MAPPER_BANK2) {
                uint8_t data = BUS_GET_DATA(pins_);
                switch (addr) {
                    case sms_constants::MAPPER_CTRL:  mapper_ctrl_ = data; break;
                    case sms_constants::MAPPER_BANK0: mapper_bank_[0] = data; break;
                    case sms_constants::MAPPER_BANK1: mapper_bank_[1] = data; break;
                    case sms_constants::MAPPER_BANK2: mapper_bank_[2] = data; break;
                }
                update_mapper();
            }
        }
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

void SegaSMSSystem::run_frame() {
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

void SegaSMSSystem::configure_bus_memory_map() {
    board_.apply(bus_);
    update_mapper();
}

void SegaSMSSystem::update_mapper() {
    // Sega mapper: three 16KB ROM bank slots controlled by $FFFD/$FFFE/$FFFF.
    // Bank 0 ($0000-$3FFF), Bank 1 ($4000-$7FFF), Bank 2 ($8000-$BFFF).
    // Bank indices wrap to the actual ROM size.
    const uint8_t mask = rom_banks_ - 1;  // power-of-2 wrap
    board_.select_bank_at(bus_, 0, kCartRomSlot, mapper_bank_[0] & mask, 0x00);
    board_.select_bank_at(bus_, 0, kCartRomSlot, mapper_bank_[1] & mask, 0x40);
    board_.select_bank_at(bus_, 0, kCartRomSlot, mapper_bank_[2] & mask, 0x80);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

bus_state_t SegaSMSSystem::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // SN76489 write ($7E-$7F)
    if (!is_read && (port & 0xFE) == sms_constants::PSG_PORT) {
        board_.psg.write(BUS_GET_DATA(pins));
        return pins;
    }

    // VDP data port ($BE)
    if (port == sms_constants::VDP_DATA_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 0);  // port 0 = data
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = SEGA_315_5124::port_read(&board_.vdp, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            SEGA_315_5124::port_write(&board_.vdp, vdp_bus);
        }
        return pins;
    }

    // VDP control port ($BF)
    if (port == sms_constants::VDP_CTRL_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 1);  // port 1 = control/status
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = SEGA_315_5124::port_read(&board_.vdp, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            SEGA_315_5124::port_write(&board_.vdp, vdp_bus);
        }
        return pins;
    }

    // I/O port A ($DC)
    if (is_read && port == sms_constants::IO_PORT_A) {
        BUS_SET_DATA(pins, joypad1_state_);
        return pins;
    }

    // I/O port B ($DD)
    if (is_read && port == sms_constants::IO_PORT_B) {
        BUS_SET_DATA(pins, joypad2_state_);
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

bool SegaSMSSystem::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;
    // TODO: Load .sms ROM file
    log_info("Sega Master System: File loading not yet implemented: %s\n", filepath);
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

uint32_t SegaSMSSystem::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return board_.psg.audio_read(buffer, max_samples);
}

void SegaSMSSystem::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    board_.psg.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

void SegaSMSSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // SMS joypad 1: Up/Down/Left/Right/1/2
    // Port $DC bits: [P2-Down|P2-Up|P1-TR|P1-TL|P1-R|P1-L|P1-D|P1-U]
    struct JoyMapping { SDL_Keycode sdl_key; int bit; };
    static constexpr JoyMapping mappings[] = {
        { SDLK_UP,    0 }, { SDLK_DOWN,  1 },
        { SDLK_LEFT,  2 }, { SDLK_RIGHT, 3 },
        { SDLK_z,     4 }, // Button 1 (TL)
        { SDLK_x,     5 }, // Button 2 (TR)
    };

    for (const auto& m : mappings) {
        if (m.sdl_key == key) {
            if (pressed)
                joypad1_state_ &= ~(1 << m.bit);
            else
                joypad1_state_ |= (1 << m.bit);
        }
    }
}

REGISTER_SYSTEM(sms_descriptor, [] {
    return std::make_unique<SegaSMSSystem>();
});
