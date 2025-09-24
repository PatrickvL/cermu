#ifndef MEMORY_OPERATIONS_HPP
#define MEMORY_OPERATIONS_HPP

#include "cpu_defs.hpp"
#include "unified_helpers.hpp"
#include "../../../core/system_lines.h"

// Direct bus operations - no legacy compatibility macros
// Use native system_lines.h macros directly for maximum performance

namespace fam65xx_cpp {

template<typename BusConfig>
class MemoryOperations {
public:
    template<typename RegArray, typename CpuInstance>
    HOT_PATH static inline bus_state_t execute_memory_operation(bus_state_t bus_state, RegArray& reg,
                                                              MemOp mem_op, CpuInstance& cpu) {
        if (UNLIKELY(mem_op == MemOp::NOP)) return bus_state;
        
        uint16_t addr = 0;
        uint16_t pc = 0;
        
        // Execute memory operation - optimized with fallthrough for shared functionality
        switch (mem_op) {
            case MemOp::NOP:
                return bus_state;
            
            // PC operations - OPTIMIZED with fast helpers
            case MemOp::READ_PC_INC:
                pc = get_pc_unified(reg);
                bus_state = set_bus_address_unified(bus_state, pc);
                pc = increment_pc_unified(reg);
                break;
                
            case MemOp::READ_PC:
                pc = get_pc_unified(reg);
                bus_state = set_bus_address_unified(bus_state, pc);
                // Update ABL/ABH registers so get_address() returns correct values
                set_abs_address(reg, pc);
                break;
                
            // Absolute addressing - OPTIMIZED with shared calculation and fast helpers
            case MemOp::WRITE_ABS:
                bus_state = set_bus_write_unified(bus_state);
                [[fallthrough]];
            case MemOp::READ_ABS:
            // Note: READ_VECTOR is aliased to READ_ABS, so it's handled by this same case
                addr = get_abs_address(reg);
                
                // CONSTEXPR NMOS 6502 PAGE BOUNDARY BUG IMPLEMENTATION - TEMPORARILY DISABLED FOR TESTING
                // Only apply for NMOS variants and JMP indirect instruction cycle 4
                if constexpr (false && !BusConfig::has_cmos_fixes) {
                    if (UNLIKELY(cpu.get_opcode() == 0x6C && cpu.get_cycle_step() == 4)) {
                        // JMP indirect cycle 4: reading high byte of target address
                        // NMOS bug: when indirect pointer's low byte is 0xFF,
                        // the high byte is read from $xx00 instead of $(xx+1)00
                        
                        // Check if we're about to read from address $xx00 (indicating previous was $xxFF)
                        if (UNLIKELY((addr & 0xFF) == 0x00)) {
                            // This suggests the original indirect pointer was $xxFF
                            // NMOS bug: instead of reading from $(xx+1)00, read from $xx00
                            // Keep the same page but reset low byte to 0x00
                            addr = (addr & 0xFF00) | 0x00;
                        }
                    }
                }
                
                bus_state = set_bus_address_unified(bus_state, addr);
                break;
                
            // Zero page addressing - OPTIMIZED with fast helpers
            case MemOp::WRITE_ZP:
                bus_state = set_bus_write_unified(bus_state);
                [[fallthrough]];
            case MemOp::READ_ZP:
                bus_state = set_bus_address_unified(bus_state, reg[CpuReg::ABL]);
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
    HOT_PATH static inline bus_state_t handle_write_data(bus_state_t bus_state, CpuInstance& cpu,
                                               MemOp mem_op, DataOp data_op, uint8_t pending_data = 0) {
        // OPTIMIZED: Use fast write operation detection
        if (LIKELY(is_write_operation_unified(mem_op))) {
            // Handle store operations first - DEDUPLICATION: Use CPU helper function
            switch (data_op) {
                case DataOp::STORE_A:
                case DataOp::STORE_X:
                case DataOp::STORE_Y:
                case DataOp::STORE_ZERO:
                    {
                        // OPTIMIZED: Use fast store value retrieval and bus data setting
                        uint8_t value = cpu.get_store_register_value(data_op);
                        bus_state = set_bus_data_unified(bus_state, value);
                    }
                    break;
                case DataOp::ALU:
                    // OPTIMIZED: ALU result will be set by caller using pending_data
                    bus_state = set_bus_data_unified(bus_state, pending_data);
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
};

} // namespace fam65xx_cpp

#endif // MEMORY_OPERATIONS_HPP