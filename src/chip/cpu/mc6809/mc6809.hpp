#pragma once
/*
 * mc6809.hpp — Motorola 6809 Family CPU Emulator (Main Template)
 *
 * Template-based MC6809 CPU core using NTTP (non-type template parameters),
 * following the fam65xx/Z80 pattern.  Each 6809 variant (MC6809, MC6809E,
 * HD6309) is a distinct template instantiation with compile-time feature
 * gating via `if constexpr`.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * 1. Zero overhead abstractions — templates eliminate runtime dispatch
 * 2. Conditional features — `if constexpr` for HD6309 extensions
 * 3. Pin-accurate bus interface — bus_state_t pins protocol
 * 4. Cycle-exact stepping — each tick() = one bus cycle (E clock cycle)
 * 5. Chips are system-agnostic — no includes of system headers
 *
 * MC6809 BUS PROTOCOL:
 * ====================
 * The MC6809 uses a two-phase clock (E and Q) with each bus cycle consisting
 * of one E clock period.  Memory is accessed when E is high.
 *
 *   - Address valid: rising edge of Q (before E goes high)
 *   - Data transfer: during E high phase
 *   - R/W signal stable before rising edge of E
 *
 * Bus signals shared with other CPU families via bus_state_t:
 *   - ADDR, DATA, RW, IRQ, NMI, RES — same bit positions
 * MC6809-specific signals:
 *   - FIRQ, BA, BS, HALT, E, Q, AVMA, BUSY, LIC
 *
 * EXECUTION MODEL:
 * ================
 * Each tick() call advances the CPU by one bus cycle.  The system must
 * service the bus between calls.  Instructions take variable numbers of
 * cycles; the handler-based FSM manages multi-cycle operations.
 *
 * USAGE:
 * ======
 * ```cpp
 * #include "chip/cpu/mc6809/motorola_mc6809.hpp"
 *
 * MotorolaMC6809 cpu;
 * bus_state_t pins = cpu.init();
 * pins = cpu.tick(pins);   // One bus cycle
 * ```
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <utility>

#include "chip/cpu/mc6809/mc6809_traits.hpp"
#include "chip/cpu/mc6809/mc6809_types.hpp"
#include "chip/cpu/cpu_chip_base.hpp"
#include "core/system_lines.hpp"
#include "core/cermu.hpp"

namespace mc6809 {

// ============================================================================
// MC6809-specific bus signal bit positions
// ============================================================================
// Uses reserved range in bus_state_t not used by fam65xx or Z80 families.

// Input pins
#define MC6809_FIRQ_BIT     40   // Fast interrupt request (active-low)
#define MC6809_HALT_BIT     41   // Halt (active-low)

// Output pins
#define MC6809_BA_BIT       55   // Bus Available (active-high)
#define MC6809_BS_BIT       56   // Bus Status (active-high)
#define MC6809_AVMA_BIT     57   // Advanced Valid Memory Address
#define MC6809_BUSY_BIT     58   // Busy (16-bit operations)
#define MC6809_LIC_BIT      59   // Last Instruction Cycle
#define MC6809_E_BIT        50   // E clock output (reuses PHI2 position)
#define MC6809_Q_BIT        51   // Q clock output (reuses SP position)

// BA/BS combinations indicate processor state:
//   BA=0 BS=0: Normal (running)
//   BA=0 BS=1: Interrupt/reset acknowledge
//   BA=1 BS=0: SYNC acknowledge
//   BA=1 BS=1: Halted / bus granted

// ============================================================================
// MC6809 DECL — CPU register layout for debug infrastructure
// ============================================================================
//
// All internal registers are mirrored into a flat byte array (debug_regs_)
// in host-endian byte order.  The DECL table describes each register's byte
// offset and width, allowing the DECL renderer to display them without any
// hardcoded per-register callbacks.
//
// Byte layout (little-endian hosts; values stored via std::memcpy):
//   Offset  Size  Register
//   0-1     2     D   (Accumulator D = A:B, A is high byte)
//   2-3     2     X   (Index Register X)
//   4-5     2     Y   (Index Register Y)
//   6-7     2     U   (User Stack Pointer)
//   8-9     2     S   (System Stack Pointer)
//   10-11   2     PC  (Program Counter)
//   12      1     DP  (Direct Page Register)
//   13      1     CC  (Condition Code Register, with flag sub-fields)
//   14-15   2     W   (Accumulator W = E:F, HD6309 only)
//   16-17   2     V   (V Register, HD6309 only)
//   18      1     MD  (Mode Register, HD6309 only)

static constexpr uint16_t MC6809_DBG_SIZE = 19;

#define MC6809_DECL(REG, FLD, CMP) \
    REG( 0, D,   "D Accumulator (A:B)",  Value, 15:0) \
      FLD(D,  A,   15:8, "Accumulator A",  Value, 0, 0)  \
      FLD(D,  B,    7:0, "Accumulator B",  Value, 0, 0)  \
    REG( 2, X,   "Index Register X",     Address, 15:0) \
    REG( 4, Y,   "Index Register Y",     Address, 15:0) \
    REG( 6, U,   "User Stack Pointer",   Address, 15:0) \
    REG( 8, S,   "System Stack Pointer", Address, 15:0) \
    REG(10, PC,  "Program Counter",      Address, 15:0) \
    REG(12, DP,  "Direct Page Register", Value, 7:0)    \
    REG(13, CC,  "Condition Codes")                     \
      FLD(CC, E,   7:7, "Entire",         Flag, 0, 0)  \
      FLD(CC, F,   6:6, "FIRQ mask",      Flag, 0, 0)  \
      FLD(CC, H,   5:5, "Half carry",     Flag, 0, 0)  \
      FLD(CC, I,   4:4, "IRQ mask",       Flag, 0, 0)  \
      FLD(CC, N,   3:3, "Negative",       Flag, 0, 0)  \
      FLD(CC, Z,   2:2, "Zero",           Flag, 0, 0)  \
      FLD(CC, V,   1:1, "Overflow",       Flag, 0, 0)  \
      FLD(CC, C,   0:0, "Carry",          Flag, 0, 0)  \
    REG(14, W,   "W Accumulator (E:F)",  Value, 15:0)  \
      FLD(W,  E_ACC, 15:8, "Accumulator E", Value, 0, 0) \
      FLD(W,  F_ACC,  7:0, "Accumulator F", Value, 0, 0) \
    REG(16, V_REG, "V Register",         Address, 15:0) \
    REG(18, MD,  "Mode Register")                       \
      FLD(MD, FM,  1:1, "FIRQ mode",      Flag, 0, 0)  \
      FLD(MD, NM,  0:0, "Native mode",    Flag, 0, 0)

// --- Extract register byte-offset constants ---
namespace dbg {
MC6809_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} // namespace dbg

// --- Chip-local bitfield namespace ---
namespace fld {
#define MC6809_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
MC6809_DECL(DECL_REG_NOP, MC6809_X_FLD_NS_, DECL_CMP_NOP)
#undef MC6809_X_FLD_NS_
} // namespace fld

} // namespace mc6809

DECL_EXTRACT(MC6809, MC6809_DECL)

namespace mc6809 {

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template <const MC6809Traits& Traits>
class mc6809_t : public CpuChipBase {
public:
    // === Handler function pointer type ===
    using InstructionHandler = bus_state_t (mc6809_t::*)(bus_state_t);

    // === Compile-time feature detection ===
    static constexpr bool is_hd6309()        { return Traits.is_hd6309(); }
    static constexpr bool has_w_register()   { return Traits.has_w_register(); }
    static constexpr bool has_native_mode()  { return Traits.has_native_mode(); }

    // === Default bus state ===
    static constexpr bus_state_t default_bus_state() {
        bus_state_t s = BUS_BIT(BUS_RW_BIT)        |  // R/W high (read mode)
                        BUS_BIT(BUS_RDY_BIT)       |  // Ready
                        BUS_BIT(BUS_RES_BIT)       |  // Reset deasserted
                        BUS_BIT(BUS_IRQ_BIT)       |  // IRQ deasserted (active-low)
                        BUS_BIT(BUS_NMI_BIT)       |  // NMI deasserted (active-low)
                        BUS_BIT(MC6809_FIRQ_BIT)   |  // FIRQ deasserted (active-low)
                        BUS_BIT(MC6809_HALT_BIT)   |  // HALT deasserted (active-low)
                        BUS_BIT(MC6809_LIC_BIT);      // LIC asserted (ready for new instruction)
        return s;
    }

    // === Chip identity ===
    mc6809_t() : CpuChipBase(ChipInfo(Traits.chip_id, Traits.vendor)) {
        display_name_ = Traits.chip_id;
        short_name_   = Traits.chip_id;
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    // === Lifecycle ===

    /// Initialize CPU state.  Returns default bus state.
    bus_state_t init() override {
        std::memset(&regs_, 0, sizeof(regs_));
        regs_.s  = 0xFFFF;
        regs_.dp = 0x00;
        regs_.cc = Flags::I | Flags::F;  // IRQ and FIRQ masked at power-on
        halted_ = false;
        nmi_armed_ = false;
        nmi_pending_ = false;
        irq_pending_ = false;
        firq_pending_ = false;
        sync_wait_ = false;
        cwai_wait_ = false;
        step_ = 0;
        current_handler_ = &mc6809_t::op_reset;
        bus_prev_ = default_bus_state();
        if constexpr (has_native_mode()) {
            regs_.md = 0;
        }
        return default_bus_state();
    }

    /// Reset CPU (active-low RESET).
    bus_state_t reset(bus_state_t pins = 0) override {
        regs_.dp = 0x00;
        regs_.cc |= Flags::I | Flags::F;  // Mask interrupts
        halted_ = false;
        nmi_armed_ = false;
        nmi_pending_ = false;
        irq_pending_ = false;
        firq_pending_ = false;
        sync_wait_ = false;
        cwai_wait_ = false;
        step_ = 0;
        current_handler_ = &mc6809_t::op_reset;
        if constexpr (has_native_mode()) {
            regs_.md = 0;
        }
        return pins;
    }

    /// Execute one bus cycle.
    bus_state_t tick(bus_state_t pins) {
        // RESET check (active-low)
        if (unlikely(!BUS_GET_BIT(pins, BUS_RES_BIT))) {
            pins = reset(pins);
            bus_prev_ = pins;
            return pins;
        }

        // HALT check (active-low)
        if (unlikely(!BUS_GET_BIT(pins, MC6809_HALT_BIT))) {
            halted_ = true;
            BUS_SET_BIT(pins, MC6809_BA_BIT);  // BA=1
            BUS_SET_BIT(pins, MC6809_BS_BIT);  // BS=1 → halted
            bus_prev_ = pins;
            return pins;
        }
        if (halted_) {
            halted_ = false;
            BUS_CLR_BIT(pins, MC6809_BA_BIT);
            BUS_CLR_BIT(pins, MC6809_BS_BIT);
        }

        // Normal execution — dispatch to current handler
        pins = (this->*current_handler_)(pins);
        bus_prev_ = pins;
        return pins;
    }

    // === Register access (for debugger/test harness) ===
    uint8_t  a()  const { return regs_.a; }
    uint8_t  b()  const { return regs_.b; }
    uint16_t d()  const { return regs_.d; }
    uint16_t x()  const { return regs_.x; }
    uint16_t y()  const { return regs_.y; }
    uint16_t u()  const { return regs_.u; }
    uint16_t s()  const { return regs_.s; }
    uint16_t pc() const { return regs_.pc; }
    uint8_t  dp() const { return regs_.dp; }
    uint8_t  cc() const { return regs_.cc; }

    void set_a(uint8_t v)   { regs_.a = v; }
    void set_b(uint8_t v)   { regs_.b = v; }
    void set_d(uint16_t v)  { regs_.d = v; }
    void set_x(uint16_t v)  { regs_.x = v; }
    void set_y(uint16_t v)  { regs_.y = v; }
    void set_u(uint16_t v)  { regs_.u = v; }
    void set_s(uint16_t v)  { regs_.s = v; }
    void set_pc(uint16_t v) { regs_.pc = v; }
    void set_dp(uint8_t v)  { regs_.dp = v; }
    void set_cc(uint8_t v)  { regs_.cc = v; }

    // HD6309-specific register access
    uint8_t  e_reg() const {
        if constexpr (has_w_register()) return regs_.e;
        return 0;
    }
    uint8_t  f_reg() const {
        if constexpr (has_w_register()) return regs_.f;
        return 0;
    }
    uint16_t w()  const {
        if constexpr (has_w_register()) return regs_.w;
        return 0;
    }
    uint16_t v()  const {
        if constexpr (has_w_register()) return regs_.v;
        return 0;
    }
    uint8_t  md() const {
        if constexpr (has_native_mode()) return regs_.md;
        return 0;
    }
    void set_e_reg(uint8_t val) {
        if constexpr (has_w_register()) regs_.e = val;
    }
    void set_f_reg(uint8_t val) {
        if constexpr (has_w_register()) regs_.f = val;
    }
    void set_w(uint16_t val) {
        if constexpr (has_w_register()) regs_.w = val;
    }
    void set_v(uint16_t val) {
        if constexpr (has_w_register()) regs_.v = val;
    }
    void set_md(uint8_t val) {
        if constexpr (has_native_mode()) regs_.md = val;
    }

    /// Returns true when the CPU is at an instruction boundary.
    bool opdone() const {
        return current_handler_ == &mc6809_t::fetch_opcode
            && step_ == 0
            && !sync_wait_
            && !cwai_wait_;
    }

    bool halted() const { return halted_; }
    bool sync_waiting() const { return sync_wait_; }
    bool cwai_waiting() const { return cwai_wait_; }

private:
    // ========================================================================
    // REGISTER FILE
    // ========================================================================
    struct Registers {
        // Accumulators (D = A:B, big-endian: A is high byte)
        union { struct { uint8_t a, b; }; uint16_t d; };

        // Index registers
        uint16_t x, y;

        // Stack pointers
        uint16_t u;   // User stack pointer
        uint16_t s;   // System (hardware) stack pointer

        // Program counter
        uint16_t pc;

        // Control registers
        uint8_t dp;   // Direct page register
        uint8_t cc;   // Condition code register

        // HD6309-specific registers
        union { struct { uint8_t e, f; }; uint16_t w; };  // W = E:F
        uint16_t v;   // V register
        uint8_t  md;  // Mode register (HD6309)
    };

    // ========================================================================
    // REGISTER ACCESS HELPERS (included mid-class)
    // ========================================================================

#include "chip/cpu/mc6809/mc6809_registers.inc.hpp"

    // ========================================================================
    // ALU OPERATIONS (included mid-class)
    // ========================================================================

#include "chip/cpu/mc6809/mc6809_alu.inc.hpp"

    // ========================================================================
    // BUS SIGNAL HELPERS
    // ========================================================================

    /// Set up a memory read cycle
    inline bus_state_t bus_setup_read(bus_state_t pins, uint16_t addr) {
        BUS_SET_ADDR(pins, addr);
        BUS_SET_BIT(pins, BUS_RW_BIT);     // Read mode
        return pins;
    }

    /// Set up a memory write cycle
    inline bus_state_t bus_setup_write(bus_state_t pins, uint16_t addr, uint8_t data) {
        BUS_SET_ADDR(pins, addr);
        BUS_SET_DATA(pins, data);
        BUS_CLR_BIT(pins, BUS_RW_BIT);     // Write mode
        return pins;
    }

    /// Read a byte from bus (call after system services the read)
    inline uint8_t bus_read_data(bus_state_t pins) {
        return BUS_GET_DATA(pins);
    }

    /// Internal-only cycle (VMA=0, no bus access)
    inline bus_state_t bus_internal(bus_state_t pins) {
        BUS_SET_ADDR(pins, 0xFFFF);  // Conventional: $FFFF during internal cycles
        BUS_SET_BIT(pins, BUS_RW_BIT);
        return pins;
    }

    // ========================================================================
    // MEMORY ACCESS HELPERS (for register push/pull)
    // ========================================================================

    // Temporary memory access tracking for cycle-level bus fidelity.
    // These are used by multi-cycle push/pull stack operations.
    uint8_t  pending_data_ = 0;
    uint16_t pending_addr_ = 0;
    bool     pending_write_ = false;

    // Simplified read/write for ALU helpers that don't track bus cycles
    // (only used during initial bring-up; will be replaced with proper
    // bus-cycle-level operations for cycle accuracy)
    inline uint8_t read_byte(uint16_t addr) {
        (void)addr;
        return 0; // Placeholder — replaced by bus-level access
    }

    inline void write_byte(uint16_t addr, uint8_t data) {
        (void)addr;
        (void)data;
        // Placeholder — replaced by bus-level access
    }

    // ========================================================================
    // TRANSITION HELPERS
    // ========================================================================

    inline void transition_to_fetch() {
        current_handler_ = &mc6809_t::fetch_opcode;
        step_ = 0;
    }

    inline void transition_to(InstructionHandler handler) {
        current_handler_ = handler;
        step_ = 0;
    }

    // ========================================================================
    // INTERRUPT HELPERS
    // ========================================================================

    /// Check for pending interrupts at instruction boundary
    void check_interrupts() {
        // NMI edge detection
        bool nmi_active = !BUS_GET_BIT(bus_prev_, BUS_NMI_BIT);
        if (nmi_active && nmi_armed_) {
            nmi_pending_ = true;
            nmi_armed_ = false;  // Edge-triggered: re-arm on deassert
        }
        if (!nmi_active) {
            nmi_armed_ = true;
        }

        // IRQ level detection (only when not masked)
        irq_pending_ = !BUS_GET_BIT(bus_prev_, BUS_IRQ_BIT) && !(regs_.cc & Flags::I);

        // FIRQ level detection (only when not masked)
        firq_pending_ = !BUS_GET_BIT(bus_prev_, MC6809_FIRQ_BIT) && !(regs_.cc & Flags::F);
    }

    // ========================================================================
    // OPCODE FETCH
    // ========================================================================

    bus_state_t fetch_opcode(bus_state_t pins) {
        switch (step_++) {
        case 0: {
            // Check interrupts at instruction boundary
            check_interrupts();

            // Service pending interrupts (priority: RESET > NMI > FIRQ > IRQ)
            if (nmi_pending_) {
                nmi_pending_ = false;
                transition_to(&mc6809_t::op_nmi);
                return bus_internal(pins);
            }
            if (firq_pending_) {
                firq_pending_ = false;
                transition_to(&mc6809_t::op_firq);
                return bus_internal(pins);
            }
            if (irq_pending_) {
                irq_pending_ = false;
                transition_to(&mc6809_t::op_irq);
                return bus_internal(pins);
            }

            // Set LIC=1 on the last cycle of previous instruction
            BUS_SET_BIT(pins, MC6809_LIC_BIT);

            // Fetch opcode from PC
            return bus_setup_read(pins, regs_.pc);
        }

        case 1: {
            // Opcode byte received
            BUS_CLR_BIT(pins, MC6809_LIC_BIT);
            opcode_ = bus_read_data(pins);
            regs_.pc++;

            // Check for page prefix bytes ($10, $11)
            if (opcode_ == 0x10) {
                page_ = 1;
                step_ = 0;  // Re-fetch next byte as the actual opcode
                return bus_setup_read(pins, regs_.pc);
            }
            if (opcode_ == 0x11) {
                page_ = 2;
                step_ = 0;
                return bus_setup_read(pins, regs_.pc);
            }

            page_ = 0;
            return decode_and_execute(pins);
        }
        default:
            transition_to_fetch();
            return pins;
        }
    }

    // ========================================================================
    // OPCODE DECODE AND DISPATCH
    // ========================================================================

    bus_state_t decode_and_execute(bus_state_t pins);

    // ========================================================================
    // INSTRUCTION HANDLERS (included from .inc files)
    // ========================================================================

#include "chip/cpu/mc6809/operations/mc6809_base_ops.inc.hpp"
#include "chip/cpu/mc6809/operations/mc6809_page2_ops.inc.hpp"
#include "chip/cpu/mc6809/operations/mc6809_page3_ops.inc.hpp"

    // ========================================================================
    // RESET HANDLER
    // ========================================================================

    bus_state_t op_reset(bus_state_t pins) {
        switch (step_++) {
        case 0:
            regs_.dp = 0x00;
            regs_.cc |= Flags::I | Flags::F;
            if constexpr (has_native_mode()) regs_.md = 0;
            return bus_internal(pins);
        case 1:
            return bus_internal(pins);
        case 2:
            return bus_setup_read(pins, Vector::RESET);
        case 3:
            regs_.pc = static_cast<uint16_t>(bus_read_data(pins)) << 8;
            return bus_setup_read(pins, Vector::RESET + 1);
        case 4:
            regs_.pc |= bus_read_data(pins);
            nmi_armed_ = true;  // NMI armed after first S load
            transition_to_fetch();
            return pins;
        default:
            transition_to_fetch();
            return pins;
        }
    }

    // ========================================================================
    // INTERRUPT HANDLERS
    // ========================================================================

    /// NMI: push entire state, vector through $FFFC
    bus_state_t op_nmi(bus_state_t pins);

    /// IRQ: push entire state, vector through $FFF8
    bus_state_t op_irq(bus_state_t pins);

    /// FIRQ: push CC and PC only, vector through $FFF6
    bus_state_t op_firq(bus_state_t pins);

    // ========================================================================
    // INTERNAL STATE
    // ========================================================================

    Registers regs_{};
    InstructionHandler current_handler_ = &mc6809_t::fetch_opcode;
    uint8_t step_ = 0;

    uint8_t opcode_ = 0;      // Current opcode
    uint8_t page_ = 0;        // Page prefix: 0=page1, 1=page2 ($10), 2=page3 ($11)

    bool halted_ = false;
    bool nmi_armed_ = false;   // NMI edge detector state
    bool nmi_pending_ = false;
    bool irq_pending_ = false;
    bool firq_pending_ = false;
    bool sync_wait_ = false;   // SYNC instruction waiting
    bool cwai_wait_ = false;   // CWAI instruction waiting

    // Addressing mode temporaries
    uint16_t ea_ = 0;          // Effective address
    uint8_t  postbyte_ = 0;    // Index/stack post-byte
    uint8_t  data_hi_ = 0;     // Temporary data (high byte)
    uint8_t  data_lo_ = 0;     // Temporary data (low byte)
    int16_t  offset_ = 0;      // Signed offset for indexed/relative

    bus_state_t bus_prev_ = 0;

    // ========================================================================
    // DEBUG INFRASTRUCTURE
    // ========================================================================

    uint8_t debug_regs_[MC6809_DBG_SIZE]{};  // Flat byte array for DECL debug

    // ── Debug registration ─────────────────────────────────────
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

    /// Update debug_regs_ from current register state (call before debug rendering).
    /// Values stored in host byte order via memcpy for DECL renderer access.
    void sync_debug_snapshot() {
        // D = A:B (big-endian semantics: A is high byte, B is low byte)
        uint16_t d_val = (static_cast<uint16_t>(regs_.a) << 8) | regs_.b;
        std::memcpy(&debug_regs_[0], &d_val, 2);
        // X, Y, U, S, PC — 16-bit registers
        std::memcpy(&debug_regs_[2],  &regs_.x, 2);
        std::memcpy(&debug_regs_[4],  &regs_.y, 2);
        std::memcpy(&debug_regs_[6],  &regs_.u, 2);
        std::memcpy(&debug_regs_[8],  &regs_.s, 2);
        std::memcpy(&debug_regs_[10], &regs_.pc, 2);
        // DP, CC — 8-bit registers
        debug_regs_[12] = regs_.dp;
        debug_regs_[13] = regs_.cc;
        // HD6309-only registers
        if constexpr (has_w_register()) {
            uint16_t w_val = (static_cast<uint16_t>(regs_.e) << 8) | regs_.f;
            std::memcpy(&debug_regs_[14], &w_val, 2);
            std::memcpy(&debug_regs_[16], &regs_.v, 2);
        }
        if constexpr (has_native_mode()) {
            debug_regs_[18] = regs_.md;
        }
    }

    // ========================================================================
    // GUI SUPPORT
    // ========================================================================

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif
};

// ============================================================================
// DECODE AND EXECUTE — Page 1 (no prefix) opcode dispatch
// ============================================================================

template <const MC6809Traits& Traits>
bus_state_t mc6809_t<Traits>::decode_and_execute(bus_state_t pins) {
    // MC6809 page 1 opcode map
    switch (opcode_) {

    // === 0x00-0x0F: Direct addressing - unary operations ===
    case 0x00: transition_to(&mc6809_t::op_neg_direct); return pins;
    case 0x01:   // Undocumented: NEG (same as 0x00)
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_oim_direct); return pins; }
        transition_to(&mc6809_t::op_neg_direct); return pins;
    case 0x02:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_aim_direct); return pins; }
        // Undocumented: illegal / NOP-like on MC6809
        transition_to_fetch(); return pins;
    case 0x03: transition_to(&mc6809_t::op_com_direct); return pins;
    case 0x04: transition_to(&mc6809_t::op_lsr_direct); return pins;
    case 0x05:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_eim_direct); return pins; }
        // Undocumented: LSR (same as 0x04)
        transition_to(&mc6809_t::op_lsr_direct); return pins;
    case 0x06: transition_to(&mc6809_t::op_ror_direct); return pins;
    case 0x07: transition_to(&mc6809_t::op_asr_direct); return pins;
    case 0x08: transition_to(&mc6809_t::op_asl_direct); return pins;
    case 0x09: transition_to(&mc6809_t::op_rol_direct); return pins;
    case 0x0A: transition_to(&mc6809_t::op_dec_direct); return pins;
    case 0x0B:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_tim_direct); return pins; }
        // Undocumented: DEC (same as 0x0A)
        transition_to(&mc6809_t::op_dec_direct); return pins;
    case 0x0C: transition_to(&mc6809_t::op_inc_direct); return pins;
    case 0x0D: transition_to(&mc6809_t::op_tst_direct); return pins;
    case 0x0E: transition_to(&mc6809_t::op_jmp_direct); return pins;
    case 0x0F: transition_to(&mc6809_t::op_clr_direct); return pins;

    // === 0x10, 0x11: Page prefixes (handled in fetch_opcode) ===
    // Should never reach here

    // === 0x12-0x1F: Inherent/Misc ===
    case 0x12: /* NOP */ transition_to_fetch(); return pins;
    case 0x13: transition_to(&mc6809_t::op_sync); return pins;
    case 0x14: // Undocumented
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_sexw); return pins; }
        transition_to_fetch(); return pins;
    case 0x15: // Undocumented / illegal
        transition_to_fetch(); return pins;
    case 0x16: transition_to(&mc6809_t::op_lbra); return pins;
    case 0x17: transition_to(&mc6809_t::op_lbsr); return pins;
    case 0x18: // Undocumented / illegal
        transition_to_fetch(); return pins;
    case 0x19: transition_to(&mc6809_t::op_daa); return pins;
    case 0x1A: transition_to(&mc6809_t::op_orcc); return pins;
    case 0x1B: // Undocumented / illegal
        transition_to_fetch(); return pins;
    case 0x1C: transition_to(&mc6809_t::op_andcc); return pins;
    case 0x1D: transition_to(&mc6809_t::op_sex); return pins;
    case 0x1E: transition_to(&mc6809_t::op_exg); return pins;
    case 0x1F: transition_to(&mc6809_t::op_tfr); return pins;

    // === 0x20-0x2F: Short branch instructions ===
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x2C: case 0x2D: case 0x2E: case 0x2F:
        transition_to(&mc6809_t::op_bra_short); return pins;

    // === 0x30-0x3F: Indexed/Stack/Misc ===
    case 0x30: transition_to(&mc6809_t::op_leax); return pins;
    case 0x31: transition_to(&mc6809_t::op_leay); return pins;
    case 0x32: transition_to(&mc6809_t::op_leas); return pins;
    case 0x33: transition_to(&mc6809_t::op_leau); return pins;
    case 0x34: transition_to(&mc6809_t::op_pshs); return pins;
    case 0x35: transition_to(&mc6809_t::op_puls); return pins;
    case 0x36: transition_to(&mc6809_t::op_pshu); return pins;
    case 0x37: transition_to(&mc6809_t::op_pulu); return pins;
    case 0x38: // Undocumented
        transition_to_fetch(); return pins;
    case 0x39: transition_to(&mc6809_t::op_rts); return pins;
    case 0x3A: transition_to(&mc6809_t::op_abx); return pins;
    case 0x3B: transition_to(&mc6809_t::op_rti); return pins;
    case 0x3C: transition_to(&mc6809_t::op_cwai); return pins;
    case 0x3D: transition_to(&mc6809_t::op_mul); return pins;
    case 0x3E: // Undocumented / RESET
        transition_to(&mc6809_t::op_reset); return pins;
    case 0x3F: transition_to(&mc6809_t::op_swi); return pins;

    // === 0x40-0x4F: Inherent - register A operations ===
    case 0x40: regs_.a = alu_neg8(regs_.a); transition_to_fetch(); return pins; // NEGA
    case 0x41: // Undocumented
        transition_to_fetch(); return pins;
    case 0x42: // Undocumented
        transition_to_fetch(); return pins;
    case 0x43: regs_.a = alu_com8(regs_.a); transition_to_fetch(); return pins; // COMA
    case 0x44: regs_.a = alu_lsr8(regs_.a); transition_to_fetch(); return pins; // LSRA
    case 0x45: // Undocumented
        transition_to_fetch(); return pins;
    case 0x46: regs_.a = alu_ror8(regs_.a); transition_to_fetch(); return pins; // RORA
    case 0x47: regs_.a = alu_asr8(regs_.a); transition_to_fetch(); return pins; // ASRA
    case 0x48: regs_.a = alu_asl8(regs_.a); transition_to_fetch(); return pins; // ASLA
    case 0x49: regs_.a = alu_rol8(regs_.a); transition_to_fetch(); return pins; // ROLA
    case 0x4A: regs_.a = alu_dec8(regs_.a); transition_to_fetch(); return pins; // DECA
    case 0x4B: // Undocumented
        transition_to_fetch(); return pins;
    case 0x4C: regs_.a = alu_inc8(regs_.a); transition_to_fetch(); return pins; // INCA
    case 0x4D: alu_tst8(regs_.a); transition_to_fetch(); return pins;           // TSTA
    case 0x4E: // Undocumented
        transition_to_fetch(); return pins;
    case 0x4F: regs_.a = alu_clr8(); transition_to_fetch(); return pins;        // CLRA

    // === 0x50-0x5F: Inherent - register B operations ===
    case 0x50: regs_.b = alu_neg8(regs_.b); transition_to_fetch(); return pins; // NEGB
    case 0x51: // Undocumented
        transition_to_fetch(); return pins;
    case 0x52: // Undocumented
        transition_to_fetch(); return pins;
    case 0x53: regs_.b = alu_com8(regs_.b); transition_to_fetch(); return pins; // COMB
    case 0x54: regs_.b = alu_lsr8(regs_.b); transition_to_fetch(); return pins; // LSRB
    case 0x55: // Undocumented
        transition_to_fetch(); return pins;
    case 0x56: regs_.b = alu_ror8(regs_.b); transition_to_fetch(); return pins; // RORB
    case 0x57: regs_.b = alu_asr8(regs_.b); transition_to_fetch(); return pins; // ASRB
    case 0x58: regs_.b = alu_asl8(regs_.b); transition_to_fetch(); return pins; // ASLB
    case 0x59: regs_.b = alu_rol8(regs_.b); transition_to_fetch(); return pins; // ROLB
    case 0x5A: regs_.b = alu_dec8(regs_.b); transition_to_fetch(); return pins; // DECB
    case 0x5B: // Undocumented
        transition_to_fetch(); return pins;
    case 0x5C: regs_.b = alu_inc8(regs_.b); transition_to_fetch(); return pins; // INCB
    case 0x5D: alu_tst8(regs_.b); transition_to_fetch(); return pins;           // TSTB
    case 0x5E: // Undocumented
        transition_to_fetch(); return pins;
    case 0x5F: regs_.b = alu_clr8(); transition_to_fetch(); return pins;        // CLRB

    // === 0x60-0x6F: Indexed - unary memory operations ===
    case 0x60: transition_to(&mc6809_t::op_neg_indexed); return pins;
    case 0x61:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_oim_indexed); return pins; }
        transition_to_fetch(); return pins;
    case 0x62:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_aim_indexed); return pins; }
        transition_to_fetch(); return pins;
    case 0x63: transition_to(&mc6809_t::op_com_indexed); return pins;
    case 0x64: transition_to(&mc6809_t::op_lsr_indexed); return pins;
    case 0x65:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_eim_indexed); return pins; }
        transition_to_fetch(); return pins;
    case 0x66: transition_to(&mc6809_t::op_ror_indexed); return pins;
    case 0x67: transition_to(&mc6809_t::op_asr_indexed); return pins;
    case 0x68: transition_to(&mc6809_t::op_asl_indexed); return pins;
    case 0x69: transition_to(&mc6809_t::op_rol_indexed); return pins;
    case 0x6A: transition_to(&mc6809_t::op_dec_indexed); return pins;
    case 0x6B:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_tim_indexed); return pins; }
        transition_to_fetch(); return pins;
    case 0x6C: transition_to(&mc6809_t::op_inc_indexed); return pins;
    case 0x6D: transition_to(&mc6809_t::op_tst_indexed); return pins;
    case 0x6E: transition_to(&mc6809_t::op_jmp_indexed); return pins;
    case 0x6F: transition_to(&mc6809_t::op_clr_indexed); return pins;

    // === 0x70-0x7F: Extended - unary memory operations ===
    case 0x70: transition_to(&mc6809_t::op_neg_extended); return pins;
    case 0x71:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_oim_extended); return pins; }
        transition_to_fetch(); return pins;
    case 0x72:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_aim_extended); return pins; }
        transition_to_fetch(); return pins;
    case 0x73: transition_to(&mc6809_t::op_com_extended); return pins;
    case 0x74: transition_to(&mc6809_t::op_lsr_extended); return pins;
    case 0x75:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_eim_extended); return pins; }
        transition_to_fetch(); return pins;
    case 0x76: transition_to(&mc6809_t::op_ror_extended); return pins;
    case 0x77: transition_to(&mc6809_t::op_asr_extended); return pins;
    case 0x78: transition_to(&mc6809_t::op_asl_extended); return pins;
    case 0x79: transition_to(&mc6809_t::op_rol_extended); return pins;
    case 0x7A: transition_to(&mc6809_t::op_dec_extended); return pins;
    case 0x7B:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_tim_extended); return pins; }
        transition_to_fetch(); return pins;
    case 0x7C: transition_to(&mc6809_t::op_inc_extended); return pins;
    case 0x7D: transition_to(&mc6809_t::op_tst_extended); return pins;
    case 0x7E: transition_to(&mc6809_t::op_jmp_extended); return pins;
    case 0x7F: transition_to(&mc6809_t::op_clr_extended); return pins;

    // === 0x80-0x8F: Immediate - A/D operations ===
    case 0x80: transition_to(&mc6809_t::op_suba_imm); return pins;
    case 0x81: transition_to(&mc6809_t::op_cmpa_imm); return pins;
    case 0x82: transition_to(&mc6809_t::op_sbca_imm); return pins;
    case 0x83: transition_to(&mc6809_t::op_subd_imm); return pins;
    case 0x84: transition_to(&mc6809_t::op_anda_imm); return pins;
    case 0x85: transition_to(&mc6809_t::op_bita_imm); return pins;
    case 0x86: transition_to(&mc6809_t::op_lda_imm); return pins;
    case 0x87: // Undocumented (STA immediate — nonsensical)
        transition_to_fetch(); return pins;
    case 0x88: transition_to(&mc6809_t::op_eora_imm); return pins;
    case 0x89: transition_to(&mc6809_t::op_adca_imm); return pins;
    case 0x8A: transition_to(&mc6809_t::op_ora_imm); return pins;
    case 0x8B: transition_to(&mc6809_t::op_adda_imm); return pins;
    case 0x8C: transition_to(&mc6809_t::op_cmpx_imm); return pins;
    case 0x8D: transition_to(&mc6809_t::op_bsr); return pins;
    case 0x8E: transition_to(&mc6809_t::op_ldx_imm); return pins;
    case 0x8F: // Undocumented (STX immediate)
        transition_to_fetch(); return pins;

    // === 0x90-0x9F: Direct - A/D operations ===
    case 0x90: transition_to(&mc6809_t::op_suba_direct); return pins;
    case 0x91: transition_to(&mc6809_t::op_cmpa_direct); return pins;
    case 0x92: transition_to(&mc6809_t::op_sbca_direct); return pins;
    case 0x93: transition_to(&mc6809_t::op_subd_direct); return pins;
    case 0x94: transition_to(&mc6809_t::op_anda_direct); return pins;
    case 0x95: transition_to(&mc6809_t::op_bita_direct); return pins;
    case 0x96: transition_to(&mc6809_t::op_lda_direct); return pins;
    case 0x97: transition_to(&mc6809_t::op_sta_direct); return pins;
    case 0x98: transition_to(&mc6809_t::op_eora_direct); return pins;
    case 0x99: transition_to(&mc6809_t::op_adca_direct); return pins;
    case 0x9A: transition_to(&mc6809_t::op_ora_direct); return pins;
    case 0x9B: transition_to(&mc6809_t::op_adda_direct); return pins;
    case 0x9C: transition_to(&mc6809_t::op_cmpx_direct); return pins;
    case 0x9D: transition_to(&mc6809_t::op_jsr_direct); return pins;
    case 0x9E: transition_to(&mc6809_t::op_ldx_direct); return pins;
    case 0x9F: transition_to(&mc6809_t::op_stx_direct); return pins;

    // === 0xA0-0xAF: Indexed - A/D operations ===
    case 0xA0: transition_to(&mc6809_t::op_suba_indexed); return pins;
    case 0xA1: transition_to(&mc6809_t::op_cmpa_indexed); return pins;
    case 0xA2: transition_to(&mc6809_t::op_sbca_indexed); return pins;
    case 0xA3: transition_to(&mc6809_t::op_subd_indexed); return pins;
    case 0xA4: transition_to(&mc6809_t::op_anda_indexed); return pins;
    case 0xA5: transition_to(&mc6809_t::op_bita_indexed); return pins;
    case 0xA6: transition_to(&mc6809_t::op_lda_indexed); return pins;
    case 0xA7: transition_to(&mc6809_t::op_sta_indexed); return pins;
    case 0xA8: transition_to(&mc6809_t::op_eora_indexed); return pins;
    case 0xA9: transition_to(&mc6809_t::op_adca_indexed); return pins;
    case 0xAA: transition_to(&mc6809_t::op_ora_indexed); return pins;
    case 0xAB: transition_to(&mc6809_t::op_adda_indexed); return pins;
    case 0xAC: transition_to(&mc6809_t::op_cmpx_indexed); return pins;
    case 0xAD: transition_to(&mc6809_t::op_jsr_indexed); return pins;
    case 0xAE: transition_to(&mc6809_t::op_ldx_indexed); return pins;
    case 0xAF: transition_to(&mc6809_t::op_stx_indexed); return pins;

    // === 0xB0-0xBF: Extended - A/D operations ===
    case 0xB0: transition_to(&mc6809_t::op_suba_extended); return pins;
    case 0xB1: transition_to(&mc6809_t::op_cmpa_extended); return pins;
    case 0xB2: transition_to(&mc6809_t::op_sbca_extended); return pins;
    case 0xB3: transition_to(&mc6809_t::op_subd_extended); return pins;
    case 0xB4: transition_to(&mc6809_t::op_anda_extended); return pins;
    case 0xB5: transition_to(&mc6809_t::op_bita_extended); return pins;
    case 0xB6: transition_to(&mc6809_t::op_lda_extended); return pins;
    case 0xB7: transition_to(&mc6809_t::op_sta_extended); return pins;
    case 0xB8: transition_to(&mc6809_t::op_eora_extended); return pins;
    case 0xB9: transition_to(&mc6809_t::op_adca_extended); return pins;
    case 0xBA: transition_to(&mc6809_t::op_ora_extended); return pins;
    case 0xBB: transition_to(&mc6809_t::op_adda_extended); return pins;
    case 0xBC: transition_to(&mc6809_t::op_cmpx_extended); return pins;
    case 0xBD: transition_to(&mc6809_t::op_jsr_extended); return pins;
    case 0xBE: transition_to(&mc6809_t::op_ldx_extended); return pins;
    case 0xBF: transition_to(&mc6809_t::op_stx_extended); return pins;

    // === 0xC0-0xCF: Immediate - B operations ===
    case 0xC0: transition_to(&mc6809_t::op_subb_imm); return pins;
    case 0xC1: transition_to(&mc6809_t::op_cmpb_imm); return pins;
    case 0xC2: transition_to(&mc6809_t::op_sbcb_imm); return pins;
    case 0xC3: transition_to(&mc6809_t::op_addd_imm); return pins;
    case 0xC4: transition_to(&mc6809_t::op_andb_imm); return pins;
    case 0xC5: transition_to(&mc6809_t::op_bitb_imm); return pins;
    case 0xC6: transition_to(&mc6809_t::op_ldb_imm); return pins;
    case 0xC7: // Undocumented (STB immediate)
        transition_to_fetch(); return pins;
    case 0xC8: transition_to(&mc6809_t::op_eorb_imm); return pins;
    case 0xC9: transition_to(&mc6809_t::op_adcb_imm); return pins;
    case 0xCA: transition_to(&mc6809_t::op_orb_imm); return pins;
    case 0xCB: transition_to(&mc6809_t::op_addb_imm); return pins;
    case 0xCC: transition_to(&mc6809_t::op_ldd_imm); return pins;
    case 0xCD:
        if constexpr (is_hd6309()) { transition_to(&mc6809_t::op_ldq_imm); return pins; }
        transition_to_fetch(); return pins;
    case 0xCE: transition_to(&mc6809_t::op_ldu_imm); return pins;
    case 0xCF: // Undocumented (STU immediate)
        transition_to_fetch(); return pins;

    // === 0xD0-0xDF: Direct - B operations ===
    case 0xD0: transition_to(&mc6809_t::op_subb_direct); return pins;
    case 0xD1: transition_to(&mc6809_t::op_cmpb_direct); return pins;
    case 0xD2: transition_to(&mc6809_t::op_sbcb_direct); return pins;
    case 0xD3: transition_to(&mc6809_t::op_addd_direct); return pins;
    case 0xD4: transition_to(&mc6809_t::op_andb_direct); return pins;
    case 0xD5: transition_to(&mc6809_t::op_bitb_direct); return pins;
    case 0xD6: transition_to(&mc6809_t::op_ldb_direct); return pins;
    case 0xD7: transition_to(&mc6809_t::op_stb_direct); return pins;
    case 0xD8: transition_to(&mc6809_t::op_eorb_direct); return pins;
    case 0xD9: transition_to(&mc6809_t::op_adcb_direct); return pins;
    case 0xDA: transition_to(&mc6809_t::op_orb_direct); return pins;
    case 0xDB: transition_to(&mc6809_t::op_addb_direct); return pins;
    case 0xDC: transition_to(&mc6809_t::op_ldd_direct); return pins;
    case 0xDD: transition_to(&mc6809_t::op_std_direct); return pins;
    case 0xDE: transition_to(&mc6809_t::op_ldu_direct); return pins;
    case 0xDF: transition_to(&mc6809_t::op_stu_direct); return pins;

    // === 0xE0-0xEF: Indexed - B operations ===
    case 0xE0: transition_to(&mc6809_t::op_subb_indexed); return pins;
    case 0xE1: transition_to(&mc6809_t::op_cmpb_indexed); return pins;
    case 0xE2: transition_to(&mc6809_t::op_sbcb_indexed); return pins;
    case 0xE3: transition_to(&mc6809_t::op_addd_indexed); return pins;
    case 0xE4: transition_to(&mc6809_t::op_andb_indexed); return pins;
    case 0xE5: transition_to(&mc6809_t::op_bitb_indexed); return pins;
    case 0xE6: transition_to(&mc6809_t::op_ldb_indexed); return pins;
    case 0xE7: transition_to(&mc6809_t::op_stb_indexed); return pins;
    case 0xE8: transition_to(&mc6809_t::op_eorb_indexed); return pins;
    case 0xE9: transition_to(&mc6809_t::op_adcb_indexed); return pins;
    case 0xEA: transition_to(&mc6809_t::op_orb_indexed); return pins;
    case 0xEB: transition_to(&mc6809_t::op_addb_indexed); return pins;
    case 0xEC: transition_to(&mc6809_t::op_ldd_indexed); return pins;
    case 0xED: transition_to(&mc6809_t::op_std_indexed); return pins;
    case 0xEE: transition_to(&mc6809_t::op_ldu_indexed); return pins;
    case 0xEF: transition_to(&mc6809_t::op_stu_indexed); return pins;

    // === 0xF0-0xFF: Extended - B operations ===
    case 0xF0: transition_to(&mc6809_t::op_subb_extended); return pins;
    case 0xF1: transition_to(&mc6809_t::op_cmpb_extended); return pins;
    case 0xF2: transition_to(&mc6809_t::op_sbcb_extended); return pins;
    case 0xF3: transition_to(&mc6809_t::op_addd_extended); return pins;
    case 0xF4: transition_to(&mc6809_t::op_andb_extended); return pins;
    case 0xF5: transition_to(&mc6809_t::op_bitb_extended); return pins;
    case 0xF6: transition_to(&mc6809_t::op_ldb_extended); return pins;
    case 0xF7: transition_to(&mc6809_t::op_stb_extended); return pins;
    case 0xF8: transition_to(&mc6809_t::op_eorb_extended); return pins;
    case 0xF9: transition_to(&mc6809_t::op_adcb_extended); return pins;
    case 0xFA: transition_to(&mc6809_t::op_orb_extended); return pins;
    case 0xFB: transition_to(&mc6809_t::op_addb_extended); return pins;
    case 0xFC: transition_to(&mc6809_t::op_ldd_extended); return pins;
    case 0xFD: transition_to(&mc6809_t::op_std_extended); return pins;
    case 0xFE: transition_to(&mc6809_t::op_ldu_extended); return pins;
    case 0xFF: transition_to(&mc6809_t::op_stu_extended); return pins;

    default:
        // Catch-all for illegal/undocumented opcodes
        transition_to_fetch();
        return pins;
    }
}

