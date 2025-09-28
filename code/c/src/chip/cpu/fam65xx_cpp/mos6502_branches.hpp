#ifndef MOS6502_BRANCHES_HPP
#define MOS6502_BRANCHES_HPP

#include "mos6502_optimized.hpp"

namespace fam65xx_cpp {

/**
 * MOS6502 Branch Instruction Handler
 * 
 * Provides hardware-accurate branch processing with:
 * - All 8 conditional branch instructions
 * - Page crossing detection for cycle timing
 * - Hardware-accurate signed offset calculation
 * - Dynamic cycle count (2-4 cycles depending on conditions)
 */

template<typename Config>
class BranchController {
private:
    MOS6502Optimized<Config>& cpu;
    
    // Branch state tracking
    bool branch_taken = false;
    bool page_crossed = false;
    uint16_t original_pc = 0;
    int8_t branch_offset = 0;
    
public:
    BranchController(MOS6502Optimized<Config>& cpu_ref) : cpu(cpu_ref) {}
    
    // Evaluate branch condition based on opcode
    bool evaluate_branch_condition(uint8_t opcode) const {
        const uint8_t flags = cpu.get_p();
        
        switch (opcode) {
            // Negative flag branches
            case 0x10: // BPL - Branch if Plus (N=0)
                return !(flags & P_NEGATIVE);
            case 0x30: // BMI - Branch if Minus (N=1)
                return (flags & P_NEGATIVE);
                
            // Overflow flag branches  
            case 0x50: // BVC - Branch if Overflow Clear (V=0)
                return !(flags & P_OVERFLOW);
            case 0x70: // BVS - Branch if Overflow Set (V=1)
                return (flags & P_OVERFLOW);
                
            // Carry flag branches
            case 0x90: // BCC - Branch if Carry Clear (C=0)
                return !(flags & P_CARRY);
            case 0xB0: // BCS - Branch if Carry Set (C=1)
                return (flags & P_CARRY);
                
            // Zero flag branches
            case 0xD0: // BNE - Branch if Not Equal (Z=0)
                return !(flags & P_ZERO);
            case 0xF0: // BEQ - Branch if Equal (Z=1)
                return (flags & P_ZERO);
                
            // 65C02 unconditional branch
            case 0x80: // BRA - Branch Always (65C02 only)
                if constexpr (Config::has_cmos_fixes) {
                    return true;
                }
                return false;
                
            default:
                return false;
        }
    }
    
    // Execute branch instruction cycle
    bus_state_t execute_branch_cycle(bus_state_t bus_state, uint8_t opcode, uint8_t cycle) {
        switch (cycle) {
            case 1:
                // Cycle 1: Fetch branch offset
                branch_offset = static_cast<int8_t>(BUS_GET_DATA(bus_state));
                original_pc = cpu.get_pc();
                
                // Evaluate branch condition
                branch_taken = evaluate_branch_condition(opcode);
                
                if (!branch_taken) {
                    // Branch not taken - instruction completes in 2 cycles
                    return bus_state;
                }
                break;
                
            case 2:
                // Cycle 2: Branch taken - calculate target address
                if (branch_taken) {
                    uint16_t target_pc = original_pc + branch_offset;
                    cpu.set_pc(target_pc);
                    
                    // Check for page crossing
                    page_crossed = detect_page_crossing(original_pc, target_pc);
                    
                    if (!page_crossed) {
                        // No page crossing - instruction completes in 3 cycles
                        return bus_state;
                    }
                }
                break;
                
            case 3:
                // Cycle 3: Page crossing fix-up (if needed)
                if (branch_taken && page_crossed) {
                    // Additional cycle for page crossing - instruction completes in 4 cycles
                    return bus_state;
                }
                break;
        }
        
        return bus_state;
    }
    
    // Get dynamic cycle count for branch instruction
    uint8_t get_branch_cycle_count(uint8_t opcode) const {
        // BRA (65C02) is always 3 cycles regardless of page crossing
        if constexpr (Config::has_cmos_fixes) {
            if (opcode == 0x80) return 3;
        }
        
        // Standard conditional branches
        if (!branch_taken) return 2;           // Branch not taken
        if (!page_crossed) return 3;          // Branch taken, same page
        return 4;                             // Branch taken, page crossed
    }
    
    // Check if branch was taken (for cycle timing)
    bool was_branch_taken() const { return branch_taken; }
    
    // Check if page was crossed (for cycle timing)
    bool was_page_crossed() const { return page_crossed; }
    
    // Reset branch state (call at instruction completion)
    void reset_branch_state() {
        branch_taken = false;
        page_crossed = false;
        original_pc = 0;
        branch_offset = 0;
    }
    
private:
    // Detect page boundary crossing
    bool detect_page_crossing(uint16_t old_addr, uint16_t new_addr) const {
        return (old_addr & 0xFF00) != (new_addr & 0xFF00);
    }
};

/**
 * Transfer and Flag Instructions
 * 
 * Handles register transfer and flag manipulation instructions:
 * - TAX, TXA, TAY, TYA, TSX, TXS (transfer instructions)
 * - CLC, SEC, CLI, SEI, CLV, CLD, SED (flag instructions)
 * - INX, DEX, INY, DEY (register increment/decrement)
 */
template<typename Config>
class TransferFlagInstructions {
private:
    MOS6502Optimized<Config>& cpu;
    
public:
    TransferFlagInstructions(MOS6502Optimized<Config>& cpu_ref) : cpu(cpu_ref) {}
    
