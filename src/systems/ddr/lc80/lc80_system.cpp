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
// CONSTRUCTION / DESTRUCTION
// ============================================================================

LC80System::LC80System() : System(), pins_(LC80_BUS_DEFAULT_STATE) {
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

// ============================================================================
// LIFECYCLE
// ============================================================================

bool LC80System::initialize() {
    printf("LC 80: Initializing system\n");

    // ── Create memory chips from manifest and wire bus ────────────────────
    bus_mem_.create_chips(&pins_);
    bus_mem_.apply(bus_);

    // ── Configure page tables (mirroring) ───────────────────────────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio1_.init();
    pio2_.init();
    ctc_.init();

    load_roms();

    // ── Register chips for Hardware menu ────────────────────────────────
    register_chip(static_cast<ChipBase*>(cpu_),
        "U880 CPU", "U880", "CPU", 0x0000);
    register_chip(&pio1_,
        "U855 PIO #1", "U855", "I/O", lc80_constants::PIO1_PORT_A);
    register_chip(&pio2_,
        "U855 PIO #2", "U855", "I/O", lc80_constants::PIO2_PORT_A);
    register_chip(&ctc_,
        "U857 CTC", "U857", "Timer", lc80_constants::CTC_CH0);
    register_bus_chips(bus_mem_);

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

// ============================================================================
// EXECUTION
// ============================================================================

void LC80System::tick() {
    if (!cpu_) return;

    // CPU tick
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = bus_.tick(0, pins_);
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

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

void LC80System::configure_bus_memory_map() {
    using ChipId      = PT::ChipId;
    using WriteChipId = PT::WriteChipId;

    // apply() maps:
    //   ROM: pages $00-$07 (read only, $0000-$07FF)
    //   RAM: pages $20-$23 (read+write, $2000-$23FF)
    // We need to add mirrors for the full decoded address ranges.

    // ROM is 2 KB = 8 pages, mirrored 4x through $0000-$1FFF (32 pages)
    constexpr size_t rom_pages = lc80_constants::ROM_SIZE / 256;   // 8
    constexpr size_t rom_range = 0x2000 / 256;                     // 32 pages
    for (size_t base = rom_pages; base < rom_range; base += rom_pages) {
        for (size_t p = 0; p < rom_pages; ++p) {
            bus_.set_read_page(0, base + p, ChipId(p));
        }
    }

    // RAM is 1 KB = 4 pages, mirrored 8x through $2000-$3FFF (32 pages)
    constexpr size_t ram_base_id = kLC80Chips.base_id(lc80_chips::kRamSlot, LC80BusSpec::PageBits);
    constexpr size_t ram_pages   = lc80_constants::RAM_SIZE_MIN / 256;  // 4
    constexpr size_t ram_first   = 0x2000 / 256;                        // page 32
    constexpr size_t ram_range   = 0x2000 / 256;                        // 32 pages
    for (size_t base = ram_pages; base < ram_range; base += ram_pages) {
        for (size_t p = 0; p < ram_pages; ++p) {
            auto id = ChipId(ram_base_id + p);
            bus_.set_read_page (0, ram_first + base + p, id);
            bus_.set_write_page(0, ram_first + base + p, WriteChipId(id));
        }
    }

    // Pages $40-$FF ($4000-$FFFF) remain unmapped — reads return bus default
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

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
