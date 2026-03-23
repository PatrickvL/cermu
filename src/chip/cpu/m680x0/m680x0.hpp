#pragma once
/*
 * m680x0.hpp — Motorola 680x0 Family CPU Emulator (Main Template)
 *
 * NTTP template parameterised on M680x0Traits for compile-time variant
 * dispatch across the MC68000/08/10/20/30/40 family.
 *
 * Execution model
 * ---------------
 * tick() is called once per clock cycle.  Internally each bus cycle
 * occupies a minimum of 4 clocks (S0..S6/S7 half-clock states, paired
 * into 4 full-clock steps).  Instruction execution is a sequence of
 * bus cycles and internal processing steps, driven by a handler +
 * step counter state machine identical to the Z80 approach.
 *
 * Prefetch
 * --------
 * The 68000 maintains a two-word prefetch pipe:
 *   IRC — Instruction Register Capture (latest word from bus)
 *   IR  — Instruction Register (next instruction to decode)
 *   IRD — Instruction Register Decoder (currently executing)
 *
 * Endianness
 * ----------
 * The 68000 is big-endian.  Bus reads/writes always go through the
 * system's memory map.  No host-endian tricks at the CPU level —
 * endianness translation (e.g. the Darek Mihocka XOR trick) can be
 * applied at the memory subsystem level.
 */

#include <array>
#include <cstdint>
#include <cstring>

#include "chip/cpu/m680x0/m680x0_traits.hpp"
#include "chip/cpu/m680x0/m680x0_types.hpp"
#include "chip/cpu/m680x0/m680x0_opcodes.hpp"
#include "chip/cpu/cpu_chip_base.hpp"
#include "core/system_lines.hpp"
#include "core/cermu.hpp"
#include "core/register_file.hpp"

// Opcode table generation
#include "chip/cpu/m680x0/m680x0_opcode_tables.inc.hpp"

namespace m680x0 {

// ── M680x0-specific bus signal bit positions ───────────────────────
//
// Output pins (bits 55-63, CPU-specific reserved range)
#define M68K_AS_BIT     55   // Address Strobe
#define M68K_LDS_BIT    56   // Lower Data Strobe
#define M68K_UDS_BIT    57   // Upper Data Strobe
#define M68K_FC0_BIT    58   // Function Code 0
#define M68K_FC1_BIT    59   // Function Code 1
#define M68K_FC2_BIT    60   // Function Code 2
#define M68K_BG_BIT     61   // Bus Grant (output)
#define M68K_HALT_BIT   62   // Halt (output — active on double bus fault)

// Output pins (bits 48-54, shared range)
// BUS_RW_BIT = 48     (reused — R/W line)
#define M68K_VMA_BIT    50   // Valid Memory Address (6800 peripheral compat)
#define M68K_E_BIT      51   // Enable clock (6800 peripheral compat)

// Input pins (bits 32-47, shared + CPU-specific)
// BUS_RES_BIT = 32   (reused — RESET, active-low)
#define M68K_DTACK_BIT  36   // Data Transfer Acknowledge
#define M68K_IPL0_BIT   37   // Interrupt Priority Level 0
#define M68K_IPL1_BIT   38   // Interrupt Priority Level 1
#define M68K_IPL2_BIT   39   // Interrupt Priority Level 2
#define M68K_BR_BIT     40   // Bus Request (input)
#define M68K_BGACK_BIT  41   // Bus Grant Acknowledge (input)
#define M68K_VPA_BIT    42   // Valid Peripheral Address (input — 6800 compat)
#define M68K_BERR_BIT   43   // Bus Error (input)

// Convenience: RESET reuse
#define M68K_RESET_BIT  BUS_RES_BIT

// ── Memory callback types ──────────────────────────────────────
// VIC-II-style callbacks: the CPU invokes these within its tick()
// to perform memory accesses synchronously.  The system injects
// concrete implementations at init time.  This decouples the CPU
// from any particular memory map.
using m68k_read_fn  = uint16_t (*)(void* ctx, uint32_t addr);
using m68k_write_fn = void     (*)(void* ctx, uint32_t addr, uint16_t data);

// ── 16-bit data bus convention ─────────────────────────────────
// With callbacks the CPU reads/writes 16-bit words directly.
// Legacy macros kept for bus-signal-level code (test harness,
// system integration where pin-level bus matters).
#define M68K_SET_DATA_WORD(state, word) do { \
    BUS_SET_DATA(state, ((word) >> 8) & 0xFF); \
    (state) = ((state) & ~BUS_BANK_MASK) | (((bus_state_t)((word) & 0xFF)) << BUS_BANK_SHIFT); \
} while(0)

#define M68K_GET_DATA_WORD(state) \
    ((uint16_t)((BUS_GET_DATA(state) << 8) | (((state) >> BUS_BANK_SHIFT) & 0xFF)))

// Function code values (bits FC2:FC1:FC0)
namespace FunctionCode {
    constexpr uint8_t USER_DATA       = 0b001;
    constexpr uint8_t USER_PROGRAM    = 0b010;
    constexpr uint8_t SUPER_DATA      = 0b101;
    constexpr uint8_t SUPER_PROGRAM   = 0b110;
    constexpr uint8_t CPU_SPACE       = 0b111;  // Interrupt acknowledge
} // namespace FunctionCode

// ============================================================================
// M68K DECL — CPU register layout for debug infrastructure
// ============================================================================
//
// All internal registers are mirrored into a flat byte array (debug_regs_)
// in host-endian byte order.  The DECL table describes each register's byte
// offset and width, allowing the DECL renderer to display them without any
// hardcoded per-register callbacks.
//
// Byte layout (little-endian hosts; values stored via std::memcpy):
//   Offset  Size  Register
//   0-31    4×8   D0-D7 (Data registers)
//   32-63   4×8   A0-A7 (Address registers)
//   64-67   4     PC    (Program Counter)
//   68-69   2     SR    (Status Register: CCR at +68, system byte at +69)
//   70-73   4     USP   (User Stack Pointer)
//   74-77   4     SSP   (Supervisor Stack Pointer)
//   78-79   2     IRD   (Instruction Register Decoder)
//   80-81   2     IR    (Instruction Register)
//   82-83   2     IRC   (Instruction Register Capture)
//   84-87   4     VBR   (Vector Base Register, 68010+)
//   88      1     SFC   (Source Function Code, 68010+)
//   89      1     DFC   (Dest Function Code, 68010+)
//   90-93   4     CACR  (Cache Control, 68020+)
//   94-97   4     CAAR  (Cache Address, 68020+)

static constexpr uint16_t M68K_DBG_SIZE = 98;

