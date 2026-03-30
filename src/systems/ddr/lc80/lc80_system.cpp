/*
 * lc80_system.cpp — LC 80 learning computer system implementation
 */

#include "systems/ddr/lc80/lc80_system.hpp"
#include "core/system_registry.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor lc80_descriptor = {
    "LC 80", "LC80",
    "VEB Mikroelektronik LC 80 — U880 @ 900kHz, 1KB RAM, 7-segment LED display (1984)",
    "lc80", {"LC80", "LC-80"},
    nullptr, {}, nullptr,
    "Robotron", 1984, z80::U880Traits.display_name, SystemType::Other
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

LC80System::~LC80System() = default;

const SystemDescriptor& LC80System::get_descriptor() const { return lc80_descriptor; }
bool LC80System::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool LC80System::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

bool LC80System::initialize() {
    printf("LC 80: Initializing system\n");
    register_board(&board_);

    // ── Bind and create chips from manifest, wire bus ───────────────────
    { size_t slot_idx_ = 0;
      LC80_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_) }
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // ── Configure page tables (mirroring) ───────────────────────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    pins_ = board_.z80.init();
    board_.pio.init();
    board_.pio2.init();
    board_.ctc.init();

    load_roms();

    // ── Register chips for Hardware menu ────────────────────────
    register_bus_chips(board_);

    system_ready_ = true;
    return true;
}

void LC80System::shutdown() { system_ready_ = false; }

void LC80System::reset() {
    if (!system_ready_) return;
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    std::memset(led_segments_, 0, sizeof(led_segments_));
}

// ============================================================================
// EXECUTION
// ============================================================================

void LC80System::tick() {
    if (!system_ready_) return;

    // CPU tick
    pins_ = board_.z80.tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // CTC tick (speaker on channel 2)
    board_.ctc.tick();

    total_cycles_++;
}

void LC80System::run_frame() {
    for (uint32_t i = 0; i < lc80_constants::CYCLES_PER_UPDATE; ++i) tick();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

void LC80System::configure_bus_memory_map() {
    // ROM mirroring ($0000-$1FFF, 2 KB mirrored 4×) and RAM mirroring
    // ($2000-$3FFF, 1 KB mirrored 8×) are now handled declaratively by the
    // manifest: each slot declares the full address range as size_bytes
    // with addr_mask set to the physical chip size minus one, so apply()
    // wraps all accesses automatically.
    //
    // Pages $40-$FF ($4000-$FFFF) remain unmapped — reads return bus default.
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

bus_state_t LC80System::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        if (board_.ctc.interrupt_pending()) {
            BUS_SET_DATA(pins, board_.ctc.interrupt_vector());
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
            BUS_SET_DATA(pins, board_.pio.read_data(port_idx));
        } else {
            if (is_ctrl) {
                board_.pio.write_control(port_idx, data);
            } else {
                board_.pio.write_data(port_idx, data);
            }
        }
        return pins;
    }

    // PIO 2 at $F8-$FB (keyboard scan + cassette)
    if ((port & 0xFC) == lc80_constants::PIO2_PORT_A) {
        int port_idx = port & 0x01;
        bool is_ctrl = (port >> 1) & 0x01;
        if (is_read) {
            BUS_SET_DATA(pins, board_.pio2.read_data(port_idx));
        } else {
            if (is_ctrl) {
                board_.pio2.write_control(port_idx, data);
            } else {
                board_.pio2.write_data(port_idx, data);
            }
        }
        return pins;
    }

    // CTC at $EC-$EF (4 channels, speaker on ch2)
    if ((port & 0xFC) == lc80_constants::CTC_CH0) {
        int channel = port & 0x03;
        if (is_read) {
            BUS_SET_DATA(pins, board_.ctc.read(channel));
        } else {
            board_.ctc.write(channel, data);
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
