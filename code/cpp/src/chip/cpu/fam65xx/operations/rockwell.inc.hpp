/*
 * rockwell.inc.hpp - Rockwell 65C02 Bit Manipulation Operations
 *
 * This file contains bit manipulation operation implementations (RMB/SMB/BBR/BBS)
 * that are included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// ROCKWELL 65C02 BIT MANIPULATION OPERATIONS
// ============================================================================

/* Helper function for RMB/SMB operations */
bus_state_t bit_modify_helper(bus_state_t pins, uint8_t bit_mask, bool set_bit) {
    return this->rmw_operation_helper(pins, [this, bit_mask, set_bit](uint8_t& value) {
        if (set_bit) {
            value |= bit_mask;   // Set bit
        } else {
            value &= ~bit_mask;  // Clear bit
        }
    });
}

/* RMB0-RMB7 - Reset Memory Bit */
bus_state_t op_rmb0(bus_state_t pins) { return bit_modify_helper(pins, 0x01, false); }
bus_state_t op_rmb1(bus_state_t pins) { return bit_modify_helper(pins, 0x02, false); }
bus_state_t op_rmb2(bus_state_t pins) { return bit_modify_helper(pins, 0x04, false); }
bus_state_t op_rmb3(bus_state_t pins) { return bit_modify_helper(pins, 0x08, false); }
bus_state_t op_rmb4(bus_state_t pins) { return bit_modify_helper(pins, 0x10, false); }
bus_state_t op_rmb5(bus_state_t pins) { return bit_modify_helper(pins, 0x20, false); }
bus_state_t op_rmb6(bus_state_t pins) { return bit_modify_helper(pins, 0x40, false); }
bus_state_t op_rmb7(bus_state_t pins) { return bit_modify_helper(pins, 0x80, false); }

/* SMB0-SMB7 - Set Memory Bit */
bus_state_t op_smb0(bus_state_t pins) { return bit_modify_helper(pins, 0x01, true); }
bus_state_t op_smb1(bus_state_t pins) { return bit_modify_helper(pins, 0x02, true); }
bus_state_t op_smb2(bus_state_t pins) { return bit_modify_helper(pins, 0x04, true); }
bus_state_t op_smb3(bus_state_t pins) { return bit_modify_helper(pins, 0x08, true); }
bus_state_t op_smb4(bus_state_t pins) { return bit_modify_helper(pins, 0x10, true); }
bus_state_t op_smb5(bus_state_t pins) { return bit_modify_helper(pins, 0x20, true); }
bus_state_t op_smb6(bus_state_t pins) { return bit_modify_helper(pins, 0x40, true); }
bus_state_t op_smb7(bus_state_t pins) { return bit_modify_helper(pins, 0x80, true); }

/* Helper function for bit branch operations (BBR/BBS) - simplified for AM_ZPR addressing mode */
bus_state_t bit_branch_helper(bus_state_t pins, uint8_t bit_mask, bool bit_set) {
    // AM_ZPR addressing mode has already read the zero page address and branch offset
    // AB register contains the zero page address, DL contains the branch offset
    
    // PHI2: Read value from zero page address
    pins = this->phi2_read(pins, REG_AB, REG_ABH);
    if (FAM65XX_GET_RDY(pins)) {
        // PHI1: Test bit and decide whether to branch
        bool bit_is_set = (this->get(REG_ABH) & bit_mask) != 0;
        bool branch_taken = (bit_is_set == bit_set);
        
        if (branch_taken) {
            int8_t signed_offset = (int8_t)this->get(REG_DL);
            // Branch taken: calculate target address and jump
            this->set(REG_PC, this->get(REG_PC) + signed_offset);
        }
        transition_to_fetch();
    }
    return pins;
}

/* BBR0-BBR7 - Branch on Bit Reset */
bus_state_t op_bbr0(bus_state_t pins) { return bit_branch_helper(pins, 0x01, false); }
bus_state_t op_bbr1(bus_state_t pins) { return bit_branch_helper(pins, 0x02, false); }
bus_state_t op_bbr2(bus_state_t pins) { return bit_branch_helper(pins, 0x04, false); }
bus_state_t op_bbr3(bus_state_t pins) { return bit_branch_helper(pins, 0x08, false); }
bus_state_t op_bbr4(bus_state_t pins) { return bit_branch_helper(pins, 0x10, false); }
bus_state_t op_bbr5(bus_state_t pins) { return bit_branch_helper(pins, 0x20, false); }
bus_state_t op_bbr6(bus_state_t pins) { return bit_branch_helper(pins, 0x40, false); }
bus_state_t op_bbr7(bus_state_t pins) { return bit_branch_helper(pins, 0x80, false); }

/* BBS0-BBS7 - Branch on Bit Set */
bus_state_t op_bbs0(bus_state_t pins) { return bit_branch_helper(pins, 0x01, true); }
bus_state_t op_bbs1(bus_state_t pins) { return bit_branch_helper(pins, 0x02, true); }
bus_state_t op_bbs2(bus_state_t pins) { return bit_branch_helper(pins, 0x04, true); }
bus_state_t op_bbs3(bus_state_t pins) { return bit_branch_helper(pins, 0x08, true); }
bus_state_t op_bbs4(bus_state_t pins) { return bit_branch_helper(pins, 0x10, true); }
bus_state_t op_bbs5(bus_state_t pins) { return bit_branch_helper(pins, 0x20, true); }
bus_state_t op_bbs6(bus_state_t pins) { return bit_branch_helper(pins, 0x40, true); }
bus_state_t op_bbs7(bus_state_t pins) { return bit_branch_helper(pins, 0x80, true); }

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