// ============================================================================
// INTERRUPT HANDLERS
// ============================================================================

template <const MC6809Traits& Traits>
bus_state_t mc6809_t<Traits>::op_nmi(bus_state_t pins) {
    switch (step_++) {
    case 0:
        regs_.cc |= Flags::E;  // Set E flag — entire state will be saved
        return bus_internal(pins);
    case 1:  return bus_setup_write(pins, --regs_.s, regs_.pc & 0xFF);
    case 2:  return bus_setup_write(pins, --regs_.s, regs_.pc >> 8);
    case 3:  return bus_setup_write(pins, --regs_.s, regs_.u & 0xFF);
    case 4:  return bus_setup_write(pins, --regs_.s, regs_.u >> 8);
    case 5:  return bus_setup_write(pins, --regs_.s, regs_.y & 0xFF);
    case 6:  return bus_setup_write(pins, --regs_.s, regs_.y >> 8);
    case 7:  return bus_setup_write(pins, --regs_.s, regs_.x & 0xFF);
    case 8:  return bus_setup_write(pins, --regs_.s, regs_.x >> 8);
    case 9:  return bus_setup_write(pins, --regs_.s, regs_.dp);
    case 10: return bus_setup_write(pins, --regs_.s, regs_.b);
    case 11: return bus_setup_write(pins, --regs_.s, regs_.a);
    case 12: return bus_setup_write(pins, --regs_.s, regs_.cc);
    case 13:
        regs_.cc |= Flags::I | Flags::F;  // Mask both IRQ and FIRQ
        return bus_setup_read(pins, Vector::NMI);
    case 14:
        regs_.pc = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, Vector::NMI + 1);
    case 15:
        regs_.pc |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default:
        transition_to_fetch();
        return pins;
    }
}

