#pragma once
/*
 * z80.hpp — Zilog Z80 Family CPU Emulator (Main Template)
 *
 * Template-based Z80 CPU core using NTTP (non-type template parameters),
 * following the fam65xx pattern.  Each Z80 variant (Z80, Z80A, Z80B, U880)
 * is a distinct template instantiation with compile-time feature gating
 * via `if constexpr`.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * 1. Zero overhead abstractions — templates eliminate runtime dispatch
 * 2. Conditional features — `if constexpr` for variant-specific behavior
 * 3. Pin-accurate bus interface — bus_state_t pins protocol
 * 4. Cycle-exact T-state stepping — each tick() = one T-state
 * 5. Chips are system-agnostic — no includes of system headers
 *
 * Z80 BUS PROTOCOL:
 * =================
 * The Z80 uses M-cycles (machine cycles) composed of T-states:
 *   - M1 (opcode fetch):    4 T-states (+ wait states)
 *   - Memory read/write:    3 T-states
 *   - I/O read/write:       4 T-states (extra wait state)
 *   - Interrupt ack:        varies by mode (IM0/1/2)
 *
 * Bus signals shared with 6502 family via bus_state_t:
 *   - ADDR, DATA, RW, IRQ, NMI, RES, RDY — same bit positions
 * Z80-specific signals use reserved bit positions:
 *   - MREQ, IORQ, M1, RFSH, HALT, BUSREQ, BUSACK, WAIT
 *
 * EXECUTION MODEL:
 * ================
 * Each tick() call advances the CPU by one T-state. The system must service
 * the bus between calls (read/write memory, respond to I/O).
 *
 * Instruction handlers use a step counter (step_) with switch(step_++)
 * to manage their T-states. Simple 4T instructions execute inline during
 * the M1 decode phase. Multi-cycle instructions transition to dedicated
 * handler functions.
 *
 * USAGE:
 * ======
 * ```cpp
 * #include "chip/cpu/z80/zilog_z80a.hpp"
 *
 * ZilogZ80A cpu;
 * bus_state_t pins = cpu.init();
 * pins = cpu.tick(pins);   // One T-state
 * ```
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <utility> // std::swap

#include "chip/cpu/z80/z80_traits.hpp"
#include "chip/cpu/z80/z80_types.hpp"
#include "chip/cpu/z80/z80_opcodes.hpp"
#include "chip/cpu/cpu_chip_base.hpp"
#include "core/system_lines.hpp"
#include "core/cermu.hpp"
#include "core/register_file.hpp"

// Opcode table generation — self-contained with own namespace wrapper
#include "chip/cpu/z80/z80_opcode_tables.inc.hpp"

namespace z80 {

// ============================================================================
// Z80-specific bus signal bit positions
// ============================================================================
// These use the reserved range in bus_state_t (bits 55-63 of output pins,
// bits 40-47 of input pins) that are not used by the 6502 family.

// Output pins (active-low convention matches Z80 datasheets)
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
// Register file constants — typed indices into RegisterFile<28, uint16_t>.
// ============================================================================

// Main register pairs
constexpr r16 REG_AF{0};                   // AF (A=hi, F=lo)
constexpr r8  REG_A  = hi_b(REG_AF);      // Accumulator
constexpr r8  REG_F  = lo_b(REG_AF);      // Flags
constexpr r16 REG_BC{2};                   // BC
constexpr r8  REG_B  = hi_b(REG_BC);
constexpr r8  REG_C  = lo_b(REG_BC);
constexpr r16 REG_DE{4};                   // DE
constexpr r8  REG_D  = hi_b(REG_DE);
constexpr r8  REG_E  = lo_b(REG_DE);
constexpr r16 REG_HL{6};                   // HL
constexpr r8  REG_H  = hi_b(REG_HL);
constexpr r8  REG_L  = lo_b(REG_HL);

// Index registers
constexpr r16 REG_IX{8};
constexpr r16 REG_IY{10};

// Shadow (alternate) register set
constexpr r16 REG_AF_{12};
constexpr r16 REG_BC_{14};
constexpr r16 REG_DE_{16};
constexpr r16 REG_HL_{18};

// Internal WZ register (MEMPTR)
constexpr r16 REG_WZ{20};
constexpr r8  REG_W  = hi_b(REG_WZ);
constexpr r8  REG_Z  = lo_b(REG_WZ);

// Special registers
constexpr r8  REG_I{22};                   // Interrupt vector page
constexpr r8  REG_R{23};                   // DRAM refresh counter

// Stack pointer and program counter
constexpr r16 REG_SP{24};
constexpr r16 REG_PC{26};

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template <const Z80Traits& Traits>
class z80_t : public CpuChipBase {
public:
    // === Handler function pointer type ===
    using InstructionHandler = bus_state_t (z80_t::*)(bus_state_t);

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
                        BUS_BIT(Z80_M1_BIT)       |  // M1 deasserted
                        BUS_BIT(Z80_RFSH_BIT)     |  // RFSH deasserted
                        BUS_BIT(Z80_HALT_BIT)     |  // HALT deasserted
                        BUS_BIT(Z80_WAIT_BIT)     |  // WAIT deasserted (active-low)
                        BUS_BIT(Z80_BUSREQ_BIT)   |  // BUSREQ deasserted (active-low)
                        BUS_BIT(Z80_BUSACK_BIT);     // BUSACK deasserted
        return s;
    }

    // === Chip identity ===
    z80_t() : CpuChipBase(ChipInfo(Traits.chip_id, Traits.vendor)) {
        display_name_ = Traits.chip_id;
        short_name_   = Traits.chip_id;
    }

    // === Lifecycle ===

    /// Initialize CPU state. Returns default bus state.
    bus_state_t init() override {
        std::memset(regs_.data, 0, sizeof(regs_.data));
        regs_[REG_SP] = 0xFFFF;
        regs_[REG_AF] = 0xFFFF;  // Documented power-on state
        im_ = 0;
        iff1_ = false;
        iff2_ = false;
        halted_ = false;
        ei_pending_ = false;
        nmi_pending_ = false;
        ix_iy_prefix_ = 0;
        prefix_state_ = PREFIX_NONE;
        step_ = 0;
        current_handler_ = &z80_t::m1_fetch;
        bus_prev_ = default_bus_state();
        return default_bus_state();
    }

    /// Reset CPU (active-low RESET held for at least 3 clock cycles).
    bus_state_t reset(bus_state_t pins = 0) override {
        regs_[REG_PC] = 0x0000;
        regs_[REG_SP] = 0xFFFF;
        regs_[REG_AF] = 0xFFFF;
        regs_[REG_I]  = 0;
        regs_[REG_R]  = 0;
        im_ = 0;
        iff1_ = false;
        iff2_ = false;
        halted_ = false;
        ei_pending_ = false;
        nmi_pending_ = false;
        ix_iy_prefix_ = 0;
        prefix_state_ = PREFIX_NONE;
        step_ = 0;
        current_handler_ = &z80_t::m1_fetch;
        BUS_SET_BIT(pins, Z80_HALT_BIT);  // Deassert HALT
        return pins;
    }

    /// Execute one T-state.  External code must service the bus between calls.
    /// Returns bus state with address/data/control signals for the current T-state.
    bus_state_t tick(bus_state_t pins) {
        // RESET check (active-low) — held for at least 3 T-states
        if (unlikely(!BUS_GET_BIT(pins, Z80_RESET_BIT))) {
            pins = reset(pins);
            bus_prev_ = pins;
            return pins;
        }

        // BUSREQ check (active-low) — DMA controller requests bus
        if (unlikely(!BUS_GET_BIT(pins, Z80_BUSREQ_BIT))) {
            BUS_CLR_BIT(pins, Z80_BUSACK_BIT); // Acknowledge
            bus_prev_ = pins;
            return pins; // CPU tri-states, doesn't process
        }
        BUS_SET_BIT(pins, Z80_BUSACK_BIT); // Release BUSACK

        // Normal execution — dispatch to current handler
        pins = (this->*current_handler_)(pins);
        bus_prev_ = pins;
        return pins;
    }

    // === Register access (for debugger/test harness) ===
    uint16_t pc() const { return regs_[REG_PC]; }
    uint16_t sp() const { return regs_[REG_SP]; }
    uint16_t af() const { return regs_[REG_AF]; }
    uint16_t bc() const { return regs_[REG_BC]; }
    uint16_t de() const { return regs_[REG_DE]; }
    uint16_t hl() const { return regs_[REG_HL]; }
    uint16_t ix() const { return regs_[REG_IX]; }
    uint16_t iy() const { return regs_[REG_IY]; }
    uint8_t  i()  const { return regs_[REG_I]; }
    uint8_t  r()  const { return regs_[REG_R]; }
    uint8_t  im() const { return im_; }
    bool iff1()   const { return iff1_; }
    bool iff2()   const { return iff2_; }
    bool halted() const { return halted_; }
    uint16_t wz() const { return regs_[REG_WZ]; }

    void set_pc(uint16_t v) { regs_[REG_PC] = v; }
    void set_sp(uint16_t v) { regs_[REG_SP] = v; }
    void set_af(uint16_t v) { regs_[REG_AF] = v; }
    void set_bc(uint16_t v) { regs_[REG_BC] = v; }
    void set_de(uint16_t v) { regs_[REG_DE] = v; }
    void set_hl_direct(uint16_t v) { regs_[REG_HL] = v; }
    void set_ix(uint16_t v) { regs_[REG_IX] = v; }
    void set_iy(uint16_t v) { regs_[REG_IY] = v; }
    void set_i(uint8_t v)   { regs_[REG_I] = v; }
    void set_r(uint8_t v)   { regs_[REG_R] = v; }
    void set_im(uint8_t v)  { im_ = v; }
    void set_iff1(bool v)   { iff1_ = v; }
    void set_iff2(bool v)   { iff2_ = v; }
    void set_wz(uint16_t v) { regs_[REG_WZ] = v; }

    // === Shadow register access ===
    uint16_t af_prime() const { return regs_[REG_AF_]; }
    uint16_t bc_prime() const { return regs_[REG_BC_]; }
    uint16_t de_prime() const { return regs_[REG_DE_]; }
    uint16_t hl_prime() const { return regs_[REG_HL_]; }

    void set_af_prime(uint16_t v) { regs_[REG_AF_] = v; }
    void set_bc_prime(uint16_t v) { regs_[REG_BC_] = v; }
    void set_de_prime(uint16_t v) { regs_[REG_DE_] = v; }
    void set_hl_prime(uint16_t v) { regs_[REG_HL_] = v; }

    // === Execution state access (for test harness) ===
    void set_halted(bool v)      { halted_ = v; }
    void set_ei_pending(bool v)  { ei_pending_ = v; }
    void set_q(bool v)           { q_ = v; }
    bool q() const               { return q_; }

    /// Returns true when the CPU is at an instruction boundary.
    /// Used by test harnesses to detect instruction completion.
    bool opdone() const {
        return current_handler_ == &z80_t::m1_fetch
            && step_ == 0
            && ix_iy_prefix_ == 0
            && prefix_state_ == PREFIX_NONE;
    }

private:
    // === Prefix state constants ===
    static constexpr uint8_t PREFIX_NONE = 0;
    static constexpr uint8_t PREFIX_CB   = 1;
    static constexpr uint8_t PREFIX_ED   = 2;

    // ========================================================================
    // REGISTER FILE
    // ========================================================================
    RegisterFile<28, uint16_t> regs_;

    // CPU state (not part of the programmer-visible register model)
    uint8_t  im_ = 0;      // Interrupt mode (0, 1, 2)
    bool     iff1_ = false; // Interrupt flip-flop 1 (master enable)
    bool     iff2_ = false; // Interrupt flip-flop 2 (saved during NMI)
    bool     q_ = false;    // Q flag (tracks F modification for SCF/CCF)

    // ========================================================================
    // REGISTER ACCESS HELPERS (included mid-class)
    // ========================================================================

#include "chip/cpu/z80/z80_registers.inc.hpp"

    // ========================================================================
    // ALU OPERATIONS AND FLAG TABLES (included mid-class)
    // ========================================================================

#include "chip/cpu/z80/z80_alu.inc.hpp"

    // ========================================================================
    // BUS SIGNAL HELPERS
    // ========================================================================

    /// Set up a memory read cycle (T1 of a memory read M-cycle)
    inline bus_state_t bus_setup_mem_read(bus_state_t pins, uint16_t addr) {
        BUS_SET_ADDR(pins, addr);
        BUS_CLR_BIT(pins, Z80_MREQ_BIT);  // Assert MREQ (active-low)
        BUS_SET_BIT(pins, BUS_RW_BIT);     // Read mode
        return pins;
    }

    /// Set up a memory write cycle (T1 of a memory write M-cycle)
    inline bus_state_t bus_setup_mem_write(bus_state_t pins, uint16_t addr, uint8_t data) {
        BUS_SET_ADDR(pins, addr);
        BUS_SET_DATA(pins, data);
        BUS_CLR_BIT(pins, Z80_MREQ_BIT);  // Assert MREQ
        BUS_CLR_BIT(pins, BUS_RW_BIT);    // Write mode
        return pins;
    }

    /// Finish a memory access cycle (T3: deassert MREQ)
    inline void bus_finish_mem(bus_state_t& pins) {
        BUS_SET_BIT(pins, Z80_MREQ_BIT);  // Deassert MREQ
        BUS_SET_BIT(pins, BUS_RW_BIT);    // Back to read idle
    }

    /// Set up an I/O read cycle (T1 of an I/O read M-cycle)
    inline bus_state_t bus_setup_io_read(bus_state_t pins, uint16_t port) {
        BUS_SET_ADDR(pins, port);
        BUS_CLR_BIT(pins, Z80_IORQ_BIT);  // Assert IORQ
        BUS_SET_BIT(pins, BUS_RW_BIT);    // Read mode
        return pins;
    }

    /// Set up an I/O write cycle
    inline bus_state_t bus_setup_io_write(bus_state_t pins, uint16_t port, uint8_t data) {
        BUS_SET_ADDR(pins, port);
        BUS_SET_DATA(pins, data);
        BUS_CLR_BIT(pins, Z80_IORQ_BIT);  // Assert IORQ
        BUS_CLR_BIT(pins, BUS_RW_BIT);    // Write mode
        return pins;
    }

    /// Finish an I/O access cycle
    inline void bus_finish_io(bus_state_t& pins) {
        BUS_SET_BIT(pins, Z80_IORQ_BIT);  // Deassert IORQ
        BUS_SET_BIT(pins, BUS_RW_BIT);
    }

    /// Check WAIT state (T2 of any M-cycle). Returns true if OK to proceed.
    /// If WAIT is asserted (active-low), decrements step_ to repeat.
    inline bool wait_check(bus_state_t pins) {
        if (!BUS_GET_BIT(pins, Z80_WAIT_BIT)) {
            step_--;
            return false;
        }
        return true;
    }

    // ========================================================================
    // TRANSITION HELPERS
    // ========================================================================

    /// Transition to M1 fetch for the next instruction.
    /// Clears prefix state (DD/FD/CB/ED all done).
    inline void transition_to_fetch() {
        // Update Q: if F was modified during this instruction, set Q = true
        q_ = (regs_[REG_F] != f_snapshot_);
        ix_iy_prefix_ = 0;
        prefix_state_ = PREFIX_NONE;
        current_handler_ = &z80_t::m1_fetch;
        step_ = 0;
    }

    /// Transition to M1 fetch for prefix continuation.
    /// Preserves DD/FD and CB/ED prefix state for the next M1 decode.
    /// Also preserves Q so prefixes don't break Q tracking for SCF/CCF.
    inline void transition_to_fetch_prefix() {
        prefix_fetch_ = true;
        current_handler_ = &z80_t::m1_fetch;
        step_ = 0;
    }

    /// Transition to an instruction handler for remaining M-cycles.
    inline void transition_to(InstructionHandler handler) {
        current_handler_ = handler;
        step_ = 0;
    }

    // ========================================================================
    // M1 OPCODE FETCH CYCLE (4 T-states)
    // ========================================================================
    //
    // T1: Address = PC on bus, assert /M1 + /MREQ + RD
    // T2: Sample opcode from data bus, check /WAIT, increment PC
    // T3: Refresh address (I:R) on bus, assert /RFSH + /MREQ, increment R
    // T4: Deassert signals, decode opcode and dispatch

    bus_state_t m1_fetch(bus_state_t pins) {
        switch (step_++) {
        case 0: { // T1: interrupt check + address setup
            // Save Q from previous instruction for SCF/CCF, then snapshot F
            // Skip during prefix fetches (DD/FD/CB/ED) — prefixes are transparent to Q
            if (!prefix_fetch_) {
                q_saved_ = q_;
                f_snapshot_ = regs_[REG_F];
                q_ = false;
            }
            prefix_fetch_ = false;

            // EI suppresses interrupt checking for one instruction.
            // The IFF flags were already set by the EI instruction itself.
            bool suppress_int = ei_pending_;
            if (ei_pending_) {
                ei_pending_ = false;
            }

            // NMI edge detection (falling edge of /NMI = HIGH→LOW transition)
            bool nmi_active = !BUS_GET_BIT(pins, Z80_NMI_BIT);
            bool nmi_was    = !BUS_GET_BIT(bus_prev_, Z80_NMI_BIT);
            if (nmi_active && !nmi_was) {
                nmi_pending_ = true;
            }

            // Check NMI (higher priority than INT, not suppressed by EI)
            if (nmi_pending_) {
                nmi_pending_ = false;
                if (halted_) {
                    halted_ = false;
                    BUS_SET_BIT(pins, Z80_HALT_BIT);
                }
                iff2_ = iff1_; // Save IFF1 state
                iff1_ = false;       // Disable interrupts
                transition_to(&z80_t::op_nmi);
                return pins; // 1 internal T-state consumed
            }

            // Check INT (level-sensitive, only when IFF1 is set)
            // Suppressed for one instruction after EI
            if (!suppress_int && iff1_ && !BUS_GET_BIT(pins, Z80_INT_BIT)) {
                if (halted_) {
                    halted_ = false;
                    BUS_SET_BIT(pins, Z80_HALT_BIT);
                }
                iff1_ = false;
                iff2_ = false;
                transition_to(&z80_t::op_int);
                return pins;
            }

            // Normal M1 fetch: place PC on address bus
            BUS_SET_ADDR(pins, regs_[REG_PC]);
            BUS_CLR_BIT(pins, Z80_M1_BIT);    // Assert M1
            BUS_CLR_BIT(pins, Z80_MREQ_BIT);  // Assert MREQ
            BUS_SET_BIT(pins, BUS_RW_BIT);     // Read mode
            return pins;
        }

        case 1: // T2: sample opcode + WAIT check
            if (!BUS_GET_BIT(pins, Z80_WAIT_BIT)) {
                step_--; // Insert TW state
                return pins;
            }
            opcode_ = BUS_GET_DATA(pins);
            regs_[REG_PC]++;
            // Deassert M1 and MREQ (will be reasserted for refresh)
            BUS_SET_BIT(pins, Z80_M1_BIT);
            BUS_SET_BIT(pins, Z80_MREQ_BIT);
            return pins;

        case 2: // T3: refresh cycle
            BUS_SET_ADDR(pins, (static_cast<uint16_t>(regs_[REG_I]) << 8) | (regs_[REG_R] & 0x7F));
            BUS_CLR_BIT(pins, Z80_RFSH_BIT);  // Assert RFSH
            BUS_CLR_BIT(pins, Z80_MREQ_BIT);  // Assert MREQ for refresh
            // Increment R counter (lower 7 bits, bit 7 preserved)
            regs_[REG_R] = (regs_[REG_R] & 0x80) | ((regs_[REG_R] + 1) & 0x7F);
            return pins;

        case 3: // T4: end refresh, decode and execute
            BUS_SET_BIT(pins, Z80_RFSH_BIT);   // Deassert RFSH
            BUS_SET_BIT(pins, Z80_MREQ_BIT);   // Deassert MREQ

            // Check prefix state from a previous M1 cycle
            if (prefix_state_ == PREFIX_CB) {
                prefix_state_ = PREFIX_NONE;
                // DD/FD + CB is handled directly in decode_and_execute,
                // so this path is only reached for plain CB prefix
                return decode_cb(pins, opcode_);
            }
            if (prefix_state_ == PREFIX_ED) {
                prefix_state_ = PREFIX_NONE;
                ix_iy_prefix_ = 0; // ED cancels DD/FD prefix
                return decode_ed(pins, opcode_);
            }

            return decode_and_execute(pins, opcode_);
        }
        return pins;
    }

    // ========================================================================
    // OPCODE DECODE — Base (unprefixed) instructions
    // ========================================================================
    //
    // Uses standard Z80 bit field extraction:
    //   x = bits 7-6,  y = bits 5-3,  z = bits 2-0
    //   p = bits 5-4 (= y >> 1),  q = bit 3 (= y & 1)
    //
    // 4T instructions execute inline. Multi-cycle transition to handlers.

    bus_state_t decode_and_execute(bus_state_t pins, uint8_t op) {
        const uint8_t x = (op >> 6) & 3;
        const uint8_t y = (op >> 3) & 7;
        const uint8_t z = op & 7;
        const uint8_t p = y >> 1;
        const uint8_t q = y & 1;

        switch (x) {
        case 0:
            switch (z) {
            case 0:
                switch (y) {
                case 0: // NOP
                    transition_to_fetch();
                    return pins;
                case 1: // EX AF,AF'
                    regs_.swap(REG_AF, REG_AF_);
                    transition_to_fetch();
                    return pins;
                case 2: // DJNZ d
                    transition_to(&z80_t::op_djnz);
                    return pins;
                case 3: // JR d
                    transition_to(&z80_t::op_jr_e);
                    return pins;
                default: // y=4..7: JR cc,d (cc = y-4)
                    transition_to(&z80_t::op_jr_cc_e);
                    return pins;
                }

            case 1:
                if (q == 0) {
                    // LD rr,nn
                    transition_to(&z80_t::op_ld_rr_nn);
                } else {
                    // ADD HL,rr
                    transition_to(&z80_t::op_add_hl_rr);
                }
                return pins;

            case 2:
                if (q == 0) {
                    switch (p) {
                    case 0: case 1: // LD (BC),A / LD (DE),A
                        transition_to(&z80_t::op_ld_indirect_a);
                        break;
                    case 2: // LD (nn),HL
                        transition_to(&z80_t::op_ld_nn_hl);
                        break;
                    case 3: // LD (nn),A
                        transition_to(&z80_t::op_ld_nn_a);
                        break;
                    }
                } else {
                    switch (p) {
                    case 0: case 1: // LD A,(BC) / LD A,(DE)
                        transition_to(&z80_t::op_ld_a_indirect);
                        break;
                    case 2: // LD HL,(nn)
                        transition_to(&z80_t::op_ld_hl_nn);
                        break;
                    case 3: // LD A,(nn)
                        transition_to(&z80_t::op_ld_a_nn);
                        break;
                    }
                }
                return pins;

            case 3:
                if (q == 0) {
                    transition_to(&z80_t::op_inc_rr);
                } else {
                    transition_to(&z80_t::op_dec_rr);
                }
                return pins;

            case 4: // INC r (or INC (HL))
                if (y == 6) {
                    transition_to(&z80_t::op_inc_hl);
                } else {
                    set_reg8(y, alu_inc(get_reg8(y)));
                    transition_to_fetch();
                }
                return pins;

            case 5: // DEC r (or DEC (HL))
                if (y == 6) {
                    transition_to(&z80_t::op_dec_hl);
                } else {
                    set_reg8(y, alu_dec(get_reg8(y)));
                    transition_to_fetch();
                }
                return pins;

            case 6: // LD r,n (or LD (HL),n)
                if (y == 6) {
                    transition_to(&z80_t::op_ld_hl_n);
                } else {
                    transition_to(&z80_t::op_ld_r_n);
                }
                return pins;

            case 7: // Accumulator operations
                switch (y) {
                case 0: alu_rlca(); break;
                case 1: alu_rrca(); break;
                case 2: alu_rla();  break;
                case 3: alu_rra();  break;
                case 4: alu_daa();  break;
                case 5: alu_cpl();  break;
                case 6: alu_scf();  break;
                case 7: alu_ccf();  break;
                }
                transition_to_fetch();
                return pins;
            }
            break;

        case 1: // LD block (0x40-0x7F)
            if (z == 6 && y == 6) {
                // HALT — 4T (executed inline during M1, like NOP)
                // PC already incremented by M1 fetch; halted_ flag controls
                // NOP re-execution until an interrupt arrives.
                halted_ = true;
                BUS_CLR_BIT(pins, Z80_HALT_BIT); // Assert HALT signal
                transition_to_fetch();
            } else if (z == 6) {
                // LD r,(HL)
                transition_to(&z80_t::op_ld_r_hl);
            } else if (y == 6) {
                // LD (HL),r
                transition_to(&z80_t::op_ld_hl_r);
            } else {
                // LD r,r' (4T — inline execution)
                set_reg8(y, get_reg8(z));
                transition_to_fetch();
            }
            return pins;

        case 2: // ALU A,r block (0x80-0xBF)
            if (z == 6) {
                // ALU A,(HL)
                transition_to(&z80_t::op_alu_hl);
            } else {
                // ALU A,r (4T — inline execution)
                alu_op(y, get_reg8(z));
                transition_to_fetch();
            }
            return pins;

        case 3: // Control block (0xC0-0xFF)
            switch (z) {
            case 0: // RET cc
                transition_to(&z80_t::op_ret_cc);
                return pins;

            case 1:
                if (q == 0) {
                    // POP rr
                    transition_to(&z80_t::op_pop);
                } else {
                    switch (p) {
                    case 0: // RET
                        transition_to(&z80_t::op_ret);
                        break;
                    case 1: // EXX
                        regs_.swap(REG_BC, REG_BC_);
                        regs_.swap(REG_DE, REG_DE_);
                        regs_.swap(REG_HL, REG_HL_);
                        transition_to_fetch();
                        break;
                    case 2: // JP (HL)
                        regs_[REG_PC] = get_hl();
                        transition_to_fetch();
                        break;
                    case 3: // LD SP,HL
                        transition_to(&z80_t::op_ld_sp_hl);
                        break;
                    }
                }
                return pins;

            case 2: // JP cc,nn
                transition_to(&z80_t::op_jp_cc_nn);
                return pins;

            case 3:
                switch (y) {
                case 0: // JP nn
                    transition_to(&z80_t::op_jp_nn);
                    break;
                case 1: // CB prefix
                    if (has_ix_iy_prefix()) {
                        // DD CB / FD CB: displacement + sub-opcode follow as non-M1 reads
                        transition_to(&z80_t::op_ddfd_cb);
                    } else {
                        prefix_state_ = PREFIX_CB;
                        transition_to_fetch_prefix();
                    }
                    break;
                case 2: // OUT (n),A
                    transition_to(&z80_t::op_out_n_a);
                    break;
                case 3: // IN A,(n)
                    transition_to(&z80_t::op_in_a_n);
                    break;
                case 4: // EX (SP),HL
                    transition_to(&z80_t::op_ex_sp_hl);
                    break;
                case 5: // EX DE,HL
                    regs_.swap(REG_DE, REG_HL);
                    transition_to_fetch();
                    break;
                case 6: // DI
                    iff1_ = false;
                    iff2_ = false;
                    transition_to_fetch();
                    break;
                case 7: // EI
                    // EI sets IFF1/IFF2 immediately, but the Z80 does not
                    // check interrupts until after the NEXT instruction.
                    // ei_pending_ suppresses the interrupt check in the
                    // following M1 fetch cycle.
                    iff1_ = true;
                    iff2_ = true;
                    ei_pending_ = true;
                    transition_to_fetch();
                    break;
                }
                return pins;

            case 4: // CALL cc,nn
                transition_to(&z80_t::op_call_cc_nn);
                return pins;

            case 5:
                if (q == 0) {
                    // PUSH rr
                    transition_to(&z80_t::op_push);
                } else {
                    switch (p) {
                    case 0: // CALL nn
                        transition_to(&z80_t::op_call_nn);
                        break;
                    case 1: // DD prefix (IX)
                        ix_iy_prefix_ = 0xDD;
                        transition_to_fetch_prefix();
                        break;
                    case 2: // ED prefix (extended)
                        prefix_state_ = PREFIX_ED;
                        transition_to_fetch_prefix();
                        break;
                    case 3: // FD prefix (IY)
                        ix_iy_prefix_ = 0xFD;
                        transition_to_fetch_prefix();
                        break;
                    }
                }
                return pins;

            case 6: // ALU A,n
                transition_to(&z80_t::op_alu_n);
                return pins;

            case 7: // RST p*8
                transition_to(&z80_t::op_rst);
                return pins;
            }
            break;
        }

        // Should not reach here — treat as NOP
        transition_to_fetch();
        return pins;
    }

    // ========================================================================
    // CB PREFIX DECODE
    // ========================================================================
    //
    // CB opcodes: x=shift type, y=bit/op, z=register
    //   x=0: shift/rotate   x=1: BIT   x=2: RES   x=3: SET
    //
    // Register operations (z != 6): 8T total (M1:4 + M1:4) — execute inline.
    // (HL) operations (z == 6): need extra memory access — transition to handler.

    bus_state_t decode_cb(bus_state_t pins, uint8_t op) {
        cb_opcode_ = op;
        const uint8_t x = (op >> 6) & 3;
        const uint8_t y = (op >> 3) & 7;
        const uint8_t z = op & 7;

        if (z == 6) {
            // (HL) variants — need memory access
            switch (x) {
            case 0: transition_to(&z80_t::op_cb_shift_hl); break;
            case 1: transition_to(&z80_t::op_cb_bit_hl);   break;
            default: transition_to(&z80_t::op_cb_setres_hl); break; // 2=RES, 3=SET
            }
            return pins;
        }

        // Register variants — execute inline (8T total, inline at T4 of 2nd M1)
        switch (x) {
        case 0: // shift/rotate
            set_reg8_direct(z, cb_shift_op(y, get_reg8_direct(z)));
            break;
        case 1: // BIT
            alu_bit(y, get_reg8_direct(z));
            break;
        case 2: // RES
            set_reg8_direct(z, get_reg8_direct(z) & ~(1 << y));
            break;
        case 3: // SET
            set_reg8_direct(z, get_reg8_direct(z) | (1 << y));
            break;
        }
        transition_to_fetch();
        return pins;
    }

    // ========================================================================
    // ED PREFIX DECODE
    // ========================================================================
    //
    // ED opcodes:
    //   x=1: most valid ED instructions
    //   x=2, y>=4, z<=3: block instructions (LDI, CPI, INI, OUTI, etc.)
    //   Other combinations: NOP (invalid but harmless)

    bus_state_t decode_ed(bus_state_t pins, uint8_t op) {
        ed_opcode_ = op;
        const uint8_t x = (op >> 6) & 3;
        const uint8_t y = (op >> 3) & 7;
        const uint8_t z = op & 7;
        const uint8_t q = y & 1;

        if (x == 1) {
            switch (z) {
            case 0: // IN r,(C)
                transition_to(&z80_t::op_in_r_c);
                return pins;

            case 1: // OUT (C),r
                transition_to(&z80_t::op_out_c_r);
                return pins;

            case 2: // SBC/ADC HL,rr
                transition_to(&z80_t::op_adc_sbc_hl);
                return pins;

            case 3: // LD (nn),rr / LD rr,(nn)
                if (q == 0) {
                    transition_to(&z80_t::op_ld_nn_rr);
                } else {
                    transition_to(&z80_t::op_ed_ld_rr_nn);
                }
                return pins;

            case 4: // NEG (all y-values are duplicates)
                alu_neg();
                transition_to_fetch();
                return pins;

            case 5: // RETI / RETN
                transition_to(&z80_t::op_reti_retn);
                return pins;

            case 6: { // IM
                static constexpr uint8_t im_table[8] = { 0, 0, 1, 2, 0, 0, 1, 2 };
                im_ = im_table[y];
                transition_to_fetch();
                return pins;
            }

            case 7:
                switch (y) {
                case 0: // LD I,A (ED 47, y=0)
                case 1: // LD R,A (ED 4F, y=1)
                    transition_to(&z80_t::op_ld_ir_a);
                    return pins;
                case 2: // LD A,I (ED 57, y=2)
                case 3: // LD A,R (ED 5F, y=3)
                    transition_to(&z80_t::op_ld_a_ir);
                    return pins;
                case 4: // RRD
                    transition_to(&z80_t::op_rrd);
                    return pins;
                case 5: // RLD
                    transition_to(&z80_t::op_rld);
                    return pins;
                default: // NOP (y=6, y=7)
                    transition_to_fetch();
                    return pins;
                }
            }
        }

        if (x == 2 && z <= 3 && y >= 4) {
            // Block instructions
            // y=4: LDI/CPI/INI/OUTI (z=0/1/2/3)
            // y=5: LDD/CPD/IND/OUTD
            // y=6: LDIR/CPIR/INIR/OTIR
            // y=7: LDDR/CPDR/INDR/OTDR
            switch (z) {
            case 0: // Block transfer (LDI/LDD/LDIR/LDDR)
                if (y <= 5) {
                    transition_to(&z80_t::op_ldi_ldd);
                } else {
                    transition_to(&z80_t::op_ldir_lddr);
                }
                return pins;
            case 1: // Block search (CPI/CPD/CPIR/CPDR)
                if (y <= 5) {
                    transition_to(&z80_t::op_cpi_cpd);
                } else {
                    transition_to(&z80_t::op_cpir_cpdr);
                }
                return pins;
            case 2: // Block input (INI/IND/INIR/INDR)
                if (y <= 5) {
                    transition_to(&z80_t::op_ini_ind);
                } else {
                    transition_to(&z80_t::op_inir_indr);
                }
                return pins;
            case 3: // Block output (OUTI/OUTD/OTIR/OTDR)
                if (y <= 5) {
                    transition_to(&z80_t::op_outi_outd);
                } else {
                    transition_to(&z80_t::op_otir_otdr);
                }
                return pins;
            }
        }

        // Invalid ED opcode — treat as NOP (8T consumed)
        transition_to_fetch();
        return pins;
    }

    // ========================================================================
    // INTERRUPT HANDLERS
    // ========================================================================

    /// NMI handler — 11T (5 internal + 3 push PCH + 3 push PCL)
    bus_state_t op_nmi(bus_state_t pins) {
        switch (step_++) {
        case 0: case 1: case 2: case 3: case 4: // 5 internal T-states
            return pins;
        case 5:
            regs_[REG_SP]--;
            return bus_setup_mem_write(pins, regs_[REG_SP], static_cast<uint8_t>(regs_[REG_PC] >> 8));
        case 6: if (!wait_check(pins)) return pins; return pins;
        case 7:
            bus_finish_mem(pins);
            regs_[REG_SP]--;
            return pins;
        case 8:
            return bus_setup_mem_write(pins, regs_[REG_SP], static_cast<uint8_t>(regs_[REG_PC] & 0xFF));
        case 9: if (!wait_check(pins)) return pins; return pins;
        case 10:
            bus_finish_mem(pins);
            regs_[REG_PC] = 0x0066;
            regs_[REG_WZ] = 0x0066;
            transition_to_fetch();
            return pins;
        }
        return pins;
    }

    /// INT handler — 13T for IM0/IM1, 19T for IM2
    /// IM0: RST 38h (simplified — real Z80 reads opcode from bus)
    /// IM1: RST 38h (always)
    /// IM2: jump to vector at I*256 + data_bus_byte
    bus_state_t op_int(bus_state_t pins) {
        switch (step_++) {
        case 0: case 1: // 2 internal T-states
            return pins;
        case 2: // Interrupt acknowledge: assert /IORQ + /M1
            BUS_CLR_BIT(pins, Z80_IORQ_BIT);
            BUS_CLR_BIT(pins, Z80_M1_BIT);
            return pins;
        case 3: case 4: // 2 more internal T-states (WAIT possible)
            if (step_ == 4) {
                int_data_latch_ = BUS_GET_DATA(pins); // Latch data bus for IM2
                BUS_SET_BIT(pins, Z80_IORQ_BIT);
                BUS_SET_BIT(pins, Z80_M1_BIT);
            }
            return pins;
        case 5: case 6: // 2 more internal
            return pins;
        case 7:
            regs_[REG_SP]--;
            return bus_setup_mem_write(pins, regs_[REG_SP], static_cast<uint8_t>(regs_[REG_PC] >> 8));
        case 8: if (!wait_check(pins)) return pins; return pins;
        case 9:
            bus_finish_mem(pins);
            regs_[REG_SP]--;
            return pins;
        case 10:
            return bus_setup_mem_write(pins, regs_[REG_SP], static_cast<uint8_t>(regs_[REG_PC] & 0xFF));
        case 11: if (!wait_check(pins)) return pins; return pins;
        case 12:
            bus_finish_mem(pins);
            switch (im_) {
            case 0: // IM 0: execute instruction from data bus (simplified as RST 38h)
            case 1: // IM 1: always RST 38h
                regs_[REG_PC] = 0x0038;
                regs_[REG_WZ] = regs_[REG_PC];
                transition_to_fetch();
                return pins;
            case 2: // IM 2: vectored — need to read 2 bytes from vector table
                addr_latch_ = (static_cast<uint16_t>(regs_[REG_I]) << 8) | (int_data_latch_ & 0xFE);
                transition_to(&z80_t::op_int_im2_read);
                return pins;
            }
            transition_to_fetch();
            return pins;
        }
        return pins;
    }

    /// IM 2 vector table read — reads 16-bit address from (I*256 + data)
    bus_state_t op_int_im2_read(bus_state_t pins) {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, addr_latch_);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            data_latch_ = BUS_GET_DATA(pins);
            bus_finish_mem(pins);
            return pins;
        case 3: return bus_setup_mem_read(pins, addr_latch_ + 1);
        case 4: if (!wait_check(pins)) return pins; return pins;
        case 5:
            regs_[REG_PC] = data_latch_ | (static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8);
            regs_[REG_WZ] = regs_[REG_PC];
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
        return pins;
    }

    // ========================================================================
    // INSTRUCTION HANDLER IMPLEMENTATIONS (included with template context)
    // ========================================================================

#define Z80_TEMPLATE_CONTEXT
#include "chip/cpu/z80/operations/z80_base_ops.inc.hpp"
#include "chip/cpu/z80/operations/z80_cb_ops.inc.hpp"
#include "chip/cpu/z80/operations/z80_ed_ops.inc.hpp"
#undef Z80_TEMPLATE_CONTEXT

    // ========================================================================
    // INTERNAL STATE
    // ========================================================================

    // Instruction handler dispatch
    InstructionHandler current_handler_ = &z80_t::m1_fetch;
    uint8_t step_ = 0;         // T-state counter within current handler

    // Current instruction context
    uint8_t opcode_ = 0;       // Base opcode (from M1 fetch)
    uint8_t cb_opcode_ = 0;    // CB sub-opcode
    uint8_t ed_opcode_ = 0;    // ED sub-opcode

    // Prefix state
    uint8_t ix_iy_prefix_ = 0; // 0 = none, 0xDD = IX, 0xFD = IY
    uint8_t prefix_state_ = PREFIX_NONE; // CB or ED prefix pending

    // Execution state
    bool halted_ = false;
    bool ei_pending_ = false;   // EI delays interrupt enable by one instruction
    bool nmi_pending_ = false;  // NMI edge detected, waiting to be serviced

    // Temporary latches for multi-cycle operations
    int8_t   displacement_ = 0;    // IX/IY+d displacement byte
    uint8_t  data_latch_ = 0;      // Temporary data storage
    uint16_t addr_latch_ = 0;      // Temporary address assembly
    uint8_t  int_data_latch_ = 0;  // Data bus value during INT ack (for IM2)

    // Bus state snapshot for edge detection (NMI, etc.)
    bus_state_t bus_prev_ = 0;

    // Q register support: saved Q from previous instruction + F snapshot for change detection
    bool q_saved_ = false;       // Q value from previous instruction (used by SCF/CCF)
    uint8_t f_snapshot_ = 0;     // F at instruction start (to detect if instruction modified flags)
    bool prefix_fetch_ = false;  // Next M1 is a prefix continuation (DD/FD/CB/ED), skip Q update

    // ========================================================================
    // ChipBase VIRTUAL METHOD IMPLEMENTATIONS
    // ========================================================================

#ifdef CERMU_HAS_GUI
    // Declared here, defined in z80_gui.cpp with explicit instantiations
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif
};

} // namespace z80
