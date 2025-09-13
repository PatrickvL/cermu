#ifndef CPU_DEFS_HPP
#define CPU_DEFS_HPP

#include <cstdint>

// CPU register indices - type-safe enum
enum class CpuReg : uint8_t {
    A = 0, X = 1, Y = 2, S = 3, P = 4,
    PCL = 5, PCH = 6, ABL = 7, ABH = 8, DL = 9,
    ADL = 10, ADH = 11, DBR = 12, PBR = 13, COUNT = 14
};

// Status register bits
constexpr uint8_t P_CARRY = 0x01, P_ZERO = 0x02, P_IRQ_DIS = 0x04, P_DECIMAL = 0x08;
constexpr uint8_t P_BREAK = 0x10, P_UNUSED = 0x20, P_OVERFLOW = 0x40, P_NEGATIVE = 0x80;

// State flags
constexpr uint16_t STATE_RESET_PENDING = 0x0001, STATE_SYNC_NEXT = 0x0002;
constexpr uint16_t STATE_JAM_STATE = 0x0004, STATE_PAGE_CROSSED = 0x0008;
constexpr uint16_t STATE_BRANCH_TAKEN = 0x0010, STATE_NMI_EDGE = 0x0020;
constexpr uint16_t STATE_IRQ_LINE = 0x0040, STATE_NMI_PENDING = 0x0080;
constexpr uint16_t STATE_IRQ_PENDING = 0x0100, STATE_PENDING_DATA = 0x0200;
constexpr uint16_t STATE_RDY_WAIT = 0x0400, STATE_INTERRUPT_SEQUENCE = 0x0800;
constexpr uint16_t STATE_DMA_CYCLE = 0x1000, STATE_WAI_MODE = 0x2000;
constexpr uint16_t STATE_STP_MODE = 0x4000, STATE_SO_EDGE = 0x8000;

// Virtual opcode constants for interrupt handlers (stored at indices 256+ in cycle table)
constexpr uint16_t VIRTUAL_OPCODE_RESET = 256;  // Virtual opcode for RESET sequence
constexpr uint16_t VIRTUAL_OPCODE_NMI   = 257;  // Virtual opcode for NMI sequence
constexpr uint16_t VIRTUAL_OPCODE_IRQ   = 258;  // Virtual opcode for IRQ sequence

// Memory operations - type-safe enum
enum class MemOp : uint8_t {
    NOP = 0, READ_PC_INC, READ_PC, READ_ABS, WRITE_ABS,
    READ_ZP, WRITE_ZP, READ_ZPX, WRITE_ZPX, READ_ZPY,
    WRITE_ZPY, READ_SP, WRITE_SP_DEC, READ_SP_INC,
    READ_INDIRECT, READ_VECTOR
};

// Data operations - type-safe enum
enum class DataOp : uint8_t {
    LOAD_A = 0, LOAD_X = 1, LOAD_Y = 2,
    STORE_A = 3, STORE_X = 4, STORE_Y = 5, STORE_ZERO = 6,
    ALU = 14, BRANCH = 13, ADDR_CALC_LOW = 12, ADDR_CALC_HIGH = 11,
    STACK_PUSH = 10, STACK_PULL = 9, INTERRUPT_VEC = 8,
    BIT_TEST = 7, ILLEGAL_COMBO = 6, JMP = 5, NOP = 15,
    TEMP_STORE = 16, TEMP_MODIFY = 17, ADDR_ADD_X = 18, ADDR_ADD_Y = 19,
    INDIRECT_LOW = 20, INDIRECT_HIGH = 21
};

// ALU operations - type-safe enum (expanded for 6-bit field)
enum class AluOp : uint8_t {
    NOP = 0, ADC, SBC, AND, ORA, EOR, CMP, CPX, CPY,
    ASL, LSR, ROL, ROR, INC, DEC, BIT,
    TXA, TAX, TYA, TAY, TSX, TXS,
    CLC, SEC, CLI, SEI, CLV, CLD, SED,
    STZ, TSB, TRB, WAI, STP, PHX, PHY, PLX, PLY,
    LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA,
    ANC, ALR, ARR, JAM
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

// Type alias for CPU register array
using CpuRegisterArray = type_safe_array<uint8_t, static_cast<std::size_t>(CpuReg::COUNT)>;

#endif // CPU_DEFS_HPP