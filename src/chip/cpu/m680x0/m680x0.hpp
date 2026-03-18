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

// ── 16-bit data bus convention ─────────────────────────────────
// The 68000 has a 16-bit data bus, but bus_state_t only has 8 data
// bits.  For word transfers (both UDS and LDS active), we carry the
// high byte (D15-D8) in the DATA field and the low byte (D7-D0) in
// the BANK field.  This is safe because the address has already been
// latched before data appears on the bus.
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
// M68K DECL — Status Register layout for debug infrastructure
// ============================================================================
//
// The 68000 Status Register is 16 bits, split into two 8-bit "registers"
// for the DECL system (which works with byte-granularity):
//   Offset 0: CCR (Condition Code Register) — bits 0-4 of SR
//   Offset 1: System byte — bits 8-15 of SR (IPM, M, S, T0, T1)
//
// The sr_snapshot_[2] array is passed to set_registers() and kept in
// sync before rendering.  The fld:: namespace provides masks/shifts
// for these per-byte fields.

#define M68K_DECL(REG, FLD, CMP) \
    REG(0x00, SR_CCR,     "Condition Code Register")                              \
      FLD(SR_CCR,  C,       0:0, "Carry",             Flag,  0, 0)               \
      FLD(SR_CCR,  V,       1:1, "Overflow",          Flag,  0, 0)               \
      FLD(SR_CCR,  Z,       2:2, "Zero",              Flag,  0, 0)               \
      FLD(SR_CCR,  N,       3:3, "Negative",          Flag,  0, 0)               \
      FLD(SR_CCR,  X,       4:4, "Extend",            Flag,  0, 0)               \
    REG(0x01, SR_SYS,     "System byte")                                          \
      FLD(SR_SYS,  IPM,     2:0, "Interrupt mask",    Value, 0, 0)               \
      FLD(SR_SYS,  M,       4:4, "Master/ISP",        Flag,  0, 0)               \
      FLD(SR_SYS,  S,       5:5, "Supervisor",        Flag,  0, 0)               \
      FLD(SR_SYS,  T0,      6:6, "Trace 0",           Flag,  0, 0)               \
      FLD(SR_SYS,  T1,      7:7, "Trace 1",           Flag,  0, 0)

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

