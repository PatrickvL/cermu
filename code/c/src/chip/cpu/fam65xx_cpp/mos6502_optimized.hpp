#ifndef MOS6502_OPTIMIZED_HPP
#define MOS6502_OPTIMIZED_HPP

#include <cstdint>
#include <array>
#include <cstddef>
#include "cpu_config.hpp"
#include "cpu_defs.hpp"
#include "unified_helpers.hpp"
#include "../../../core/system_lines.h"

namespace fam65xx_cpp {

/**
 * MOS6502 Optimized Emulator - Hardware Accurate
 *
 * Key Features:
 * - Compact cycle definitions using mutually exclusive circuit groups
 * - Template-driven design supports multiple CPU variants with constexpr optimization
 * - Extended register system with proper ABH/ABL/ADH/ADL internal registers
 * - Complete φ1/φ2 separation with pending_data_sample memory coordination
 * - Hardware-accurate timing and pin state management
 */

// Mutually Exclusive Circuit Groups (5 groups total - reduces storage dramatically)
enum class AluGroup : uint8_t {
    NONE = 0, ADC = 1, SBC = 2, AND = 3, ORA = 4, EOR = 5, CMP = 6, CPX = 7, CPY = 8,
    ASL = 9, LSR = 10, ROL = 11, ROR = 12, INC = 13, DEC = 14, BIT = 15
};

enum class AddressGroup : uint8_t {
    NONE = 0, PC = 1, SP = 2, ABH_ABL = 3, ZERO = 4, 
    IMM = 5, ZP = 6, ABS = 7, INDEXED_X = 8, INDEXED_Y = 9
};

enum class RegTargetGroup : uint8_t {
    NONE = 0, A = 1, X = 2, Y = 3, P = 4, SP = 5, 
    PCL = 6, PCH = 7, ABL = 8, ABH = 9, DL = 10, ADL = 11, ADH = 12
};

enum class BusDriverGroup : uint8_t {
    NONE = 0, READ = 1, WRITE = 2, STACK_PUSH = 3, STACK_PULL = 4, 
    VECTOR_READ = 5, MODIFY_WRITE = 6, PAGE_CROSS_FIX = 7
};

// Compact 16-bit cycle definition using mutually exclusive groups
union CompactCycleDef {
    uint16_t raw;
    struct {
        uint16_t alu_group      : 4;  // bits 0-3: ALU operation (0-15)
        uint16_t addr_group     : 4;  // bits 4-7: Address source (0-9) 
        uint16_t target_group   : 4;  // bits 8-11: Register target (0-12)
        uint16_t bus_group      : 3;  // bits 12-14: Bus driver (0-7)
        uint16_t last_cycle     : 1;  // bit 15: Last cycle flag
    };
    
    constexpr CompactCycleDef(uint16_t value = 0) : raw(value) {}
    
    constexpr CompactCycleDef(AluGroup alu, AddressGroup addr, RegTargetGroup target, 
                             BusDriverGroup bus, bool last = false) : raw(0) {
        alu_group = static_cast<uint16_t>(alu) & 0xF;
        addr_group = static_cast<uint16_t>(addr) & 0xF;
        target_group = static_cast<uint16_t>(target) & 0xF;
        bus_group = static_cast<uint16_t>(bus) & 0x7;
        last_cycle = last ? 1 : 0;
    }
    
    constexpr bool is_last_cycle() const { return last_cycle != 0; }
    constexpr AluGroup get_alu() const { return static_cast<AluGroup>(alu_group); }
    constexpr AddressGroup get_address() const { return static_cast<AddressGroup>(addr_group); }
    constexpr RegTargetGroup get_target() const { return static_cast<RegTargetGroup>(target_group); }
    constexpr BusDriverGroup get_bus() const { return static_cast<BusDriverGroup>(bus_group); }
};

// Extended register indices for hardware accuracy
enum class CpuReg8 : uint8_t {
    // Core 6502 registers
    A = 0, X = 1, Y = 2, P = 3, SP = 4,
    // Program counter (split)
    PCL = 5, PCH = 6,
    // Address bus registers (internal)
    ABL = 7, ABH = 8,
    // Address latch registers (internal)
    ADL = 9, ADH = 10,
    // Data latch
    DL = 11,
    // Bank registers (65816 only)
    DBR = 12, PBR = 13,
    // Count
    COUNT = 14
};

// 16-bit register indices (aligned to 16-bit boundaries)
enum class CpuReg16 : uint8_t {
    // Program counter (PCL=5, PCH=6 -> word index 2 = bytes 4,5)
    PC = static_cast<uint8_t>(CpuReg8::PCH) / 2,
    // Address bus (ABL=7, ABH=8 -> word index 3 = bytes 6,7)
    AB = static_cast<uint8_t>(CpuReg8::ABH) / 2,
    // Address latch (ADL=9, ADH=10 -> word index 4 = bytes 8,9)
    AD = static_cast<uint8_t>(CpuReg8::ADH) / 2,
    // Bank registers (DBR=12, PBR=13 -> word index 6 = bytes 12,13)
    BANK = static_cast<uint8_t>(CpuReg8::PBR) / 2,
    // Count
    COUNT = static_cast<uint8_t>(CpuReg8::COUNT) / 2
};

// Optimized register array with union for zero-overhead 8/16-bit access
template<std::size_t N8, std::size_t N16>
struct CpuRegArray {
    union {
        uint8_t bytes[N8];
        uint16_t words[N16];
    };
    
