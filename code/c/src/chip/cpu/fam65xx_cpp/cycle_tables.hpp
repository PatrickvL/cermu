#ifndef CYCLE_TABLES_HPP
#define CYCLE_TABLES_HPP

#include "cpu_defs.hpp"

namespace fam65xx_cpp {

// Cycle descriptor structure - still uses bit fields for packing
struct cycle_desc_t {
    uint8_t mem_op : 4;   // Packed as uint8_t for efficiency
    uint8_t data_op : 4;  // Packed as uint8_t for efficiency
    uint8_t alu_op : 8;   // Packed as uint8_t for efficiency
    
    // Type-safe accessors that return proper enum types
    constexpr MemOp get_mem_op() const noexcept { return static_cast<MemOp>(mem_op); }
    constexpr DataOp get_data_op() const noexcept { return static_cast<DataOp>(data_op); }
    constexpr AluOp get_alu_op() const noexcept { return static_cast<AluOp>(alu_op); }
};

// Cycle descriptor creation macros
#define CD_MAKE_SYNC(mem, data, alu) \
    cycle_desc_t{static_cast<uint8_t>(mem), static_cast<uint8_t>(data), static_cast<uint8_t>(alu)}

#define CD_MAKE(mem, data, alu) \
    cycle_desc_t{static_cast<uint8_t>(mem), static_cast<uint8_t>(data), static_cast<uint8_t>(alu)}

template<typename BusConfig>
class CycleTables {
public:
    // LDA immediate - single cycle
    static constexpr cycle_desc_t make_lda_imm() {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_A, AluOp::NOP);
    }
    
    // LDA zero page - 2 cycles
    static constexpr cycle_desc_t make_lda_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::LOAD_A, AluOp::NOP);
    }
    
    // LDA absolute - 3 cycles
    static constexpr cycle_desc_t make_lda_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP);
    }
    
    // ADC immediate - single cycle
    static constexpr cycle_desc_t make_adc_imm() {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::ADC);
    }
    
    // NOP - single cycle
    static constexpr cycle_desc_t make_nop() {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
    }
    
    // BRK - 7 cycles
    static constexpr cycle_desc_t make_brk(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 5: return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);
            case 6: return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);
            default: return make_nop();
        }
    }
    
    // LAX zero page - illegal opcode, 2 cycles
    static constexpr cycle_desc_t make_lax_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::ILLEGAL_COMBO, AluOp::NOP);
    }
    
    // Main cycle table lookup
    static constexpr cycle_desc_t get_cycle(uint8_t opcode, uint8_t cycle) {
        // Basic instruction decode
        switch (opcode) {
            // LDA variants
            case 0xA9: return make_lda_imm();          // LDA #$nn
            case 0xA5: return make_lda_zp(cycle);      // LDA $nn
            case 0xAD: return make_lda_abs(cycle);     // LDA $nnnn
            
            // ADC variants  
            case 0x69: return make_adc_imm();          // ADC #$nn
            
            // System
            case 0xEA: return make_nop();              // NOP
            case 0x00: return make_brk(cycle);         // BRK
            
            // Illegal opcodes
            case 0xA7: return make_lax_zp(cycle);      // LAX $nn (illegal)
            
            default:
                // Unknown opcode - default to NOP behavior
                return make_nop();
        }
    }
    
    // Get number of cycles for an instruction
    static constexpr uint8_t get_cycle_count(uint8_t opcode) {
        switch (opcode) {
            case 0xA9:   // LDA #$nn
            case 0x69:   // ADC #$nn  
            case 0xEA:   // NOP
                return 2;
                
            case 0xA5:   // LDA $nn
            case 0xA7:   // LAX $nn (illegal)
                return 3;
                
            case 0xAD:   // LDA $nnnn
                return 4;
                
            case 0x00:   // BRK
                return 7;
                
            default:
                return 2; // Default to 2 cycles
        }
    }
};

} // namespace fam65xx_cpp

#endif // CYCLE_TABLES_HPP