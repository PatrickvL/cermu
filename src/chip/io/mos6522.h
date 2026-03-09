#pragma once

#include <cstdint>
#include "../../core/chip.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h"
#include "../../core/ioport.h"       // For io_port<Mask> and io_port_state

// ============================================================================
// MOS6522 REGISTER TABLE — single source of truth
// ============================================================================

// X(addr, symbol, description)
#define MOS6522_REG_TABLE(X) \
    X(0x00, PORTB,    "Port B data/latch")    \
    X(0x01, PORTA,    "Port A data/latch")    \
    X(0x02, DDRB,     "Port B direction")     \
    X(0x03, DDRA,     "Port A direction")     \
    X(0x04, T1CL,     "Timer 1 counter lo")   \
    X(0x05, T1CH,     "Timer 1 counter hi")   \
    X(0x06, T1LL,     "Timer 1 latch lo")     \
    X(0x07, T1LH,     "Timer 1 latch hi")     \
    X(0x08, T2CL,     "Timer 2 counter lo")   \
    X(0x09, T2CH,     "Timer 2 counter hi")   \
    X(0x0A, SR,       "Shift register")       \
    X(0x0B, ACR,      "Auxiliary control")     \
    X(0x0C, PCR,      "Peripheral control")   \
    X(0x0D, IFR,      "Interrupt flags")      \
    X(0x0E, IER,      "Interrupt enable")     \
    X(0x0F, PORTA_NH, "Port A no handshake")

// --- Extract address constants (prefix MOS6522_ added by macro) ---
#define MOS6522_X_CONST_(a, s, l) static constexpr uint8_t MOS6522_##s = a;
MOS6522_REG_TABLE(MOS6522_X_CONST_)
#undef MOS6522_X_CONST_

// --- Extract register info array (label from #symbol, desc from string) ---
#define MOS6522_X_INFO_(a, s, l) { #s, l },
static constexpr RegEntry MOS6522_REG_INFO[] = { MOS6522_REG_TABLE(MOS6522_X_INFO_) };
#undef MOS6522_X_INFO_

// Interrupt flags
#define MOS6522_IFR_IRQ      0x80
#define MOS6522_IFR_SET_CLR  0x80
#define MOS6522_IFR_T1       0x40
#define MOS6522_IFR_T2       0x20
#define MOS6522_IFR_CB1      0x10
#define MOS6522_IFR_CB2      0x08
#define MOS6522_IFR_SR       0x04
#define MOS6522_IFR_CA1      0x02
#define MOS6522_IFR_CA2      0x01

// ACR bits
#define MOS6522_ACR_SR_MODE  0x1C
#define MOS6522_ACR_SR_EXT   0x10
#define MOS6522_ACR_SR_OUT   0x08
#define MOS6522_ACR_SR_IN    0x04
#define MOS6522_ACR_T1_MODE  0x03
#define MOS6522_ACR_T1_CONT  0x40
#define MOS6522_ACR_T1_PB7   0x80
#define MOS6522_ACR_T2_MODE  0x0C
#define MOS6522_ACR_T2_CONT  0x20
#define MOS6522_ACR_T2_PB6   0x10

// PCR bits
#define MOS6522_PCR_CA2_OUT  0x80
#define MOS6522_PCR_CA1_OUT  0x40
#define MOS6522_PCR_CB2_OUT  0x20
#define MOS6522_PCR_CB1_OUT  0x10
#define MOS6522_PCR_CA2_IN   0x08
#define MOS6522_PCR_CA1_IN   0x04
#define MOS6522_PCR_CB2_IN   0x02
#define MOS6522_PCR_CB1_IN   0x01

// ============================================================================
// MOS 6522 VIA (Versatile Interface Adapter) chip structure
// ============================================================================

static constexpr uint8_t MOS6522_NUM_REGS = 16;

struct mos6522_t : public ChipBase {
    mos6522_t() : ChipBase(ChipInfo{"MOS6522", "MOS Technology"}) {
        category_ = "I/O";
#ifdef CERMU_HAS_CHIP_DEBUG
        debug_registry_.set_registers(regs_, MOS6522_NUM_REGS, MOS6522_REG_INFO);
        register_debug_fields();
#endif
    }

    void* bus = nullptr;

    // ========================================================================
    // REGISTERS — flat array matching the VIA register map ($00-$0F)
    // ========================================================================
    // Timer counter/latch bytes (indices 4-9) are NOT live here — timers use
    // separate uint16_t fields for hot-path performance.  The regs_[] slots
    // at 4-9 remain zero and are not authoritative.
    uint8_t regs_[MOS6522_NUM_REGS] = {};

    // Physical pin state (not CPU-visible registers — separate from regs_[])
    uint8_t port_a_pins_ = 0xFF;   // Pull-ups default HIGH
    uint8_t port_b_pins_ = 0xFF;