    // Zero-overhead 8-bit register access
    inline uint8_t& reg8(CpuReg8 reg) {
        return bytes[static_cast<std::size_t>(reg)];
    }
    inline const uint8_t& reg8(CpuReg8 reg) const {
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
using MOS6502RegArray = CpuRegArray<16, 8>;  // 16 bytes, 8 words (with padding for alignment)

template<typename Config>
class MOS6502Optimized {
private:
    // Optimized register array with union-based 8/16-bit access
    alignas(16) MOS6502RegArray registers;
    
    // CPU execution state
    uint8_t current_opcode = 0;
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
    
    // Compact cycle table: 256 opcodes × 8 cycles = 2048 entries × 16 bits = 4KB
    static const std::array<CompactCycleDef, 2048> cycle_table;
    
public:
    // Constructor
    MOS6502Optimized() {
        init_registers();
        current_opcode = 0xEA;  // Start with NOP
    }
    
    // Zero-overhead register access
    uint8_t get_register(CpuReg8 reg) const {
        return registers.reg8(reg);
    }
    
    void set_register(CpuReg8 reg, uint8_t value) {
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
        if (cycle.get_alu() != AluGroup::NONE) {
            execute_alu_operation(cycle.get_alu(), registers.reg8(CpuReg8::DL));
        }
        
        // Handle address calculation
        temp_address = calculate_address(cycle.get_address());
        
        return bus_state;
    }
    
    bus_state_t tick_phi2(bus_state_t bus_state) {
        // φ2: External operations (bus control, memory access)
        const CompactCycleDef& cycle = cycle_table[current_opcode * 8 + current_cycle];
        
        // Set address on bus - for PC operations, use current PC and increment
        if (cycle.get_address() == AddressGroup::PC) {
            bus_state = BUS_SET_ADDR(bus_state, get_pc());
            set_pc(get_pc() + 1);  // Increment PC after setting address
        } else if (temp_address != 0) {
            bus_state = BUS_SET_ADDR(bus_state, temp_address);
        }
        
        // Handle bus operations
        bus_state = execute_bus_operation(cycle.get_bus(), bus_state);
        
        // Sample data from bus
        uint8_t bus_data = BUS_GET_DATA(bus_state);
        
        // For new instruction (cycle 0), capture the opcode
        if (current_cycle == 0) {
            // This is the opcode fetch cycle - store the opcode for next instruction
            // Don't change current_opcode yet, wait until instruction completes
        }
        
        // Update target register if specified
        if (cycle.get_target() != RegTargetGroup::NONE) {
            write_target_register(cycle.get_target(), bus_data);
        } else {
            // Always update DL for next cycle if no specific target
            registers.reg8(CpuReg8::DL) = bus_data;
        }
        
        // Update internal state - advance to next cycle
        if (cycle.is_last_cycle()) {
            // Prepare for next instruction - fetch opcode for next instruction
            current_opcode = bus_data;  // New opcode from the bus
            current_cycle = 0;
            state_flags |= STATE_SYNC_NEXT;
        } else {
            current_cycle++;
        }
        
        // Handle data sampling for memory coordination
        if (data_sample_pending) {
            pending_data_sample = bus_data;
            data_sample_pending = false;
        }
        
        return bus_state;
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
                registers.reg8(CpuReg8::P) |= P_OVERFLOW;
            }
            prev_so_state = state;
        }
    }
    
    // API compatibility methods
    uint8_t get_a() const { return get_register(CpuReg8::A); }
    uint8_t get_x() const { return get_register(CpuReg8::X); }
    uint8_t get_y() const { return get_register(CpuReg8::Y); }
    uint8_t get_p() const { return get_register(CpuReg8::P); }
    uint8_t get_sp() const { return get_register(CpuReg8::SP); }
    