#define M68K_DECL(REG, FLD, CMP) \
    REG( 0, D0,      "Data Register 0",       Value, 31:0) \
    REG( 4, D1,      "Data Register 1",       Value, 31:0) \
    REG( 8, D2,      "Data Register 2",       Value, 31:0) \
    REG(12, D3,      "Data Register 3",       Value, 31:0) \
    REG(16, D4,      "Data Register 4",       Value, 31:0) \
    REG(20, D5,      "Data Register 5",       Value, 31:0) \
    REG(24, D6,      "Data Register 6",       Value, 31:0) \
    REG(28, D7,      "Data Register 7",       Value, 31:0) \
    REG(32, A0,      "Address Register 0",    Value, 31:0) \
    REG(36, A1,      "Address Register 1",    Value, 31:0) \
    REG(40, A2,      "Address Register 2",    Value, 31:0) \
    REG(44, A3,      "Address Register 3",    Value, 31:0) \
    REG(48, A4,      "Address Register 4",    Value, 31:0) \
    REG(52, A5,      "Address Register 5",    Value, 31:0) \
    REG(56, A6,      "Address Register 6",    Value, 31:0) \
    REG(60, A7,      "Address Register 7",    Value, 31:0) \
    REG(64, PC,      "Program Counter",       Address, 31:0) \
    REG(68, SR_CCR,  "Condition Code Reg")                  \
      FLD(SR_CCR,  C,       0:0, "Carry",             Flag,  0, 0)  \
      FLD(SR_CCR,  V,       1:1, "Overflow",          Flag,  0, 0)  \
      FLD(SR_CCR,  Z,       2:2, "Zero",              Flag,  0, 0)  \
      FLD(SR_CCR,  N,       3:3, "Negative",          Flag,  0, 0)  \
      FLD(SR_CCR,  X,       4:4, "Extend",            Flag,  0, 0)  \
    REG(69, SR_SYS,  "System byte")                         \
      FLD(SR_SYS,  IPM,     2:0, "Interrupt mask",    Value, 0, 0)  \
      FLD(SR_SYS,  M,       4:4, "Master/ISP",        Flag,  0, 0)  \
      FLD(SR_SYS,  S,       5:5, "Supervisor",        Flag,  0, 0)  \
      FLD(SR_SYS,  T0,      6:6, "Trace 0",           Flag,  0, 0)  \
      FLD(SR_SYS,  T1,      7:7, "Trace 1",           Flag,  0, 0)  \
    REG(70, USP,     "User Stack Pointer",    Address, 31:0) \
    REG(74, SSP,     "Supervisor Stack Ptr",  Address, 31:0) \
    REG(78, IRD,     "Instr Reg Decoder",     Value, 15:0)  \
    REG(80, IR,      "Instruction Register",  Value, 15:0)  \
    REG(82, IRC,     "Instr Reg Capture",     Value, 15:0)  \
    REG(84, VBR,     "Vector Base Register",  Address, 31:0) \
    REG(88, SFC,     "Source Function Code",  Value, 7:0)   \
    REG(89, DFC,     "Dest Function Code",    Value, 7:0)   \
    REG(90, CACR,    "Cache Control",         Value, 31:0)  \
    REG(94, CAAR,    "Cache Address",         Value, 31:0)

// --- Extract address constants ---
M68K_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)

// --- Chip-local bitfield namespace ---
namespace m68k {
namespace fld {
#define M68K_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
M68K_DECL(DECL_REG_NOP, M68K_X_FLD_NS_, DECL_CMP_NOP)
#undef M68K_X_FLD_NS_
} // namespace fld
} // namespace m68k

DECL_EXTRACT(M68K, M68K_DECL)

// ── RegisterFile constants (byte offsets matching M68K_DECL layout) ──

// Data registers D0-D7 — uint32_t at indices 0-7 (NativeT stride)
constexpr r32 REG_D0{0},  REG_D1{4},  REG_D2{8},  REG_D3{12};
constexpr r32 REG_D4{16}, REG_D5{20}, REG_D6{24}, REG_D7{28};

// Address registers A0-A7 — uint32_t at indices 8-15
constexpr r32 REG_A0{32}, REG_A1{36}, REG_A2{40}, REG_A3{44};
constexpr r32 REG_A4{48}, REG_A5{52}, REG_A6{56}, REG_A7{60};

constexpr r32 REG_PC{64};
constexpr r16 REG_SR{68};           // CCR at lo_b, system byte at hi_b
constexpr r32 REG_USP{70};
constexpr r32 REG_SSP{74};
constexpr r16 REG_IRD{78};
constexpr r16 REG_IR{80};
constexpr r16 REG_IRC{82};
constexpr r32 REG_VBR{84};
constexpr r8  REG_SFC{88};
constexpr r8  REG_DFC{89};
constexpr r32 REG_CACR{90};
constexpr r32 REG_CAAR{94};

// ── Pending ALU operation (for memory EA bus cycle handlers) ──────

enum PendingOp : uint8_t {
    OP_ADD, OP_SUB, OP_AND, OP_OR, OP_EOR,
    OP_CMP, OP_CMPA_W, OP_CMPA_L,
    OP_ADDA_W, OP_ADDA_L, OP_SUBA_W, OP_SUBA_L,
    OP_MOVE, OP_MOVEA,
    OP_CLR, OP_NEG, OP_NOT, OP_NEGX, OP_TST,
    OP_BTST_DYN, OP_BCHG_DYN, OP_BCLR_DYN, OP_BSET_DYN,
};

static inline bool is_unary_op(uint8_t op) {
    return op >= OP_CLR && op <= OP_TST;
}

enum BusOpMode : uint8_t {
    BUS_READ_TO_DN,           // read src → ALU → write Dn → prefetch
    BUS_READ_TO_DN_LONG,      // same but with 2-idle post-prefetch (long ops)
    BUS_RMW,                  // read dst → ALU → prefetch → write dst
    BUS_RMW_LONG,             // same for long (read 2, prefetch, write 2)
    BUS_READ_ONLY,            // read src → ALU (no writeback) → prefetch (CMP/TST/BTST)
};

// ── Execution states ──────────────────────────────────────────────

enum class ExecState : uint8_t {
    RESET_SEQUENCE,      // Reading reset vectors
    PREFETCH,            // Prefetching next instruction word
    DECODE,              // Decoding IRD and dispatching
    EXECUTE,             // Multi-cycle instruction execution
    EXCEPTION,           // Exception processing
    BUS_READ,            // Bus read cycle (4+ clocks)
    BUS_WRITE,           // Bus write cycle (4+ clocks)
    HALTED,              // CPU halted (double bus fault)
    STOPPED,             // STOP instruction — waiting for interrupt
    BUS_GRANTED,         // Bus tri-stated, another master active
};

// ════════════════════════════════════════════════════════════════════
// m680x0_t — Main CPU template
// ════════════════════════════════════════════════════════════════════

template <const M680x0Traits& Traits>
class m680x0_t : public CpuChipBase {
public:
    // ── Type aliases ──────────────────────────────────────────────
    using InstructionHandler = bus_state_t (m680x0_t::*)(bus_state_t);

    // ── Compile-time feature queries ─────────────────────────────
    static constexpr bool has_vbr()         { return Traits.has_vbr(); }
    static constexpr bool has_cache()       { return Traits.has_cache(); }
    static constexpr bool has_mmu()         { return Traits.has_mmu(); }
    static constexpr bool has_fpu()         { return Traits.has_fpu(); }
    static constexpr bool has_32bit_addr()  { return Traits.has_32bit_addr(); }
    static constexpr bool has_bit_fields()  { return Traits.has_bit_fields(); }
    static constexpr bool has_mul64()       { return Traits.has_mul64(); }
    static constexpr bool has_long_branch() { return Traits.has_long_branch(); }

    static constexpr uint32_t address_mask() { return Traits.address_mask(); }

    // ── Default bus state ────────────────────────────────────────
    // Active-low signals: all deasserted (set high)
    static constexpr bus_state_t default_bus_state() {
        return BUS_BIT(BUS_RW_BIT) |    // R/W = read
               BUS_BIT(M68K_AS_BIT) |   // AS deasserted
               BUS_BIT(M68K_LDS_BIT) |  // LDS deasserted
               BUS_BIT(M68K_UDS_BIT);   // UDS deasserted
    }

