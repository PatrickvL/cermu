#ifndef ALU_OPERATIONS_HPP
#define ALU_OPERATIONS_HPP

#include "cpu_defs.hpp"
#include "fam65xx_validation.hpp"

namespace fam65xx_cpp {

template<typename BusConfig>
class AluOperations {
public:
    // Optimized templated constexpr N/Z flag updates - universal for any engine
    template<typename RegArray>
    static constexpr void set_nz_flags(RegArray& reg, uint8_t value) {
        // Optimized flag computation using bit manipulation
        constexpr uint8_t NZ_MASK = P_NEGATIVE | P_ZERO;
        const uint8_t flags = ((value == 0) ? P_ZERO : 0) |
                              (value & P_NEGATIVE);
        
        reg[CpuReg::P] = (reg[CpuReg::P] & ~NZ_MASK) | flags;
    }
    
    // Optimized flag operations with compile-time constants
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
        if (condition) {
            reg[CpuReg::P] |= FLAG_MASK;
        } else {
            reg[CpuReg::P] &= ~FLAG_MASK;
        }
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
            case AluOp::BRK_FLAG: reg[CpuReg::P] |= (P_BREAK | P_IRQ_DIS); return; // BRK flags only
                
            // === ARITHMETIC OPERATIONS - Set N/Z flags ===
            case AluOp::ADC: {
                uint8_t nz_flag_value;
                if (reg[CpuReg::P] & P_DECIMAL) {
                    // Decimal (BCD) mode addition
                    // CRITICAL: On 6502, N/Z/V flags are set based on BINARY result, not BCD result
                    uint8_t carry_in = (reg[CpuReg::P] & P_CARRY) ? 1 : 0;
                    
                    // First perform binary addition for flag calculation
                    temp = a + data + carry_in;
                    uint8_t binary_result = temp & 0xFF;
                    nz_flag_value = binary_result; // N/Z flags based on binary result
                    
                    // Calculate V flag based on binary arithmetic (NOT carry flag yet)
                    flags = ((~(a ^ data) & (a ^ binary_result) & 0x80) ? P_OVERFLOW : 0);
                    
                    // Then perform BCD correction for the final stored result and carry flag
                    uint16_t lo_nibble = (a & 0x0F) + (data & 0x0F) + carry_in;
                    uint16_t hi_nibble = (a >> 4) + (data >> 4);
                    
                    if (lo_nibble > 9) {
                        lo_nibble += 6;
                        hi_nibble += 1;
                    }
                    if (hi_nibble > 9) {
                        hi_nibble += 6;
                    }
                    
                    result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
                    
                    // Set carry flag based on BCD overflow (NOT binary overflow)
                    flags |= (hi_nibble > 0x0F ? P_CARRY : 0);
                } else {
                    // Binary mode addition
                    temp = a + data + (reg[CpuReg::P] & P_CARRY);
                    result = temp & 0xFF;
                    nz_flag_value = result; // N/Z flags based on actual result
                    flags = (temp > 0xFF ? P_CARRY : 0) |
                            ((~(a ^ data) & (a ^ result) & 0x80) ? P_OVERFLOW : 0);
                }
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = result;
                set_nz_flags(reg, nz_flag_value);
                return; // Skip the common N/Z flag setting at end
            }
                
            case AluOp::SBC:
                if (reg[CpuReg::P] & P_DECIMAL) {
                    // Decimal (BCD) mode subtraction
                    uint8_t borrow = (reg[CpuReg::P] & P_CARRY) ? 0 : 1;
                    int16_t lo_nibble = (a & 0x0F) - (data & 0x0F) - borrow;
                    int16_t hi_nibble = (a >> 4) - (data >> 4);
                    
                    if (lo_nibble < 0) {
                        lo_nibble -= 6;
                        hi_nibble -= 1;
                    }
                    if (hi_nibble < 0) {
                        hi_nibble -= 6;
                    }
                    
                    result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
                    flags = (hi_nibble >= 0 ? P_CARRY : 0) |
                            ((a ^ data) & (a ^ result) & 0x80 ? P_OVERFLOW : 0);
                } else {
                    // Binary mode subtraction
                    temp = a - data - !(reg[CpuReg::P] & P_CARRY);
                    result = temp & 0xFF;
                    flags = (temp < 0x100 ? P_CARRY : 0) |
                            ((a ^ data) & (a ^ result) & 0x80 ? P_OVERFLOW : 0);
                }
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = result;
                break; // Sets N/Z flags
   
