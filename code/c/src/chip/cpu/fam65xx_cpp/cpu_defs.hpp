#ifndef CPU_DEFS_HPP
#define CPU_DEFS_HPP

#include <cstdint>
#include <cstddef>

// CPU register indices - type-safe enum
enum class CpuReg : uint8_t {
    A = 0, X = 1, Y = 2, S = 3, P = 4,
    PCL = 5, PCH = 6, ABL = 7, ABH = 8, DL = 9,
    ADL = 10, ADH = 11, DBR = 12, PBR = 13, COUNT = 14
};

// Status register bits
constexpr uint8_t P_CARRY = 0x01, P_ZERO = 0x02, P_IRQ_DIS = 0x04, P_DECIMAL = 0x08;
constexpr uint8_t P_BREAK = 0x10, P_UNUSED = 0x20, P_OVERFLOW = 0x40, P_NEGATIVE = 0x80;

// State flags - ALL USE uint32_t to prevent type casting bugs with extended flags
constexpr uint32_t STATE_RESET_PENDING = 0x0001, STATE_SYNC_NEXT = 0x0002;
constexpr uint32_t STATE_JAM_STATE = 0x0004, STATE_PAGE_CROSSED = 0x0008;
constexpr uint32_t STATE_BRANCH_TAKEN = 0x0010, STATE_NMI_EDGE = 0x0020;
constexpr uint32_t STATE_IRQ_LINE = 0x0040, STATE_NMI_PENDING = 0x0080;
constexpr uint32_t STATE_IRQ_PENDING = 0x0100, STATE_PENDING_DATA = 0x0200;
constexpr uint32_t STATE_RDY_WAIT = 0x0400, STATE_INTERRUPT_SEQUENCE = 0x0800;
constexpr uint32_t STATE_DMA_CYCLE = 0x1000, STATE_WAI_MODE = 0x2000;
constexpr uint32_t STATE_STP_MODE = 0x4000, STATE_SO_EDGE = 0x8000;

// Extended state flags for 65C816 - consistent uint32_t type prevents casting bugs
constexpr uint32_t STATE_ABORT_PENDING = 0x10000; // Bit 16
constexpr uint32_t STATE_COP_PENDING = 0x20000;   // Bit 17

// φ1/φ2 Phase coordination flags - hardware-accurate timing
constexpr uint32_t STATE_PHI1_ACTIVE = 0x40000;   // Bit 18 - φ1 phase active
constexpr uint32_t STATE_PHI2_ACTIVE = 0x80000;   // Bit 19 - φ2 phase active
constexpr uint32_t STATE_BUS_AVAILABLE = 0x100000; // Bit 20 - Bus available to CPU (not DMA)
constexpr uint32_t STATE_ADDRESS_SETUP = 0x200000; // Bit 21 - Address setup complete

// Memory operations - type-safe enum (OPTIMIZED: reads first, writes second for branchless comparison)
enum class MemOp : uint8_t {
    // === READ OPERATIONS (0-8) ===
    NOP = 0, READ_PC_INC, READ_PC, READ_ABS, READ_ZP,
    READ_ZPX, READ_ZPY, READ_SP, READ_SP_INC,
    // === WRITE OPERATIONS (9+) ===
    WRITE_ABS = 9, WRITE_ZP, WRITE_ZPX, WRITE_ZPY, WRITE_SP_DEC,
    // Interrupt vector reading - functionally identical to READ_ABS but semantically distinct
    READ_VECTOR = READ_ABS  // Alias to READ_ABS - same memory operation, different semantic meaning
};

// PERFORMANCE: Branchless read/write detection cutoff point
constexpr uint8_t MEMOP_WRITE_CUTOFF = static_cast<uint8_t>(MemOp::WRITE_ABS);

// Data operations - type-safe enum
enum class DataOp : uint8_t {
    LOAD_A = 0, LOAD_X = 1, LOAD_Y = 2,
    STORE_A = 3, STORE_X = 4, STORE_Y = 5, STORE_ZERO = 6,
    BIT_TEST = 7, INTERRUPT_VEC = 8, STACK_PULL = 9, STACK_PUSH = 10,
    ADDR_CALC_HIGH = 11, ADDR_CALC_LOW = 12, BRANCH = 13, ALU = 14, NOP = 15,
    TEMP_STORE = 16, TEMP_MODIFY = 17, ADDR_ADD_X = 18, ADDR_ADD_Y = 19,
    INDIRECT_LOW = 20, INDIRECT_HIGH = 21, JMP = 22, ILLEGAL_COMBO = 23
};

// ALU operations - type-safe enum (expanded for 6-bit field)
enum class AluOp : uint8_t {
    NOP = 0, ADC, SBC, AND, ORA, EOR, CMP, CPX, CPY,
    ASL, LSR, ROL, ROR, INC, DEC, BIT,
    TXA, TAX, TYA, TAY, TSX, TXS,
    INX, DEX, INY, DEY,  // Register-specific increment/decrement operations
    CLC, SEC, CLI, SEI, CLV, CLD, SED,
    STZ, TSB, TRB, WAI, STP, PHX, PHY, PLX, PLY,
    LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA,
    ANC, ALR, ARR, JAM, BRK_FLAG,
    ASL_ACC, LSR_ACC, ROL_ACC, ROR_ACC  // Accumulator-specific shift/rotate operations
};

