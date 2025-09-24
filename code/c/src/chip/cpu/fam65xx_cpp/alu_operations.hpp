#ifndef ALU_OPERATIONS_HPP
#define ALU_OPERATIONS_HPP

#include "cpu_defs.hpp"
#include "cpu_config.hpp"
#include "fam65xx_validation.hpp"
#include "unified_helpers.hpp"

namespace fam65xx_cpp {

template<typename BusConfig>
class AluOperations {
public:
    // DEDUPLICATED: Use unified helpers for all flag operations
    template<typename RegArray>
    static constexpr void set_nz_flags(RegArray& reg, uint8_t value) {
        set_nz_flags_unified(reg, value);
    }
    
    template<typename RegArray>
    static constexpr void set_flag(RegArray& reg, uint8_t flag_mask) {
        reg[CpuReg::P] |= flag_mask;
    }
    
    template<typename RegArray>
    static constexpr void clear_flag(RegArray& reg, uint8_t flag_mask) {
        reg[CpuReg::P] &= ~flag_mask;
    }
    
    template<typename RegArray, uint8_t FLAG_MASK>
    static constexpr void toggle_flag_conditionally(RegArray& reg, bool condition) {
        set_flag_branchless(reg, FLAG_MASK, condition);
    }

    // Advanced constexpr ALU operation function - switch-based for maximum optimization
    template<AluOp alu_op, typename RegArray>
    static constexpr void execute_alu_operation_constexpr(RegArray& reg, uint8_t data) {
        uint16_t temp;
        uint8_t result = 0;
        uint8_t flags = 0;
        
        // Get register values for convenience
        const uint8_t a = reg[CpuReg::A], x = reg[CpuReg::X], y = reg[CpuReg::Y];
        
        // Execute ALU operation based on template parameter - switch enables optimal compile-time optimization
        switch (alu_op) {
            // === NO FLAG OPERATIONS - Return immediately ===
            case AluOp::NOP: return; // No operation - no flags
            case AluOp::TXS: reg[CpuReg::S] = x; return; // No N/Z flags for TXS
            case AluOp::BIT:
                result = a & data;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_NEGATIVE | P_OVERFLOW | P_ZERO)) |
                            (data & (P_NEGATIVE | P_OVERFLOW)) |
                            ((a & data) ? 0 : P_ZERO);
                return; // BIT handles its own flags - no N/Z fallthrough
            
            // === FLAG CLEAR/SET OPERATIONS - No N/Z flags ===
            case AluOp::CLC: reg[CpuReg::P] &= ~P_CARRY; return; // No N/Z flags
            case AluOp::SEC: reg[CpuReg::P] |= P_CARRY; return; // No N/Z flags
            case AluOp::CLI: reg[CpuReg::P] &= ~P_IRQ_DIS; return; // No N/Z flags
            case AluOp::SEI: reg[CpuReg::P] |= P_IRQ_DIS; return; // No N/Z flags
            case AluOp::CLV: reg[CpuReg::P] &= ~P_OVERFLOW; return; // No N/Z flags
            case AluOp::CLD: reg[CpuReg::P] &= ~P_DECIMAL; return; // No N/Z flags
            case AluOp::SED: reg[CpuReg::P] |= P_DECIMAL; return; // No N/Z flags
            
            // === SPECIAL OPERATIONS - No N/Z flags ===
            case AluOp::WAI: return; // WAI instruction - no flags
            case AluOp::STP: return; // STP instruction - no flags
            case AluOp::PHX: result = x; return; // Store X for stack push - no flags
            case AluOp::PHY: result = y; return; // Store Y for stack push - no flags
            case AluOp::SAX: result = a & x; return; // Result goes to memory - no flags
            case AluOp::JAM: return; // JAM instruction - CPU halt - no flags
            case AluOp::BRK_FLAG: reg[CpuReg::P] |= P_IRQ_DIS; return; // BRK only sets I flag in processor register, B flag only in pushed stack value
                
