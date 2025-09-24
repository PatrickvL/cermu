#ifndef PERFORMANCE_OPTIMIZATIONS_HPP
#define PERFORMANCE_OPTIMIZATIONS_HPP

#include "cpu_defs.hpp"
#include "cpu_config.hpp"
#include "../../../core/system_lines.h"

namespace fam65xx_cpp {

// === PERFORMANCE OPTIMIZATION HELPERS ===
// High-performance inline helper functions to eliminate function call overhead
// and optimize hot path execution in the CPU emulation

// === CONSTEXPR COMPILE-TIME OPTIMIZATIONS ===

// Compile-time opcode classification helpers
template<uint8_t OPCODE>
constexpr bool is_branch_instruction() {
    return (OPCODE >= 0x10 && OPCODE <= 0xF0 && (OPCODE & 0x1F) == 0x10);
}

template<uint8_t OPCODE>
constexpr bool is_stack_instruction() {
    return OPCODE == 0x08 || OPCODE == 0x28 || OPCODE == 0x48 || OPCODE == 0x68 ||
           OPCODE == 0x20 || OPCODE == 0x60 || OPCODE == 0x40;
}

template<uint8_t OPCODE>
constexpr bool is_jump_instruction() {
    return OPCODE == 0x4C || OPCODE == 0x6C || OPCODE == 0x20;
}

template<uint8_t OPCODE>
constexpr bool is_nop_instruction() {
    return OPCODE == 0xEA || OPCODE == 0x1A || OPCODE == 0x3A || OPCODE == 0x5A || 
           OPCODE == 0x7A || OPCODE == 0xDA || OPCODE == 0xFA ||  // Single-byte NOPs
           OPCODE == 0x80 || OPCODE == 0x82 || OPCODE == 0x89 || OPCODE == 0xC2 || 
           OPCODE == 0xE2 ||  // Immediate NOPs
           OPCODE == 0x04 || OPCODE == 0x44 || OPCODE == 0x64 ||  // Zero page NOPs
           OPCODE == 0x0C ||  // Absolute NOP
           OPCODE == 0x14 || OPCODE == 0x34 || OPCODE == 0x54 || OPCODE == 0x74 || 
           OPCODE == 0xD4 || OPCODE == 0xF4 ||  // Zero page,X NOPs
           OPCODE == 0x1C || OPCODE == 0x3C || OPCODE == 0x5C || OPCODE == 0x7C || 
           OPCODE == 0xDC || OPCODE == 0xFC;  // Absolute,X NOPs
}

// === BRANCHLESS OPTIMIZATION HELPERS ===

// Branchless flag operations using bit manipulation
template<typename RegArray>
inline void set_flag_branchless(RegArray& reg, uint8_t flag_mask, bool condition) {
    // Branchless: Set flag if condition is true, clear if false
    reg[CpuReg::P] = (reg[CpuReg::P] & ~flag_mask) | (condition ? flag_mask : 0);
}

template<typename RegArray>
inline void set_nz_flags_optimized(RegArray& reg, uint8_t value) {
    // Ultra-optimized N/Z flag setting using branchless bit manipulation
    constexpr uint8_t NZ_MASK = P_NEGATIVE | P_ZERO;
    const uint8_t flags = (value & P_NEGATIVE) | ((value == 0) << 1);
    reg[CpuReg::P] = (reg[CpuReg::P] & ~NZ_MASK) | flags;
}

// === MEMORY ACCESS OPTIMIZATIONS ===

// Optimized PC operations with minimal overhead
template<typename RegArray>
inline uint16_t get_pc_fast(const RegArray& reg) {
    return (static_cast<uint16_t>(reg[CpuReg::PCH]) << 8) | reg[CpuReg::PCL];
}

template<typename RegArray>
inline void set_pc_fast(RegArray& reg, uint16_t pc) {
    reg[CpuReg::PCL] = pc & 0xFF;
    reg[CpuReg::PCH] = (pc >> 8) & 0xFF;
}

template<typename RegArray>
inline uint16_t increment_pc_fast(RegArray& reg) {
    const uint16_t pc = get_pc_fast(reg);
    const uint16_t new_pc = (pc + 1) & 0xFFFF;
    set_pc_fast(reg, new_pc);
    return new_pc;
}

// Optimized address calculations
template<typename RegArray>
inline uint16_t get_abs_address(const RegArray& reg) {
    return (static_cast<uint16_t>(reg[CpuReg::ABH]) << 8) | reg[CpuReg::ABL];
}

template<typename RegArray>
inline void set_abs_address(RegArray& reg, uint16_t addr) {
    reg[CpuReg::ABL] = addr & 0xFF;
    reg[CpuReg::ABH] = (addr >> 8) & 0xFF;
}

// === HOT PATH OPTIMIZATIONS ===

// Ultra-fast opcode classification using constexpr functions (C++11 compatible)
constexpr bool is_branch_opcode_fast(uint8_t opcode) {
    return opcode == 0x10 || opcode == 0x30 || opcode == 0x50 || opcode == 0x70 ||
           opcode == 0x90 || opcode == 0xB0 || opcode == 0xD0 || opcode == 0xF0;
}

constexpr bool is_stack_opcode_fast(uint8_t opcode) {
    return opcode == 0x08 || opcode == 0x28 || opcode == 0x48 || opcode == 0x68 ||
           opcode == 0x20 || opcode == 0x60 || opcode == 0x40;
}

constexpr bool is_jump_opcode_fast(uint8_t opcode) {
    return opcode == 0x4C || opcode == 0x6C || opcode == 0x20;
}

// === TEMPLATE SPECIALIZATION OPTIMIZATIONS ===

// Template-specialized branch condition checking
template<uint8_t OPCODE, typename RegArray>
inline bool check_branch_condition(const RegArray& reg) {
    static_assert(is_branch_instruction<OPCODE>(), "OPCODE must be a branch instruction");
    
    const uint8_t p_reg = reg[CpuReg::P];
    
    // Compile-time dispatch based on opcode
    if constexpr (OPCODE == 0x10) return !(p_reg & P_NEGATIVE);  // BPL
    if constexpr (OPCODE == 0x30) return (p_reg & P_NEGATIVE);   // BMI  
    if constexpr (OPCODE == 0x50) return !(p_reg & P_OVERFLOW);  // BVC
    if constexpr (OPCODE == 0x70) return (p_reg & P_OVERFLOW);   // BVS
    if constexpr (OPCODE == 0x90) return !(p_reg & P_CARRY);     // BCC
    if constexpr (OPCODE == 0xB0) return (p_reg & P_CARRY);      // BCS
    if constexpr (OPCODE == 0xD0) return !(p_reg & P_ZERO);      // BNE
    if constexpr (OPCODE == 0xF0) return (p_reg & P_ZERO);       // BEQ
    
    return false; // Should never reach here due to static_assert
}

// Template-specialized store register value retrieval
template<DataOp DATA_OP, typename RegArray>
inline uint8_t get_store_value_fast(const RegArray& reg) {
    static_assert(DATA_OP >= DataOp::STORE_A && DATA_OP <= DataOp::STORE_ZERO, 
                  "DATA_OP must be a store operation");
    
    // Compile-time dispatch
    if constexpr (DATA_OP == DataOp::STORE_A) return reg[CpuReg::A];
    if constexpr (DATA_OP == DataOp::STORE_X) return reg[CpuReg::X];
    if constexpr (DATA_OP == DataOp::STORE_Y) return reg[CpuReg::Y];
    if constexpr (DATA_OP == DataOp::STORE_ZERO) return 0x00;
    
    return 0x00; // Fallback
}

// === ARITHMETIC OPTIMIZATIONS ===

// Optimized decimal mode detection and calculation
template<typename BusConfig, typename RegArray>
inline bool is_decimal_mode_active(const RegArray& reg) {
    if constexpr (BusConfig::has_decimal_mode) {
        return (reg[CpuReg::P] & P_DECIMAL) != 0;
    } else {
        return false; // Compile-time constant
    }
}

// Fast BCD arithmetic helpers
inline uint8_t bcd_add_fast(uint8_t a, uint8_t b, uint8_t carry) {
    uint16_t lo_nibble = (a & 0x0F) + (b & 0x0F) + carry;
    uint16_t hi_nibble = (a >> 4) + (b >> 4);
    
    if (lo_nibble > 9) {
        lo_nibble += 6;
        hi_nibble += 1;
    }
    if (hi_nibble > 9) {
        hi_nibble += 6;
    }
    
    return ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
}

inline uint8_t bcd_sub_fast(uint8_t a, uint8_t b, uint8_t borrow) {
    int16_t lo_nibble = (a & 0x0F) - (b & 0x0F) - borrow;
    int16_t hi_nibble = (a >> 4) - (b >> 4);
    
    if (lo_nibble < 0) {
        lo_nibble -= 6;
        hi_nibble -= 1;
    }
    if (hi_nibble < 0) {
        hi_nibble -= 6;
    }
    
    return ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
}

// === CYCLE TIMING OPTIMIZATIONS ===

// Fast interrupt priority checking
template<uint32_t STATE_FLAGS>
inline constexpr uint32_t get_highest_priority_interrupt() {
    // Compile-time priority resolution using bit manipulation
    if constexpr (STATE_FLAGS & STATE_RESET_PENDING) return STATE_RESET_PENDING;
    if constexpr (STATE_FLAGS & STATE_ABORT_PENDING) return STATE_ABORT_PENDING;
    if constexpr (STATE_FLAGS & STATE_NMI_PENDING) return STATE_NMI_PENDING;
    if constexpr (STATE_FLAGS & STATE_COP_PENDING) return STATE_COP_PENDING;
    if constexpr (STATE_FLAGS & STATE_IRQ_PENDING) return STATE_IRQ_PENDING;
    return 0;
}

// Fast page crossing detection
inline bool page_crossed_fast(uint16_t addr1, uint16_t addr2) {
    return (addr1 ^ addr2) & 0xFF00;
}

// === MEMORY OPERATION OPTIMIZATIONS ===

// Optimized memory operation type detection
inline bool is_write_operation_fast(MemOp mem_op) {
    return static_cast<uint8_t>(mem_op) >= MEMOP_WRITE_CUTOFF;
}

inline bool is_read_operation_fast(MemOp mem_op) {
    const uint8_t op_value = static_cast<uint8_t>(mem_op);
    return (op_value > 0) && (op_value < MEMOP_WRITE_CUTOFF);
}

// Optimized stack address calculation
inline uint16_t get_stack_address_fast(uint8_t sp) {
    return 0x0100 | sp;
}

// === BUS STATE OPTIMIZATIONS ===

// Fast bus state manipulation using direct bit operations
inline bus_state_t set_bus_write_fast(bus_state_t bus_state) {
    return bus_state & ~BUS_RW_BIT; // Clear RW bit for write
}

inline bus_state_t set_bus_read_fast(bus_state_t bus_state) {
    return bus_state | BUS_RW_BIT; // Set RW bit for read  
}

inline bus_state_t set_bus_address_fast(bus_state_t bus_state, uint16_t addr) {
    return BUS_SET_ADDR(bus_state, addr);
}

inline bus_state_t set_bus_data_fast(bus_state_t bus_state, uint8_t data) {
    return BUS_SET_DATA(bus_state, data);
}

inline uint16_t get_bus_address_fast(bus_state_t bus_state) {
    return BUS_GET_ADDR(bus_state);
}

inline uint8_t get_bus_data_fast(bus_state_t bus_state) {
    return BUS_GET_DATA(bus_state);
}

// === REGISTER OPERATION OPTIMIZATIONS ===

// Fast register transfer operations
template<typename RegArray>
inline void transfer_a_to_x_fast(RegArray& reg) {
    const uint8_t value = reg[CpuReg::A];
    reg[CpuReg::X] = value;
    set_nz_flags_optimized(reg, value);
}

template<typename RegArray>
inline void transfer_x_to_a_fast(RegArray& reg) {
    const uint8_t value = reg[CpuReg::X];
    reg[CpuReg::A] = value;
    set_nz_flags_optimized(reg, value);
}

template<typename RegArray>
inline void transfer_a_to_y_fast(RegArray& reg) {
    const uint8_t value = reg[CpuReg::A];
    reg[CpuReg::Y] = value;
    set_nz_flags_optimized(reg, value);
}

template<typename RegArray>
inline void transfer_y_to_a_fast(RegArray& reg) {
    const uint8_t value = reg[CpuReg::Y];
    reg[CpuReg::A] = value;
    set_nz_flags_optimized(reg, value);
}

// Fast increment/decrement operations
template<typename RegArray>
inline void increment_x_fast(RegArray& reg) {
    const uint8_t result = (reg[CpuReg::X] + 1) & 0xFF;
    reg[CpuReg::X] = result;
    set_nz_flags_optimized(reg, result);
}

template<typename RegArray>
inline void decrement_x_fast(RegArray& reg) {
    const uint8_t result = (reg[CpuReg::X] - 1) & 0xFF;
    reg[CpuReg::X] = result;
    set_nz_flags_optimized(reg, result);
}

template<typename RegArray>
inline void increment_y_fast(RegArray& reg) {
    const uint8_t result = (reg[CpuReg::Y] + 1) & 0xFF;
    reg[CpuReg::Y] = result;
    set_nz_flags_optimized(reg, result);
}

template<typename RegArray>
inline void decrement_y_fast(RegArray& reg) {
    const uint8_t result = (reg[CpuReg::Y] - 1) & 0xFF;
    reg[CpuReg::Y] = result;
    set_nz_flags_optimized(reg, result);
}

// === COMPARISON OPTIMIZATIONS ===

// Fast comparison operations
template<typename RegArray>
inline void compare_a_fast(RegArray& reg, uint8_t value) {
    const uint16_t temp = reg[CpuReg::A] - value;
    const uint8_t result = temp & 0xFF;
    
    set_flag_branchless(reg, P_CARRY, temp < 0x100);
    set_nz_flags_optimized(reg, result);
}

template<typename RegArray>
inline void compare_x_fast(RegArray& reg, uint8_t value) {
    const uint16_t temp = reg[CpuReg::X] - value;
    const uint8_t result = temp & 0xFF;
    
    set_flag_branchless(reg, P_CARRY, temp < 0x100);
    set_nz_flags_optimized(reg, result);
}

template<typename RegArray>
inline void compare_y_fast(RegArray& reg, uint8_t value) {
    const uint16_t temp = reg[CpuReg::Y] - value;
    const uint8_t result = temp & 0xFF;
    
    set_flag_branchless(reg, P_CARRY, temp < 0x100);
    set_nz_flags_optimized(reg, result);
}

// === SHIFT/ROTATE OPTIMIZATIONS ===

// Fast shift operations with optimized flag setting
template<typename RegArray>
inline uint8_t arithmetic_shift_left_fast(RegArray& reg, uint8_t value) {
    const uint8_t result = (value << 1) & 0xFF;
    
    set_flag_branchless(reg, P_CARRY, value & 0x80);
    set_nz_flags_optimized(reg, result);
    
    return result;
}

template<typename RegArray>
inline uint8_t logical_shift_right_fast(RegArray& reg, uint8_t value) {
    const uint8_t result = value >> 1;
    
    set_flag_branchless(reg, P_CARRY, value & 0x01);
    set_nz_flags_optimized(reg, result);
    
    return result;
}

template<typename RegArray>
inline uint8_t rotate_left_fast(RegArray& reg, uint8_t value) {
    const bool carry_in = (reg[CpuReg::P] & P_CARRY) != 0;
    const uint8_t result = ((value << 1) | (carry_in ? 1 : 0)) & 0xFF;
    
    set_flag_branchless(reg, P_CARRY, value & 0x80);
    set_nz_flags_optimized(reg, result);
    
    return result;
}

template<typename RegArray>
inline uint8_t rotate_right_fast(RegArray& reg, uint8_t value) {
    const bool carry_in = (reg[CpuReg::P] & P_CARRY) != 0;
    const uint8_t result = (value >> 1) | (carry_in ? 0x80 : 0);
    
    set_flag_branchless(reg, P_CARRY, value & 0x01);
    set_nz_flags_optimized(reg, result);
    
    return result;
}

// === PERFORMANCE PROFILING HELPERS ===

// Runtime performance hints
#ifdef __GNUC__
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

// Hot/cold path annotations
#ifdef __GNUC__
    #define HOT_PATH __attribute__((hot))
    #define COLD_PATH __attribute__((cold))
#else
    #define HOT_PATH
    #define COLD_PATH
#endif

} // namespace fam65xx_cpp

#endif // PERFORMANCE_OPTIMIZATIONS_HPP