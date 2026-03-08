#pragma once
/*
 * pia6532.h — MOS 6532 RIOT (RAM-I/O-Timer)
 *
 * The 6532 combines three functions in one 40-pin DIP:
 *   1) 128 bytes of static RAM ($80-$FF in Atari 2600 address space)
 *   2) Two 8-bit bidirectional I/O ports (A and B) with data direction regs
 *   3) Programmable interval timer (1/8/64/1024 clock divider)
 *
 * Used in: Atari 2600 (VCS), Atari 7800, various arcade boards.
 *
 * Register map (active when A9=1, addr bits 0-4 are meaningful):
 *   Read:
 *     A2=0, A0=0: Port A data (SWCHA)
 *     A2=0, A0=1: Port A DDR
 *     A2=1, A0=0: Port B data (SWCHB — console switches)
 *     A2=1, A0=1: Port B DDR
 *     A0=0: Read INTIM (timer value)
 *     A0=1: Read interrupt flag (bit 7 = timer underflow)
 *
 *   Write:
 *     A2=0, A0=0: Port A data
 *     A2=0, A0=1: Port A DDR
 *     A2=1, A0=0: Port B data
 *     A2=1, A0=1: Port B DDR
 *     A4:A3 + A0=0: Set timer interval (divider from A4:A3)
 *
 * In the 2600:
 *   RIOT RAM: $0080-$00FF (A12=0, A7=1, A9=0)
 *   RIOT I/O: $0280-$02FF (A12=0, A7=1, A9=1)
 */

#include "../../core/chip.h"
#include <cstdint>

// ============================================================================
// RIOT REGISTER ADDRESSES (address bits within the I/O range)
// ============================================================================

// Timer divider values (set via address bits A4:A3)
static constexpr uint16_t RIOT_TIM1T   = 0x14;   // Divide by 1
static constexpr uint16_t RIOT_TIM8T   = 0x15;   // Divide by 8
static constexpr uint16_t RIOT_TIM64T  = 0x16;   // Divide by 64
static constexpr uint16_t RIOT_TIM1024T = 0x17;   // Divide by 1024

struct pia6532_t : public ChipBase {
    pia6532_t() : ChipBase(ChipInfo{"PIA6532", "MOS Technology"}) {
#ifdef CERMU_HAS_GUI
        register_debug_fields();
#endif
    }

    // --- ChipBase GUI interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* get_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ========================================================================
    // RAM — 128 bytes ($80-$FF)
    // ========================================================================
    uint8_t ram[128] = {};

    // ========================================================================
    // I/O PORTS
    // ========================================================================

    // Port A — direction lines and data
    uint8_t port_a_data = 0x00;       // Output latch
    uint8_t port_a_ddr  = 0x00;       // Data direction (0=input, 1=output)

    // Port B — direction lines and data
    uint8_t port_b_data = 0x00;       // Output latch
    uint8_t port_b_ddr  = 0x00;       // Data direction

    // External port inputs (set by system)
    uint8_t port_a_input = 0xFF;      // Input pins (active-low for joystick lines)
    uint8_t port_b_input = 0xFF;      // Input pins (console switches)

    // ========================================================================
    // TIMER
    // ========================================================================

    uint8_t  timer_value   = 0xFF;    // Current timer count
    uint16_t timer_divider = 1024;    // Clock divider (1, 8, 64, 1024)
    uint16_t timer_counter = 0;       // Sub-divider counter
    bool     timer_underflow = false; // Set when timer wraps past $00
    bool     timer_interrupt_enabled = false; // Enable interrupt on underflow

    // ========================================================================
    // INTERFACE
    // ========================================================================

    void init();
    void reset();

    /// Tick the timer by one CPU clock.  Call once per CPU cycle.
    void tick();

    /// Read an I/O register (addr is the full address — chip decodes relevant bits).
    uint8_t read_io(uint16_t addr);

    /// Write an I/O register.
    void write_io(uint16_t addr, uint8_t data);

    /// Read RAM (addr should be $00-$7F offset into the 128-byte block).
    inline uint8_t read_ram(uint8_t offset) const {
        return ram[offset & 0x7F];
    }

    /// Write RAM.
    inline void write_ram(uint8_t offset, uint8_t data) {
        ram[offset & 0x7F] = data;
    }

    /// Read a port with data direction masking.
    uint8_t read_port_a() const;
    uint8_t read_port_b() const;

private:
    void register_debug_fields() {
        using PI = const pia6532_t;
        debug_registry_
            .category("I/O Ports")
            .port("Port A",
                  +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->port_a_data; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->port_a_ddr; })
            .value("Port A Input", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->port_a_input; })
            .value("Port A Effective", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->read_port_a(); })
            .port("Port B",
                  +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->port_b_data; },
                  +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->port_b_ddr; })
            .value("Port B Input", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->port_b_input; })
            .value("Port B Effective", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->read_port_b(); })

            .category("Timer")
            .value("Value", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->timer_value; })
            .state("Divider", +[](const ChipBase* c) -> uint32_t {
                switch (static_cast<PI*>(c)->timer_divider) {
                    case 1:    return 0;
                    case 8:    return 1;
                    case 64:   return 2;
                    case 1024: return 3;
                    default:   return 0;
                }
            }, divider_names_, 4)
            .counter("Sub-counter", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->timer_counter; },
                     +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->timer_divider; })
            .flag("Underflow", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->timer_underflow; })
            .flag("IRQ Enable", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->timer_interrupt_enabled; })

            .category("RAM (128 bytes)", false)
            .memory("RAM", +[](const ChipBase* c) -> std::pair<const uint8_t*, size_t> {
                return {static_cast<PI*>(c)->ram, 128};
            }, 0x0080, 128);
    }

    static constexpr const char* divider_names_[] = {
        "TIM1T (\xC3\xB71)", "TIM8T (\xC3\xB78)",
        "TIM64T (\xC3\xB764)", "TIM1024T (\xC3\xB71024)"
    };
};