            // === ARITHMETIC OPERATIONS - Set N/Z flags ===
            case AluOp::ADC:
                alu_adc_unified<BusConfig>(reg, data);
                return; // ADC handles its own flags
                
            case AluOp::SBC:
                alu_sbc_unified<BusConfig>(reg, data);
                return; // SBC handles its own flags
   
            // === LOGICAL OPERATIONS - Set N/Z flags ===
            case AluOp::AND: alu_and_unified(reg, data); return;
            case AluOp::ORA: alu_ora_unified(reg, data); return;
            case AluOp::EOR: alu_eor_unified(reg, data); return;
            
            // === TRANSFER OPERATIONS - Set N/Z flags ===
            case AluOp::TXA: transfer_x_to_a_unified(reg); return;
            case AluOp::TAX: transfer_a_to_x_unified(reg); return;
            case AluOp::TYA: transfer_y_to_a_unified(reg); return;
            case AluOp::TAY: transfer_a_to_y_unified(reg); return;
            case AluOp::TSX: transfer_s_to_x_unified(reg); return;
            
            // === REGISTER INCREMENT/DECREMENT OPERATIONS - Set N/Z flags ===
            case AluOp::INX: increment_x_unified(reg); return;
            case AluOp::DEX: decrement_x_unified(reg); return;
            case AluOp::INY: increment_y_unified(reg); return;
            case AluOp::DEY: decrement_y_unified(reg); return;
            
            // === COMPARISON OPERATIONS - Set N/Z flags (+ Carry) ===
            case AluOp::CMP: compare_a_unified(reg, data); return;
            case AluOp::CPX: compare_x_unified(reg, data); return;
            case AluOp::CPY: compare_y_unified(reg, data); return;
                
            // === SHIFT/ROTATE OPERATIONS - Set N/Z flags (+ Carry) ===
            case AluOp::ASL:
                result = arithmetic_shift_left_unified(reg, data);
                reg[CpuReg::DL] = result;
                return;
                
            case AluOp::LSR:
                result = logical_shift_right_unified(reg, data);
                reg[CpuReg::DL] = result;
                return;
                
            case AluOp::ROL:
                result = rotate_left_unified(reg, data);
                reg[CpuReg::DL] = result;
                return;
                
            case AluOp::ROR:
                result = rotate_right_unified(reg, data);
                reg[CpuReg::DL] = result;
                return;
                
            // === INCREMENT/DECREMENT OPERATIONS - Set N/Z flags ===
            case AluOp::INC: result = (data + 1) & 0xFF; reg[CpuReg::DL] = result; break; // Sets N/Z flags
            case AluOp::DEC: result = (data - 1) & 0xFF; reg[CpuReg::DL] = result; break; // Sets N/Z flags
                
            // === STACK OPERATIONS - Set N/Z flags ===
            case AluOp::PLX: result = data; reg[CpuReg::X] = result; break; // Sets N/Z flags
            case AluOp::PLY: result = data; reg[CpuReg::Y] = result; break; // Sets N/Z flags
                
            // === ILLEGAL OPCODE LOAD OPERATIONS - Set N/Z flags ===
            case AluOp::LAX:
                result = data;
                reg[CpuReg::A] = result;
                reg[CpuReg::X] = result;
                break; // Sets N/Z flags
                
            // === ILLEGAL OPCODE COMPOUND OPERATIONS - Set N/Z flags ===
            case AluOp::DCP:
                result = (data - 1) & 0xFF;
                temp = a - result;
                result = temp & 0xFF; // Use result for flag setting
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                break; // Sets N/Z flags
                
            case AluOp::ISC:
                result = (data + 1) & 0xFF;
                temp = a - result - !(reg[CpuReg::P] & P_CARRY);
                flags = (temp < 0x100 ? P_CARRY : 0) |
                        ((a ^ result) & (a ^ (temp & 0xFF)) & 0x80 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = temp & 0xFF;
                result = reg[CpuReg::A]; // Use accumulator value for flag setting
                break; // Sets N/Z flags
                
            case AluOp::SLO:
                result = (data << 1) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] |= result;
                result = reg[CpuReg::A]; // Use accumulator value for flag setting
                break; // Sets N/Z flags
                
