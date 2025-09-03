#ifndef ALU_OPERATIONS_HPP
#define ALU_OPERATIONS_HPP

#include "cpu_defs.hpp"

namespace fam65xx_cpp {

template<typename BusConfig>
class AluOperations {
public:
    template<typename RegArray>
    static inline void set_nz_flags(RegArray& reg, uint8_t value) {
        uint8_t flags = 0;
        if (value == 0) flags |= P_ZERO;
        if (value & 0x80) flags |= P_NEGATIVE;
        
        reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_NEGATIVE | P_ZERO)) | flags;
    }

    template<typename RegArray>
    static inline void execute_alu_operation(RegArray& reg, AluOp alu_op, uint8_t data) {
        if (alu_op == AluOp::NOP) return;
        
        uint16_t temp;
        uint8_t result = 0;
        uint8_t flags = 0;
        
        // Get register values for convenience
        const uint8_t a = reg[CpuReg::A], x = reg[CpuReg::X], y = reg[CpuReg::Y];
        
        // Execute ALU operation based on opcode
        switch (alu_op) {
            case AluOp::NOP: return;
            
            // Arithmetic operations
            case AluOp::ADC:
                temp = a + data + (reg[CpuReg::P] & P_CARRY);
                result = temp & 0xFF;
                flags = (temp > 0xFF ? P_CARRY : 0) |
                        ((a ^ result) & (data ^ result) & 0x80 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = result;
                break;
                
            case AluOp::SBC:
                temp = a - data - !(reg[CpuReg::P] & P_CARRY);
                result = temp & 0xFF;
                flags = (temp < 0x100 ? P_CARRY : 0) |
                        ((a ^ data) & (a ^ result) & 0x80 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = result;
                break;
            
            // Logical operations
            case AluOp::AND: result = a & data; reg[CpuReg::A] = result; break;
            case AluOp::ORA: result = a | data; reg[CpuReg::A] = result; break;
            case AluOp::EOR: result = a ^ data; reg[CpuReg::A] = result; break;
            
            // Transfer operations
            case AluOp::TXA: result = x; reg[CpuReg::A] = result; break;
            case AluOp::TAX: result = a; reg[CpuReg::X] = result; break;
            case AluOp::TYA: result = y; reg[CpuReg::A] = result; break;
            case AluOp::TAY: result = a; reg[CpuReg::Y] = result; break;
            case AluOp::TSX: result = reg[CpuReg::S]; reg[CpuReg::X] = result; break;
            case AluOp::TXS: reg[CpuReg::S] = x; return; // No flags
            
            // Comparison operations
            case AluOp::CMP:
                temp = a - data;
                result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                break;
            case AluOp::CPX:
                temp = x - data;
                result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                break;
            case AluOp::CPY:
                temp = y - data;
                result = temp & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                break;
            
            // Shift/rotate operations
            case AluOp::ASL:
                result = (data << 1) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                break;
                
            case AluOp::LSR:
                result = data >> 1;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                break;
                
            case AluOp::ROL:
                result = ((data << 1) | (reg[CpuReg::P] & P_CARRY ? 1 : 0)) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                break;
                
            case AluOp::ROR:
                result = (data >> 1) | (reg[CpuReg::P] & P_CARRY ? 0x80 : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                break;
            
            // Inc/Dec operations
            case AluOp::INC: result = (data + 1) & 0xFF; break;
            case AluOp::DEC: result = (data - 1) & 0xFF; break;
            
            // Bit test operation
            case AluOp::BIT:
                result = a & data;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_NEGATIVE | P_OVERFLOW | P_ZERO)) |
                            (data & (P_NEGATIVE | P_OVERFLOW)) |
                            ((a & data) ? 0 : P_ZERO);
                return; // BIT has special flag handling
                
            // Flag operations - early returns
            case AluOp::CLC: reg[CpuReg::P] &= ~P_CARRY; return;
            case AluOp::SEC: reg[CpuReg::P] |= P_CARRY; return;
            case AluOp::CLI: reg[CpuReg::P] &= ~P_IRQ_DIS; return;
            case AluOp::SEI: reg[CpuReg::P] |= P_IRQ_DIS; return;
            case AluOp::CLV: reg[CpuReg::P] &= ~P_OVERFLOW; return;
            case AluOp::CLD: reg[CpuReg::P] &= ~P_DECIMAL; return;
            case AluOp::SED: reg[CpuReg::P] |= P_DECIMAL; return;
            
            // CMOS additions - compile-time conditional
            case AluOp::WAI:
                // WAI instruction support - implementation depends on config
                return;
                
            case AluOp::STP:
                // STP instruction support - implementation depends on config
                return;
                
            // Stack operations - 65C02
            case AluOp::PHX: result = x; break; // Store X for stack push
            case AluOp::PHY: result = y; break; // Store Y for stack push
            case AluOp::PLX: result = data; reg[CpuReg::X] = result; break;
            case AluOp::PLY: result = data; reg[CpuReg::Y] = result; break;
            
            // Illegal opcodes - compile-time conditional
            case AluOp::LAX:
                result = data;
                reg[CpuReg::A] = result;
                reg[CpuReg::X] = result;
                break;
                
            case AluOp::SAX:
                result = a & x;
                // Used for SAX instruction - result goes to memory
                break;
                
            case AluOp::DCP:
                result = (data - 1) & 0xFF;
                // Then compare with A
                temp = a - result;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (temp < 0x100 ? P_CARRY : 0);
                set_nz_flags(reg, temp & 0xFF);
                return;
                
            case AluOp::ISC:
                result = (data + 1) & 0xFF;
                // Then SBC with A
                temp = a - result - !(reg[CpuReg::P] & P_CARRY);
                flags = (temp < 0x100 ? P_CARRY : 0) |
                        ((a ^ result) & (a ^ (temp & 0xFF)) & 0x80 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = temp & 0xFF;
                set_nz_flags(reg, reg[CpuReg::A]);
                return;
                
            case AluOp::SLO:
                result = (data << 1) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] |= result;
                set_nz_flags(reg, reg[CpuReg::A]);
                return;
                
            case AluOp::RLA:
                result = ((data << 1) | (reg[CpuReg::P] & P_CARRY ? 1 : 0)) & 0xFF;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x80 ? P_CARRY : 0);
                reg[CpuReg::A] &= result;
                set_nz_flags(reg, reg[CpuReg::A]);
                return;
                
            case AluOp::SRE:
                result = data >> 1;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                reg[CpuReg::A] ^= result;
                set_nz_flags(reg, reg[CpuReg::A]);
                return;
                
            case AluOp::RRA:
                result = (data >> 1) | (reg[CpuReg::P] & P_CARRY ? 0x80 : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (data & 0x01 ? P_CARRY : 0);
                // Then ADC with A
                temp = a + result + (reg[CpuReg::P] & P_CARRY);
                flags = (temp > 0xFF ? P_CARRY : 0) |
                        ((a ^ (temp & 0xFF)) & (result ^ (temp & 0xFF)) & 0x80 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                reg[CpuReg::A] = temp & 0xFF;
                set_nz_flags(reg, reg[CpuReg::A]);
                return;
                
            case AluOp::ANC:
                result = a & data;
                reg[CpuReg::A] = result;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | (result & 0x80 ? P_CARRY : 0);
                break;
                
            case AluOp::ALR:
                result = (a & data) >> 1;
                reg[CpuReg::A] = result;
                reg[CpuReg::P] = (reg[CpuReg::P] & ~P_CARRY) | ((a & data) & 0x01 ? P_CARRY : 0);
                break;
                
            case AluOp::ARR:
                temp = a & data;
                result = (temp >> 1) | (reg[CpuReg::P] & P_CARRY ? 0x80 : 0);
                reg[CpuReg::A] = result;
                flags = (result & 0x40 ? P_CARRY : 0) |
                        ((result ^ (result << 1)) & 0x40 ? P_OVERFLOW : 0);
                reg[CpuReg::P] = (reg[CpuReg::P] & ~(P_CARRY | P_OVERFLOW)) | flags;
                break;
                
            case AluOp::JAM:
                // JAM instruction - CPU enters infinite loop
                return;
                
            default:
                result = data; // Default pass-through
                break;
        }
        
        // Set N and Z flags for most operations (except those that handle flags specially)
        if (alu_op != AluOp::TXS && alu_op != AluOp::BIT &&
            alu_op != AluOp::CLC && alu_op != AluOp::SEC && alu_op != AluOp::CLI &&
            alu_op != AluOp::SEI && alu_op != AluOp::CLV && alu_op != AluOp::CLD &&
            alu_op != AluOp::SED && alu_op != AluOp::WAI && alu_op != AluOp::STP &&
            alu_op != AluOp::JAM && alu_op != AluOp::DCP && alu_op != AluOp::ISC &&
            alu_op != AluOp::SLO && alu_op != AluOp::RLA && alu_op != AluOp::SRE &&
            alu_op != AluOp::RRA) {
            set_nz_flags(reg, result);
        }
    }
};

} // namespace fam65xx_cpp

#endif // ALU_OPERATIONS_HPP