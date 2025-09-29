#ifndef MOS6502_OPTIMIZED_HPP
#define MOS6502_OPTIMIZED_HPP

#include <cstdint>
#include <array>
#include <cstddef>
#include "cpu_config.hpp"
#include "../../../core/system_lines.h"

// Processor status flags - copied from cpu_defs.hpp to avoid enum conflicts
constexpr uint8_t P_CARRY     = 0x01;  // Carry flag
constexpr uint8_t P_ZERO      = 0x02;  // Zero flag
constexpr uint8_t P_IRQ_DIS   = 0x04;  // Interrupt disable flag
constexpr uint8_t P_DECIMAL   = 0x08;  // Decimal mode flag
constexpr uint8_t P_BREAK     = 0x10;  // Break flag
constexpr uint8_t P_UNUSED    = 0x20;  // Unused flag (always set)
constexpr uint8_t P_OVERFLOW  = 0x40;  // Overflow flag
constexpr uint8_t P_NEGATIVE  = 0x80;  // Negative flag

// Internal state flags
constexpr uint32_t STATE_NMI_PENDING    = 0x00000001;
constexpr uint32_t STATE_IRQ_PENDING    = 0x00000002;
constexpr uint32_t STATE_RESET_PENDING  = 0x00000004;
constexpr uint32_t STATE_NMI_EDGE       = 0x00000008;
constexpr uint32_t STATE_SO_EDGE        = 0x00000010;
constexpr uint32_t STATE_SYNC_NEXT      = 0x00000020;

namespace fam65xx_cpp {

/**
 * MOS6502 Optimized Emulator - Hardware Accurate
 *
 * Key Features:
 * - Compact cycle definitions using mutually exclusive circuit operations
 * - Template-driven design supports multiple CPU variants with constexpr optimization
 * - Extended register system with proper ABH/ABL/ADH/ADL internal registers
 * - Complete φ1/φ2 separation with pending_data_sample memory coordination
 * - Hardware-accurate timing and pin state management
 */

// ALU operations (mutually exclusive - reduces storage dramatically)
enum class AluOp : uint8_t {
    NONE = 0, ADC = 1, SBC = 2, AND = 3, ORA = 4, EOR = 5, CMP = 6, CPX = 7, CPY = 8,
    ASL = 9, LSR = 10, ROL = 11, ROR = 12, INC = 13, DEC = 14, BIT = 15
};

// Address modes (mutually exclusive addressing schemes)
enum class AddressMode : uint8_t {
    NONE = 0, PC = 1, SP = 2, ABH_ABL = 3, ZERO = 4,
    IMM = 5, ZP = 6, ABS = 7, INDEXED_X = 8, INDEXED_Y = 9,
    // Unified vector address - actual vector determined by opcode
    VECTOR = 10          // Vector addressing (IRQ/NMI/RESET determined by opcode)
};

// Register indices (8-bit registers with 16-bit pairs aligned for endianness)
enum class CpuReg : uint8_t {
    // Core 6502 registers
    A = 0, X = 1, Y = 2, P = 3, SP = 4,
    // Data latch
    DL = 5,
    // 16-bit register pairs - aligned for host-native access after DL
    // Program counter (Low/High byte order for endianness)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    PCL = 6, PCH = 7,           // Little endian: Low byte at lower address
#else
    PCH = 6, PCL = 7,           // Big endian: High byte at lower address
#endif
    // Address bus registers (Low/High byte order for endianness)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ABL = 8, ABH = 9,           // Little endian: Low byte at lower address
#else
    ABH = 8, ABL = 9,           // Big endian: High byte at lower address
#endif
    // Address latch registers (Low/High byte order for endianness)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ADL = 10, ADH = 11,         // Little endian: Low byte at lower address
#else
    ADH = 10, ADL = 11,         // Big endian: High byte at lower address
#endif
    // Bank registers (65816 only, Low/High byte order for endianness)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    DBR = 12, PBR = 13,         // Little endian: Low byte at lower address
#else
    PBR = 12, DBR = 13,         // Big endian: High byte at lower address
#endif
    // Reserved for future expansion
    RESERVED = 14,
    // Special cases (must be ≤15 for 4-bit CompactCycleDef constraint)
    NONE = 15,
    // Total registers
    COUNT = 16
};

// Bus control operations (mutually exclusive bus operations)
enum class BusControl : uint8_t {
    NONE = 0, READ = 1, WRITE = 2, STACK_PUSH = 3, STACK_PULL = 4,
    VECTOR_READ = 5, MODIFY_WRITE = 6, PAGE_CROSS_FIX = 7
};

// Compact 16-bit cycle definition using mutually exclusive operations
union CompactCycleDef {
    uint16_t raw;
    struct {
        uint16_t alu_op         : 4;  // bits 0-3: ALU operation (0-15)
        uint16_t addr_mode      : 4;  // bits 4-7: Address mode (0-10)
        uint16_t target_reg     : 4;  // bits 8-11: Register target (0-15)
        uint16_t bus_ctrl       : 3;  // bits 12-14: Bus control (0-7)
        uint16_t last_cycle     : 1;  // bit 15: Last cycle flag
    };
    