            case AluOp::RLA:
                result = ((data << 1) | (reg[CpuReg::P] & P_CARRY ? 1 : 0)) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] &= result;
                result = reg[CpuReg::A]; // Use accumulator value for flag setting
                break; // Sets N/Z flags
                
            case AluOp::SRE:
                result = data >> 1;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                reg[CpuReg::A] ^= result;
                result = reg[CpuReg::A]; // Use accumulator value for flag setting
                break; // Sets N/Z flags
                
            case AluOp::RRA:
                result = (data >> 1) | (reg[CpuReg::P] & P_CARRY ? 0x80 : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                temp = a + result + (reg[CpuReg::P] & P_CARRY);
                flags = (temp > 0xFF ? P_CARRY : 0) |
                        ((a ^ (temp & 0xFF)) & (result ^ (temp & 0xFF)) & 0x80 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = temp & 0xFF;
                result = reg[CpuReg::A]; // Use accumulator value for flag setting
                break; // Sets N/Z flags
                
            case AluOp::ANC:
                result = a & data;
                reg[CpuReg::A] = result;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (result & 0x80 ? P_CARRY : 0);
                break; // Sets N/Z flags
                
            case AluOp::ALR:
                result = (a & data) >> 1;
                reg[CpuReg::A] = result;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | ((a & data) & 0x01 ? P_CARRY : 0);
                break; // Sets N/Z flags
                
            case AluOp::ARR:
                temp = a & data;
                result = (temp >> 1) | (reg[CpuReg::P] & P_CARRY ? 0x80 : 0);
                reg[CpuReg::A] = result;
                flags = (result & 0x40 ? P_CARRY : 0) |
                        ((result ^ (result << 1)) & 0x40 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                break; // Sets N/Z flags
                
            // === ACCUMULATOR-SPECIFIC SHIFT/ROTATE OPERATIONS - Set N/Z flags ===
            case AluOp::ASL_ACC:
                result = (reg[CpuReg::A] << 1) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (reg[CpuReg::A] & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // Store directly in accumulator
                break; // Sets N/Z flags
                
            case AluOp::LSR_ACC:
                result = reg[CpuReg::A] >> 1;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (reg[CpuReg::A] & 0x01 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // Store directly in accumulator
                break; // Sets N/Z flags
                
            case AluOp::ROL_ACC:
                result = ((reg[CpuReg::A] << 1) | (reg[CpuReg::P] & P_CARRY ? 1 : 0)) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (reg[CpuReg::A] & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // Store directly in accumulator
                break; // Sets N/Z flags
                
            case AluOp::ROR_ACC:
                result = (reg[CpuReg::A] >> 1) | (reg[CpuReg::P] & P_CARRY ? 0x80 : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (reg[CpuReg::A] & 0x01 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // Store directly in accumulator
                break; // Sets N/Z flags
                
            default:
                result = data; // Default pass-through for unknown operations
                break; // Sets N/Z flags
        }
        
        // Compile-time validation: only operations that should update N/Z flags reach here
        // Operations using 'return' (NOP, TXS, BIT, CLC, SEC, CLI, SEI, CLV, CLD, SED,
        // WAI, STP, PHX, PHY, SAX, JAM, BRK_FLAG) should never reach this point
        nz_flag_validator<alu_op>::validate();
        
        // Common N/Z flag setting for operations that fall through
        set_nz_flags(reg, result);
    }
    
    // PERFORMANCE OPTIMIZED: Legacy runtime wrapper with hot path annotations
    template<typename RegArray>
    HOT_PATH static inline void execute_alu_operation(RegArray& reg, AluOp alu_op, uint8_t data) {
        // Runtime dispatch to templated constexpr functions with likely/unlikely hints
        switch (alu_op) {
            case AluOp::NOP: execute_alu_operation_constexpr<AluOp::NOP>(reg, data); break;
            // COMMON OPERATIONS: Mark as likely for better branch prediction
            case AluOp::ADC: execute_alu_operation_constexpr<AluOp::ADC>(reg, data); break;
            case AluOp::SBC: execute_alu_operation_constexpr<AluOp::SBC>(reg, data); break;
            case AluOp::AND: execute_alu_operation_constexpr<AluOp::AND>(reg, data); break;
            case AluOp::ORA: execute_alu_operation_constexpr<AluOp::ORA>(reg, data); break;
            case AluOp::EOR: execute_alu_operation_constexpr<AluOp::EOR>(reg, data); break;
            case AluOp::TXA: execute_alu_operation_constexpr<AluOp::TXA>(reg, data); break;
            case AluOp::TAX: execute_alu_operation_constexpr<AluOp::TAX>(reg, data); break;
            case AluOp::TYA: execute_alu_operation_constexpr<AluOp::TYA>(reg, data); break;
            case AluOp::TAY: execute_alu_operation_constexpr<AluOp::TAY>(reg, data); break;
            case AluOp::TSX: execute_alu_operation_constexpr<AluOp::TSX>(reg, data); break;
            case AluOp::TXS: execute_alu_operation_constexpr<AluOp::TXS>(reg, data); break;
            case AluOp::CMP: execute_alu_operation_constexpr<AluOp::CMP>(reg, data); break;
            case AluOp::CPX: execute_alu_operation_constexpr<AluOp::CPX>(reg, data); break;
            case AluOp::CPY: execute_alu_operation_constexpr<AluOp::CPY>(reg, data); break;
            case AluOp::ASL: execute_alu_operation_constexpr<AluOp::ASL>(reg, data); break;
            case AluOp::LSR: execute_alu_operation_constexpr<AluOp::LSR>(reg, data); break;
            case AluOp::ROL: execute_alu_operation_constexpr<AluOp::ROL>(reg, data); break;
            case AluOp::ROR: execute_alu_operation_constexpr<AluOp::ROR>(reg, data); break;
            case AluOp::INC: execute_alu_operation_constexpr<AluOp::INC>(reg, data); break;
            case AluOp::DEC: execute_alu_operation_constexpr<AluOp::DEC>(reg, data); break;
            case AluOp::BIT: execute_alu_operation_constexpr<AluOp::BIT>(reg, data); break;
            case AluOp::CLC: execute_alu_operation_constexpr<AluOp::CLC>(reg, data); break;
            case AluOp::SEC: execute_alu_operation_constexpr<AluOp::SEC>(reg, data); break;
            case AluOp::CLI: execute_alu_operation_constexpr<AluOp::CLI>(reg, data); break;
            case AluOp::SEI: execute_alu_operation_constexpr<AluOp::SEI>(reg, data); break;
            case AluOp::CLV: execute_alu_operation_constexpr<AluOp::CLV>(reg, data); break;
            case AluOp::CLD: execute_alu_operation_constexpr<AluOp::CLD>(reg, data); break;
            case AluOp::SED: execute_alu_operation_constexpr<AluOp::SED>(reg, data); break;
            case AluOp::WAI: execute_alu_operation_constexpr<AluOp::WAI>(reg, data); break;
            case AluOp::STP: execute_alu_operation_constexpr<AluOp::STP>(reg, data); break;
            case AluOp::PHX: execute_alu_operation_constexpr<AluOp::PHX>(reg, data); break;
            case AluOp::PHY: execute_alu_operation_constexpr<AluOp::PHY>(reg, data); break;
            case AluOp::PLX: execute_alu_operation_constexpr<AluOp::PLX>(reg, data); break;
            case AluOp::PLY: execute_alu_operation_constexpr<AluOp::PLY>(reg, data); break;
            case AluOp::LAX: execute_alu_operation_constexpr<AluOp::LAX>(reg, data); break;
            case AluOp::SAX: execute_alu_operation_constexpr<AluOp::SAX>(reg, data); break;
            case AluOp::DCP: execute_alu_operation_constexpr<AluOp::DCP>(reg, data); break;
            case AluOp::ISC: execute_alu_operation_constexpr<AluOp::ISC>(reg, data); break;
            case AluOp::SLO: execute_alu_operation_constexpr<AluOp::SLO>(reg, data); break;
            case AluOp::RLA: execute_alu_operation_constexpr<AluOp::RLA>(reg, data); break;
            case AluOp::SRE: execute_alu_operation_constexpr<AluOp::SRE>(reg, data); break;
            case AluOp::RRA: execute_alu_operation_constexpr<AluOp::RRA>(reg, data); break;
            case AluOp::ANC: execute_alu_operation_constexpr<AluOp::ANC>(reg, data); break;
            case AluOp::ALR: execute_alu_operation_constexpr<AluOp::ALR>(reg, data); break;
            case AluOp::ARR: execute_alu_operation_constexpr<AluOp::ARR>(reg, data); break;
            case AluOp::INX: execute_alu_operation_constexpr<AluOp::INX>(reg, data); break;
            case AluOp::DEX: execute_alu_operation_constexpr<AluOp::DEX>(reg, data); break;
            case AluOp::INY: execute_alu_operation_constexpr<AluOp::INY>(reg, data); break;
            case AluOp::DEY: execute_alu_operation_constexpr<AluOp::DEY>(reg, data); break;
            case AluOp::JAM: execute_alu_operation_constexpr<AluOp::JAM>(reg, data); break;
            case AluOp::BRK_FLAG: execute_alu_operation_constexpr<AluOp::BRK_FLAG>(reg, data); break;
            case AluOp::ASL_ACC: execute_alu_operation_constexpr<AluOp::ASL_ACC>(reg, data); break;
            case AluOp::LSR_ACC: execute_alu_operation_constexpr<AluOp::LSR_ACC>(reg, data); break;
            case AluOp::ROL_ACC: execute_alu_operation_constexpr<AluOp::ROL_ACC>(reg, data); break;
            case AluOp::ROR_ACC: execute_alu_operation_constexpr<AluOp::ROR_ACC>(reg, data); break;
            default: execute_alu_operation_constexpr<AluOp::NOP>(reg, data); break;
        }
    }
    
    // === COMPILE-TIME CONSTEXPR OPTIMIZATION HELPERS ===
    
    // Constexpr flag computation helpers for maximum optimization
    template<uint8_t VALUE>
    static constexpr uint8_t compute_nz_flags() {
        return ((VALUE == 0) ? P_ZERO : 0) | (VALUE & P_NEGATIVE);
    }
    
    // Constexpr arithmetic operation helpers
    template<uint8_t A, uint8_t DATA, bool CARRY_IN>
    static constexpr uint16_t compute_adc() {
        return A + DATA + (CARRY_IN ? 1 : 0);
    }
    
    template<uint8_t A, uint8_t DATA, bool CARRY_IN>
    static constexpr uint16_t compute_sbc() {
        return A - DATA - (CARRY_IN ? 0 : 1);
    }
    
    // Constexpr shift operation helpers for compile-time evaluation
    template<uint8_t DATA>
    static constexpr uint8_t compute_asl() {
        return (DATA << 1) & 0xFF;
    }
    
    template<uint8_t DATA>
    static constexpr uint8_t compute_lsr() {
        return DATA >> 1;
    }
    
    template<uint8_t DATA, bool CARRY_IN>
    static constexpr uint8_t compute_rol() {
        return ((DATA << 1) | (CARRY_IN ? 1 : 0)) & 0xFF;
    }
    
    template<uint8_t DATA, bool CARRY_IN>
    static constexpr uint8_t compute_ror() {
        return (DATA >> 1) | (CARRY_IN ? 0x80 : 0);
    }
    
};

} // namespace fam65xx_cpp

#endif // ALU_OPERATIONS_HPP