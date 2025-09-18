#ifndef MEMORY_OPERATIONS_HPP
#define MEMORY_OPERATIONS_HPP

#include "cpu_defs.hpp"
#include "../../../core/system_lines.h"

// Direct bus operations - no legacy compatibility macros
// Use native system_lines.h macros directly for maximum performance

namespace fam65xx_cpp {

template<typename BusConfig>
class MemoryOperations {
public:
    template<typename RegArray>
    static inline bus_state_t execute_memory_operation(bus_state_t bus_state, RegArray& reg,
                                                      MemOp mem_op) {
        if (mem_op == MemOp::NOP) return bus_state;
        
        uint16_t addr = 0;
        uint16_t pc = 0;
        
        // Execute memory operation - optimized with fallthrough for shared functionality
        switch (mem_op) {
            case MemOp::NOP:
                return bus_state;
            
            // PC operations
            case MemOp::READ_PC_INC:
                pc = (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL];
                BUS_SET_ADDR(bus_state, pc);
                pc = (pc + 1) & 0xFFFF;
                reg[CpuReg::PCL] = pc & 0xFF;
                reg[CpuReg::PCH] = (pc >> 8) & 0xFF;
                break;
                
            case MemOp::READ_PC:
                pc = (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL];
                BUS_SET_ADDR(bus_state, pc);
                // Update ABL/ABH registers so get_address() returns correct values
                reg[CpuReg::ABL] = pc & 0xFF;
                reg[CpuReg::ABH] = (pc >> 8) & 0xFF;
                break;
                
            // Absolute addressing - shared address calculation with fallthrough
            case MemOp::WRITE_ABS:
                bus_state &= ~BUS_RW_BIT; // Write (RW=0)
                [[fallthrough]];
            case MemOp::READ_ABS:
            // Note: READ_VECTOR is aliased to READ_ABS, so it's handled by this same case
                addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
                BUS_SET_ADDR(bus_state, addr);
                break;
                
            // Zero page addressing - shared address calculation with fallthrough
            case MemOp::WRITE_ZP:
                bus_state &= ~BUS_RW_BIT; // Write (RW=0)
                [[fallthrough]];
            case MemOp::READ_ZP:
                BUS_SET_ADDR(bus_state, reg[CpuReg::ABL]);
                break;
                
            // Zero page X indexed - shared calculation with fallthrough
            case MemOp::WRITE_ZPX:
                bus_state &= ~BUS_RW_BIT; // Write (RW=0)
                [[fallthrough]];
            case MemOp::READ_ZPX:
                addr = (reg[CpuReg::ABL] + reg[CpuReg::X]) & 0xFF;
                BUS_SET_ADDR(bus_state, addr);
                break;
                
            // Zero page Y indexed - shared calculation with fallthrough
            case MemOp::WRITE_ZPY:
                bus_state &= ~BUS_RW_BIT; // Write (RW=0)
                [[fallthrough]];
            case MemOp::READ_ZPY:
                addr = (reg[CpuReg::ABL] + reg[CpuReg::Y]) & 0xFF;
                BUS_SET_ADDR(bus_state, addr);
                break;
                
            // Stack operations
            case MemOp::READ_SP:
                BUS_SET_ADDR(bus_state, 0x0100 | reg[CpuReg::S]);
                break;
                
            case MemOp::WRITE_SP_DEC:
                {
                    const uint16_t stack_addr = 0x0100 | reg[CpuReg::S];
                    BUS_SET_ADDR(bus_state, stack_addr);
                    // Update ABL/ABH registers so get_address() returns correct values
                    reg[CpuReg::ABL] = stack_addr & 0xFF;
                    reg[CpuReg::ABH] = (stack_addr >> 8) & 0xFF;
                    reg[CpuReg::S] = (reg[CpuReg::S] - 1) & 0xFF;
                    bus_state &= ~BUS_RW_BIT; // Write (RW=0)
                }
                break;
                
            case MemOp::READ_SP_INC:
                reg[CpuReg::S] = (reg[CpuReg::S] + 1) & 0xFF;
                BUS_SET_ADDR(bus_state, 0x0100 | reg[CpuReg::S]);
                break;
                
            default:
                // Unknown operation - default to PC read
                pc = (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL];
                BUS_SET_ADDR(bus_state, pc);
                break;
        }
        return bus_state;
    }
    