    // Execute transfer instruction
    void execute_transfer(uint8_t opcode) {
        switch (opcode) {
            case 0xAA: // TAX - Transfer A to X
                cpu.set_x(cpu.get_a());
                update_nz_flags(cpu.get_x());
                break;
                
            case 0xA8: // TAY - Transfer A to Y  
                cpu.set_y(cpu.get_a());
                update_nz_flags(cpu.get_y());
                break;
                
            case 0x8A: // TXA - Transfer X to A
                cpu.set_a(cpu.get_x());
                update_nz_flags(cpu.get_a());
                break;
                
            case 0x98: // TYA - Transfer Y to A
                cpu.set_a(cpu.get_y());
                update_nz_flags(cpu.get_a());
                break;
                
            case 0xBA: // TSX - Transfer S to X
                cpu.set_x(cpu.get_sp());
                update_nz_flags(cpu.get_x());
                break;
                
            case 0x9A: // TXS - Transfer X to S
                cpu.set_sp(cpu.get_x());
                // TXS doesn't affect flags
                break;
        }
    }
    
    // Execute flag manipulation instruction
    void execute_flag_instruction(uint8_t opcode) {
        uint8_t flags = cpu.get_p();
        
        switch (opcode) {
            case 0x18: // CLC - Clear Carry
                flags &= ~P_CARRY;
                break;
                
            case 0x38: // SEC - Set Carry
                flags |= P_CARRY;
                break;
                
            case 0x58: // CLI - Clear Interrupt Disable
                flags &= ~P_IRQ_DIS;
                break;
                
            case 0x78: // SEI - Set Interrupt Disable
                flags |= P_IRQ_DIS;
                break;
                
            case 0xB8: // CLV - Clear Overflow
                flags &= ~P_OVERFLOW;
                break;
                
            case 0xD8: // CLD - Clear Decimal
                flags &= ~P_DECIMAL;
                break;
                
            case 0xF8: // SED - Set Decimal
                flags |= P_DECIMAL;
                break;
        }
        
        cpu.set_p(flags);
    }
    
    // Execute register increment/decrement
    void execute_register_incdec(uint8_t opcode) {
        switch (opcode) {
            case 0xE8: // INX - Increment X
                {
                    uint8_t result = cpu.get_x() + 1;
                    cpu.set_x(result);
                    update_nz_flags(result);
                }
                break;
                
            case 0xCA: // DEX - Decrement X
                {
                    uint8_t result = cpu.get_x() - 1;
                    cpu.set_x(result);
                    update_nz_flags(result);
                }
                break;
                
            case 0xC8: // INY - Increment Y
                {
                    uint8_t result = cpu.get_y() + 1;
                    cpu.set_y(result);
                    update_nz_flags(result);
                }
                break;
                
            case 0x88: // DEY - Decrement Y
                {
                    uint8_t result = cpu.get_y() - 1;
                    cpu.set_y(result);
                    update_nz_flags(result);
                }
                break;
        }
    }
    
    // Execute compare instruction
    void execute_compare(uint8_t opcode, uint8_t data) {
        uint16_t result;
        
        switch (opcode) {
            case 0xC9: // CMP #$nn - Compare Accumulator Immediate
            case 0xC5: // CMP $nn - Compare Accumulator Zero Page
            case 0xD5: // CMP $nn,X - Compare Accumulator Zero Page,X
            case 0xCD: // CMP $nnnn - Compare Accumulator Absolute  
            case 0xDD: // CMP $nnnn,X - Compare Accumulator Absolute,X
            case 0xD9: // CMP $nnnn,Y - Compare Accumulator Absolute,Y
            case 0xC1: // CMP ($nn,X) - Compare Accumulator Indirect,X
            case 0xD1: // CMP ($nn),Y - Compare Accumulator Indirect,Y
                result = cpu.get_a() - data;
                break;
                
            case 0xE0: // CPX #$nn - Compare X Register Immediate
            case 0xE4: // CPX $nn - Compare X Register Zero Page
            case 0xEC: // CPX $nnnn - Compare X Register Absolute
                result = cpu.get_x() - data;
                break;
                
            case 0xC0: // CPY #$nn - Compare Y Register Immediate
            case 0xC4: // CPY $nn - Compare Y Register Zero Page
            case 0xCC: // CPY $nnnn - Compare Y Register Absolute
                result = cpu.get_y() - data;
                break;
                
            default:
                return;
        }
        
        // Set flags based on comparison result
        uint8_t flags = cpu.get_p() & ~(P_NEGATIVE | P_ZERO | P_CARRY);
        flags |= (result & 0x80) ? P_NEGATIVE : 0;      // N flag
        flags |= ((result & 0xFF) == 0) ? P_ZERO : 0;   // Z flag
        flags |= (result < 0x100) ? P_CARRY : 0;        // C flag (no borrow)
        
        cpu.set_p(flags);
    }
    
private:
    // Helper to update N and Z flags
    void update_nz_flags(uint8_t value) {
        uint8_t flags = cpu.get_p() & ~(P_NEGATIVE | P_ZERO);
        flags |= (value & 0x80) ? P_NEGATIVE : 0;
        flags |= (value == 0) ? P_ZERO : 0;
        cpu.set_p(flags);
    }
};

} // namespace fam65xx_cpp

#endif // MOS6502_BRANCHES_HPP