    constexpr CompactCycleDef(uint16_t value = 0) : raw(value) {}
    
    constexpr CompactCycleDef(AluOp alu, AddressMode addr, CpuReg target,
                             BusControl bus, bool last = false) : raw(0) {
        alu_op = static_cast<uint16_t>(alu) & 0xF;
        addr_mode = static_cast<uint16_t>(addr) & 0xF;
        target_reg = static_cast<uint16_t>(target) & 0xF;
        bus_ctrl = static_cast<uint16_t>(bus) & 0x7;
        last_cycle = last ? 1 : 0;
    }
    
    constexpr bool is_last_cycle() const { return last_cycle != 0; }
    constexpr AluOp get_alu() const { return static_cast<AluOp>(alu_op); }
    constexpr AddressMode get_address() const { return static_cast<AddressMode>(addr_mode); }
    constexpr CpuReg get_target() const { return static_cast<CpuReg>(target_reg); }
    constexpr BusControl get_bus() const { return static_cast<BusControl>(bus_ctrl); }
};

// 16-bit register indices (aligned to 16-bit boundaries with endianness support)
enum class CpuReg16 : uint8_t {
    // Program counter
    PC = static_cast<uint8_t>(CpuReg::PCH) / 2,
    // Address bus
    AB = static_cast<uint8_t>(CpuReg::ABH) / 2,
    // Address latch
    AD = static_cast<uint8_t>(CpuReg::ADH) / 2,
    // Bank registers
    BANK = static_cast<uint8_t>(CpuReg::PBR) / 2,
    // Count
    COUNT = static_cast<uint8_t>(CpuReg::COUNT) / 2,
};

// Optimized register array with union for zero-overhead 8/16-bit access
template<std::size_t N8>
struct CpuRegArray {
    union {
        uint8_t bytes[N8];
        uint16_t words[N8 / 2];
    };
    
    // Zero-overhead 8-bit register access
    inline uint8_t& reg8(CpuReg reg) {
        return bytes[static_cast<std::size_t>(reg)];
    }
    inline const uint8_t& reg8(CpuReg reg) const {
        return bytes[static_cast<std::size_t>(reg)];
    }
    
    // Zero-overhead 16-bit register access
    inline uint16_t& reg16(CpuReg16 reg) {
        return words[static_cast<std::size_t>(reg)];
    }
    inline const uint16_t& reg16(CpuReg16 reg) const {
        return words[static_cast<std::size_t>(reg)];
    }
    
    // Initialize all registers to zero
    CpuRegArray() {
        std::fill(bytes, bytes + N8, 0);
    }
    
    // Iterator support for range-based loops
    uint8_t* begin() { return bytes; }
    const uint8_t* begin() const { return bytes; }
    uint8_t* end() { return bytes + N8; }
    const uint8_t* end() const { return bytes + N8; }
};

// Type alias for the MOS6502 register array
using MOS6502RegArray = CpuRegArray<static_cast<int>(CpuReg::COUNT)>; // 16 bytes, 8 words (proper alignment for 16-bit access)

template<typename Config>
class MOS6502Optimized {
private:
    // Optimized register array with union-based 8/16-bit access
    alignas(16) MOS6502RegArray registers;
    