            // === LOGICAL OPERATIONS - Set N/Z flags ===
            case AluOp::AND: result = a & data; reg[CpuReg::A] = result; break; // Sets N/Z flags
            case AluOp::ORA: result = a | data; reg[CpuReg::A] = result; break; // Sets N/Z flags
            case AluOp::EOR: result = a ^ data; reg[CpuReg::A] = result; break; // Sets N/Z flags
            
            // === TRANSFER OPERATIONS - Set N/Z flags ===
            case AluOp::TXA: result = x; reg[CpuReg::A] = result; break; // Sets N/Z flags
            case AluOp::TAX: result = a; reg[CpuReg::X] = result; break; // Sets N/Z flags
            case AluOp::TYA: result = y; reg[CpuReg::A] = result; break; // Sets N/Z flags
            case AluOp::TAY: result = a; reg[CpuReg::Y] = result; break; // Sets N/Z flags
            case AluOp::TSX: result = reg[CpuReg::S]; reg[CpuReg::X] = result; break; // Sets N/Z flags
            
            // === COMPARISON OPERATIONS - Set N/Z flags (+ Carry) ===
            case AluOp::CMP:
                temp = a - data;
                result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                break; // Sets N/Z flags
                
            case AluOp::CPX:
                temp = x - data;
                result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                break; // Sets N/Z flags
                
            case AluOp::CPY:
                temp = y - data;
                result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                break; // Sets N/Z flags
                
            // === SHIFT/ROTATE OPERATIONS - Set N/Z flags (+ Carry) ===
            case AluOp::ASL:
                result = (data << 1) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // CRITICAL FIX: Write result back to accumulator
                break; // Sets N/Z flags
                
            case AluOp::LSR:
                result = data >> 1;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // CRITICAL FIX: Write result back to accumulator
                break; // Sets N/Z flags
                
            case AluOp::ROL:
                result = ((data << 1) | (reg[CpuReg::P] & P_CARRY ? 1 : 0)) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // CRITICAL FIX: Write result back to accumulator
                break; // Sets N/Z flags
                
