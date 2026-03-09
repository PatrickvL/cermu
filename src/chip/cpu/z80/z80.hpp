#pragma once
/*
 * z80.hpp — Zilog Z80 Family CPU Emulator (Main Template)
 *
 * Template-based Z80 CPU core using NTTP (non-type template parameters),
 * following the fam65xx pattern.  Each Z80 variant (Z80, Z80A, Z180, eZ80,
 * R800) is a distinct template instantiation with compile-time feature gating
 * via `if constexpr`.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * 1. Zero overhead abstractions — templates eliminate runtime dispatch
 * 2. Conditional features — `if constexpr` for variant-specific behavior
 * 3. Pin-accurate bus interface — bus_state_t pins protocol
 * 4. Split-phase ticks — separate T-state phases (like fam65xx PHI1/PHI2)
 * 5. Chips are system-agnostic — no includes of system headers
 *
 * Z80 BUS PROTOCOL:
 * =================
 * The Z80 uses M-cycles (machine cycles) composed of T-states:
 *   - M1 (opcode fetch):    4 T-states (or 6 with wait)
 *   - Memory read/write:    3 T-states
 *   - I/O read/write:       4 T-states (extra wait state)
 *   - Interrupt ack:        varies by mode (IM0/1/2)
 *
 * Bus signals shared with 6502 family via bus_state_t:
 *   - ADDR, DATA, RW, IRQ, NMI, RES, RDY — same bit positions
 * Z80-specific signals use reserved bit positions:
 *   - MREQ, IORQ, M1, RFSH, HALT, BUSREQ, BUSACK, WAIT
 *
 * USAGE:
 * ======
 * ```cpp
 * #include "zilog_z80.h"   // or "zilog_z80a.h", etc.
 *
 * ZilogZ80A cpu;
 * bus_state_t pins = cpu.init();
 * pins = cpu.tick(pins);   // One T-state
 * ```
 */

#include <array>
#include <cstdint>
#include <cstring>

#include "z80_traits.hpp"
#include "z80_types.h"
#include "../cpu_chip_base.h"
#include "../../../core/system_lines.h"

namespace z80 {

// ============================================================================
// Z80-specific bus signal bit positions
// ============================================================================
// These use the reserved range in bus_state_t (bits 55-63 of output pins,
// bits 40-47 of input pins) that are not used by the 6502 family.

// Output pins (active-low accent convention matches Z80 datasheets)
#define Z80_MREQ_BIT    55   // Memory Request (active-low)
#define Z80_IORQ_BIT    56   // I/O Request (active-low)
#define Z80_M1_BIT      57   // Machine cycle 1 (opcode fetch indicator)
#define Z80_RFSH_BIT    58   // DRAM Refresh (active during refresh cycle)
#define Z80_HALT_BIT    59   // CPU halted

// Input pins
#define Z80_WAIT_BIT    43   // Wait (active-low, memory/IO inserts wait states)
#define Z80_BUSREQ_BIT  44   // Bus Request (active-low, DMA controller)
#define Z80_BUSACK_BIT  60   // Bus Acknowledge (output, active-low)
#define Z80_INT_BIT     BUS_IRQ_BIT  // Maskable interrupt (same as IRQ)
#define Z80_NMI_BIT     BUS_NMI_BIT  // Non-maskable interrupt (same as NMI)
#define Z80_RESET_BIT   BUS_RES_BIT  // Reset (same as RES)

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template <const Z80Traits& Traits>
class z80_t : public CpuChipBase {
public:
    // === Phase for split-phase tick (maps to Z80 T-state boundaries) ===
    enum class Phase {
        T1,     // Address setup, control signals
        T2,     // Data transfer (read: sample data, write: data valid)
        T3,     // Data latch / internal operation
        T4,     // Internal operation (M1 cycles only)
    };

    // === Compile-time feature detection ===
    static constexpr bool has_undocumented_ops() { return Traits.has_undocumented_ops(); }
    static constexpr bool has_block_int_bug()    { return Traits.has_block_int_bug(); }
    static constexpr bool has_q_register()       { return Traits.has_q_register(); }
    static constexpr bool has_mmu()              { return Traits.has_mmu(); }
    static constexpr bool is_nmos()              { return Traits.is_nmos(); }

    // === Default bus state with Z80 pull-ups ===
    static constexpr bus_state_t default_bus_state() {
        bus_state_t s = BUS_BIT(BUS_RW_BIT)       |  // R/W high (read mode)
                        BUS_BIT(BUS_RDY_BIT)      |  // Ready
                        BUS_BIT(BUS_RES_BIT)      |  // Reset deasserted
                        BUS_BIT(BUS_IRQ_BIT)      |  // INT deasserted (active-low)
                        BUS_BIT(BUS_NMI_BIT)      |  // NMI deasserted (active-low)
                        BUS_BIT(Z80_MREQ_BIT)     |  // MREQ deasserted (active-low)
                        BUS_BIT(Z80_IORQ_BIT)     |  // IORQ deasserted (active-low)
                        BUS_BIT(Z80_WAIT_BIT);       // WAIT deasserted (active-low)
        return s;
    }