    // CPU execution state
    uint16_t current_opcode = 0;  // Support pseudo opcodes 256+ for IRQ/NMI/RESET
    uint8_t current_cycle = 0;
    uint32_t state_flags = 0;
    
    // Memory coordination
    uint8_t pending_data_sample = 0;
    bool data_sample_pending = false;
    
    // Hardware pin state (variant-specific)
    bool prev_nmi_state = true;
    bool prev_irq_state = true;
    bool prev_so_state = true;
    
    // Internal coordination variables
    uint8_t temp_data = 0;
    uint16_t temp_address = 0;
    bool branch_taken = false;
    bool page_crossed = false;
    
    // Compact cycle table: 256 opcodes × 8 cycles + interrupt sequences = 2072 entries × 16 bits = 4.1KB
    static const std::array<CompactCycleDef, 2072> cycle_table;
    
public:
    // Constructor
    MOS6502Optimized() {
        init_registers();
        current_opcode = 0x00;  // Will be set by first opcode fetch
        current_cycle = 0;      // Start ready to fetch first opcode
    }
    
    // Zero-overhead register access
    uint8_t get_register(CpuReg reg) const {
        return registers.reg8(reg);
    }
    
    void set_register(CpuReg reg, uint8_t value) {
        registers.reg8(reg) = value;
    }
    
    // Zero-overhead 16-bit register access
    uint16_t get_pc() const {
        return registers.reg16(CpuReg16::PC);
    }
    
    void set_pc(uint16_t pc) {
        registers.reg16(CpuReg16::PC) = pc;
    }
    
    uint16_t get_ab() const {
        return registers.reg16(CpuReg16::AB);
    }
    
    void set_ab(uint16_t ab) {
        registers.reg16(CpuReg16::AB) = ab;
    }
    
    uint16_t get_ad() const {
        return registers.reg16(CpuReg16::AD);
    }
    
    void set_ad(uint16_t ad) {
        registers.reg16(CpuReg16::AD) = ad;
    }
    
    // Main execution methods with φ1/φ2 separation
    bus_state_t tick_phi1(bus_state_t bus_state) {
        // φ1: Internal operations (decode, ALU, register updates)
        const CompactCycleDef& cycle = cycle_table[current_opcode * 8 + current_cycle];
        
        // Execute ALU operation if specified
        if (cycle.get_alu() != AluOp::NONE) {
            execute_alu_operation(cycle.get_alu(), registers.reg8(CpuReg::DL));
        }
        
        // Handle address calculation
        temp_address = calculate_address(cycle.get_address());
        
        return bus_state;
    }
    
    bus_state_t tick_phi2(bus_state_t bus_state) {
        // φ2: External operations (bus control, memory access)
        const CompactCycleDef& cycle = cycle_table[current_opcode * 8 + current_cycle];
        
        // Set address on bus with vector address adjustment
        if (cycle.get_address() != AddressMode::NONE) {
            uint16_t final_address = temp_address;
            
            // Handle vector address adjustment based on opcode and target register
            if (cycle.get_address() == AddressMode::VECTOR) {
                uint16_t vector_base = get_vector_address_for_opcode(current_opcode);
                if (cycle.get_target() == CpuReg::ABH) {
                    final_address = vector_base + 1; // High byte of vector
                } else {
                    final_address = vector_base; // Low byte of vector
                }
            }
            
            bus_state = BUS_SET_ADDR(bus_state, final_address);
        }
        
        // Handle bus operations
        bus_state = execute_bus_operation(cycle.get_bus(), bus_state);
        
        // NOTE: Data sampling happens externally after phi2 completes
        // The test harness will provide read data via external interface
        
        // Update internal state - advance to next cycle or complete instruction
        if (cycle.is_last_cycle()) {
            // For vector-reading instructions (BRK, IRQ, etc.), don't fetch next opcode yet
            // The vector jump will be handled in sample_bus_data when the vector read completes
            if (cycle.get_address() == AddressMode::VECTOR &&
                cycle.get_target() == CpuReg::ABH) {
                // This is the final vector read - PC will be set by sample_bus_data
                current_cycle = 0;
                // Vector reads should NOT set up opcode fetch - wait for sample_bus_data to complete jump
                return bus_state;
            } else {
                // Normal instruction completion - fetch next opcode
                bus_state = BUS_SET_ADDR(bus_state, get_pc());
                bus_state = execute_bus_operation(BusControl::READ, bus_state);
                
                // PC increment for opcode fetch happens here
                set_pc(get_pc() + 1);
                current_cycle = 0;
            }
        } else {
            current_cycle++;
        }
        
        return bus_state;
    }
    
