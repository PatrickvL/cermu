#pragma once
/*
 * fam65xx_types.h - C Interface Types and Compatibility Layer
 *
 * This file contains all C-compatible type definitions, macros, and function
 * prototypes for the MOS 65xx family CPU emulator. It provides the C interface
 * layer that bridges between C code and the C++ template implementation.
 */

#include <cstdint>
#include <type_traits>

// Include system-wide bus definitions
#include "core/cermu.hpp"
#include "chip/cpu/cpu_chip_base.hpp"
#include "core/system_lines.hpp"

// ============================================================================
// Bus State and Pin Definitions
// ============================================================================

// Legacy pin compatibility - map to system_lines.h definitions
#define FAM65XX_RW BUS_BIT(BUS_RW_BIT)
#define FAM65XX_SYNC BUS_BIT(BUS_SYNC_BIT)
#define FAM65XX_IRQ BUS_BIT(BUS_IRQ_BIT)
#define FAM65XX_NMI BUS_BIT(BUS_NMI_BIT)
#define FAM65XX_RDY BUS_BIT(BUS_RDY_BIT)
#define FAM65XX_AEC BUS_BIT(BUS_AEC_BIT)
#define FAM65XX_RES BUS_BIT(BUS_RES_BIT)
#define FAM65XX_SO  BUS_BIT(BUS_SO_BIT)

// Macros for FAM65XX bus access
#define FAM65XX_GET_ADDR(p) BUS_GET_ADDR(p)
#define FAM65XX_SET_ADDR(p, d) BUS_SET_ADDR(p, d)
#define FAM65XX_GET_DATA(p) BUS_GET_DATA(p)
#define FAM65XX_SET_DATA(p, d) BUS_SET_DATA(p, d)

// Bank byte handling for 65C816 (uses upper 8 bits of 32-bit bus state)
#define FAM65XX_GET_BANK(p) (((p) >> 24) & 0xFF)
#define FAM65XX_SET_BANK(p, bank)                                              \
  ((p) = ((p) & ~0xFF000000ULL) | (((uint64_t)(bank)&0xFF) << 24))

// CPU pin access using project definitions
#define FAM65XX_GET_RDY(pins) ((pins)&FAM65XX_RDY)
#define FAM65XX_GET_AEC(pins) ((pins)&FAM65XX_AEC)
#define FAM65XX_SET_SYNC(pins, v)                                              \
  ((pins) = ((v) ? ((pins) | FAM65XX_SYNC) : ((pins) & ~FAM65XX_SYNC)))
#define FAM65XX_GET_SYNC(pins) ((pins)&FAM65XX_SYNC)
#define FAM65XX_GET_IRQ(pins) ((pins)&FAM65XX_IRQ)
#define FAM65XX_GET_NMI(pins) ((pins)&FAM65XX_NMI)
#define FAM65XX_GET_RW(pins) ((pins)&FAM65XX_RW)

// ============================================================================
// CPU Flags
// ============================================================================

// CPU Flags - Modern C++ class enum
enum class cpu_flags : uint16_t {
  CARRY = 0x01,             // Carry
  ZERO = 0x02,              // Zero
  INTERRUPT_DISABLE = 0x04, // Interrupt Disable
  DECIMAL_MODE = 0x08,      // Decimal Mode
  BREAK_FLAG = 0x10,        // Break (renamed to avoid conflicts)
  UNUSED = 0x20,            // Unused (always 1) - 6502/6510/65C02
  OVERFLOW_FLAG = 0x40,     // Overflow (renamed to avoid conflicts)
  NEGATIVE = 0x80,          // Negative

  // 65C816-specific flags (redefine bit meanings in native mode)
  INDEX_SELECT = 0x10, // Index Register Select (0 = 16-bit, 1 = 8-bit) - 65C816
  MEMORY_SELECT =
      0x20, // Memory/Accumulator Select (0 = 16-bit, 1 = 8-bit) - 65C816
  EMULATION_MODE = 0x100 // Emulation mode (not in P register, separate)
};

// Flag access macros
#define FLAG_C static_cast<uint8_t>(cpu_flags::CARRY)
#define FLAG_Z static_cast<uint8_t>(cpu_flags::ZERO)
#define FLAG_I static_cast<uint8_t>(cpu_flags::INTERRUPT_DISABLE)
#define FLAG_D static_cast<uint8_t>(cpu_flags::DECIMAL_MODE)
#define FLAG_B static_cast<uint8_t>(cpu_flags::BREAK_FLAG)
#define FLAG_U static_cast<uint8_t>(cpu_flags::UNUSED)
#define FLAG_V static_cast<uint8_t>(cpu_flags::OVERFLOW_FLAG)
#define FLAG_N static_cast<uint8_t>(cpu_flags::NEGATIVE)
#define FLAG_X                                                                 \
  static_cast<uint8_t>(cpu_flags::INDEX_SELECT) // INDEX_SELECT (65C816)