    // I/O Ports — io_port views over DDR/data bytes in regs_[] + separate pin bytes
    // CIA pattern: io_port(ddr_ref, data_ref, pins_ref)
    io_port<0xFF> port_a{regs_[MOS6522_DDRA], regs_[MOS6522_PORTA], port_a_pins_};
    io_port<0xFF> port_b{regs_[MOS6522_DDRB], regs_[MOS6522_PORTB], port_b_pins_};

    // Callbacks for port input reads (used for keyboard matrix scanning)
    // These callbacks allow external devices (keyboard, joystick) to pull port lines LOW
    // Called when VIA reads from port to get external device state
    uint8_t (*port_a_read_callback)(void* context, uint8_t port_a_output) = nullptr;
    void* port_a_read_context = nullptr;
    uint8_t (*port_b_read_callback)(void* context, uint8_t port_b_output) = nullptr;
    void* port_b_read_context = nullptr;

    // Timers
    uint16_t timer1_latch = 0xFFFF;
    uint16_t timer1_counter = 0xFFFF;
    uint16_t timer2_latch = 0xFFFF;
    uint16_t timer2_counter = 0xFFFF;

    // Shift register
    uint8_t& shift_register = regs_[MOS6522_SR];
    uint8_t shift_counter = 0;

    // Control registers — reference aliases into regs_[]
    uint8_t& acr = regs_[MOS6522_ACR];   // Auxiliary Control Register
    uint8_t& pcr = regs_[MOS6522_PCR];   // Peripheral Control Register
    uint8_t& ifr = regs_[MOS6522_IFR];   // Interrupt Flag Register
    uint8_t& ier = regs_[MOS6522_IER];   // Interrupt Enable Register

    // Timer control
    bool timer1_running = false;
    bool timer2_running = false;

    // Interrupt state
    bool interrupt_active = false;
    int interrupt_bit = 0;  // Bus pin bit index (BUS_IRQ_BIT or BUS_NMI_BIT); 0 = not wired

    // --- ChipBase interface ---
#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override;
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif

    // Lifecycle
    void reset();
    void bus_attach(void* bus);

    // Memory-mapped register access
    bus_state_t registers_read(bus_state_t bus_state);
    bus_state_t registers_write(bus_state_t bus_state);

    // Tick (timer processing + interrupt assertion)
    bus_state_t tick(bus_state_t bus_state);

    // Port read callback registration (used for keyboard matrix scanning, joystick, etc.)
    void set_port_a_read_callback(uint8_t (*callback)(void*, uint8_t), void* context);
    void set_port_b_read_callback(uint8_t (*callback)(void*, uint8_t), void* context);

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using VI = const mos6522_t;
        static constexpr const char* ifr_labels[] = {
            "IRQ", "T1", "T2", "CB1", "CB2", "SR", "CA1", "CA2"
        };
        static constexpr const char* t1_mode_names[] = { "One-shot", "Free-running" };
        static constexpr const char* ca1_edge_names[] = { "Negative edge", "Positive edge" };

        // --- Data Ports ---
        debug_registry_.category("Data Ports")
            .port("Port A",
                  RegSource{MOS6522_PORTA},
                  RegSource{MOS6522_DDRA})
            .port("Port B",
                  RegSource{MOS6522_PORTB},
                  RegSource{MOS6522_DDRB});

        // --- Timers ---
        debug_registry_.category("Timers")
            .timer("Timer 1",
                   +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timer1_counter; },
                   +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timer1_latch; },
                   +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timer1_running; },
                   +[](const ChipBase* c) -> const char* {
                       return (static_cast<VI*>(c)->acr & MOS6522_ACR_T1_CONT) ? "Free-running" : "One-shot";
                   })
            .timer("Timer 2",
                   +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timer2_counter; },
                   +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timer2_latch; },
                   +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timer2_running; });

        // --- Control Registers ---
        debug_registry_.category("Control Registers")
            .value("ACR", MOS6522_ACR)
            .state("T1 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->acr & MOS6522_ACR_T1_CONT) ? 1u : 0u; },
                   t1_mode_names, 2)
            .flag("T1 PB7 Output", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->acr & MOS6522_ACR_T1_PB7) ? 1u : 0u; })
            .value("SR Mode", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->acr & MOS6522_ACR_SR_MODE) >> 2; })
            .value("PCR", MOS6522_PCR)
            .state("CA1 Control", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->pcr & 0x01u; },
                   ca1_edge_names, 2)
            .value("CA2 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->pcr >> 1) & 0x07u; })
            .state("CB1 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->pcr & 0x10u) ? 1u : 0u; },
                   ca1_edge_names, 2)
            .value("CB2 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->pcr >> 5) & 0x07u; });

        // --- Interrupt Control ---
        debug_registry_.category("Interrupt Control")
            .bitfield("IFR", MOS6522_IFR, 8, ifr_labels)
            .bitfield("IER", MOS6522_IER, 8, ifr_labels)
            .flag("IRQ Active", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->interrupt_active; });

        // --- Shift Register ---
        debug_registry_.category("Shift Register", false)
            .value("Shift Register", MOS6522_SR)
            .value("Shift Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->shift_counter; });
    }
#endif
};

// Function declarations