    // External interface for memory coordination - called by test harness after phi2
    void sample_bus_data(uint8_t data) {
        // Get the cycle definition for the cycle that just completed
        // If current_cycle is 0, we just completed the last cycle (7 for BRK), so use cycle 6 (0-indexed)
        uint8_t cycle_index = (current_cycle == 0) ? 7 : current_cycle;  // Cycle that just executed
        const CompactCycleDef& cycle = cycle_table[current_opcode * 8 + cycle_index - 1];  // Convert to 0-indexed
        
        // Cycle-driven vector handling - unified for all vector types
        if (cycle.get_address() == AddressMode::VECTOR) {
            // Handle vector reads by determining if this is low or high byte based on target
            if (cycle.get_target() == CpuReg::ABL) {
                // Reading vector low byte - store in ABL (Address Bus Low) like real hardware
                registers.reg8(CpuReg::ABL) = data;  // Store vector low using endianness-aware access
                // Vector read complete for low byte - don't do normal register handling
                return;
            } else if (cycle.get_target() == CpuReg::ABH) {
                // Reading vector high byte and complete vector jump
                registers.reg8(CpuReg::ABH) = data;  // Store vector high using endianness-aware access
                // Jump to interrupt vector using properly aligned AB register
                set_pc(get_ab());  // Jump to interrupt vector (now properly aligned with endianness)
                
                // Set processor flags based on interrupt type
                if (current_opcode == 0x00) {
                    // BRK instruction: set both B and I flags in the processor status
                    registers.reg8(CpuReg::P) |= P_BREAK | P_IRQ_DIS;
                    // Clear V flag - BRK should not affect overflow flag
                    registers.reg8(CpuReg::P) &= ~P_OVERFLOW;
                } else if (current_opcode >= 256) {
                    // Hardware interrupts (pseudo opcodes 256+): only set I flag for IRQ
                    // NMI and RESET don't affect I flag
                    if (current_opcode == 256) { // IRQ pseudo-opcode
                        registers.reg8(CpuReg::P) |= P_IRQ_DIS;
                    }
                    // For NMI (257) and RESET (258), no flag changes needed
                }
                
                // Vector jump is now complete - ready to execute from new PC
                // Reset cycle to 0 to indicate instruction completion
                current_cycle = 0;
                
                // Vector jump is now complete - return without normal target register handling
                return;
            }
        }
        
        // Update target register if specified (normal non-vector operations)
        if (cycle.get_target() != CpuReg::NONE) {
            // For non-read operations, use data from DL (previous cycle's read)
            uint8_t target_data = (cycle.get_bus() == BusControl::READ) ? data : registers.reg8(CpuReg::DL);
            write_target_register(cycle.get_target(), target_data);
        } else if (cycle.get_bus() == BusControl::READ) {
            // Always update DL for read operations if no specific target
            registers.reg8(CpuReg::DL) = data;
        }
    }
    
    // External interface to complete opcode fetch
    void complete_opcode_fetch(uint16_t opcode) {
        current_opcode = opcode;
    }
    
    // Memory coordination interface
    void set_pending_data_sample(uint8_t data) { 
        pending_data_sample = data; 
        data_sample_pending = true;
    }
    
    uint8_t get_pending_data_sample() const { 
        return data_sample_pending ? pending_data_sample : 0; 
    }
    
    void clear_pending_data_sample() { 
        data_sample_pending = false; 
    }
    