    // ── Constructor ──────────────────────────────────────────────
    m680x0_t()
        : CpuChipBase(ChipInfo{Traits.chip_id, Traits.vendor}) {
        display_name_ = Traits.chip_id;
        short_name_   = Traits.chip_id;
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    // ── Memory callback registration ─────────────────────────────
    // Call once after construction to inject system memory callbacks.
    void set_memory_callbacks(m68k_read_fn read_fn, m68k_write_fn write_fn, void* ctx) {
        mem_read_  = read_fn;
        mem_write_ = write_fn;
        mem_ctx_   = ctx;
    }

    // ── Lifecycle ────────────────────────────────────────────────

    bus_state_t init() override {
        std::memset(regs_.data, 0, sizeof(regs_.data));
        regs_[REG_SR] = SRBits::S | (7 << SRBits::IPM_SHIFT);  // Supervisor mode, IPM=7
        state_        = ExecState::RESET_SEQUENCE;
        step_         = 0;
        halted_       = false;
        stopped_      = false;
        bus_prev_     = 0;
        ipl_pending_  = 0;
        addr_latch_   = 0;
        data_latch_   = 0;
        return default_bus_state();
    }

    bus_state_t reset(bus_state_t pins = 0) override {
        // On reset: Supervisor mode, IPM=7, fetch SSP from vector 0, PC from vector 1
        regs_[REG_SR] = SRBits::S | (7 << SRBits::IPM_SHIFT);
        if constexpr (has_vbr()) {
            regs_[REG_VBR] = 0;
        }
        state_        = ExecState::RESET_SEQUENCE;
        step_         = 0;
        halted_       = false;
        stopped_      = false;
        return default_bus_state();
    }

    // ── Tick ─────────────────────────────────────────────────────
    // One call = one clock cycle.
    //
    // Hybrid model: the handler state machine stays (reset, exception,
    // decode dispatch all work as before), but memory-accessing handlers
    // now complete synchronously via mem_read_ / mem_write_ callbacks
    // and set clocks_remaining_ for proper timing.  While the counter
    // is positive, tick() just decrements and returns.

    inline bus_state_t tick(bus_state_t pins) {
        // Edge detection for RESET (active-low)
        if (unlikely(!BUS_GET_BIT(pins, M68K_RESET_BIT))) {
            reset_counter_++;
            if (reset_counter_ >= 124) {  // 68000 requires ~124 clocks of RESET asserted
                pins = reset(pins);
            }
            bus_prev_ = pins;
            return pins;
        }
        reset_counter_ = 0;

        // Bus request handling
        if (BUS_GET_BIT(pins, M68K_BR_BIT) && state_ != ExecState::BUS_GRANTED) {
            pins = BUS_SET_BIT(pins, M68K_BG_BIT);
            if (BUS_GET_BIT(pins, M68K_BGACK_BIT)) {
                state_ = ExecState::BUS_GRANTED;
            }
        }

        if (state_ == ExecState::BUS_GRANTED) {
            if (!BUS_GET_BIT(pins, M68K_BGACK_BIT)) {
                state_ = ExecState::PREFETCH;
                step_  = 0;
            }
            bus_prev_ = pins;
            return pins;
        }

        // Sample IPL lines
        uint8_t ipl = 0;
        if (BUS_GET_BIT(pins, M68K_IPL0_BIT)) ipl |= 1;
        if (BUS_GET_BIT(pins, M68K_IPL1_BIT)) ipl |= 2;
        if (BUS_GET_BIT(pins, M68K_IPL2_BIT)) ipl |= 4;
        ipl_pending_ = 7 - ipl;  // IPL lines are active-low, inverted → priority level

        // Count down remaining clocks from previous instruction/bus cycle
        if (clocks_remaining_ > 0) {
            clocks_remaining_--;
            bus_prev_ = pins;
            return pins;
        }

        // Dispatch via handler state machine (unchanged architecture)
        pins = (this->*current_handler_)(pins);

        bus_prev_ = pins;
        return pins;
    }

    // ── Register file (byte layout matches M68K_DECL) ─────────────
    // D0-D7 at 0-31, A0-A7 at 32-63, PC at 64, SR at 68,
    // USP at 70, SSP at 74, IRD/IR/IRC at 78-83, VBR at 84,
    // SFC at 88, DFC at 89, CACR at 90, CAAR at 94.
    // data[] IS the debug backing store — no sync, no mirror.

private:
    // ── Mid-class includes (register access + ALU) ──────────────
    #include "chip/cpu/m680x0/m680x0_registers.inc.hpp"
    #include "chip/cpu/m680x0/m680x0_alu.inc.hpp"

    // ── Bus cycle helpers ───────────────────────────────────────
    // These set up the bus signals for a memory access and return
    // the updated pin state.

    /// Set function code outputs based on supervisor mode and data/program space
    inline bus_state_t set_fc(bus_state_t pins, uint8_t fc) {
        if (fc & 1) pins = BUS_SET_BIT(pins, M68K_FC0_BIT);
        else        pins = BUS_CLR_BIT(pins, M68K_FC0_BIT);
        if (fc & 2) pins = BUS_SET_BIT(pins, M68K_FC1_BIT);
        else        pins = BUS_CLR_BIT(pins, M68K_FC1_BIT);
        if (fc & 4) pins = BUS_SET_BIT(pins, M68K_FC2_BIT);
        else        pins = BUS_CLR_BIT(pins, M68K_FC2_BIT);
        return pins;
    }

    /// Get the appropriate function code for data access
    inline uint8_t fc_data() const {
        return (regs_[REG_SR] & SRBits::S) ? FunctionCode::SUPER_DATA : FunctionCode::USER_DATA;
    }

    /// Get the appropriate function code for program access
    inline uint8_t fc_program() const {
        return (regs_[REG_SR] & SRBits::S) ? FunctionCode::SUPER_PROGRAM : FunctionCode::USER_PROGRAM;
    }

    /// Begin a word read bus cycle — set address, assert AS+UDS+LDS, R/W=read
    inline bus_state_t begin_read_word(bus_state_t pins, uint32_t addr, uint8_t fc) {
        addr &= address_mask();
        pins = BUS_SET_ADDR(pins, addr);
        pins = set_fc(pins, fc);
        pins = BUS_SET_BIT(pins, BUS_RW_BIT);    // R/W = read
        pins = BUS_CLR_BIT(pins, M68K_AS_BIT);   // Assert AS (active-low)
        pins = BUS_CLR_BIT(pins, M68K_UDS_BIT);  // Assert UDS
        pins = BUS_CLR_BIT(pins, M68K_LDS_BIT);  // Assert LDS
        return pins;
    }

    /// Begin a byte read bus cycle — set address, assert AS + UDS or LDS depending on A0
    inline bus_state_t begin_read_byte(bus_state_t pins, uint32_t addr, uint8_t fc) {
        addr &= address_mask();
        pins = BUS_SET_ADDR(pins, addr);
        pins = set_fc(pins, fc);
        pins = BUS_SET_BIT(pins, BUS_RW_BIT);    // R/W = read
        pins = BUS_CLR_BIT(pins, M68K_AS_BIT);   // Assert AS
        if (addr & 1) {
            pins = BUS_CLR_BIT(pins, M68K_LDS_BIT);  // Odd address → LDS
            pins = BUS_SET_BIT(pins, M68K_UDS_BIT);
        } else {
            pins = BUS_CLR_BIT(pins, M68K_UDS_BIT);  // Even address → UDS
            pins = BUS_SET_BIT(pins, M68K_LDS_BIT);
        }
        return pins;
    }

    /// Begin a word write bus cycle
    inline bus_state_t begin_write_word(bus_state_t pins, uint32_t addr, uint16_t data, uint8_t fc) {
        addr &= address_mask();
        pins = BUS_SET_ADDR(pins, addr);
        M68K_SET_DATA_WORD(pins, data);  // High byte in DATA, low byte in BANK
        pins = set_fc(pins, fc);
        pins = BUS_CLR_BIT(pins, BUS_RW_BIT);    // R/W = write
        pins = BUS_CLR_BIT(pins, M68K_AS_BIT);   // Assert AS
        pins = BUS_CLR_BIT(pins, M68K_UDS_BIT);  // Assert UDS
        pins = BUS_CLR_BIT(pins, M68K_LDS_BIT);  // Assert LDS
        return pins;
    }

    /// Begin a byte write bus cycle
    inline bus_state_t begin_write_byte(bus_state_t pins, uint32_t addr, uint8_t data, uint8_t fc) {
        addr &= address_mask();
        pins = BUS_SET_ADDR(pins, addr);
        pins = BUS_SET_DATA(pins, data);
        pins = set_fc(pins, fc);
        pins = BUS_CLR_BIT(pins, BUS_RW_BIT);    // R/W = write
        pins = BUS_CLR_BIT(pins, M68K_AS_BIT);   // Assert AS
        if (addr & 1) {
            pins = BUS_CLR_BIT(pins, M68K_LDS_BIT);
            pins = BUS_SET_BIT(pins, M68K_UDS_BIT);
        } else {
            pins = BUS_CLR_BIT(pins, M68K_UDS_BIT);
            pins = BUS_SET_BIT(pins, M68K_LDS_BIT);
        }
        return pins;
    }

    /// End a bus cycle — deassert AS, UDS, LDS
    inline bus_state_t end_bus_cycle(bus_state_t pins) {
        pins = BUS_SET_BIT(pins, M68K_AS_BIT);   // Deassert AS
        pins = BUS_SET_BIT(pins, M68K_UDS_BIT);  // Deassert UDS
        pins = BUS_SET_BIT(pins, M68K_LDS_BIT);  // Deassert LDS
        pins = BUS_SET_BIT(pins, BUS_RW_BIT);    // R/W back to read
        return pins;
    }

    /// Read data word from bus (after DTACK)
    inline uint16_t read_data_word(bus_state_t pins) const {
        // 68000 word transfer convention: high byte in DATA field,
        // low byte in BANK field. See M68K_GET_DATA_WORD.
        return M68K_GET_DATA_WORD(pins);
    }

    /// Read data byte from bus (after DTACK)
    /// Both even and odd addresses deliver the byte via the DATA field
    /// (the test harness and real hardware place the byte on D15-D8
    /// for even addresses / UDS, and on D7-D0 for odd / LDS; our
    /// bus convention uses DATA for both).
    inline uint8_t read_data_byte(bus_state_t pins) const {
        return BUS_GET_DATA(pins);
    }

    // ── Generic bus cycle handlers for memory EA ────────────────

    // -- Read byte/word from ea_addr_ into data_latch_ -----------
    // On completion: transition to cont_handler_ (next tick).
    bus_state_t handle_read_bw(bus_state_t pins) {
        if (mem_read_) {
            uint32_t addr = ea_addr_ & address_mask();
            if (op_sz_ == OpSize::Byte) {
                uint16_t word = mem_read_(mem_ctx_, addr & ~1u);
                data_latch_ = static_cast<uint32_t>((addr & 1) ? (word & 0xFF) : (word >> 8));
            } else {
                data_latch_ = static_cast<uint32_t>(mem_read_(mem_ctx_, addr));
            }
            clocks_remaining_ += 3;  // 4-clock bus cycle
            transition_to(cont_handler_);
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            case 0:  // S0/S1: begin read
                return (op_sz_ == OpSize::Byte)
                    ? begin_read_byte(pins, ea_addr_, fc_data())
                    : begin_read_word(pins, ea_addr_, fc_data());
            case 1:  // S2/S3: propagation
                return pins;
            case 2:  // S4/S5: DTACK + latch
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                data_latch_ = (op_sz_ == OpSize::Byte)
                    ? static_cast<uint32_t>(read_data_byte(pins))
                    : static_cast<uint32_t>(read_data_word(pins));
                return pins;
            case 3:  // S6/S7: end bus cycle
                pins = end_bus_cycle(pins);
                transition_to(cont_handler_);
                return pins;  // Don't chain — cont runs next tick
        }
        return pins;
    }

    // -- Read long (two words) from ea_addr_ into data_latch_ ----
    // Reads hi word at ea_addr_, lo word at ea_addr_+2.
    bus_state_t handle_read_l(bus_state_t pins) {
        if (mem_read_) {
            uint32_t addr = ea_addr_ & address_mask();
            uint16_t hi = mem_read_(mem_ctx_, addr);
            uint16_t lo = mem_read_(mem_ctx_, addr + 2);
            data_latch_ = (static_cast<uint32_t>(hi) << 16) | lo;
            clocks_remaining_ += 7;  // 2 × 4-clock bus cycles
            transition_to(cont_handler_);
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            // First word (high)
            case 0: return begin_read_word(pins, ea_addr_, fc_data());
            case 1: return pins;
            case 2:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                data_latch_ = static_cast<uint32_t>(read_data_word(pins)) << 16;
                return pins;
            case 3:
                pins = end_bus_cycle(pins);
                return pins;  // Gap between bus cycles
            // Second word (low)
            case 4: return begin_read_word(pins, ea_addr_ + 2, fc_data());
            case 5: return pins;
            case 6:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                data_latch_ |= read_data_word(pins);
                return pins;
            case 7:
                pins = end_bus_cycle(pins);
                transition_to(cont_handler_);
                return pins;  // Don't chain
        }
        return pins;
    }

    // -- Write byte/word from data_latch_ to ea_addr_ ------------
    bus_state_t handle_write_bw(bus_state_t pins) {
        if (mem_write_) {
            uint32_t addr = ea_addr_ & address_mask();
            if (op_sz_ == OpSize::Byte) {
                // Read-modify-write: only change the target byte
                uint32_t even_addr = addr & ~1u;
                uint16_t existing = mem_read_(mem_ctx_, even_addr);
                uint16_t word;
                if (addr & 1)
                    word = (existing & 0xFF00) | (data_latch_ & 0xFF);
                else
                    word = ((data_latch_ & 0xFF) << 8) | (existing & 0x00FF);
                mem_write_(mem_ctx_, even_addr, word);
            } else {
                mem_write_(mem_ctx_, addr, static_cast<uint16_t>(data_latch_));
            }
            clocks_remaining_ += 3;  // 4-clock bus cycle
            transition_to(cont_handler_);
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            case 0:
                return (op_sz_ == OpSize::Byte)
                    ? begin_write_byte(pins, ea_addr_, static_cast<uint8_t>(data_latch_), fc_data())
                    : begin_write_word(pins, ea_addr_, static_cast<uint16_t>(data_latch_), fc_data());
            case 1: return pins;
            case 2:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                return pins;
            case 3:
                pins = end_bus_cycle(pins);
                transition_to(cont_handler_);
                return pins;
        }
        return pins;
    }

    // -- Write long from data_latch_ to ea_addr_ (lo first!) -----
    // 68000 writes long words in reverse order: low word first.
    bus_state_t handle_write_l(bus_state_t pins) {
        if (mem_write_) {
            uint32_t addr = ea_addr_ & address_mask();
            // 68000 writes long words low word first, but address order doesn't affect
            // final memory state. Use natural order for callbacks.
            mem_write_(mem_ctx_, addr,     static_cast<uint16_t>((data_latch_ >> 16) & 0xFFFF));
            mem_write_(mem_ctx_, addr + 2, static_cast<uint16_t>(data_latch_ & 0xFFFF));
            clocks_remaining_ += 7;  // 2 × 4-clock bus cycles
            transition_to(cont_handler_);
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            // Low word first (at ea_addr_+2)
            case 0: return begin_write_word(pins, ea_addr_ + 2,
                        static_cast<uint16_t>(data_latch_ & 0xFFFF), fc_data());
            case 1: return pins;
            case 2:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                return pins;
            case 3:
                pins = end_bus_cycle(pins);
                return pins;
            // High word (at ea_addr_)
            case 4: return begin_write_word(pins, ea_addr_,
                        static_cast<uint16_t>((data_latch_ >> 16) & 0xFFFF), fc_data());
            case 5: return pins;
            case 6:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                return pins;
            case 7:
                pins = end_bus_cycle(pins);
                transition_to(cont_handler_);
                return pins;
        }
        return pins;
    }

    // -- Prefetch that chains to cont_handler_ (not decode) ------
    // Used mid-instruction (e.g., between read and write in RMW).
    bus_state_t handle_prefetch_continue(bus_state_t pins) {
        if (mem_read_) {
            uint16_t word = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
            regs_[REG_IR]  = regs_[REG_IRC];
            regs_[REG_IRC] = word;
            regs_[REG_PC] += 2;
            regs_[REG_IRD] = regs_[REG_IR];
            clocks_remaining_ += 3;  // 4-clock bus cycle
            transition_to(cont_handler_);
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            case 0: return begin_read_word(pins, regs_[REG_PC], fc_program());
            case 1: return pins;
            case 2:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                regs_[REG_IR]  = regs_[REG_IRC];
                regs_[REG_IRC] = read_data_word(pins);
                regs_[REG_PC] += 2;
                return pins;
            case 3:
                pins = end_bus_cycle(pins);
                regs_[REG_IRD] = regs_[REG_IR];
                transition_to(cont_handler_);
                return pins;  // Don't chain
        }
        return pins;
    }

    // -- Idle clocks then chain to cont_handler_ -----------------
    bus_state_t handle_idle_continue(bus_state_t pins) {
        if (idle_remaining_ > 0) {
            idle_remaining_--;
            return pins;
        }
        // Done idling — chain to continuation immediately
        transition_to(cont_handler_);
        return (this->*cont_handler_)(pins);
    }

    // -- Post-prefetch idle then decode --------------------------
    // Burns remaining idle clocks after prefetch, then enters decode.
    bus_state_t handle_idle_to_decode(bus_state_t pins) {
        if (idle_remaining_ > 0) {
            idle_remaining_--;
            return pins;
        }
        transition_to(&m680x0_t::handle_decode);
        return pins;
    }

    // -- Apply pending ALU operation -----------------------------
    inline uint32_t apply_alu(uint32_t src, uint32_t dst, OpSize sz) {
        switch (pending_op_) {
            case OP_ADD:  return alu_add(src, dst, sz);
            case OP_SUB:  return alu_sub(src, dst, sz);
            case OP_AND:  return alu_and(src, dst, sz);
            case OP_OR:   return alu_or(src, dst, sz);
            case OP_EOR:  return alu_eor(src, dst, sz);
            case OP_CMP:  alu_cmp(src, dst, sz); return 0;
            case OP_CMPA_W: {
                int32_t s = static_cast<int32_t>(static_cast<int16_t>(src & 0xFFFF));
                alu_cmp(static_cast<uint32_t>(s), dst, OpSize::Long);
                return 0;
            }
            case OP_CMPA_L: alu_cmp(src, dst, OpSize::Long); return 0;
            case OP_ADDA_W: {
                int32_t s = static_cast<int32_t>(static_cast<int16_t>(src & 0xFFFF));
                return dst + static_cast<uint32_t>(s);
            }
            case OP_ADDA_L: return dst + src;
            case OP_SUBA_W: {
                int32_t s = static_cast<int32_t>(static_cast<int16_t>(src & 0xFFFF));
                return dst - static_cast<uint32_t>(s);
            }
            case OP_SUBA_L: return dst - src;
            case OP_CLR:  { uint8_t ccr = (get_ccr() & Flags::X) | Flags::Z; set_ccr(ccr); return 0; }
            case OP_NEG:  return alu_sub(src, 0, sz);
            case OP_NOT:  return alu_not(src, sz);
            case OP_NEGX: return alu_subx(src, 0, sz);
            case OP_TST:  alu_tst(src, sz); return src;
            default: return src;
        }
    }

    // -- Post-read: execute ALU and determine next phase ---------
    bus_state_t handle_post_read(bus_state_t pins) {
        uint32_t ea_val = data_latch_ & size_mask(op_sz_);
        uint32_t result;

        switch (bus_op_mode_) {
            case BUS_READ_TO_DN: {
                uint32_t dn_val = read_dn(reg_idx_, op_sz_);
                result = apply_alu(ea_val, dn_val, op_sz_);
                if (pending_op_ != OP_CMP)
                    write_dn(reg_idx_, result, op_sz_);
                return do_prefetch(pins);
            }
            case BUS_READ_TO_DN_LONG: {
                uint32_t dn_val = read_dn(reg_idx_, OpSize::Long);
                result = apply_alu(data_latch_, dn_val, OpSize::Long);
                if (pending_op_ != OP_CMP && pending_op_ != OP_CMPA_W && pending_op_ != OP_CMPA_L)
                    write_dn(reg_idx_, result, OpSize::Long);
                // Prefetch + 2 idle after
                cont_handler_ = &m680x0_t::handle_idle_to_decode;
                idle_remaining_ = 1;  // 2 idle clocks after prefetch
                transition_to(&m680x0_t::handle_prefetch_continue);
                return handle_prefetch_continue(pins);
            }
            case BUS_RMW: {
                uint32_t src, dst;
                if (is_unary_op(pending_op_)) {
                    src = ea_val; dst = 0;
                } else {
                    src = read_dn(reg_idx_, op_sz_); dst = ea_val;
                }
                result = apply_alu(src, dst, op_sz_);
                data_latch_ = result;
                cont_handler_ = &m680x0_t::handle_start_write_bw;
                transition_to(&m680x0_t::handle_prefetch_continue);
                return handle_prefetch_continue(pins);
            }
            case BUS_RMW_LONG: {
                uint32_t src, dst;
                if (is_unary_op(pending_op_)) {
                    src = data_latch_; dst = 0;
                } else {
                    src = read_dn(reg_idx_, OpSize::Long); dst = data_latch_;
                }
                result = apply_alu(src, dst, OpSize::Long);
                data_latch_ = result;
                cont_handler_ = &m680x0_t::handle_start_write_l;
                transition_to(&m680x0_t::handle_prefetch_continue);
                return handle_prefetch_continue(pins);
            }
            case BUS_READ_ONLY: {
                uint32_t dn_val = read_dn(reg_idx_, op_sz_);
                apply_alu(ea_val, dn_val, op_sz_);
                return do_prefetch(pins);
            }
            default:
                return do_prefetch(pins);
        }
    }

    // -- Post-read for ADDA/SUBA/CMPA: ea reads to address reg ---
    bus_state_t handle_post_read_addr(bus_state_t pins) {
        uint32_t ea_val = data_latch_;
        uint32_t an_val = get_a(reg_idx_);
        uint32_t result = apply_alu(ea_val, an_val, op_sz_);

        bool is_cmp = (pending_op_ == OP_CMPA_W || pending_op_ == OP_CMPA_L);
        if (!is_cmp) {
            set_a(reg_idx_, result);
            if (reg_idx_ == 7) sync_sp();
        }

        // Address operations always use long result and may need extra idle
        if (op_sz_ == OpSize::Word && !is_cmp) {
            // ADDA.w/SUBA.w from memory: 8 clocks (no extra idle)
            return do_prefetch(pins);
        }
        if (op_sz_ == OpSize::Long || is_cmp) {
            // ADDA.l/SUBA.l/CMPA from memory: 6 idle + prefetch? No —
            // depends on EA source size. For now: word/byte source = no idle,
            // long source = 2 idle after prefetch.
        }
        return do_prefetch(pins);
    }

    // -- Start write (byte/word) after prefetch ------------------
    bus_state_t handle_start_write_bw(bus_state_t pins) {
        cont_handler_ = &m680x0_t::handle_decode;
        transition_to(&m680x0_t::handle_write_bw);
        return handle_write_bw(pins);
    }

    // -- Start write (long) after prefetch -----------------------
    bus_state_t handle_start_write_l(bus_state_t pins) {
        cont_handler_ = &m680x0_t::handle_decode;
        transition_to(&m680x0_t::handle_write_l);
        return handle_write_l(pins);
    }

    // -- Begin memory EA read with optional pre-idle -------------
    // Sets up read handler chain. For predecrement mode, adds 2 idle.
    inline bus_state_t begin_ea_read(bus_state_t pins, uint8_t ea_mode) {
        // Address error: word/long access to odd address
        if (unlikely((ea_addr_ & 1) && op_sz_ != OpSize::Byte)) {
            // -(An): 2 idle clocks before the bus cycle that would have faulted
            if (ea_mode == 4) clocks_remaining_ += 2;
            process_address_error_sync(ea_addr_, true /*read*/, fc_data());
            return pins;
        }
        cont_handler_ = &m680x0_t::handle_post_read;
        if (ea_mode == 4) {
            // -(An): 2 idle before read
            idle_remaining_ = 1;
            InstructionHandler read_handler = (op_sz_ == OpSize::Long)
                ? &m680x0_t::handle_read_l
                : &m680x0_t::handle_read_bw;
            InstructionHandler saved_cont = cont_handler_;
            cont_handler_ = read_handler;
            // Stash the real continuation for after the read
            // We chain: idle → read → post_read
            // But cont_handler_ is used by both idle and read...
            // Solution: idle chains to cont (=read handler), read handler's
            // cont is set inside the read handler? No — we need to set it here.
            // Use a two-step approach: idle → begin_ea_read_phase2
            cont_handler_ = &m680x0_t::handle_begin_read_phase2;
            transition_to(&m680x0_t::handle_idle_continue);
            return pins;  // First idle tick consumed
        }
        // No idle needed for modes 2, 3
        if (op_sz_ == OpSize::Long) {
            transition_to(&m680x0_t::handle_read_l);
            return handle_read_l(pins);
        }
        transition_to(&m680x0_t::handle_read_bw);
        return handle_read_bw(pins);
    }

    // Phase 2: after pre-idle, start the actual read
    bus_state_t handle_begin_read_phase2(bus_state_t pins) {
        cont_handler_ = &m680x0_t::handle_post_read;
        if (op_sz_ == OpSize::Long) {
            transition_to(&m680x0_t::handle_read_l);
            return handle_read_l(pins);
        }
        transition_to(&m680x0_t::handle_read_bw);
        return handle_read_bw(pins);
    }

    // ── Vector read ─────────────────────────────────────────────
    /// Calculate vector address (with VBR for 68010+)
    inline uint32_t vector_addr(uint8_t vector_num) const {
        uint32_t base = 0;
        if constexpr (has_vbr()) {
            base = regs_[REG_VBR];
        }
        return base + (static_cast<uint32_t>(vector_num) << 2);
    }

    // ── Handler: Reset sequence ─────────────────────────────────
    // Reads SSP from vector 0, PC from vector 1, then starts prefetch
    bus_state_t handle_reset(bus_state_t pins) {
        if (mem_read_) {
            // Synchronous: read 4 words (SSP hi/lo, PC hi/lo)
            uint16_t ssp_hi = mem_read_(mem_ctx_, 0x00000000);
            uint16_t ssp_lo = mem_read_(mem_ctx_, 0x00000002);
            regs_[15] = (static_cast<uint32_t>(ssp_hi) << 16) | ssp_lo;
            regs_[REG_SSP]  = regs_[15];
            uint16_t pc_hi = mem_read_(mem_ctx_, 0x00000004);
            uint16_t pc_lo = mem_read_(mem_ctx_, 0x00000006);
            regs_[REG_PC] = (static_cast<uint32_t>(pc_hi) << 16) | pc_lo;
            state_ = ExecState::PREFETCH;
            transition_to(&m680x0_t::handle_prefetch);
            // 4 word reads × 4 clocks each = 16 clocks, minus current tick
            clocks_remaining_ = 15;
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            // Read SSP high word (vector 0, offset 0)
            case 0:
                pins = begin_read_word(pins, 0x00000000, FunctionCode::SUPER_DATA);
                return pins;
            case 1:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) return pins;  // Wait for DTACK
                data_latch_ = static_cast<uint32_t>(read_data_word(pins)) << 16;
                pins = end_bus_cycle(pins);
                return pins;
            // Read SSP low word (vector 0, offset 2)
            case 2:
                pins = begin_read_word(pins, 0x00000002, FunctionCode::SUPER_DATA);
                return pins;
            case 3:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) return pins;
                data_latch_ |= read_data_word(pins);
                regs_[15] = data_latch_;
                regs_[REG_SSP]  = data_latch_;
                pins = end_bus_cycle(pins);
                return pins;
            // Read PC high word (vector 1, offset 4)
            case 4:
                pins = begin_read_word(pins, 0x00000004, FunctionCode::SUPER_DATA);
                return pins;
            case 5:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) return pins;
                data_latch_ = static_cast<uint32_t>(read_data_word(pins)) << 16;
                pins = end_bus_cycle(pins);
                return pins;
            // Read PC low word (vector 1, offset 6)
            case 6:
                pins = begin_read_word(pins, 0x00000006, FunctionCode::SUPER_DATA);
                return pins;
            case 7:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) return pins;
                data_latch_ |= read_data_word(pins);
                regs_[REG_PC] = data_latch_;
                pins = end_bus_cycle(pins);
                // Transition to instruction prefetch
                transition_to(&m680x0_t::handle_prefetch);
                return pins;
            default:
                return pins;
        }
    }

    // ── Handler: Prefetch ───────────────────────────────────────
    // Fetches the next instruction word from [PC], advances PC.
    // One bus cycle = 4 clocks (S0-S7 in half-clock notation).
    bus_state_t handle_prefetch(bus_state_t pins) {
        if (mem_read_) {
            // Synchronous: fetch word, shift pipeline, set timing
            uint16_t word = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
            regs_[REG_IR]  = regs_[REG_IRC];
            regs_[REG_IRC] = word;
            regs_[REG_PC] += 2;
            regs_[REG_IRD] = regs_[REG_IR];
            clocks_remaining_ += 3;  // 4-clock bus cycle minus current tick
            transition_to(&m680x0_t::handle_decode);
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            case 0:  // S0/S1: Output address, begin read
                pins = begin_read_word(pins, regs_[REG_PC], fc_program());
                return pins;
            case 1:  // S2/S3: Wait for address propagation
                return pins;
            case 2:  // S4/S5: Wait for DTACK, latch data
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                // Shift prefetch pipeline
                regs_[REG_IR]  = regs_[REG_IRC];
                regs_[REG_IRC] = read_data_word(pins);
                regs_[REG_PC] += 2;
                return pins;
            case 3:  // S6/S7: End bus cycle, transition to decode
                pins = end_bus_cycle(pins);
                regs_[REG_IRD] = regs_[REG_IR];
                transition_to(&m680x0_t::handle_decode);
                return pins;
            default:
                return pins;
        }
    }

    // ── Handler: Decode ─────────────────────────────────────────
    // Decodes IRD and dispatches to the appropriate instruction handler.
    // Decode is "free" (0 clocks) — it chains to the instruction handler
    // on the same tick, so the instruction handler's first step runs
    // in the same clock cycle.
    bus_state_t handle_decode(bus_state_t pins) {
        address_error_ = false;
        uint16_t opcode = regs_[REG_IRD];
        uint8_t group = instr_group(opcode);

        // Dispatch by instruction group (bits 15-12)
        switch (static_cast<InstrGroup>(group)) {
            case InstrGroup::GROUP_0:     return decode_group0(pins, opcode);
            case InstrGroup::GROUP_1:     return decode_move(pins, opcode, OpSize::Byte);
            case InstrGroup::GROUP_2:     return decode_move(pins, opcode, OpSize::Long);
            case InstrGroup::GROUP_3:     return decode_move(pins, opcode, OpSize::Word);
            case InstrGroup::GROUP_4:     return decode_group4(pins, opcode);
            case InstrGroup::GROUP_5:     return decode_group5(pins, opcode);
            case InstrGroup::GROUP_6:     return decode_group6(pins, opcode);
            case InstrGroup::GROUP_7:     return decode_moveq(pins, opcode);
            case InstrGroup::GROUP_8:     return decode_group8(pins, opcode);
            case InstrGroup::GROUP_9:     return decode_group9(pins, opcode);
            case InstrGroup::GROUP_A:     return exception(pins, Vector::LINE_A, regs_[REG_PC] - 4);
            case InstrGroup::GROUP_B:     return decode_groupB(pins, opcode);
            case InstrGroup::GROUP_C:     return decode_groupC(pins, opcode);
            case InstrGroup::GROUP_D:     return decode_groupD(pins, opcode);
            case InstrGroup::GROUP_E:     return decode_groupE(pins, opcode);
            case InstrGroup::GROUP_F:     return exception(pins, Vector::LINE_F, regs_[REG_PC] - 4);
        }
        return pins;
    }

    // ── Exception processing ────────────────────────────────────
    bus_state_t exception(bus_state_t pins, uint8_t vector_num, uint32_t pc_to_save) {
        exception_vector_ = vector_num;
        exception_pc_ = pc_to_save;
        transition_to(&m680x0_t::handle_exception);
        return (this->*current_handler_)(pins);
    }

    // Convenience: push current "next instruction" PC (regs_[REG_PC] - 2)
    bus_state_t exception(bus_state_t pins, uint8_t vector_num) {
        return exception(pins, vector_num, regs_[REG_PC] - 2);
    }

    // ── Address error exception (group 0) — 14-byte frame ───────
    // Called from read_ea/write_ea when a word/long access hits an odd address.
    // Synchronous only (mem_read_/mem_write_ path). Sets address_error_ flag.
    inline void process_address_error_sync(uint32_t fault_addr, bool is_read, uint8_t fc) {
        exception_sr_ = regs_[REG_SR];
        enter_supervisor();
        regs_[REG_SR] &= static_cast<uint16_t>(~SRBits::T1);
        regs_[REG_SR] &= static_cast<uint16_t>(~SRBits::T0);
        // Decrement SP by 14 for the full group 0 frame
        regs_[15] -= 14;
        uint32_t base = regs_[15];
        // Address error PC points to the faulting instruction's opcode
        uint32_t pc = regs_[REG_PC] - 4;
        // Push in 68000 microcode order (non-sequential writes)
        mem_write_(mem_ctx_, (base + 12) & address_mask(), static_cast<uint16_t>(pc & 0xFFFF));         // PC low
        mem_write_(mem_ctx_, (base + 8)  & address_mask(), exception_sr_);                              // SR
        mem_write_(mem_ctx_, (base + 10) & address_mask(), static_cast<uint16_t>((pc >> 16) & 0xFFFF)); // PC high
        mem_write_(mem_ctx_, (base + 6)  & address_mask(), regs_[REG_IRD]);                                  // IR
        mem_write_(mem_ctx_, (base + 4)  & address_mask(), static_cast<uint16_t>(fault_addr & 0xFFFF)); // Fault addr low
        // SSW: upper bits from IRD, lower 5 bits = R/W(4) | IN(3) | FC(2:0)
        // IN = 1 for instruction/program fetch (FC bit 1 set), 0 for data
        uint8_t in_flag = (fc & 0x02) ? 0x08 : 0x00;
        uint16_t ssw = (regs_[REG_IRD] & 0xFFE0) | (is_read ? 0x10 : 0x00) | in_flag | fc;
        mem_write_(mem_ctx_, base        & address_mask(), ssw);                                         // SSW
        mem_write_(mem_ctx_, (base + 2)  & address_mask(), static_cast<uint16_t>((fault_addr >> 16) & 0xFFFF)); // Fault addr high
        // Read vector
        uint32_t vaddr = vector_addr(Vector::ADDRESS_ERROR);
        uint16_t vec_hi = mem_read_(mem_ctx_, vaddr);
        uint16_t vec_lo = mem_read_(mem_ctx_, vaddr + 2);
        regs_[REG_PC] = (static_cast<uint32_t>(vec_hi) << 16) | vec_lo;
        // Two-word prefetch from handler
        uint16_t word1 = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
        regs_[REG_PC] += 2;
        uint16_t word2 = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
        regs_[REG_IR]  = word1;
        regs_[REG_IRC] = word2;
        regs_[REG_PC] += 2;
        regs_[REG_IRD] = regs_[REG_IR];
        // 4 idle + 7 writes(28) + 2 vector reads(8) + 2 prefetch reads(4+4) + 2 idle = 50
        // Minus 1 for the current tick = 49
        clocks_remaining_ += 49;
        sync_sp();
        transition_to(&m680x0_t::handle_decode);
        address_error_ = true;
    }

    bus_state_t handle_exception(bus_state_t pins) {
        if (mem_read_ && mem_write_) {
            // Synchronous: enter supervisor, push context, read vector
            exception_sr_ = regs_[REG_SR];
            enter_supervisor();
            regs_[REG_SR] &= static_cast<uint16_t>(~SRBits::T1);
            regs_[REG_SR] &= static_cast<uint16_t>(~SRBits::T0);
            // Push PC (low word first, then high word — stack grows down)
            regs_[15] -= 2;
            mem_write_(mem_ctx_, regs_[15] & address_mask(),
                       static_cast<uint16_t>(exception_pc_ & 0xFFFF));
            regs_[15] -= 2;
            mem_write_(mem_ctx_, regs_[15] & address_mask(),
                       static_cast<uint16_t>((exception_pc_ >> 16) & 0xFFFF));
            // Push SR
            regs_[15] -= 2;
            mem_write_(mem_ctx_, regs_[15] & address_mask(), exception_sr_);
            // Read vector
            uint32_t vaddr = vector_addr(exception_vector_);
            uint16_t vec_hi = mem_read_(mem_ctx_, vaddr);
            uint16_t vec_lo = mem_read_(mem_ctx_, vaddr + 2);
            regs_[REG_PC] = (static_cast<uint32_t>(vec_hi) << 16) | vec_lo;
            // Two-word prefetch from handler address (same as do_branch_prefetch)
            uint16_t word1 = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
            regs_[REG_PC] += 2;
            uint16_t word2 = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
            regs_[REG_IR]  = word1;
            regs_[REG_IRC] = word2;
            regs_[REG_PC] += 2;
            regs_[REG_IRD] = regs_[REG_IR];
            // 3 writes + 2 reads (vector) + 2 reads (prefetch) + 2 idle = 34 clocks
            // Current tick consumed: clocks_remaining_ = 33
            clocks_remaining_ += 33;
            sync_sp();
            transition_to(&m680x0_t::handle_decode);
            return pins;
        }
        // Fallback: multi-tick bus signal path
        switch (step_++) {
            // Enter supervisor mode
            case 0:
                exception_sr_ = regs_[REG_SR];
                enter_supervisor();
                regs_[REG_SR] &= static_cast<uint16_t>(~SRBits::T1);  // Clear trace
                regs_[REG_SR] &= static_cast<uint16_t>(~SRBits::T0);
                return pins;
            // Push PC low word
            case 1:
                regs_[15] -= 2;
                pins = begin_write_word(pins, regs_[15],
                    static_cast<uint16_t>(exception_pc_ & 0xFFFF), FunctionCode::SUPER_DATA);
                return pins;
            case 2:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                pins = end_bus_cycle(pins);
                return pins;
            // Push PC high word
            case 3:
                regs_[15] -= 2;
                pins = begin_write_word(pins, regs_[15],
                    static_cast<uint16_t>((exception_pc_ >> 16) & 0xFFFF), FunctionCode::SUPER_DATA);
                return pins;
            case 4:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                pins = end_bus_cycle(pins);
                return pins;
            // Push SR
            case 5:
                regs_[15] -= 2;
                pins = begin_write_word(pins, regs_[15], exception_sr_, FunctionCode::SUPER_DATA);
                return pins;
            case 6:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                pins = end_bus_cycle(pins);
                return pins;
            // Read vector high word
            case 7:
                pins = begin_read_word(pins, vector_addr(exception_vector_), FunctionCode::SUPER_DATA);
                return pins;
            case 8:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                data_latch_ = static_cast<uint32_t>(read_data_word(pins)) << 16;
                pins = end_bus_cycle(pins);
                return pins;
            // Read vector low word
            case 9:
                pins = begin_read_word(pins, vector_addr(exception_vector_) + 2, FunctionCode::SUPER_DATA);
                return pins;
            case 10:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                data_latch_ |= read_data_word(pins);
                regs_[REG_PC] = data_latch_;
                pins = end_bus_cycle(pins);
                // Refill prefetch and decode
                transition_to(&m680x0_t::handle_prefetch);
                return pins;
            default:
                return pins;
        }
    }

    // ── Transition helpers ──────────────────────────────────────

    inline void transition_to(InstructionHandler handler) {
        current_handler_ = handler;
        step_ = 0;
    }

    /// Consume an extension word from IRC and refill IRC from [PC].
    /// On the real 68000, each extension word read is a 4-clock bus cycle
    /// that shifts the prefetch pipeline. Without this refill, the final
    /// do_prefetch would put the stale extension value into IRD, corrupting
    /// the next instruction decode.
    inline uint16_t consume_extension_word() {
        uint16_t word = regs_[REG_IRC];
        // Refill IRC from [PC] — PC already points past the extension word
        // (it was advanced by the previous prefetch that loaded IRC).
        if (mem_read_) {
            regs_[REG_IRC] = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
            clocks_remaining_ += 4;
        }
        regs_[REG_PC] += 2;  // Advance PC AFTER refill (next read will be from PC+2)
        return word;
    }

    /// Transition to prefetch and immediately run its first step.
    /// This chains decode + prefetch into the same clock tick,
    /// matching the real 68000 where decode is free (0 clocks).
    inline bus_state_t do_prefetch(bus_state_t pins) {
        transition_to(&m680x0_t::handle_prefetch);
        return handle_prefetch(pins);
    }

    /// After a branch/jump, the prefetch pipeline is stale.
    /// Fill it from the new PC location with two bus reads.
    inline bus_state_t do_branch_prefetch(bus_state_t pins) {
        if (mem_read_) {
            // Address error: odd PC triggers group 0 exception
            if (unlikely(regs_[REG_PC] & 1)) {
                process_address_error_sync(regs_[REG_PC], true, fc_program());
                return pins;
            }
            uint16_t word1 = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
            regs_[REG_PC] += 2;
            uint16_t word2 = mem_read_(mem_ctx_, regs_[REG_PC] & address_mask());
            regs_[REG_IR]  = word1;
            regs_[REG_IRC] = word2;
            regs_[REG_PC] += 2;
            regs_[REG_IRD] = regs_[REG_IR];
            clocks_remaining_ += 7;  // 2 × 4-clock bus reads minus current tick
            transition_to(&m680x0_t::handle_decode);
            return pins;
        }
        return do_prefetch(pins);
    }

    /// Add N idle clocks before starting the prefetch cycle.
    /// Used for instructions that take more than 4 clocks (prefetch only).
    /// The first idle clock is consumed on the current tick.
    inline bus_state_t do_idle_then_prefetch(bus_state_t pins, uint8_t idle_clocks) {
        if (mem_read_) {
            // Synchronous: prefetch now, then idle + prefetch bus cycle = total delay
            transition_to(&m680x0_t::handle_prefetch);
            handle_prefetch(pins);  // Does the fetch, sets clocks_remaining_ = 3
            // Add idle clocks to the prefetch delay
            clocks_remaining_ += idle_clocks;
            return pins;
        }
        if (idle_clocks == 0) return do_prefetch(pins);
        idle_remaining_ = idle_clocks - 1;  // -1 because we consume one now
        transition_to(&m680x0_t::handle_idle);
        return pins;
    }

    // ── Handler: Idle ───────────────────────────────────────────
    // Burns internal idle cycles, then transitions to prefetch.
    bus_state_t handle_idle(bus_state_t pins) {
        if (idle_remaining_ > 0) {
            idle_remaining_--;
            return pins;
        }
        // Done idling → start prefetch
        return do_prefetch(pins);
    }

    // ── Group decode stubs ──────────────────────────────────────
    // Declared via #include of operations/m680x0_ops.inc.hpp below.

    // ── Effective address evaluation ────────────────────────────
    // Declared via #include of operations/m680x0_ops.inc.hpp below.

    // ── Operation includes ──────────────────────────────────────
    // Each file provides the decode_groupN() implementations
    #define M680X0_TEMPLATE_CONTEXT
    #include "chip/cpu/m680x0/operations/m680x0_ops.inc.hpp"
    #undef M680X0_TEMPLATE_CONTEXT

    // ── Internal state ──────────────────────────────────────────
    RegisterFile<100, uint32_t> regs_{};
    InstructionHandler  current_handler_ = &m680x0_t::handle_reset;
    ExecState           state_           = ExecState::RESET_SEQUENCE;
    uint8_t             step_            = 0;
    bool                halted_          = false;
    bool                stopped_         = false;
    uint8_t             ipl_pending_     = 0;       // Sampled interrupt priority level
    uint32_t            addr_latch_      = 0;       // Address latch for multi-cycle ops
    uint32_t            data_latch_      = 0;       // Data latch for multi-cycle ops
    uint16_t            exception_sr_    = 0;       // SR saved during exception processing
    uint32_t            exception_pc_    = 0;       // PC to push during exception processing
    uint8_t             exception_vector_ = 0;      // Vector number for current exception
    uint32_t            ea_addr_         = 0;       // Computed effective address
    bus_state_t         bus_prev_        = 0;       // Previous bus state (edge detection)
    uint16_t            reset_counter_   = 0;       // Counts clocks with RESET asserted
    uint8_t             idle_remaining_  = 0;       // Remaining idle clocks before prefetch
    InstructionHandler  cont_handler_    = nullptr; // Continuation after bus cycle
    uint8_t             reg_idx_         = 0;       // Register index for current memory op
    OpSize              op_sz_           = OpSize::Byte;  // Size for current memory op
    uint8_t             pending_op_      = 0;       // PendingOp enum: which ALU operation
    uint8_t             bus_op_mode_     = 0;       // BusOpMode enum: read-to-Dn vs RMW etc.
    bool                address_error_   = false;   // Set by read_ea/write_ea on odd word/long access

    // ── Memory callback state ───────────────────────────────────
    m68k_read_fn        mem_read_        = nullptr; // Synchronous word read callback
    m68k_write_fn       mem_write_       = nullptr; // Synchronous word write callback
    void*               mem_ctx_         = nullptr; // Opaque context for callbacks
    uint16_t            clocks_remaining_ = 0;      // Countdown for multi-clock operations

    // ── Test harness support ────────────────────────────────────
