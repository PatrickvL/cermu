#ifndef CYCLE_ADDRESSING_HPP
#define CYCLE_ADDRESSING_HPP

#include "cycle_types.hpp"
#include "fam65xx_validation.hpp"

namespace fam65xx_cpp {

template<typename BusConfig>
class CycleAddressing {
public:
    // Empty cycle slot - used to fill unused table entries after SYNC
    static constexpr cycle_desc_t make_empty_cycle() {
        return CD_MAKE(MemOp::NOP, DataOp::NOP, AluOp::NOP);  // No SYNC
    }
    
    // === COMPREHENSIVE ADDRESSING MODE HELPERS ===
    
    // Single-cycle immediate operations (1 cycle total) - VALIDATED SYNC PLACEMENT
    static constexpr cycle_desc_t make_immediate(DataOp data_op, AluOp alu_op = AluOp::NOP) {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, data_op, alu_op);
    }
    
    // Zero page addressing helpers (2 cycles total)
    static constexpr cycle_desc_t make_zeropage(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::READ_ZP, final_op, alu_op);
        return make_empty_cycle();
    }
    
    // Zero page write operations (2 cycles total)
    static constexpr cycle_desc_t make_zeropage_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_ZP, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Absolute addressing helpers (3 cycles total)
    static constexpr cycle_desc_t make_absolute(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op);
        return make_empty_cycle();
    }
    
    // Absolute write operations (3 cycles total)
    static constexpr cycle_desc_t make_absolute_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Zero page indexed (zp,X or zp,Y) operations (3 cycles total)
    static constexpr cycle_desc_t make_zeropage_indexed(uint8_t cycle, DataOp index_op, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_ZP, final_op, alu_op);
        return make_empty_cycle();
    }
    
    // Zero page indexed write operations (3 cycles total)
    static constexpr cycle_desc_t make_zeropage_indexed_write(uint8_t cycle, DataOp index_op, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::WRITE_ZP, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Absolute indexed (abs,X or abs,Y) operations (4+ cycles, page crossing adds 1)
    static constexpr cycle_desc_t make_absolute_indexed(uint8_t cycle, DataOp index_op, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y, may cross page
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // cycle 4, always executes
        return make_empty_cycle();
    }
    
    // Absolute indexed write operations (always 4 cycles - writes always do extra cycle)
    static constexpr cycle_desc_t make_absolute_indexed_write(uint8_t cycle, DataOp index_op, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, index_op, AluOp::NOP); // Read dummy byte first (6502 quirk)
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Indirect indexed (zp,X) operations (5 cycles total)
    static constexpr cycle_desc_t make_indexed_indirect(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP); // Add X to zero page pointer
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read target address low
        if (cycle == 4) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read target address high
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // Read from target
        return make_empty_cycle();
    }
    
    // Indirect indexed write (zp,X) operations (5 cycles total)
    static constexpr cycle_desc_t make_indexed_indirect_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Indirect indexed (zp),Y operations (5+ cycles, page crossing adds 1)
    static constexpr cycle_desc_t make_indirect_indexed(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read base address low
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read base address high
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Add Y, may cross page
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // cycle 5, always executes
        return make_empty_cycle();
    }
    
    // Indirect indexed write (zp),Y operations (always 5 cycles)
    static constexpr cycle_desc_t make_indirect_indexed_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Read dummy first
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Memory modify operations (5 cycles: read address, read data, write old, write new)
    static constexpr cycle_desc_t make_memory_modify_zp(uint8_t cycle, AluOp modify_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value back
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, modify_op); // Write modified value WITHOUT SYNC
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP); // SYNC on cycle 5
        return make_empty_cycle(); // Return empty cycle for invalid cycles
    }
    
    // Memory modify operations absolute (6 cycles)
    static constexpr cycle_desc_t make_memory_modify_abs(uint8_t cycle, AluOp modify_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::TEMP_STORE, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_ABS, DataOp::TEMP_STORE, AluOp::NOP); // Write old value back
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_ABS, DataOp::TEMP_MODIFY, modify_op); // Write modified value WITHOUT SYNC
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP); // SYNC on cycle 6
        return make_empty_cycle(); // Return empty cycle for invalid cycles
    }
    
    // Stack operations
    static constexpr cycle_desc_t make_stack_push(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_stack_pull(uint8_t cycle, DataOp load_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_SP, load_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Branch operations (2-4 cycles: 2 if not taken, 3 if taken, 4 if taken + page crossed)
    // CONDITIONAL CYCLE SOLUTION: The CPU execution logic dynamically determines the final cycle
    static constexpr cycle_desc_t make_branch(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // NO SYNC - conditional
        if (cycle == 3) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // NO SYNC - conditional
        if (cycle == 4) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // NO SYNC - conditional
        return make_empty_cycle();
    }
    
    // Jump operations
    static constexpr cycle_desc_t make_jump_absolute(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_jump_indirect(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_HIGH, AluOp::NOP); // 6502 page boundary bug
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Subroutine operations
    static constexpr cycle_desc_t make_jsr(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal operation
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP); // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP); // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_rts(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal
        if (cycle == 3) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCL
        if (cycle == 4) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCH
        if (cycle == 5) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Internal - increment PC
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_rti(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal
        if (cycle == 3) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull status
        if (cycle == 4) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCL
        if (cycle == 5) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCH
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP);
        return make_empty_cycle();
    }
};

} // namespace fam65xx_cpp

#endif // CYCLE_ADDRESSING_HPP