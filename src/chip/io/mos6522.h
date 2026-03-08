#pragma once

#include <cstdint>

#include <cstdint>
#include "../../core/chip.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h"
#include "../../core/ioport.h"       // For io_port<Mask> and io_port_state

// ============================================================================
// MOS6522 REGISTER ADDRESSES
// ============================================================================

// Register addresses
#define MOS6522_PORTB  0x00
#define MOS6522_PORTA  0x01
#define MOS6522_DDRB   0x02
#define MOS6522_DDRA   0x03
#define MOS6522_T1CL   0x04
#define MOS6522_T1CH   0x05
#define MOS6522_T1LL   0x06
#define MOS6522_T1LH   0x07
#define MOS6522_T2CL   0x08
#define MOS6522_T2CH   0x09
#define MOS6522_SR     0x0A
#define MOS6522_ACR    0x0B
#define MOS6522_PCR    0x0C
#define MOS6522_IFR    0x0D
#define MOS6522_IER    0x0E
#define MOS6522_PORTA_NH 0x0F  // PORTA without handshake

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

struct mos6522_t : public ChipBase {
    mos6522_t() : ChipBase(ChipInfo{"MOS6522", "MOS Technology"}) {
#ifdef CERMU_HAS_GUI
        register_debug_fields();
#endif
    }

    void* bus = nullptr;

    // Registers
    uint8_t registers[16] = {};

    // I/O Ports — io_port_state owns DDR/ORA/ORB/pin storage, io_port<0xFF> provides view
    // data initialized to 0xFF to match existing pull-up behavior (all HIGH after reset)
    io_port_state port_a_regs{0x00, 0xFF, 0xFF};
    io_port_state port_b_regs{0x00, 0xFF, 0xFF};
    io_port<0xFF> port_a{port_a_regs};
    io_port<0xFF> port_b{port_b_regs};

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
    uint8_t shift_register = 0;
    uint8_t shift_counter = 0;

    // Interrupt flags
    uint8_t interrupt_flags = 0;
    uint8_t interrupt_enable = 0;

    // Control registers
    uint8_t acr = 0;   // Auxiliary Control Register
    uint8_t pcr = 0;   // Peripheral Control Register
    uint8_t ifr = 0;   // Interrupt Flag Register
    uint8_t ier = 0;   // Interrupt Enable Register

    // Timer control
    bool timer1_running = false;
    bool timer2_running = false;

    // Interrupt state
    bool interrupt_active = false;
    int interrupt_bit = 0;  // Bus pin bit index (BUS_IRQ_BIT or BUS_NMI_BIT); 0 = not wired

    // --- ChipBase interface ---
    bool has_settings_content() const override;
    void render_settings_content() override;
#ifdef CERMU_HAS_GUI
    ChipLayout* get_chip_layout() const override;
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
                  +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->port_a_regs.data; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->port_a_regs.ddr; })
            .port("Port B",
                  +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->port_b_regs.data; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->port_b_regs.ddr; });

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
            .value("ACR", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->acr; })
            .state("T1 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->acr & MOS6522_ACR_T1_CONT) ? 1u : 0u; },
                   t1_mode_names, 2)
            .flag("T1 PB7 Output", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->acr & MOS6522_ACR_T1_PB7) ? 1u : 0u; })
            .value("SR Mode", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->acr & MOS6522_ACR_SR_MODE) >> 2; })
            .value("PCR", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->pcr; })
            .state("CA1 Control", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->pcr & 0x01u; },
                   ca1_edge_names, 2)
            .value("CA2 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->pcr >> 1) & 0x07u; })
            .state("CB1 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->pcr & 0x10u) ? 1u : 0u; },
                   ca1_edge_names, 2)
            .value("CB2 Control", +[](const ChipBase* c) -> uint32_t { return (static_cast<VI*>(c)->pcr >> 5) & 0x07u; });

        // --- Interrupt Control ---
        debug_registry_.category("Interrupt Control")
            .bitfield("IFR", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->ifr; }, 8, ifr_labels)
            .bitfield("IER", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->ier; }, 8, ifr_labels)
            .flag("IRQ Active", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->interrupt_active; });

        // --- Shift Register ---
        debug_registry_.category("Shift Register", false)
            .value("Shift Register", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->shift_register; })
            .value("Shift Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->shift_counter; });
    }
};

// Function declarations