// Type-safe register array wrapper
template<typename T, std::size_t N>
class type_safe_array {
private:
    T data[N];
    
public:
    // Allow indexing with enum class members directly
    constexpr T& operator[](CpuReg reg) noexcept {
        return data[static_cast<std::size_t>(reg)];
    }
    
    constexpr const T& operator[](CpuReg reg) const noexcept {
        return data[static_cast<std::size_t>(reg)];
    }
    
    // Also allow traditional integer indexing for compatibility
    constexpr T& operator[](std::size_t index) noexcept {
        return data[index];
    }
    
    constexpr const T& operator[](std::size_t index) const noexcept {
        return data[index];
    }
    
    // Iterator support for range-based loops
    constexpr T* begin() noexcept { return data; }
    constexpr const T* begin() const noexcept { return data; }
    constexpr T* end() noexcept { return data + N; }
    constexpr const T* end() const noexcept { return data + N; }
    
    // Size information
    constexpr std::size_t size() const noexcept { return N; }
};

// 16-bit CPU register pairs for union-based access (similar to CIA timers)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
enum class CpuReg16 : uint8_t {
    PC = static_cast<uint8_t>(CpuReg::PCL) / 2,    // PCL/PCH pair
    AB = static_cast<uint8_t>(CpuReg::ABL) / 2,    // ABL/ABH pair
    AD = static_cast<uint8_t>(CpuReg::ADL) / 2,    // ADL/ADH pair
    COUNT_16 = 7  // Half of 14 registers (rounded up)
};
#else
enum class CpuReg16 : uint8_t {
    PC = static_cast<uint8_t>(CpuReg::PCH) / 2,    // PCH/PCL pair (big-endian)
    AB = static_cast<uint8_t>(CpuReg::ABH) / 2,    // ABH/ABL pair (big-endian)
    AD = static_cast<uint8_t>(CpuReg::ADH) / 2,    // ADH/ADL pair (big-endian)
    COUNT_16 = 7  // Half of 14 registers (rounded up)
};
#endif

// Enhanced type-safe register array with union-based 16-bit access
template<typename T, std::size_t N>
class enhanced_register_array {
private:
    union {
        T data[N];
        uint16_t words[N / 2];
    };
    
public:
    // 8-bit register access with enum class members directly
    constexpr T& operator[](CpuReg reg) noexcept {
        return data[static_cast<std::size_t>(reg)];
    }
    
    constexpr const T& operator[](CpuReg reg) const noexcept {
        return data[static_cast<std::size_t>(reg)];
    }
    
    // 16-bit register access for register pairs
    constexpr uint16_t& get_word(CpuReg16 reg16) noexcept {
        return words[static_cast<std::size_t>(reg16)];
    }
    
    constexpr const uint16_t& get_word(CpuReg16 reg16) const noexcept {
        return words[static_cast<std::size_t>(reg16)];
    }
    
    // Traditional integer indexing for compatibility
    constexpr T& operator[](std::size_t index) noexcept {
        return data[index];
    }
    
    constexpr const T& operator[](std::size_t index) const noexcept {
        return data[index];
    }
    
    // Iterator support for range-based loops
    constexpr T* begin() noexcept { return data; }
    constexpr const T* begin() const noexcept { return data; }
    constexpr T* end() noexcept { return data + N; }
    constexpr const T* end() const noexcept { return data + N; }
    
    // Size information
    constexpr std::size_t size() const noexcept { return N; }
};

// Type alias for enhanced CPU register array with union-based 16-bit access
using CpuRegisterArray = enhanced_register_array<uint8_t, static_cast<std::size_t>(CpuReg::COUNT)>;

// Inline helper functions for 16-bit CPU register operations (following CIA timer pattern)
// These provide convenient, endian-safe access to 16-bit register pairs

// Program Counter (PC) helpers
inline uint16_t get_pc_unified(const CpuRegisterArray& reg) {
    return reg.get_word(CpuReg16::PC);
}

inline void set_pc_unified(CpuRegisterArray& reg, uint16_t value) {
    reg.get_word(CpuReg16::PC) = value;
}

inline void increment_pc_unified(CpuRegisterArray& reg) {
    reg.get_word(CpuReg16::PC)++;
}

// Address Bus (AB) helpers
inline uint16_t get_ab_unified(const CpuRegisterArray& reg) {
    return reg.get_word(CpuReg16::AB);
}

inline void set_ab_unified(CpuRegisterArray& reg, uint16_t value) {
    reg.get_word(CpuReg16::AB) = value;
}

// Address Data (AD) helpers
inline uint16_t get_ad_unified(const CpuRegisterArray& reg) {
    return reg.get_word(CpuReg16::AD);
}

inline void set_ad_unified(CpuRegisterArray& reg, uint16_t value) {
    reg.get_word(CpuReg16::AD) = value;
}

// Optimized N/Z flag setting helper (extracted from fam65xx.hpp for reuse)
inline void set_nz_flags_unified(CpuRegisterArray& reg, uint8_t value) {
    reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_NEGATIVE | P_ZERO)) |
                     ((value & 0x80) ? P_NEGATIVE : 0) |
                     (value == 0 ? P_ZERO : 0);
}

#endif // CPU_DEFS_HPP