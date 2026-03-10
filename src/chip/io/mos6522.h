#pragma once

#include <cstdint>
#include "io_chip_base.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h"
#include "../../core/ioport.h"       // For io_port<Mask> and io_port_state

// ============================================================================
// MOS6522 UNIFIED DECLARATION TABLE — single source of truth
// ============================================================================

// REG(offset, symbol, description)
// FLD(reg_sym, field_sym, hi:lo, description, kind, display_shift, display_scale)
#define MOS6522_DECL(REG, FLD, CMP) \
    REG(0x00, MOS6522_PORTB,    "Port B data/latch")                            \
    REG(0x01, MOS6522_PORTA,    "Port A data/latch")                            \
    REG(0x02, MOS6522_DDRB,     "Port B direction")                             \
    REG(0x03, MOS6522_DDRA,     "Port A direction")                             \
    REG(0x04, MOS6522_T1CL,     "Timer 1 counter lo")                           \
    REG(0x05, MOS6522_T1CH,     "Timer 1 counter hi")                           \
    REG(0x06, MOS6522_T1LL,     "Timer 1 latch lo")                             \
    REG(0x07, MOS6522_T1LH,     "Timer 1 latch hi")                             \
    REG(0x08, MOS6522_T2CL,     "Timer 2 counter lo")                           \
    REG(0x09, MOS6522_T2CH,     "Timer 2 counter hi")                           \
    REG(0x0A, MOS6522_SR,       "Shift register")                               \
    REG(0x0B, MOS6522_ACR,      "Auxiliary control")                             \
      FLD(MOS6522_ACR, T1_PB7,      7:7, "T1 PB7 output",        Flag,  0, 0)  \
      FLD(MOS6522_ACR, T1_FREERUN,  6:6, "T1 free-run",           Flag,  0, 0)  \
      FLD(MOS6522_ACR, T2_COUNTPB6, 5:5, "T2 count PB6",          Flag,  0, 0)  \
      FLD(MOS6522_ACR, SR_CTRL,     4:2, "Shift register control", Value, 0, 0)  \
      FLD(MOS6522_ACR, PB_LATCH,    1:1, "Port B latch enable",   Flag,  0, 0)  \
      FLD(MOS6522_ACR, PA_LATCH,    0:0, "Port A latch enable",   Flag,  0, 0)  \
    REG(0x0C, MOS6522_PCR,      "Peripheral control")                            \
      FLD(MOS6522_PCR, CB2_CTRL,    7:5, "CB2 control",           Value, 0, 0)  \
      FLD(MOS6522_PCR, CB1_EDGE,    4:4, "CB1 edge (1=pos)",      Flag,  0, 0)  \
      FLD(MOS6522_PCR, CA2_CTRL,    3:1, "CA2 control",           Value, 0, 0)  \
      FLD(MOS6522_PCR, CA1_EDGE,    0:0, "CA1 edge (1=pos)",      Flag,  0, 0)  \
    REG(0x0D, MOS6522_IFR,      "Interrupt flags")                               \
      FLD(MOS6522_IFR, IRQ,     7:7, "IRQ active",  Flag, 0, 0)                 \
      FLD(MOS6522_IFR, T1_IF,   6:6, "Timer 1",     Flag, 0, 0)                 \
      FLD(MOS6522_IFR, T2_IF,   5:5, "Timer 2",     Flag, 0, 0)                 \
      FLD(MOS6522_IFR, CB1_IF,  4:4, "CB1",         Flag, 0, 0)                 \
      FLD(MOS6522_IFR, CB2_IF,  3:3, "CB2",         Flag, 0, 0)                 \
      FLD(MOS6522_IFR, SR_IF,   2:2, "Shift reg",   Flag, 0, 0)                 \
      FLD(MOS6522_IFR, CA1_IF,  1:1, "CA1",         Flag, 0, 0)                 \
      FLD(MOS6522_IFR, CA2_IF,  0:0, "CA2",         Flag, 0, 0)                 \
    REG(0x0E, MOS6522_IER,      "Interrupt enable")                              \
      FLD(MOS6522_IER, IE_SC,   7:7, "Set/clear",   Flag, 0, 0)                 \
      FLD(MOS6522_IER, T1_IE,   6:6, "Timer 1",     Flag, 0, 0)                 \
      FLD(MOS6522_IER, T2_IE,   5:5, "Timer 2",     Flag, 0, 0)                 \
      FLD(MOS6522_IER, CB1_IE,  4:4, "CB1",         Flag, 0, 0)                 \
      FLD(MOS6522_IER, CB2_IE,  3:3, "CB2",         Flag, 0, 0)                 \
      FLD(MOS6522_IER, SR_IE,   2:2, "Shift reg",   Flag, 0, 0)                 \
      FLD(MOS6522_IER, CA1_IE,  1:1, "CA1",         Flag, 0, 0)                 \
      FLD(MOS6522_IER, CA2_IE,  0:0, "CA2",         Flag, 0, 0)                 \
    REG(0x0F, MOS6522_PORTA_NH, "Port A no handshake")

// --- Extract address constants ---
MOS6522_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)

DECL_EXTRACT_ALL(MOS6522, MOS6522_DECL)

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

struct mos6522_t : public IoChipBase {
    mos6522_t() : IoChipBase(ChipInfo{"MOS6522", "MOS Technology"}) {
        init_regs(MOS6522_NUM_REGS);
#ifdef CERMU_HAS_CHIP_DEBUG
        debug_registry_.set_registers(regs_, MOS6522_NUM_REGS, MOS6522_REG_INFO);
        register_debug_fields();
#endif
    }

    void* bus = nullptr;

    // Timer counter/latch bytes (indices 4-9) are NOT live here — timers use
    // separate uint16_t fields for hot-path performance.  The regs_[] slots
    // at 4-9 remain zero and are not authoritative.

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

        // ACR/PCR/IFR/IER bitfields are now in the DECL walk.
        debug_registry_.set_decl_order(MOS6522_DECL_ORDER.data(), MOS6522_DECL_ORDER.size(),
                                       MOS6522_FLD_INFO, MOS6522_NUM_FIELDS,
                                       nullptr, 0, nullptr);

        // --- Data Ports ---
        debug_registry_.category("Data Ports")
            .port("Port A",
                  RegSource{MOS6522_PORTA},
                  RegSource{MOS6522_DDRA})
            .port("Port B",
                  RegSource{MOS6522_PORTB},
                  RegSource{MOS6522_DDRB});

        // --- Timers (live counters/latches — not in register mirror) ---
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

        // --- Interrupt (internal state not in register) ---
        debug_registry_.category("Interrupt Control")
            .flag("IRQ Active", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->interrupt_active; });

        // --- Shift Register (live counter not in register) ---
        debug_registry_.category("Shift Register", false)
            .value("Shift Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->shift_counter; });
    }
#endif
};

// Function declarations