#define FLAG_M                                                                 \
  static_cast<uint8_t>(cpu_flags::MEMORY_SELECT) // MEMORY_SELECT (65C816)
#define FLAG_E                                                                 \
  static_cast<uint16_t>(cpu_flags::EMULATION_MODE) // EMULATION_MODE (65C816)

// Interrupt types - Modern C++ scoped enum (ordered by priority: higher value =
// higher priority)
enum class InterruptType : uint8_t {
  NONE = 0, // No interrupt active
  BRK,      // Software interrupt (BRK instruction) and default/fallback
  IRQ,      // Maskable interrupt
  COP,      // CoProcessor instruction (65C816)
  NMI,      // Non-maskable interrupt
  ABORT,    // Abort interrupt (65C816)
  RESET     // Reset interrupt (highest priority)
};

// C-style enum aliases
using interrupt_t = InterruptType;
inline constexpr auto FAM65XX_INT_NONE = InterruptType::NONE;
inline constexpr auto FAM65XX_INT_BRK = InterruptType::BRK;
inline constexpr auto FAM65XX_INT_IRQ = InterruptType::IRQ;
inline constexpr auto FAM65XX_INT_COP = InterruptType::COP;
inline constexpr auto FAM65XX_INT_NMI = InterruptType::NMI;
inline constexpr auto FAM65XX_INT_ABORT = InterruptType::ABORT;
inline constexpr auto FAM65XX_INT_RESET = InterruptType::RESET;

// Interrupt shift register bit layout - merged system (3 bits per interrupt +
// separators) Ordered by priority: RESET > ABORT > NMI > COP > IRQ > BRK
#define INT_BRK_START_BIT 0    // BRK uses bits 0-2 (3 bits)
#define INT_BRK_SEP_BIT 3      // Separator bit after BRK (bit 3)
#define INT_IRQ_START_BIT 4    // IRQ uses bits 4-6 (3 bits)
#define INT_IRQ_SEP_BIT 7      // Separator bit after IRQ (bit 7)
#define INT_COP_START_BIT 8    // COP uses bits 8-10 (3 bits)
#define INT_COP_SEP_BIT 11     // Separator bit after COP (bit 11)
#define INT_NMI_START_BIT 12   // NMI uses bits 12-14 (3 bits)
#define INT_NMI_SEP_BIT 15     // Separator bit after NMI (bit 15)
#define INT_ABORT_START_BIT 16 // ABORT uses bits 16-18 (3 bits)
#define INT_ABORT_SEP_BIT 19   // Separator bit after ABORT (bit 19)
#define INT_RESET_START_BIT 20 // RESET uses bits 20-22 (3 bits)
#define INT_RESET_SEP_BIT 23   // Separator bit after RESET (bit 23)

// Interrupt masks (3 bits each)
#define INT_BRK_MASK (0x7 << INT_BRK_START_BIT)     // 3 bits: 0b111
#define INT_IRQ_MASK (0x7 << INT_IRQ_START_BIT)     // 3 bits: 0b111
#define INT_COP_MASK (0x7 << INT_COP_START_BIT)     // 3 bits: 0b111
#define INT_NMI_MASK (0x7 << INT_NMI_START_BIT)     // 3 bits: 0b111
#define INT_ABORT_MASK (0x7 << INT_ABORT_START_BIT) // 3 bits: 0b111
#define INT_RESET_MASK (0x7 << INT_RESET_START_BIT) // 3 bits: 0b111

// Separator bits mask
#define INT_SEPARATOR_MASK                                                     \
  ((1 << INT_BRK_SEP_BIT) | (1 << INT_IRQ_SEP_BIT) | (1 << INT_COP_SEP_BIT) |  \
   (1 << INT_NMI_SEP_BIT) | (1 << INT_ABORT_SEP_BIT) |                         \
   (1 << INT_RESET_SEP_BIT))

// ============================================================================
// ADDRESSING MODE AND OPERATION ENUMS
// ============================================================================
// Addressing modes describe how operands are fetched.
// Operations describe what the CPU does with those operands.
// These are combined in the opcode table to minimize redundancy.
//
// AM_NON (0): No addressing mode handler needed
//   - Used by: Implicit, Immediate, Accumulator, and Relative modes
//   - These modes either have no operand, operand in next byte, or
//     operate directly on registers without memory access