            case AluOp::ROR:
                result = (data >> 1) | (reg[CpuReg::P] & P_CARRY ? 0x80 : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                reg[CpuReg::A] = result; // CRITICAL FIX: Write result back to accumulator
                break; // Sets N/Z flags
                
            // === INCREMENT/DECREMENT OPERATIONS - Set N/Z flags ===
            case AluOp::INC: result = (data + 1) & 0xFF; break; // Sets N/Z flags
            case AluOp::DEC: result = (data - 1) & 0xFF; break; // Sets N/Z flags
                
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
    
    // Legacy runtime wrapper for backward compatibility and dynamic operation dispatch
    template<typename RegArray>
    static inline void execute_alu_operation(RegArray& reg, AluOp alu_op, uint8_t data) {
        // Runtime dispatch to templated constexpr functions
        switch (alu_op) {
            case AluOp::NOP: execute_alu_operation_constexpr<AluOp::NOP>(reg, data); break;
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
            case AluOp::JAM: execute_alu_operation_constexpr<AluOp::JAM>(reg, data); break;
            case AluOp::BRK_FLAG: execute_alu_operation_constexpr<AluOp::BRK_FLAG>(reg, data); break;
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
    
    // PERFORMANCE: Ultra-fast ALU operation execution with direct dispatch
    template<typename RegArray>
    static inline void execute_alu_operation_fast(RegArray& reg, AluOp alu_op, uint8_t data) {
        // ULTRA-HOT PATH: Direct operation dispatch using constexpr functions
        // This eliminates all runtime switches for maximum performance
        
        switch (alu_op) {
            // Most common operations optimized with direct execution
            case AluOp::NOP: return; // Fastest path - no operation
            
            // Transfer operations - highly optimized inline
            case AluOp::TXA: reg[CpuReg::A] = reg[CpuReg::X]; set_nz_flags(reg, reg[CpuReg::A]); return;
            case AluOp::TAX: reg[CpuReg::X] = reg[CpuReg::A]; set_nz_flags(reg, reg[CpuReg::X]); return;
            case AluOp::TYA: reg[CpuReg::A] = reg[CpuReg::Y]; set_nz_flags(reg, reg[CpuReg::A]); return;
            case AluOp::TAY: reg[CpuReg::Y] = reg[CpuReg::A]; set_nz_flags(reg, reg[CpuReg::Y]); return;
            case AluOp::TSX: reg[CpuReg::X] = reg[CpuReg::S]; set_nz_flags(reg, reg[CpuReg::X]); return;
            case AluOp::TXS: reg[CpuReg::S] = reg[CpuReg::X]; return; // No flags
            
            // Logical operations - highly optimized inline
            case AluOp::AND: {
                const uint8_t result = reg[CpuReg::A] & data;
                reg[CpuReg::A] = result;
                set_nz_flags(reg, result);
                return;
            }
            case AluOp::ORA: {
                const uint8_t result = reg[CpuReg::A] | data;
                reg[CpuReg::A] = result;
                set_nz_flags(reg, result);
                return;
            }
            case AluOp::EOR: {
                const uint8_t result = reg[CpuReg::A] ^ data;
                reg[CpuReg::A] = result;
                set_nz_flags(reg, result);
                return;
            }
            
            // Comparison operations - optimized inline
            case AluOp::CMP: {
                const uint16_t temp = reg[CpuReg::A] - data;
                const uint8_t result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                set_nz_flags(reg, result);
                return;
            }
            case AluOp::CPX: {
                const uint16_t temp = reg[CpuReg::X] - data;
                const uint8_t result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                set_nz_flags(reg, result);
                return;
            }
            case AluOp::CPY: {
                const uint16_t temp = reg[CpuReg::Y] - data;
                const uint8_t result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                set_nz_flags(reg, result);
                return;
            }
            
            // Arithmetic operations - highly optimized inline
            case AluOp::ADC: {
                const uint8_t a = reg[CpuReg::A];
                uint8_t result;
                uint8_t flags;
                uint8_t nz_flag_value; // Value to use for N/Z flag calculation
                
                if (reg[CpuReg::P] & P_DECIMAL) {
                    // Decimal (BCD) mode addition
                    // CRITICAL: On 6502, N/Z/V flags are set based on BINARY result, not BCD result
                    uint8_t carry_in = (reg[CpuReg::P] & P_CARRY) ? 1 : 0;
                    
                    // First perform binary addition for flag calculation
                    const uint16_t temp = a + data + carry_in;
                    uint8_t binary_result = temp & 0xFF;
                    nz_flag_value = binary_result; // N/Z flags based on binary result
                    
                    // Calculate V flag based on binary arithmetic (but NOT carry flag yet)
                    flags = ((~(a ^ data) & (a ^ binary_result) & 0x80) ? P_OVERFLOW : 0);
                    
                    // Then perform BCD correction for the final stored result and carry flag
                    uint16_t lo_nibble = (a & 0x0F) + (data & 0x0F) + carry_in;
                    uint16_t hi_nibble = (a >> 4) + (data >> 4);
                    
                    if (lo_nibble > 9) {
                        lo_nibble += 6;
                        hi_nibble += 1;
                    }
                    if (hi_nibble > 9) {
                        hi_nibble += 6;
                    }
                    
                    result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
                    
                    // Set carry flag based on BCD overflow (NOT binary overflow)
                    flags |= (hi_nibble > 0x0F ? P_CARRY : 0);
                } else {
                    // Binary mode addition
                    const uint16_t temp = a + data + (reg[CpuReg::P] & P_CARRY ? 1 : 0);
                    result = temp & 0xFF;
                    nz_flag_value = result; // N/Z flags based on actual result
                    flags = (temp > 0xFF ? P_CARRY : 0) |
                            ((~(a ^ data) & (a ^ result) & 0x80) ? P_OVERFLOW : 0);
                }
                
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = result;
                set_nz_flags(reg, nz_flag_value);
                return;
            }
            case AluOp::SBC: {
                const uint8_t a = reg[CpuReg::A];
                uint8_t result;
                uint8_t flags;
                
                if (reg[CpuReg::P] & P_DECIMAL) {
                    // Decimal (BCD) mode subtraction
                    uint8_t borrow = (reg[CpuReg::P] & P_CARRY) ? 0 : 1;
                    int16_t lo_nibble = (a & 0x0F) - (data & 0x0F) - borrow;
                    int16_t hi_nibble = (a >> 4) - (data >> 4);
                    
                    if (lo_nibble < 0) {
                        lo_nibble -= 6;
                        hi_nibble -= 1;
                    }
                    if (hi_nibble < 0) {
                        hi_nibble -= 6;
                    }
                    
                    result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
                    flags = (hi_nibble >= 0 ? P_CARRY : 0) |
                            ((a ^ data) & (a ^ result) & 0x80 ? P_OVERFLOW : 0);
                } else {
                    // Binary mode subtraction
                    const uint16_t temp = a - data - (reg[CpuReg::P] & P_CARRY ? 0 : 1);
                    result = temp & 0xFF;
                    flags = (temp < 0x100 ? P_CARRY : 0) |
                            ((a ^ data) & (a ^ result) & 0x80 ? P_OVERFLOW : 0);
                }
                
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = result;
                set_nz_flags(reg, result);
                return;
            }
            
            // Flag operations - optimized inline
            case AluOp::CLC: reg[CpuReg::P] &= ~P_CARRY; return;
            case AluOp::SEC: reg[CpuReg::P] |= P_CARRY; return;
            case AluOp::CLI: reg[CpuReg::P] &= ~P_IRQ_DIS; return;
            case AluOp::SEI: reg[CpuReg::P] |= P_IRQ_DIS; return;
            case AluOp::CLV: reg[CpuReg::P] &= ~P_OVERFLOW; return;
            case AluOp::CLD: reg[CpuReg::P] &= ~P_DECIMAL; return;
            case AluOp::SED: reg[CpuReg::P] |= P_DECIMAL; return;
            
            // Less common operations - fall back to constexpr implementation
            default:
                execute_alu_operation(reg, alu_op, data);
                return;
        }
    }
    
    // === ADAPTIVE PERFORMANCE ALU DISPATCH ===
    
    // Adaptive dispatch that chooses the optimal execution path based on operation frequency
    template<typename RegArray>
    static inline void execute_alu_operation_adaptive(RegArray& reg, AluOp alu_op, uint8_t data) {
        // Use branch prediction hints for maximum performance
        const auto op_index = static_cast<uint8_t>(alu_op);
        
        // Hot path operations get direct execution (most common ALU operations)
        if (__builtin_expect(op_index <= static_cast<uint8_t>(AluOp::SED), 1)) {
            execute_alu_operation_fast(reg, alu_op, data);
        } else {
            // Cold path operations use full implementation
            execute_alu_operation(reg, alu_op, data);
        }
    }
};

} // namespace fam65xx_cpp

#endif // ALU_OPERATIONS_HPP