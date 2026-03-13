/*
 * bombjack_system.cpp — Bomb Jack arcade system implementation
 */

#include "systems/arcade/bombjack/bombjack_system.hpp"
#include "core/system_registry.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor bombjack_descriptor = {
    "Bomb Jack", "BombJack",
    "Tehkan Bomb Jack — dual Z80A, 3× AY-3-8910, 256×224 sprites+tiles (1984)",
    "bombjack", {"BombJack", "Bomb Jack"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

BombJackSystem::BombJackSystem()
    : System()
    , main_pins_(BOMBJACK_BUS_DEFAULT_STATE)
    , sound_pins_(BOMBJACK_BUS_DEFAULT_STATE)
{
    HardwareTraits traits = {};
    traits.display.native_width    = bombjack_constants::FB_WIDTH;
    traits.display.native_height   = bombjack_constants::FB_HEIGHT;
    traits.display.visible_width   = bombjack_constants::FB_WIDTH;
    traits.display.visible_height  = bombjack_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = bombjack_constants::PALETTE_ENTRIES;
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = bombjack_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "3× AY-3-8910";
    traits.timing.cpu_frequency_hz = bombjack_constants::MAIN_CPU_FREQ_HZ;
    traits.timing.target_fps       = bombjack_constants::REFRESH_HZ;
    traits.timing.cycles_per_frame = bombjack_constants::MAIN_CYCLES_PER_FRAME;
    traits.timing.standard         = VideoStandard::NTSC;
    hardware_traits_ = traits;
    bombjack_descriptor.hardware_traits = traits;
}

BombJackSystem::~BombJackSystem() {}

const SystemDescriptor& BombJackSystem::get_descriptor() const { return bombjack_descriptor; }
bool BombJackSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool BombJackSystem::apply_configuration() { return true; }

bool BombJackSystem::initialize() {
    printf("Bomb Jack: Initializing arcade system\n");
    register_board(&main_board_);

    // ── Create memory chips from manifest and wire buses ─────────────────
    main_board_.create_chips(&main_pins_);
    main_board_.apply(main_bus_);

    sound_board_.create_chips(&sound_pins_);
    sound_board_.apply(sound_bus_);

    // ── Init chips ───────────────────────────────────────────────────────
    main_cpu_  = main_board_.cpu<ZilogZ80A>();
    sound_cpu_ = sound_board_.cpu<ZilogZ80A>();
    main_pins_  = main_board_.cpu_chip()->init();
    sound_pins_ = sound_board_.cpu_chip()->init();
    for (auto& ay : ay_) ay.init();

    load_roms();

    // ── Register chips for Hardware menu ─────────────────────────────────

    register_bus_chips(main_board_);
    register_bus_chips(sound_board_);

    system_ready_ = true;
    return true;
}

void BombJackSystem::shutdown() {
    main_cpu_  = nullptr;
    sound_cpu_ = nullptr;
    system_ready_ = false;
}

void BombJackSystem::reset() {
    if (!main_board_.cpu_chip() || !sound_board_.cpu_chip()) return;
    main_pins_  = main_board_.cpu_chip()->reset(main_pins_);
    sound_pins_ = sound_board_.cpu_chip()->reset(sound_pins_);
    for (auto& ay : ay_) ay.reset();
    sound_latch_ = 0;
    sound_nmi_ = false;
}

void BombJackSystem::tick() {
    if (!main_board_.cpu_chip() || !sound_board_.cpu_chip()) return;

    // Main CPU tick
    main_pins_ = main_cpu_->tick(main_pins_);

    // Main CPU bus dispatch — memory-mapped only (no IORQ for main CPU)
    if (!BUS_GET_BIT(main_pins_, Z80_MREQ_BIT)) {
        uint16_t addr = BUS_GET_ADDR(main_pins_);
        if ((addr & 0xF000) == 0xB000) {
            main_pins_ = main_io_tick(main_pins_);
        } else {
            main_pins_ = main_bus_.tick(0, main_pins_);
        }
    }

    // Sound CPU runs at 3/4 speed (3 MHz vs 4 MHz main)
    // Tick sound CPU on 3 out of every 4 main cycles
    if (total_cycles_ % 4 != 3) {
        // Manage sound NMI line (edge-triggered: assert when latch written)
        if (sound_nmi_) {
            BUS_CLR_BIT(sound_pins_, BUS_NMI_BIT);
        } else {
            BUS_SET_BIT(sound_pins_, BUS_NMI_BIT);
        }

        sound_pins_ = sound_cpu_->tick(sound_pins_);

        // Sound CPU bus dispatch
        if (!BUS_GET_BIT(sound_pins_, Z80_MREQ_BIT)) {
            uint16_t addr = BUS_GET_ADDR(sound_pins_);
            if (addr == 0x6000) {
                // Sound latch read — clear NMI
                if (BUS_GET_BIT(sound_pins_, BUS_RW_BIT)) {
                    BUS_SET_DATA(sound_pins_, sound_latch_);
                    sound_nmi_ = false;
                }
            } else {
                sound_pins_ = sound_bus_.tick(0, sound_pins_);
            }
        } else if (!BUS_GET_BIT(sound_pins_, Z80_IORQ_BIT)) {
            sound_pins_ = sound_io_tick(sound_pins_);
        }

        // AY chips tick at ~1.5 MHz (sound CPU / 2)
        if (total_cycles_ & 1) {
            for (auto& ay : ay_) ay.tick();
        }
    }

    // VBLANK NMI to main CPU (edge-triggered, once per frame)
    uint32_t frame_cycle = total_cycles_ % bombjack_constants::MAIN_CYCLES_PER_FRAME;
    if (frame_cycle == 0 && total_cycles_ > 0) {
        BUS_CLR_BIT(main_pins_, BUS_NMI_BIT);  // Assert NMI (falling edge)
    } else if (frame_cycle == 1) {
        BUS_SET_BIT(main_pins_, BUS_NMI_BIT);  // Deassert NMI
    }

    total_cycles_++;
}

void BombJackSystem::run_frame() {
    for (uint32_t i = 0; i < bombjack_constants::MAIN_CYCLES_PER_FRAME; ++i) tick();
}

bool BombJackSystem::load_file(const char*) { return false; }
uint32_t* BombJackSystem::get_framebuffer() { return framebuffer_; }
void BombJackSystem::get_display_dimensions(int* w, int* h) const {
    *w = bombjack_constants::FB_WIDTH; *h = bombjack_constants::FB_HEIGHT;
}
void BombJackSystem::set_framebuffer(uint32_t*, int, int) {}
uint32_t BombJackSystem::get_audio_samples(float*, uint32_t) { return 0; }
void BombJackSystem::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
void BombJackSystem::handle_keyboard_event(SDL_Keycode, bool) {}
void BombJackSystem::render_system_menu_items() {}
void BombJackSystem::render_configuration_ui() {}
void BombJackSystem::set_speed_multiplier(float m) { speed_multiplier_ = m; }

// ============================================================================
// I/O DISPATCH — Main CPU ($B000-$BFFF memory-mapped registers)
// ============================================================================

bus_state_t BombJackSystem::main_io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);

    if (BUS_GET_BIT(pins, BUS_RW_BIT)) {
        // Reads: input ports and DIP switches
        uint8_t data = 0xFF;
        if (addr == bombjack_constants::INPUT_P1)         data = input_p1_;
        else if (addr == bombjack_constants::INPUT_P2)    data = input_p2_;
        else if (addr == bombjack_constants::INPUT_SYSTEM) data = input_system_;
        else if (addr == bombjack_constants::DSW1)        data = dsw1_;
        else if (addr == bombjack_constants::DSW2)        data = dsw2_;
        BUS_SET_DATA(pins, data);
    } else {
        // Writes: sound latch, background select, watchdog
        uint8_t data = BUS_GET_DATA(pins);
        if (addr == bombjack_constants::SOUND_LATCH) {
            sound_latch_ = data;
            sound_nmi_ = true;
        } else if (addr == bombjack_constants::BG_SELECT) {
            bg_image_select_ = data & 0x07;
        }
        // Watchdog write at $B800 is intentionally ignored
    }

    return pins;
}
bus_state_t BombJackSystem::sound_io_tick(bus_state_t pins) {
    uint8_t port = static_cast<uint8_t>(BUS_GET_ADDR(pins));
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t data = BUS_GET_DATA(pins);

    // Determine which AY chip and whether address or data register
    int ay_idx = -1;
    bool is_data = false;

    if ((port & 0xFE) == bombjack_constants::AY1_ADDR) {
        ay_idx = 0; is_data = port & 0x01;
    } else if ((port & 0xFE) == bombjack_constants::AY2_ADDR) {
        ay_idx = 1; is_data = port & 0x01;
    } else if ((port & 0xFE) == bombjack_constants::AY3_ADDR) {
        ay_idx = 2; is_data = port & 0x01;
    }

    if (ay_idx >= 0) {
        if (is_data) {
            if (is_read) {
                BUS_SET_DATA(pins, ay_[ay_idx].read_register());
            } else {
                ay_[ay_idx].write_register(data);
            }
        } else {
            if (!is_read) {
                ay_[ay_idx].latch_address(data);
            }
        }
    }

    return pins;
}
bool BombJackSystem::load_roms() { return false; }

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(bombjack_descriptor, [] { return std::make_unique<BombJackSystem>(); });