enum class AddressingMode : uint8_t {
  // ========================================================================
  // Core addressing modes (0-19) - OPTIMIZED LAYOUT FOR EFFICIENT CHECKS
  // ========================================================================
  // NON and IMM first (no address calculation needed)
  // Zero-page/Direct Page modes grouped after IMM for single-comparison check
  // ========================================================================

  // Universal modes - supported by all 65xx family processors
  NON = 0, /* No addressing handler (Implicit/Accumulator/Relative/Special) */
  IMM,     /* Immediate - operand is next byte - All CPUs */
  
  // Zero-page/Direct Page modes grouped together for efficient range check (am <= ZPY)
  ZER, /* Zero Page (6502/6510/65C02) / Direct Page (65C816) - All CPUs */
  ZPX, /* Zero Page,X (6502/6510/65C02) / Direct Page,X (65C816) - All CPUs */
  ZPY, /* Zero Page,Y (6502/6510/65C02) / Direct Page,Y (65C816) - All CPUs */

  // Other memory addressing modes
  ABS, /* Absolute - operand at $nnnn - All CPUs */
  ABX, /* Absolute,X - operand at $nnnn + X - All CPUs */
  ABY, /* Absolute,Y - operand at $nnnn + Y - All CPUs */
  IND, /* Indirect (abs) - JMP only - 6502/6510/65C02/65C816 */
  INX, /* Indexed Indirect (zp,X) - 6502/6510 / (dp,X) - 65C02/65C816 */
  INY, /* Indirect Indexed (zp),Y - 6502/6510 / (dp),Y - 65C02/65C816 */

  // CMOS enhancements - 65C02 and 65C816 only
  ZPR, /* Zero Page Relative zp,rel - BBR/BBS - Rockwell 65C02 only */
  ZPI, /* Zero Page Indirect (dp) - 65C02/65C816 only */

  // 65C816 exclusive addressing modes
  ABI,   /* Absolute Indexed Indirect (abs,X) - JMP/JSR - 65C816 only */
  ABL,   /* Absolute Long $nnnnnn - 65C816 only */
  ABLX,  /* Absolute Long,X $nnnnnn,X - 65C816 only */
  DPIL,  /* Direct Page Indirect Long [dp] - 65C816 only */
  DPILY, /* Direct Page Indirect Long,Y [dp],Y - 65C816 only */
  SR,    /* Stack Relative n,S - 65C816 only */
  SRI,   /* Stack Relative Indirect Indexed (n,S),Y - 65C816 only */

  COUNT, /* Total count = 20, fits in 5-bit am_index */

  // ========================================================================
  // Aliases for documentation/clarity
  // ========================================================================
  ACC = NON, /* Accumulator - operate on A register - All CPUs */
  IMP = NON, /* Implied/Implicit - no operand - All CPUs */
  REL = NON, /* Relative - branch offset - All CPUs */

  // ========================================================================
  // Aliases for 65C816 compatibility
  // ========================================================================
  DP = ZER,  /* Direct Page -> Zero Page */
  DPX = ZPX, /* Direct Page,X -> Zero Page,X */
  DPY = ZPY, /* Direct Page,Y -> Zero Page,Y */
  DPI = ZPI  /* Direct Page Indirect (dp) -> Zero Page Indirect */
};

enum class Operation : uint8_t {
  // Core 6502 operations (0-45) - these are used in all processors
  LDA,
  LDX,
  LDY, // 0-2: Load operations
  STA,
  STX,
  STY, // 3-5: Store operations
  ADC,
  SBC, // 6-7: Arithmetic
  AND,
  ORA,
  EOR, // 8-10: Logic operations
  CMP,
  CPX,
  CPY, // 11-13: Compare operations
  ASL,
  LSR,
  ROL,
  ROR, // 14-17: Shift/rotate
  INC,
  DEC, // 18-19: Increment/decrement
  INX,
  INY,
  DEX,
  DEY, // 20-23: Register inc/dec
  TAX,
  TAY,
  TXA,
  TYA,
  TSX,
  TXS, // 24-29: Transfer operations
  PHA,
  PHP,
  PLA,
  PLP, // 30-33: Stack operations
  BCC,
  BCS,
  BEQ,
  BNE,
  BMI,
  BPL,
  BVC,
  BVS, // 34-41: Branches
  CLC,
  SEC,
  CLI,
  SEI,
  CLD,
  SED,
  CLV, // 42-48: Flag operations
  JMP,
  JSR,
  RTS,
  RTI,
  BRK, // 49-53: Control flow
  BIT,
  NOP,
  JAM, // 54-56: Test/misc