static constexpr uint8_t M68K_SR_SNAPSHOT_SIZE = 2;

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

    // ── Lifecycle ────────────────────────────────────────────────

    bus_state_t init() override {
        std::memset(&regs_, 0, sizeof(regs_));
        regs_.sr = SRBits::S | (7 << SRBits::IPM_SHIFT);  // Supervisor mode, IPM=7
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
        regs_.sr = SRBits::S | (7 << SRBits::IPM_SHIFT);
        if constexpr (has_vbr()) {
            regs_.vbr = 0;
        }
        state_        = ExecState::RESET_SEQUENCE;
        step_         = 0;
        halted_       = false;
        stopped_      = false;
        return default_bus_state();
    }

    // ── Tick ─────────────────────────────────────────────────────
    // One call = one clock cycle

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
            // Grant bus — tri-state all outputs
            pins = BUS_SET_BIT(pins, M68K_BG_BIT);  // Assert BG (active-low = clear? depends on convention)
            // For simplicity, enter BUS_GRANTED when BGACK is also asserted
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

        // Main state dispatch
        pins = (this->*current_handler_)(pins);

        bus_prev_ = pins;
        return pins;
    }

    // ── Registers struct ────────────────────────────────────────
    struct Registers {
        uint32_t d[8];          // Data registers D0-D7
        uint32_t a[8];          // Address registers A0-A7 (A7 = active stack pointer)
        uint32_t usp;           // User Stack Pointer (saved when in supervisor mode)
        uint32_t ssp;           // Supervisor Stack Pointer (saved when in user mode)
        uint32_t pc;            // Program Counter
        uint16_t sr;            // Status Register

        // Prefetch pipeline
        uint16_t irc;           // Instruction Register Capture (latest fetch)
        uint16_t ir;            // Instruction Register (next instruction)
        uint16_t ird;           // Instruction Register Decoder (current instruction)

        // 68010+ control registers
        uint32_t vbr;           // Vector Base Register
        uint8_t  sfc;           // Source Function Code
        uint8_t  dfc;           // Destination Function Code

        // 68020+ control registers
        uint32_t cacr;          // Cache Control Register
        uint32_t caar;          // Cache Address Register
        uint32_t msp;           // Master Stack Pointer
        uint32_t isp;           // Interrupt Stack Pointer
    };

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
        return (regs_.sr & SRBits::S) ? FunctionCode::SUPER_DATA : FunctionCode::USER_DATA;
    }

    /// Get the appropriate function code for program access
    inline uint8_t fc_program() const {
        return (regs_.sr & SRBits::S) ? FunctionCode::SUPER_PROGRAM : FunctionCode::USER_PROGRAM;
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

    // ── Vector read ─────────────────────────────────────────────
    /// Calculate vector address (with VBR for 68010+)
    inline uint32_t vector_addr(uint8_t vector_num) const {
        uint32_t base = 0;
        if constexpr (has_vbr()) {
            base = regs_.vbr;
        }
        return base + (static_cast<uint32_t>(vector_num) << 2);
    }

    // ── Handler: Reset sequence ─────────────────────────────────
    // Reads SSP from vector 0, PC from vector 1, then starts prefetch
    bus_state_t handle_reset(bus_state_t pins) {
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
                regs_.a[7] = data_latch_;
                regs_.ssp  = data_latch_;
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
                regs_.pc = data_latch_;
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
        switch (step_++) {
            case 0:  // S0/S1: Output address, begin read
                pins = begin_read_word(pins, regs_.pc, fc_program());
                return pins;
            case 1:  // S2/S3: Wait for address propagation
                return pins;
            case 2:  // S4/S5: Wait for DTACK, latch data
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                // Shift prefetch pipeline
                regs_.ir  = regs_.irc;
                regs_.irc = read_data_word(pins);
                regs_.pc += 2;
                return pins;
            case 3:  // S6/S7: End bus cycle, transition to decode
                pins = end_bus_cycle(pins);
                regs_.ird = regs_.ir;
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
        uint16_t opcode = regs_.ird;
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
            case InstrGroup::GROUP_A:     return exception(pins, Vector::LINE_A);
            case InstrGroup::GROUP_B:     return decode_groupB(pins, opcode);
            case InstrGroup::GROUP_C:     return decode_groupC(pins, opcode);
            case InstrGroup::GROUP_D:     return decode_groupD(pins, opcode);
            case InstrGroup::GROUP_E:     return decode_groupE(pins, opcode);
            case InstrGroup::GROUP_F:     return exception(pins, Vector::LINE_F);
        }
        return pins;
    }

    // ── Exception processing ────────────────────────────────────
    bus_state_t exception(bus_state_t pins, uint8_t vector_num) {
        // Simplified exception processing — push context, read vector
        exception_vector_ = vector_num;
        transition_to(&m680x0_t::handle_exception);
        return (this->*current_handler_)(pins);
    }

    bus_state_t handle_exception(bus_state_t pins) {
        switch (step_++) {
            // Enter supervisor mode
            case 0:
                exception_sr_ = regs_.sr;
                enter_supervisor();
                regs_.sr &= ~SRBits::T1;  // Clear trace
                regs_.sr &= ~SRBits::T0;
                return pins;
            // Push PC low word
            case 1:
                regs_.a[7] -= 2;
                pins = begin_write_word(pins, regs_.a[7],
                    static_cast<uint16_t>(regs_.pc & 0xFFFF), FunctionCode::SUPER_DATA);
                return pins;
            case 2:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                pins = end_bus_cycle(pins);
                return pins;
            // Push PC high word
            case 3:
                regs_.a[7] -= 2;
                pins = begin_write_word(pins, regs_.a[7],
                    static_cast<uint16_t>((regs_.pc >> 16) & 0xFFFF), FunctionCode::SUPER_DATA);
                return pins;
            case 4:
                if (BUS_GET_BIT(pins, M68K_DTACK_BIT)) { --step_; return pins; }
                pins = end_bus_cycle(pins);
                return pins;
            // Push SR
            case 5:
                regs_.a[7] -= 2;
                pins = begin_write_word(pins, regs_.a[7], exception_sr_, FunctionCode::SUPER_DATA);
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
                regs_.pc = data_latch_;
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

    /// Transition to prefetch and immediately run its first step.
    /// This chains decode + prefetch into the same clock tick,
    /// matching the real 68000 where decode is free (0 clocks).
    inline bus_state_t do_prefetch(bus_state_t pins) {
        transition_to(&m680x0_t::handle_prefetch);
        return handle_prefetch(pins);
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
    Registers           regs_{};
    uint8_t             sr_snapshot_[M68K_SR_SNAPSHOT_SIZE]{}; // SR split for DECL debug
    InstructionHandler  current_handler_ = &m680x0_t::handle_reset;
    ExecState           state_           = ExecState::RESET_SEQUENCE;
    uint8_t             step_            = 0;
    bool                halted_          = false;
    bool                stopped_         = false;
    uint8_t             ipl_pending_     = 0;       // Sampled interrupt priority level
    uint32_t            addr_latch_      = 0;       // Address latch for multi-cycle ops
    uint32_t            data_latch_      = 0;       // Data latch for multi-cycle ops
    uint16_t            exception_sr_    = 0;       // SR saved during exception processing
    uint8_t             exception_vector_ = 0;      // Vector number for current exception
    uint32_t            ea_addr_         = 0;       // Computed effective address
    bus_state_t         bus_prev_        = 0;       // Previous bus state (edge detection)
    uint16_t            reset_counter_   = 0;       // Counts clocks with RESET asserted

    // ── Test harness support ────────────────────────────────────
public:
    /// Returns true when the CPU is at an instruction boundary.
    /// Used by test harnesses to detect instruction completion.
    bool opdone() const {
        return current_handler_ == &m680x0_t::handle_decode
            && step_ == 0;
    }

    /// Set CPU to "ready to decode" state (skip reset sequence).
    /// Call after init() and loading registers to start executing from IRD.
    void prepare_for_test() {
        current_handler_ = &m680x0_t::handle_decode;
        step_ = 0;
        halted_ = false;
        stopped_ = false;
        reset_counter_ = 0;
    }

    // === Register accessors (for test harness) ===
    uint32_t reg_d(uint8_t n) const { return regs_.d[n & 7]; }
    uint32_t reg_a(uint8_t n) const { return regs_.a[n & 7]; }
    uint32_t reg_pc() const { return regs_.pc; }
    uint16_t reg_sr() const { return regs_.sr; }
    uint32_t reg_usp() const { return regs_.usp; }
    uint32_t reg_ssp() const { return regs_.ssp; }
    uint16_t reg_irc() const { return regs_.irc; }
    uint16_t reg_ir() const { return regs_.ir; }
    uint16_t reg_ird() const { return regs_.ird; }

    void set_reg_d(uint8_t n, uint32_t v) { regs_.d[n & 7] = v; }
    void set_reg_a(uint8_t n, uint32_t v) { regs_.a[n & 7] = v; }
    void set_reg_pc(uint32_t v) { regs_.pc = v; }
    void set_reg_sr(uint16_t v) { regs_.sr = v; }
    void set_reg_usp(uint32_t v) { regs_.usp = v; }
    void set_reg_ssp(uint32_t v) { regs_.ssp = v; }
    void set_reg_irc(uint16_t v) { regs_.irc = v; }
    void set_reg_ir(uint16_t v) { regs_.ir = v; }
    void set_reg_ird(uint16_t v) { regs_.ird = v; }

    // ── Debug registration ─────────────────────────────────────
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

    /// Update sr_snapshot_ from current SR (call before debug rendering)
    void sync_debug_snapshot() {
        sr_snapshot_[0] = static_cast<uint8_t>(regs_.sr & 0xFF);
        sr_snapshot_[1] = static_cast<uint8_t>((regs_.sr >> 8) & 0xFF);
    }

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