public:
    /// Returns true when the CPU is at an instruction boundary.
    /// Used by test harnesses to detect instruction completion.
    bool opdone() const {
        return current_handler_ == &m680x0_t::handle_decode
            && step_ == 0
            && clocks_remaining_ == 0;
    }

    /// Set CPU to "ready to decode" state (skip reset sequence).
    /// Call after init() and loading registers to start executing from IRD.
    void prepare_for_test() {
        current_handler_ = &m680x0_t::handle_decode;
        step_ = 0;
        clocks_remaining_ = 0;
        halted_ = false;
        stopped_ = false;
        reset_counter_ = 0;
    }

    // === Register accessors (for test harness) ===
    uint32_t reg_d(uint8_t n) const { return regs_[n & 7]; }
    uint32_t reg_a(uint8_t n) const { return regs_[8 + (n & 7)]; }
    uint32_t reg_pc() const { return regs_[REG_PC]; }
    uint16_t reg_sr() const { return regs_[REG_SR]; }
    uint32_t reg_usp() const { return regs_[REG_USP]; }
    uint32_t reg_ssp() const { return regs_[REG_SSP]; }
    uint16_t reg_irc() const { return regs_[REG_IRC]; }
    uint16_t reg_ir() const { return regs_[REG_IR]; }
    uint16_t reg_ird() const { return regs_[REG_IRD]; }

    void set_reg_d(uint8_t n, uint32_t v) { regs_[n & 7] = v; }
    void set_reg_a(uint8_t n, uint32_t v) { regs_[8 + (n & 7)] = v; }
    void set_reg_pc(uint32_t v) { regs_[REG_PC] = v; }
    void set_reg_sr(uint16_t v) { regs_[REG_SR] = v; }
    void set_reg_usp(uint32_t v) { regs_[REG_USP] = v; }
    void set_reg_ssp(uint32_t v) { regs_[REG_SSP] = v; }
    void set_reg_irc(uint16_t v) { regs_[REG_IRC] = v; }
    void set_reg_ir(uint16_t v) { regs_[REG_IR] = v; }
    void set_reg_ird(uint16_t v) { regs_[REG_IRD] = v; }

    // ── Debug registration ─────────────────────────────────────
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

    // ── GUI support ─────────────────────────────────────────────
#ifdef CERMU_HAS_GUI
public:
    bool has_debug_content() const override { return true; }
    void render_debug_content() override;

    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif // CERMU_HAS_GUI
};

} // namespace m680x0
