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
 * Address decoding (active when A9=1, addr bits 0-4 are meaningful):
 *   I/O port registers (A2 selects port A/B, A0 selects data/DDR)
 *   Timer read: A0=0 → INTIM value, A0=1 → interrupt flag (bit 7 = underflow)
 *   Timer write: A4:A3 selects divider (1/8/64/1024), A0=0
 *
 * In the 2600:
 *   RIOT RAM: $0080-$00FF (A12=0, A7=1, A9=0)
 *   RIOT I/O: $0280-$02FF (A12=0, A7=1, A9=1)
 */

#include "io_chip_base.h"
#include <cstdint>

// Timer divider values (set via address bits A4:A3)
static constexpr uint16_t RIOT_TIM1T   = 0x14;   // Divide by 1
static constexpr uint16_t RIOT_TIM8T   = 0x15;   // Divide by 8
static constexpr uint16_t RIOT_TIM64T  = 0x16;   // Divide by 64
static constexpr uint16_t RIOT_TIM1024T = 0x17;   // Divide by 1024

// ============================================================================
// RIOT UNIFIED DECLARATION TABLE — single source of truth
// ============================================================================

// REG(offset, symbol, description)
#define RIOT_DECL(REG, FLD, CMP) \
    REG(0, RIOT_REG_PORTA_DATA, "Port A output (SWCHA)")        \
    REG(1, RIOT_REG_PORTA_DDR,  "Port A direction (SWACNT)")    \
    REG(2, RIOT_REG_PORTB_DATA, "Port B output (SWCHB)")        \
    REG(3, RIOT_REG_PORTB_DDR,  "Port B direction (SWBCNT)")

// --- Extract address constants ---
RIOT_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)

DECL_EXTRACT_REGS_ONLY(RIOT, RIOT_DECL)

struct pia6532_t : public IoChipBase {
    pia6532_t() : IoChipBase(ChipInfo{"PIA6532", "MOS Technology"}) {
        init_regs(RIOT_NUM_REGS);
#ifdef CERMU_HAS_CHIP_DEBUG
        debug_registry_.set_registers(regs_, RIOT_NUM_REGS, RIOT_REG_INFO);
        register_debug_fields();
#endif
    }

    // --- ChipBase GUI interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ========================================================================
    // RAM — 128 bytes ($80-$FF)
    // ========================================================================
    uint8_t ram[128] = {};

    // I/O port named accessors (backed by ChipBase::regs_)

    uint8_t& port_a_data = regs_[RIOT_REG_PORTA_DATA];   // Output latch
    uint8_t& port_a_ddr  = regs_[RIOT_REG_PORTA_DDR];    // Data direction (0=input, 1=output)
    uint8_t& port_b_data = regs_[RIOT_REG_PORTB_DATA];   // Output latch
    uint8_t& port_b_ddr  = regs_[RIOT_REG_PORTB_DDR];    // Data direction

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
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using PI = const pia6532_t;
        debug_registry_
            .set_decl_order(RIOT_DECL_ORDER.data(), RIOT_DECL_ORDER.size(),
                            nullptr, 0,
                            nullptr, 0, nullptr);

        // Register values are shown in the DECL walk.
        // Port visualization, external inputs, timer, and RAM remain as builder chains.
        debug_registry_
            .category("I/O Ports")
            .port("Port A",
                  RegSource{RIOT_REG_PORTA_DATA},
                  RegSource{RIOT_REG_PORTA_DDR})
            .value("Port A Input", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->port_a_input; })
            .value("Port A Effective", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->read_port_a(); })
            .port("Port B",
                  RegSource{RIOT_REG_PORTB_DATA},
                  RegSource{RIOT_REG_PORTB_DDR})
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
#endif

    static constexpr const char* divider_names_[] = {
        "TIM1T (\xC3\xB7" "1)", "TIM8T (\xC3\xB7" "8)",
        "TIM64T (\xC3\xB7" "64)", "TIM1024T (\xC3\xB7" "1024)"
    };
};