    // Hardware pin interface
    void set_irq_pin(bool state) {
        if (prev_irq_state != state) {
            prev_irq_state = state;
            if (!state) { // IRQ is active low
                state_flags |= STATE_IRQ_PENDING;
            } else {
                state_flags &= ~STATE_IRQ_PENDING;
            }
        }
    }
    
    void set_nmi_pin(bool state) {
        if (prev_nmi_state && !state) { // Falling edge detection
            state_flags |= STATE_NMI_PENDING | STATE_NMI_EDGE;
        }
        prev_nmi_state = state;
    }
    
    void set_reset_pin(bool state) {
        if (!state) { // RESET is active low
            state_flags |= STATE_RESET_PENDING;
        }
    }
    
    // SO pin for NMOS variants only
    void set_so_pin(bool state) {
        if constexpr (Config::has_so_pin) {
            if (prev_so_state && !state) { // Falling edge detection
                state_flags |= STATE_SO_EDGE;
                registers.reg8(CpuReg::P) |= P_OVERFLOW;
            }
            prev_so_state = state;
        }
    }
    
    // API compatibility methods
    uint8_t get_a() const { return get_register(CpuReg::A); }
    uint8_t get_x() const { return get_register(CpuReg::X); }
    uint8_t get_y() const { return get_register(CpuReg::Y); }
    uint8_t get_p() const { return get_register(CpuReg::P); }
    uint8_t get_sp() const { return get_register(CpuReg::SP); }
    
    void set_a(uint8_t val) { set_register(CpuReg::A, val); }
    void set_x(uint8_t val) { set_register(CpuReg::X, val); }
    void set_y(uint8_t val) { set_register(CpuReg::Y, val); }
    void set_p(uint8_t val) { set_register(CpuReg::P, val); }
    void set_sp(uint8_t val) { set_register(CpuReg::SP, val); }
    
    // Debug interface
    uint16_t get_current_opcode() const { return current_opcode; }
    uint8_t get_current_cycle() const { return current_cycle; }
    uint32_t get_state_flags() const { return state_flags; }
    
private:
    // Helper function to determine vector address based on opcode
    uint16_t get_vector_address_for_opcode(uint16_t opcode) const {
        switch (opcode) {
            case 0x00:      // BRK instruction
                return 0xFFFE;  // IRQ/BRK vector
            case 256:       // IRQ pseudo-opcode
                return 0xFFFE;  // IRQ vector
            case 257:       // NMI pseudo-opcode
                return 0xFFFA;  // NMI vector
            case 258:       // RESET pseudo-opcode
                return 0xFFFC;  // RESET vector
            default:
                return 0xFFFE;  // Default to IRQ vector
        }
    }
    
    void init_registers() {
        // Initialize registers to power-up state
        std::fill(registers.begin(), registers.end(), 0);
        registers.reg8(CpuReg::SP) = 0xFF;
        registers.reg8(CpuReg::P) = P_IRQ_DIS | P_UNUSED;
        set_pc(0);
        set_ab(0);
        set_ad(0);
    }
    