    template<typename CpuInstance>
    static inline bus_state_t handle_write_data(bus_state_t bus_state, CpuInstance& cpu,
                                               MemOp mem_op, DataOp data_op, uint8_t pending_data = 0) {
        // Handle data output for write operations
        constexpr uint16_t WRITE_OPS = (1 << static_cast<uint8_t>(MemOp::WRITE_ABS)) |
                                       (1 << static_cast<uint8_t>(MemOp::WRITE_ZP)) |
                                       (1 << static_cast<uint8_t>(MemOp::WRITE_ZPX)) |
                                       (1 << static_cast<uint8_t>(MemOp::WRITE_ZPY)) |
                                       (1 << static_cast<uint8_t>(MemOp::WRITE_SP_DEC));
        
        if (WRITE_OPS & (1 << static_cast<uint8_t>(mem_op))) {
            // Handle store operations first - DEDUPLICATION: Use CPU helper function
            switch (data_op) {
                case DataOp::STORE_A:
                case DataOp::STORE_X:
                case DataOp::STORE_Y:
                case DataOp::STORE_ZERO:
                    {
                        // DEDUPLICATION: Actually call the CPU's helper function instead of duplicating logic
                        uint8_t value = cpu.get_store_register_value(data_op);
                        BUS_SET_DATA(bus_state, value);
                    }
                    break;
                case DataOp::ALU:
                    // ALU result will be set by caller using pending_data
                    BUS_SET_DATA(bus_state, pending_data);
                    break;
                case DataOp::TEMP_MODIFY:
                    // MEMORY SHIFT/ROTATE FIX: For memory modify operations (shift/rotate $nn),
                    // the ALU result is stored in DL register and must be written to memory
                    // This handles the final write cycle of read-modify-write operations
                    BUS_SET_DATA(bus_state, cpu.get_reg(CpuReg::DL));
                    break;
                case DataOp::TEMP_STORE:
                    // MEMORY MODIFY FIX: For memory modify operations cycle 3,
                    // write the original value stored in pending_data back to memory
                    // This is the "write old value back" cycle in the 6502 read-modify-write sequence
                    BUS_SET_DATA(bus_state, pending_data);
                    break;
                case DataOp::STACK_PUSH:
                    // For STACK_PUSH (PHP), the data to write is provided via pending_data
                    // JSR EXCLUSION: Skip JSR (opcode 0x20) as it handles stack push data coordination itself
                    if (cpu.get_opcode() != 0x20) {
                        BUS_SET_DATA(bus_state, pending_data);
                    }
                    break;
                default:
                    // Legacy: direct register mapping for load operations (should not be used for writes)
                    if (static_cast<uint8_t>(data_op) < static_cast<uint8_t>(CpuReg::COUNT)) {
                        BUS_SET_DATA(bus_state, cpu.get_reg(static_cast<CpuReg>(static_cast<uint8_t>(data_op))));
                    } else {
                        // Fallback for unknown operations
                        BUS_SET_DATA(bus_state, 0);
                    }
                    break;
            }
        }
        return bus_state;
    }
    
    // PERFORMANCE: Fast path memory operation execution (optimized for hot path)
    template<typename RegArray>
    static inline bus_state_t execute_memory_operation_fast(bus_state_t bus_state, RegArray& reg,
                                                          MemOp mem_op) {
        // Hot path optimization: Most common operations first with branch prediction
        
        // HOTTEST PATH: PC read with increment (instruction fetch and operand reads)
        if (__builtin_expect(mem_op == MemOp::READ_PC_INC, 1)) {
            const uint16_t pc = (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL];
            BUS_SET_ADDR(bus_state, pc);
            const uint16_t new_pc = (pc + 1) & 0xFFFF;
            reg[CpuReg::PCL] = new_pc & 0xFF;
            reg[CpuReg::PCH] = (new_pc >> 8) & 0xFF;
            return bus_state;
        }
        
        // COMMON PATH: Absolute addressing (most memory operations)
        if (__builtin_expect(mem_op == MemOp::READ_ABS, 1)) {
            const uint16_t addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
            BUS_SET_ADDR(bus_state, addr);
            return bus_state;
        }
        
        // LESS COMMON: Fall back to full implementation for other operations
        return execute_memory_operation(bus_state, reg, mem_op);
    }
};

} // namespace fam65xx_cpp

#endif // MEMORY_OPERATIONS_HPP