  // Illegal opcodes - most commonly used in 6502/6510 (57-74)
  LAX,
  SAX,
  DCP,
  ISC,
  SLO,
  RLA,
  SRE,
  RRA, // 57-64: Combo ops
  ANC,
  ASR,
  ARR,
  SBX, // 65-68: Special accumulator
  SHA,
  SHS,
  SHX,
  SHY,
  LAS, // 69-73: Store with AND
  XAA, // 74: Special operation

  // 65C02 enhancements (75-84)
  BRA,
  STZ,
  TRB,
  TSB,
  PHX,
  PHY,
  PLX,
  PLY,
  WAI,
  STP, // 75-84

  // Extended operations (85-116) - these will map to NOP in 7-bit tables
  // but can be handled via processor-specific logic
  RMB0,
  RMB1,
  RMB2,
  RMB3,
  RMB4,
  RMB5,
  RMB6,
  RMB7,
  SMB0,
  SMB1,
  SMB2,
  SMB3,
  SMB4,
  SMB5,
  SMB6,
  SMB7,
  BBR0,
  BBR1,
  BBR2,
  BBR3,
  BBR4,
  BBR5,
  BBR6,
  BBR7,
  BBS0,
  BBS1,
  BBS2,
  BBS3,
  BBS4,
  BBS5,
  BBS6,
  BBS7,
  // 65C816 16-bit operations (117-135)
  REP,
  SEP,
  XBA,
  XCE,
  COP,
  WDM,
  PEA,
  PER,
  PEI,
  PHB,
  PHD,
  PHK,
  PLB,
  PLD,
  RTL,
  JSL,
  JML,
  MVN,
  MVP,

  COUNT
};

// ============================================================================
// REGISTER FILE CONSTANTS — typed indices into RegisterFile<24, uint16_t>
// ============================================================================
//
// 16-bit pairs are declared first as r16, then lo_b/hi_b extract the
// endian-correct byte offsets.  No #ifdef blocks needed here — the
// lo_b/hi_b helpers (register_file.hpp) handle endianness at compile time.

#include "core/register_file.hpp"

namespace reg {

// --- 16-bit register pairs (byte offsets into the register file) ---
inline constexpr r16 SP  {0};   // Stack Pointer
inline constexpr r16 AB  {2};   // Address Bus
inline constexpr r16 PC  {4};   // Program Counter
inline constexpr r16 A16 {6};   // Full Accumulator (65C816)
inline constexpr r16 X16 {8};   // Full X Index (65C816)
inline constexpr r16 Y16 {10};  // Full Y Index (65C816)
inline constexpr r16 P16 {12};  // Processor Status + E flag (65C816)
inline constexpr r16 D16 {16};  // Direct Page (65C816)

// --- 8-bit sub-registers (endian-aware via lo_b/hi_b) ---
inline constexpr r8 SPL = lo_b(SP);   inline constexpr r8 SPH = hi_b(SP);
inline constexpr r8 ABL = lo_b(AB);   inline constexpr r8 ABH = hi_b(AB);
inline constexpr r8 PCL = lo_b(PC);   inline constexpr r8 PCH = hi_b(PC);
inline constexpr r8 AL  = lo_b(A16);  inline constexpr r8 AH  = hi_b(A16);
inline constexpr r8 XL  = lo_b(X16);  inline constexpr r8 XH  = hi_b(X16);
inline constexpr r8 YL  = lo_b(Y16);  inline constexpr r8 YH  = hi_b(Y16);
inline constexpr r8 PL  = lo_b(P16);  inline constexpr r8 PH  = hi_b(P16);
inline constexpr r8 DPL = lo_b(D16);  inline constexpr r8 DPH = hi_b(D16);

// --- Non-paired 8-bit registers ---
inline constexpr r8 IR  {14};  // Instruction Register (current opcode)
inline constexpr r8 DL  {15};  // Data Latch (internal)

// --- 65C816-only single-byte registers ---
inline constexpr r8 DBR {18};  // Data Bank Register
inline constexpr r8 PBR {19};  // Program Bank Register
inline constexpr r8 SBR {20};  // Source Bank Register
inline constexpr r8 ZBR {21};  // Zero Bank Register (hardwired 0x00)

// --- Register counts ---
inline constexpr uint8_t COUNT_8BIT  = 16;  // Core registers (0-15)
inline constexpr uint8_t COUNT_16BIT = 22;  // With 65C816 extended

// --- Compatibility aliases (8-bit) ---
inline constexpr r8 S   = SPL;  // S register → SPL
inline constexpr r8 A   = AL;   // A register → AL (accumulator low)
inline constexpr r8 B   = AH;   // B register → AH (accumulator high, 65C816)
inline constexpr r8 X   = XL;   // X register → XL
inline constexpr r8 Y   = YL;   // Y register → YL
inline constexpr r8 P   = PL;   // P register → PL
inline constexpr r8 MEM = DL;   // Memory pseudo-register (forces 8-bit width)

// --- Compatibility aliases (16-bit) ---
inline constexpr r16 C = A16;   // Legacy C → full 16-bit accumulator
inline constexpr r16 D = D16;   // Direct Page → D16

// --- Width mode for 65C816 template dispatch ---
// Replaces reg8_t as NTTP in calc_n_flag<>, calc_nz_flags<>, etc.
enum class WidthMode : uint8_t { ACC, IDX, MEM };

} // namespace reg
using namespace reg;