    // ALU operation implementation
    void execute_alu_operation(AluOp alu_op, uint8_t data) {
        uint8_t& acc = registers.reg8(CpuReg::A);
        uint8_t& flags = registers.reg8(CpuReg::P);
        
        switch (alu_op) {
            case AluOp::ADC: {
                const uint8_t carry = (flags & P_CARRY) ? 1 : 0;
                const uint16_t result = acc + data + carry;
                flags = (flags & ~(P_CARRY | P_OVERFLOW)) |
                        ((result > 0xFF) ? P_CARRY : 0) |
                        ((~(acc ^ data) & (acc ^ result) & 0x80) ? P_OVERFLOW : 0);
                acc = result & 0xFF;
                update_flags_for_result(acc);
                break;
            }
            case AluOp::SBC: {
                const uint8_t borrow = (flags & P_CARRY) ? 0 : 1;
                const int16_t result = static_cast<int16_t>(acc) - static_cast<int16_t>(data) - borrow;
                flags = (flags & ~(P_CARRY | P_OVERFLOW)) |
                        ((result >= 0) ? P_CARRY : 0) |
                        (((acc ^ data) & (acc ^ result) & 0x80) ? P_OVERFLOW : 0);
                acc = result & 0xFF;
                update_flags_for_result(acc);
                break;
            }
            case AluOp::AND:
                acc &= data;
                update_flags_for_result(acc);
                break;
            case AluOp::ORA:
                acc |= data;
                update_flags_for_result(acc);
                break;
            case AluOp::EOR:
                acc ^= data;
                update_flags_for_result(acc);
                break;
            case AluOp::CMP: {
                const uint16_t result = acc - data;
                flags = (flags & ~P_CARRY) | ((result < 0x100) ? P_CARRY : 0);
                update_flags_for_result(result & 0xFF);
                break;
            }
            case AluOp::ASL: {
                const uint8_t result = data << 1;
                flags = (flags & ~P_CARRY) | ((data & 0x80) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluOp::LSR: {
                const uint8_t result = data >> 1;
                flags = (flags & ~P_CARRY) | ((data & 0x01) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluOp::ROL: {
                const uint8_t carry_in = (flags & P_CARRY) ? 1 : 0;
                const uint8_t result = (data << 1) | carry_in;
                flags = (flags & ~P_CARRY) | ((data & 0x80) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluOp::ROR: {
                const uint8_t carry_in = (flags & P_CARRY) ? 0x80 : 0;
                const uint8_t result = (data >> 1) | carry_in;
                flags = (flags & ~P_CARRY) | ((data & 0x01) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluOp::INC: {
                const uint8_t result = data + 1;
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluOp::DEC: {
                const uint8_t result = data - 1;
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluOp::BIT: {
                const uint8_t result = acc & data;
                flags = (flags & ~(P_NEGATIVE | P_OVERFLOW | P_ZERO)) |
                        (data & P_NEGATIVE) | ((data & P_OVERFLOW) ? P_OVERFLOW : 0) |
                        ((result == 0) ? P_ZERO : 0);
                break;
            }
            case AluOp::NONE:
            default:
                // No ALU operation
                break;
        }
    }
    
    // Address calculation
    uint16_t calculate_address(AddressMode addr_group) {
        switch (addr_group) {
            case AddressMode::PC: {
                uint16_t addr = get_pc();
                set_pc(get_pc() + 1);  // Increment PC for fetch operations
                return addr;
            }
            case AddressMode::SP:
                return 0x0100 | registers.reg8(CpuReg::SP);
            case AddressMode::ABH_ABL:
                return get_ab();  // Now properly aligned for 16-bit access
            case AddressMode::ZERO:
                return 0;
            case AddressMode::ZP:
                return registers.reg8(CpuReg::DL);
            case AddressMode::ABS:
                return get_ab();  // Now properly aligned for 16-bit access
            case AddressMode::INDEXED_X:
                return (get_ab() + registers.reg8(CpuReg::X)) & 0xFFFF;
            case AddressMode::INDEXED_Y:
                return (get_ab() + registers.reg8(CpuReg::Y)) & 0xFFFF;
            case AddressMode::VECTOR:
                // Vector address determined by opcode - base address returned here
                return get_vector_address_for_opcode(current_opcode);
            case AddressMode::NONE:
            default:
                return 0;
        }
    }
    
    // Target register write
    void write_target_register(CpuReg target, uint8_t data) {
        if (target == CpuReg::NONE) return;
        registers.reg8(target) = data;
        switch (target) {
            case CpuReg::A:
            case CpuReg::X:
            case CpuReg::Y:
                update_flags_for_result(data);
                break;
        }
    }
    
    // Bus operation execution
    bus_state_t execute_bus_operation(BusControl bus_op, bus_state_t bus_state) {
        const CompactCycleDef& cycle = cycle_table[current_opcode * 8 + current_cycle];
        
        switch (bus_op) {
            case BusControl::READ:
                bus_state |= BUS_BIT(BUS_RW_BIT); // Set R/W to read
                break;
            case BusControl::WRITE:
                bus_state &= ~BUS_BIT(BUS_RW_BIT); // Set R/W to write
                // For write operations, get data from the target register
                if (cycle.get_target() != CpuReg::NONE) {
                    uint8_t write_data = get_write_data_for_target(cycle.get_target());
                    bus_state = BUS_SET_DATA(bus_state, write_data);
                }
                break;
            case BusControl::STACK_PUSH:
                bus_state &= ~BUS_BIT(BUS_RW_BIT); // Set R/W to write
                // For stack push, get data from the target register
                if (cycle.get_target() != CpuReg::NONE) {
                    uint8_t push_data = get_write_data_for_target(cycle.get_target());
                    
                    // Set B flag when pushing P register during interrupt sequence
                    if (cycle.get_target() == CpuReg::P) {
                        push_data |= P_BREAK; // Set B flag for interrupt stack push
                    }
                    
                    bus_state = BUS_SET_DATA(bus_state, push_data);
                }
                registers.reg8(CpuReg::SP)--; // Decrement SP after push
                break;
            case BusControl::STACK_PULL:
                bus_state |= BUS_BIT(BUS_RW_BIT); // Set R/W to read
                registers.reg8(CpuReg::SP)++; // Increment SP before pull
                break;
            case BusControl::VECTOR_READ:
                bus_state |= BUS_BIT(BUS_RW_BIT); // Set R/W to read
                break;
            case BusControl::MODIFY_WRITE:
                bus_state &= ~BUS_BIT(BUS_RW_BIT); // Set R/W to write
                // Modified data should come from ALU operation result
                if (temp_data != 0) {
                    bus_state = BUS_SET_DATA(bus_state, temp_data);
                }
                break;
            case BusControl::NONE:
            default:
                // No bus operation - this is critical for NOP and other internal operations
                break;
        }
        return bus_state;
    }
    
    // Helper function to get write data for a target register
    uint8_t get_write_data_for_target(CpuReg target) {
        if (target == CpuReg::NONE) return 0;

        uint8_t value = registers.reg8(target);
        switch (target) {
            case CpuReg::P: {
                // For stack push during BRK, set B flag and ensure U flag is set
                if (current_opcode == 0x00) { // BRK instruction
                    value |= P_BREAK | P_UNUSED;  // Set B and U flags
                    // Clear V flag - BRK should not affect overflow flag
                    value &= ~P_OVERFLOW;
                }
            }
            case CpuReg::PCL: {
                // For BRK, push PC+2 (the instruction after BRK's 2-byte pattern)
                uint16_t push_pc = get_pc();
                if (current_opcode == 0x00) {
                    // BRK pushes PC+2 (after the BRK instruction and padding byte)
                    push_pc -= 1;  // We already incremented PC twice, so step back once to get PC+1
                }
                return push_pc & 0xFF;
            }
            case CpuReg::PCH: {
                // For BRK, push PC+2 (the instruction after BRK's 2-byte pattern)
                uint16_t push_pc = get_pc();
                if (current_opcode == 0x00) {
                    // BRK pushes PC+2 (after the BRK instruction and padding byte)
                    push_pc -= 1;  // We already incremented PC twice, so step back once to get PC+1
                }
                return (push_pc >> 8) & 0xFF;
            }
        }
        return value;
    }
    
    // Helper function to update N and Z flags
    void update_flags_for_result(uint8_t result) {
        uint8_t& flags = registers.reg8(CpuReg::P);
        flags = (flags & ~(P_NEGATIVE | P_ZERO)) |
                ((result & 0x80) ? P_NEGATIVE : 0) |
                ((result == 0) ? P_ZERO : 0);
    }
};

// Forward declaration for cycle table generator
template<typename Config>
constexpr std::array<CompactCycleDef, 2072> generate_complete_cycle_table();

// Cycle table definition - implemented in mos6502_cycle_table.hpp
template<typename Config>
const std::array<CompactCycleDef, 2072> MOS6502Optimized<Config>::cycle_table = generate_complete_cycle_table<Config>();

} // namespace fam65xx_cpp

#endif // MOS6502_OPTIMIZED_HPP