    void set_a(uint8_t val) { set_register(CpuReg8::A, val); }
    void set_x(uint8_t val) { set_register(CpuReg8::X, val); }
    void set_y(uint8_t val) { set_register(CpuReg8::Y, val); }
    void set_p(uint8_t val) { set_register(CpuReg8::P, val); }
    void set_sp(uint8_t val) { set_register(CpuReg8::SP, val); }
    
    // Debug interface
    uint8_t get_current_opcode() const { return current_opcode; }
    uint8_t get_current_cycle() const { return current_cycle; }
    uint32_t get_state_flags() const { return state_flags; }
    
private:
    void init_registers() {
        // Initialize registers to power-up state
        std::fill(registers.begin(), registers.end(), 0);
        registers.reg8(CpuReg8::SP) = 0xFF;
        registers.reg8(CpuReg8::P) = P_IRQ_DIS | P_UNUSED;
        set_pc(0);
        set_ab(0);
        set_ad(0);
    }
    
    // ALU operation implementation
    void execute_alu_operation(AluGroup alu_op, uint8_t data) {
        uint8_t& acc = registers.reg8(CpuReg8::A);
        uint8_t& flags = registers.reg8(CpuReg8::P);
        
        switch (alu_op) {
            case AluGroup::ADC: {
                const uint8_t carry = (flags & P_CARRY) ? 1 : 0;
                const uint16_t result = acc + data + carry;
                flags = (flags & ~(P_CARRY | P_OVERFLOW)) |
                        ((result > 0xFF) ? P_CARRY : 0) |
                        ((~(acc ^ data) & (acc ^ result) & 0x80) ? P_OVERFLOW : 0);
                acc = result & 0xFF;
                update_flags_for_result(acc);
                break;
            }
            case AluGroup::SBC: {
                const uint8_t borrow = (flags & P_CARRY) ? 0 : 1;
                const int16_t result = static_cast<int16_t>(acc) - static_cast<int16_t>(data) - borrow;
                flags = (flags & ~(P_CARRY | P_OVERFLOW)) |
                        ((result >= 0) ? P_CARRY : 0) |
                        (((acc ^ data) & (acc ^ result) & 0x80) ? P_OVERFLOW : 0);
                acc = result & 0xFF;
                update_flags_for_result(acc);
                break;
            }
            case AluGroup::AND:
                acc &= data;
                update_flags_for_result(acc);
                break;
            case AluGroup::ORA:
                acc |= data;
                update_flags_for_result(acc);
                break;
            case AluGroup::EOR:
                acc ^= data;
                update_flags_for_result(acc);
                break;
            case AluGroup::CMP: {
                const uint16_t result = acc - data;
                flags = (flags & ~P_CARRY) | ((result < 0x100) ? P_CARRY : 0);
                update_flags_for_result(result & 0xFF);
                break;
            }
            case AluGroup::ASL: {
                const uint8_t result = data << 1;
                flags = (flags & ~P_CARRY) | ((data & 0x80) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluGroup::LSR: {
                const uint8_t result = data >> 1;
                flags = (flags & ~P_CARRY) | ((data & 0x01) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluGroup::ROL: {
                const uint8_t carry_in = (flags & P_CARRY) ? 1 : 0;
                const uint8_t result = (data << 1) | carry_in;
                flags = (flags & ~P_CARRY) | ((data & 0x80) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluGroup::ROR: {
                const uint8_t carry_in = (flags & P_CARRY) ? 0x80 : 0;
                const uint8_t result = (data >> 1) | carry_in;
                flags = (flags & ~P_CARRY) | ((data & 0x01) ? P_CARRY : 0);
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluGroup::INC: {
                const uint8_t result = data + 1;
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluGroup::DEC: {
                const uint8_t result = data - 1;
                temp_data = result;
                update_flags_for_result(result);
                break;
            }
            case AluGroup::BIT: {
                const uint8_t result = acc & data;
                flags = (flags & ~(P_NEGATIVE | P_OVERFLOW | P_ZERO)) |
                        (data & P_NEGATIVE) | ((data & P_OVERFLOW) ? P_OVERFLOW : 0) |
                        ((result == 0) ? P_ZERO : 0);
                break;
            }
            case AluGroup::NONE:
            default:
                // No ALU operation
                break;
        }
    }
    
    // Address calculation
    uint16_t calculate_address(AddressGroup addr_group) const {
        switch (addr_group) {
            case AddressGroup::PC:
                return get_pc();
            case AddressGroup::SP:
                return 0x0100 | registers.reg8(CpuReg8::SP);
            case AddressGroup::ABH_ABL:
                return get_ab();
            case AddressGroup::ZERO:
                return 0;
            case AddressGroup::ZP:
                return registers.reg8(CpuReg8::DL);
            case AddressGroup::ABS:
                return get_ab();
            case AddressGroup::INDEXED_X:
                return (get_ab() + registers.reg8(CpuReg8::X)) & 0xFFFF;
            case AddressGroup::INDEXED_Y:
                return (get_ab() + registers.reg8(CpuReg8::Y)) & 0xFFFF;
            case AddressGroup::NONE:
            default:
                return 0;
        }
    }
    
    // Target register write
    void write_target_register(RegTargetGroup target, uint8_t data) {
        switch (target) {
            case RegTargetGroup::A:
                registers.reg8(CpuReg8::A) = data;
                update_flags_for_result(data);
                break;
            case RegTargetGroup::X:
                registers.reg8(CpuReg8::X) = data;
                update_flags_for_result(data);
                break;
            case RegTargetGroup::Y:
                registers.reg8(CpuReg8::Y) = data;
                update_flags_for_result(data);
                break;
            case RegTargetGroup::P:
                registers.reg8(CpuReg8::P) = data;
                break;
            case RegTargetGroup::SP:
                registers.reg8(CpuReg8::SP) = data;
                break;
            case RegTargetGroup::PCL:
                registers.reg8(CpuReg8::PCL) = data;
                break;
            case RegTargetGroup::PCH:
                registers.reg8(CpuReg8::PCH) = data;
                break;
            case RegTargetGroup::ABL:
                registers.reg8(CpuReg8::ABL) = data;
                break;
            case RegTargetGroup::ABH:
                registers.reg8(CpuReg8::ABH) = data;
                break;
            case RegTargetGroup::DL:
                registers.reg8(CpuReg8::DL) = data;
                break;
            case RegTargetGroup::ADL:
                registers.reg8(CpuReg8::ADL) = data;
                break;
            case RegTargetGroup::ADH:
                registers.reg8(CpuReg8::ADH) = data;
                break;
            case RegTargetGroup::NONE:
            default:
                // No register write
                break;
        }
    }
    
    // Bus operation execution
    bus_state_t execute_bus_operation(BusDriverGroup bus_op, bus_state_t bus_state) {
        switch (bus_op) {
            case BusDriverGroup::READ:
                bus_state |= BUS_BIT(BUS_RW_BIT); // Set R/W to read
                break;
            case BusDriverGroup::WRITE:
                bus_state &= ~BUS_BIT(BUS_RW_BIT); // Set R/W to write
                // For write operations, get data from the register specified by target
                // This will be handled by the cycle table logic
                break;
            case BusDriverGroup::STACK_PUSH:
                bus_state &= ~BUS_BIT(BUS_RW_BIT); // Set R/W to write
                // Data to push should be specified by the cycle definition
                registers.reg8(CpuReg8::SP)--; // Decrement SP after push
                break;
            case BusDriverGroup::STACK_PULL:
                bus_state |= BUS_BIT(BUS_RW_BIT); // Set R/W to read
                registers.reg8(CpuReg8::SP)++; // Increment SP before pull
                break;
            case BusDriverGroup::VECTOR_READ:
                bus_state |= BUS_BIT(BUS_RW_BIT); // Set R/W to read
                break;
            case BusDriverGroup::MODIFY_WRITE:
                bus_state &= ~BUS_BIT(BUS_RW_BIT); // Set R/W to write
                // Modified data should come from ALU operation result
                break;
            case BusDriverGroup::NONE:
            default:
                // No bus operation - this is critical for NOP and other internal operations
                break;
        }
        return bus_state;
    }
    
    // Helper function to update N and Z flags
    void update_flags_for_result(uint8_t result) {
        uint8_t& flags = registers.reg8(CpuReg8::P);
        flags = (flags & ~(P_NEGATIVE | P_ZERO)) |
                ((result & 0x80) ? P_NEGATIVE : 0) |
                ((result == 0) ? P_ZERO : 0);
    }
};

// Forward declaration for cycle table generator
template<typename Config>
constexpr std::array<CompactCycleDef, 2048> generate_complete_cycle_table();

// Cycle table definition - implemented in mos6502_cycle_table.hpp
template<typename Config>
const std::array<CompactCycleDef, 2048> MOS6502Optimized<Config>::cycle_table = generate_complete_cycle_table<Config>();

} // namespace fam65xx_cpp

#endif // MOS6502_OPTIMIZED_HPP