template <const MC6809Traits& Traits>
bus_state_t mc6809_t<Traits>::op_irq(bus_state_t pins) {
    switch (step_++) {
    case 0:
        regs_.cc |= Flags::E;  // Set E flag — entire state will be saved
        return bus_internal(pins);
    case 1:  return bus_setup_write(pins, --regs_.s, regs_.pc & 0xFF);
    case 2:  return bus_setup_write(pins, --regs_.s, regs_.pc >> 8);
    case 3:  return bus_setup_write(pins, --regs_.s, regs_.u & 0xFF);
    case 4:  return bus_setup_write(pins, --regs_.s, regs_.u >> 8);
    case 5:  return bus_setup_write(pins, --regs_.s, regs_.y & 0xFF);
    case 6:  return bus_setup_write(pins, --regs_.s, regs_.y >> 8);
    case 7:  return bus_setup_write(pins, --regs_.s, regs_.x & 0xFF);
    case 8:  return bus_setup_write(pins, --regs_.s, regs_.x >> 8);
    case 9:  return bus_setup_write(pins, --regs_.s, regs_.dp);
    case 10: return bus_setup_write(pins, --regs_.s, regs_.b);
    case 11: return bus_setup_write(pins, --regs_.s, regs_.a);
    case 12: return bus_setup_write(pins, --regs_.s, regs_.cc);
    case 13:
        regs_.cc |= Flags::I;  // Mask IRQ
        return bus_setup_read(pins, Vector::IRQ);
    case 14:
        regs_.pc = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, Vector::IRQ + 1);
    case 15:
        regs_.pc |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default:
        transition_to_fetch();
        return pins;
    }
}

template <const MC6809Traits& Traits>
bus_state_t mc6809_t<Traits>::op_firq(bus_state_t pins) {
    switch (step_++) {
    case 0:
        regs_.cc &= ~Flags::E;  // Clear E flag — only CC and PC will be saved
        return bus_internal(pins);
    case 1:  return bus_setup_write(pins, --regs_.s, regs_.pc & 0xFF);
    case 2:  return bus_setup_write(pins, --regs_.s, regs_.pc >> 8);
    case 3:  return bus_setup_write(pins, --regs_.s, regs_.cc);
    case 4:
        regs_.cc |= Flags::I | Flags::F;  // Mask both IRQ and FIRQ
        return bus_setup_read(pins, Vector::FIRQ);
    case 5:
        regs_.pc = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, Vector::FIRQ + 1);
    case 6:
        regs_.pc |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default:
        transition_to_fetch();
        return pins;
    }
}

} // namespace mc6809
