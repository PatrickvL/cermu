/*
 * lc80_system.cpp — LC 80 learning computer system implementation
 */

#include "lc80_system.h"
#include "../../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor lc80_descriptor = {
    "LC 80", "LC80",
    "VEB Mikroelektronik LC 80 — U880 @ 900kHz, 1KB RAM, 7-segment LED display (1984)",
    "lc80", {"LC80", "LC-80"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

LC80System::LC80System() : EmulatedSystem(), pins_(LC80_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    // The LC80 has no CRT — LED display rendered into small framebuffer
    traits.display.native_width    = FB_WIDTH;
    traits.display.native_height   = FB_HEIGHT;
    traits.display.visible_width   = FB_WIDTH;
    traits.display.visible_height  = FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 2;
    traits.timing.cpu_frequency_hz = lc80_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = lc80_constants::UPDATE_RATE_HZ;
    traits.timing.cycles_per_frame = lc80_constants::CYCLES_PER_UPDATE;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
    lc80_descriptor.hardware_traits = traits;
}

LC80System::~LC80System() { delete cpu_; }

const SystemDescriptor& LC80System::get_descriptor() const { return lc80_descriptor; }
bool LC80System::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool LC80System::apply_configuration() { return true; }

bool LC80System::initialize() {
    printf("LC 80: Initializing system\n");
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio1_.init();
    pio2_.init();
    ctc_.init();
    rom_.resize(lc80_constants::ROM_SIZE, 0xFF);
    ram_.resize(lc80_constants::RAM_SIZE_MIN, 0x00);
    load_roms();
    system_ready_ = true;
    return true;
}

void LC80System::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
void LC80System::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    pio1_.init();
    pio2_.init();
    ctc_.init();
    std::memset(led_segments_, 0, sizeof(led_segments_));
}

void LC80System::tick() {
    if (!cpu_) return;

    // CPU tick
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = mem_tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // CTC tick (speaker on channel 2)
    ctc_.tick();

    total_cycles_++;
}

void LC80System::run_frame() {
    for (uint32_t i = 0; i < lc80_constants::CYCLES_PER_UPDATE; ++i) tick();
}

bool LC80System::load_file(const char*) { return false; }
uint32_t* LC80System::get_framebuffer() { return framebuffer_; }
void LC80System::get_display_dimensions(int* w, int* h) const { *w = FB_WIDTH; *h = FB_HEIGHT; }
void LC80System::set_framebuffer(uint32_t*, int, int) {}
uint32_t LC80System::get_audio_samples(float*, uint32_t) { return 0; }
void LC80System::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
void LC80System::handle_keyboard_event(SDL_Keycode, bool) {}
void LC80System::render_system_menu_items() {}
void LC80System::render_configuration_ui() {}
void LC80System::set_speed_multiplier(float m) { speed_multiplier_ = m; }

bus_state_t LC80System::mem_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (is_read) {
        uint8_t data = 0xFF;
        if (addr < 0x2000) {
            // ROM (2 KB, mirrored through $0000-$1FFF)
            data = rom_[addr & (lc80_constants::ROM_SIZE - 1)];
        } else if (addr < 0x4000) {
            // RAM (1 KB at $2000, mirrored through $2000-$3FFF)
            data = ram_[(addr - 0x2000) & (ram_.size() - 1)];
        }
        BUS_SET_DATA(pins, data);
    } else {
        uint8_t data = BUS_GET_DATA(pins);
        if (addr >= 0x2000 && addr < 0x4000) {
            // RAM write (ROM is read-only)
            ram_[(addr - 0x2000) & (ram_.size() - 1)] = data;
        }
    }

    return pins;
}
bus_state_t LC80System::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        if (ctc_.interrupt_pending()) {
            BUS_SET_DATA(pins, ctc_.interrupt_vector());
        } else {
            BUS_SET_DATA(pins, 0xFF);
        }
        return pins;
    }

    uint8_t port = static_cast<uint8_t>(BUS_GET_ADDR(pins));
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t data = BUS_GET_DATA(pins);

    // PIO 1 at $F4-$F7 (LED display + keyboard)
    // Bit 0: port (0=A, 1=B), Bit 1: data/control (0=data, 1=control)
    if ((port & 0xFC) == lc80_constants::PIO1_PORT_A) {
        int port_idx = port & 0x01;
        bool is_ctrl = (port >> 1) & 0x01;
        if (is_read) {
            BUS_SET_DATA(pins, pio1_.read_data(port_idx));
        } else {
            if (is_ctrl) {
                pio1_.write_control(port_idx, data);
            } else {
                pio1_.write_data(port_idx, data);
            }
        }
        return pins;
    }

    // PIO 2 at $F8-$FB (keyboard scan + cassette)
    if ((port & 0xFC) == lc80_constants::PIO2_PORT_A) {
        int port_idx = port & 0x01;
        bool is_ctrl = (port >> 1) & 0x01;
        if (is_read) {
            BUS_SET_DATA(pins, pio2_.read_data(port_idx));
        } else {
            if (is_ctrl) {
                pio2_.write_control(port_idx, data);
            } else {
                pio2_.write_data(port_idx, data);
            }
        }
        return pins;
    }

    // CTC at $EC-$EF (4 channels, speaker on ch2)
    if ((port & 0xFC) == lc80_constants::CTC_CH0) {
        int channel = port & 0x03;
        if (is_read) {
            BUS_SET_DATA(pins, ctc_.read(channel));
        } else {
            ctc_.write(channel, data);
        }
        return pins;
    }

    return pins;
}
bool LC80System::load_roms() { return false; }

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(lc80_descriptor, [] { return std::make_unique<LC80System>(); });