#ifdef _WIN32
// winnt.h defines 'typedef CHAR *PCH;' at global scope, which clashes with
// reg::PCH (a constexpr r8) when 'using namespace reg;' is applied.
// Force unambiguous resolution via a macro redirect.
#define PCH reg::PCH
#endif

// Addr enum class for template parameters — values ARE byte offsets
enum class Addr : uint8_t {
  AB = 2,  // Address Bus  (byte offset of r16 AB)
  PC = 4,  // Program Counter (byte offset of r16 PC)
  SP = 0   // Stack pointer (byte offset of r16 SP)
};

// Bank enum class for template parameters — values ARE byte offsets
// (used for runtime register lookup)
enum class Bank : uint8_t {
  DBR = 18, // Data Bank register
  PBR = 19, // Program Bank register
  SBR = 20, // Source Bank register
  ZBR = 21  // Zero Bank register (hardwired to 0x00)
};

// ============================================================================
// Opcode Encoding
// ============================================================================

enum class OpcodeFlags : uint8_t {
  NONE = 0x0,          // No special flags
  ILLEGAL_STORE = 0x1, // Illegal store quirk - uses wrong address on page cross
  SKIP_PAGE = 0x2,     // Can skip page cross penalty cycle (read operations only)
  OPTIMIZED_CYCLE = 0x2, // Single-cycle optimizable (AM::NON implicit operations only) - SHARES bit with SKIP_PAGE
  RMW = 0x4            // Read-Modify-Write operation
};

// ============================================================================
// CONVENIENT TYPE ALIASES FOR CLEAN SYNTAX
// ============================================================================

// Type aliases for cleaner opcode table syntax
using OP = Operation;
using AM = AddressingMode;
using OF = OpcodeFlags;

// ============================================================================
// MODERN C++ HELPER FUNCTIONS
// ============================================================================

// Helper function to convert scoped enums to indices for array access
// This eliminates the need for static_cast<size_t>() everywhere
template <typename E> constexpr inline auto to_index(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

struct opcode_info_t {
  uint16_t op_index : 8; // Operation index (0-255, bits 0-7) [type Operation]
  uint16_t am_index : 5; // Addressing mode index (0-31, bits 8-12) [type AddressingMode]
  uint16_t flags : 3;    // Opcode flags (bits 13-15) [type OpcodeFlags]

  inline bool is_illegal_store() const {
    return (flags & to_index(OF::ILLEGAL_STORE)) != 0;
  }
  
  inline bool can_skip_page() const {
    // SKIP_PAGE only valid for non-NON addressing modes (memory operations)
    return am_index != to_index(AM::NON) &&
           (flags & to_index(OF::SKIP_PAGE)) != 0;
  }
  
  inline bool is_optimized_cycle() const {
    // OPTIMIZED_CYCLE only valid for AM::NON addressing mode (implicit operations)
    // Shares bit with SKIP_PAGE but contexts are mutually exclusive
    return am_index == to_index(AM::NON) &&
           (flags & to_index(OF::OPTIMIZED_CYCLE)) != 0;
  }
  
  inline bool is_rmw() const {
    return (flags & to_index(OF::RMW)) != 0;
  }
  
  // Constructor to handle scoped enum conversion
  constexpr opcode_info_t(OP op, AM am, OF fl)
      : op_index(to_index(op)), am_index(to_index(am)), flags(to_index(fl)) {}

  // Default constructor for aggregate initialization
  constexpr opcode_info_t() : op_index(0), am_index(0), flags(0) {}
};