    // === Chip identity ===
    z80_t() : CpuChipBase(ChipInfo(Traits.chip_id, Traits.vendor)) {
        display_name_ = Traits.chip_id;
        short_name_   = Traits.chip_id;
    }

    // === Lifecycle ===

    /// Initialize CPU state. Returns default bus state.
    bus_state_t init() {
        std::memset(&regs_, 0, sizeof(regs_));
        regs_.sp = 0xFFFF;
        regs_.af = 0xFFFF;  // Documented power-on state
        regs_.iff1 = 0;
        regs_.iff2 = 0;
        regs_.im = 0;
        halted_ = false;
        t_state_ = 0;
        m_cycle_ = MCycle::M1;
        return default_bus_state();
    }

    /// Reset CPU (active-low RESET held for at least 3 clock cycles).
    bus_state_t reset(bus_state_t pins) {
        regs_.pc = 0x0000;
        regs_.sp = 0xFFFF;
        regs_.af = 0xFFFF;
        regs_.i  = 0;
        regs_.r  = 0;
        regs_.iff1 = 0;
        regs_.iff2 = 0;
        regs_.im = 0;
        halted_ = false;
        t_state_ = 0;
        m_cycle_ = MCycle::M1;
        BUS_SET_ADDR(pins, 0x0000);
        return pins;
    }

    /// Execute one T-state.  External code must service the bus between calls.
    /// Returns bus state with address/data/control signals for the current T-state.
    bus_state_t tick(bus_state_t pins) {
        // TODO: Full Z80 T-state machine implementation
        // This is the declaration/stub — the actual instruction decoder,
        // M-cycle state machine, and bus protocol will be implemented
        // following the fam65xx split-phase pattern.
        (void)pins;
        return pins;
    }

    // === Register access (for debugger/test harness) ===
    uint16_t pc() const { return regs_.pc; }
    uint16_t sp() const { return regs_.sp; }
    uint16_t af() const { return regs_.af; }
    uint16_t bc() const { return regs_.bc; }
    uint16_t de() const { return regs_.de; }
    uint16_t hl() const { return regs_.hl; }
    uint16_t ix() const { return regs_.ix; }
    uint16_t iy() const { return regs_.iy; }
    uint8_t  i()  const { return regs_.i; }
    uint8_t  r()  const { return regs_.r; }
    uint8_t  im() const { return regs_.im; }
    bool iff1()   const { return regs_.iff1; }
    bool iff2()   const { return regs_.iff2; }
    bool halted() const { return halted_; }

    void set_pc(uint16_t v) { regs_.pc = v; }

    // === Shadow register access ===
    uint16_t af_prime() const { return regs_.af_; }
    uint16_t bc_prime() const { return regs_.bc_; }
    uint16_t de_prime() const { return regs_.de_; }
    uint16_t hl_prime() const { return regs_.hl_; }

private:
    // === Machine cycle types ===
    enum class MCycle : uint8_t {
        M1,         // Opcode fetch
        MEM_READ,   // Memory read
        MEM_WRITE,  // Memory write
        IO_READ,    // I/O read
        IO_WRITE,   // I/O write
        INT_ACK,    // Interrupt acknowledge
        REFRESH,    // DRAM refresh (overlaps M1 T3-T4)
    };

    // === Register file ===
    struct Registers {
        // Main register set
        union { struct { uint8_t f, a; }; uint16_t af; };
        union { struct { uint8_t c, b; }; uint16_t bc; };
        union { struct { uint8_t e, d; }; uint16_t de; };
        union { struct { uint8_t l, h; }; uint16_t hl; };

        // Alternate (shadow) register set
        uint16_t af_, bc_, de_, hl_;

        // Index registers
        uint16_t ix, iy;

        // Special registers
        uint16_t sp;    // Stack pointer
        uint16_t pc;    // Program counter
        uint8_t  i;     // Interrupt vector page
        uint8_t  r;     // DRAM refresh counter (7 bits + bit 7 preserved)

        // Interrupt state
        uint8_t  im;    // Interrupt mode (0, 1, 2)
        bool     iff1;  // Interrupt flip-flop 1 (master enable)
        bool     iff2;  // Interrupt flip-flop 2 (saved during NMI)

        // Internal temp register (for multi-byte operations)
        uint8_t  wz_h, wz_l;  // WZ (MEMPTR) — internal but observable via flags
    };

    Registers regs_{};
    bool      halted_ = false;

    // T-state machine
    uint8_t   t_state_ = 0;     // Current T-state within M-cycle (0-based)
    MCycle    m_cycle_ = MCycle::M1;

    // Bus snapshot for edge detection
    bus_state_t bus_prev_ = 0;
};

} // namespace z80
