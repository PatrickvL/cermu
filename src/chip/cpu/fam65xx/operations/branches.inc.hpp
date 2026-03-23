/*
 * branches.inc.hpp - Branch Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "chip/cpu/fam65xx/operations/inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// BRANCH HELPER FUNCTION
// ============================================================================

/* Helper function for branch operations - hardware-accurate 6502 timing */
bus_state_t branch_helper(bus_state_t pins, uint8_t flag_mask,
                          bool flag_value) {
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Set up operand read from PC (don't modify PC - RDY retry safety) */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;
  case 1: {
    /* PHI1: Load operand, increment PC, and check branch condition */
    this->bus_load_reg(ABL, pins);
    ++regs_[PC];
    bool branch_taken = ((regs_[P] & flag_mask) != 0) == flag_value;
    
    if (!branch_taken) {
      /* Branch not taken: instruction completes after 2 cycles */
      this->transition_to_fetch();
      return pins;
    }

    /* Branch taken: save interrupt state at this penultimate cycle.
     * The shift register reflects what process_interrupt_detection found
     * during the preceding PHI2 (case 0).  On NMOS 6502 hardware the
     * penultimate cycle is the last cycle that polls interrupts — the fixup
     * cycle (T2) does NOT poll.  We compare the snapshot at the fetch
     * boundary to allow interrupts that were already detectable, while
     * suppressing those that first become detectable during the fixup.
     *
     * We use a 2-bit check (not 3-bit) because the snapshot is taken 1
     * cycle before the fetch boundary, so there's 1 fewer accumulated
     * sample than the main 3-bit detection path uses. */
    this->branch_poll_shift_reg_ = this->interrupt_shift_register;
    if constexpr (has_nmi_line()) {
      this->branch_nmi_pending_at_poll_ = this->nmi_output_latch_;
    }

    /* Branch taken: calculate correct target address */
    /* Branch offset is relative to PC after incrementing past the offset byte */
    /* PC was already incremented on line 26, so it now points past the 2-byte instruction */
    uint16_t branch_base = regs_[PC];
    regs_[AB] = branch_base + (int8_t)regs_[ABL];
    this->half_cycle++;
    return pins;
  }

  case 2:
    /* PHI2: Dummy read from incremented PC (hardware behavior) */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 3: {
    /* PHI1: Check for page cross */
    bool page_cross = this->page_crossed(regs_[PC], regs_[AB]);
    if (!page_cross) {
      /* No page cross: set final PC and complete after 3 cycles */
      /* 6502 quirk: the fixup cycle of a taken branch without page cross
       * does NOT poll interrupts.  Set suppression flag so the next fetch
       * boundary skips interrupt hijacking, allowing one more instruction
       * to execute before the interrupt is serviced. */
      regs_[PC] = regs_[AB];
      this->branch_irq_suppression_ = true;
      this->transition_to_fetch();
      return pins;
    }

    /* Page cross detected: need penalty cycle with intermediate address */
    /* Hardware behavior: Add signed offset to PC low byte only, ignore carry */
    /* The intermediate address = (PC & 0xFF00) | ((PCL + signed_offset) & 0xFF) */
    uint8_t pc_low = regs_[PCL];
    int8_t signed_offset = (int8_t)regs_[ABL];
    uint8_t new_low = (uint8_t)(pc_low + signed_offset); // Let it wrap naturally

    /* Store intermediate address in PC for penalty cycle read */
    regs_[PCL] = new_low;
    /* AB still contains the correct final target from case 1 */
    this->half_cycle++;
    return pins;
  }

  case 4:
    /* PHI2: Page cross penalty - dummy read from intermediate address in PC */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 5:
    /* PHI1: Set final correct target PC and complete instruction */
    /* AB contains the correct target from case 1 */
    regs_[PC] = regs_[AB];
    this->transition_to_fetch();
    return pins;
  }

  return pins;
}

// ============================================================================
// CONDITIONAL BRANCH OPERATIONS
// ============================================================================

/* BCC - Branch if Carry Clear */
bus_state_t op_bcc(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_C, false);
}

/* BCS - Branch if Carry Set */
bus_state_t op_bcs(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_C, true);
}

/* BEQ - Branch if Equal (Zero Set) */
bus_state_t op_beq(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_Z, true);
}

/* BNE - Branch if Not Equal (Zero Clear) */
bus_state_t op_bne(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_Z, false);
}

/* BMI - Branch if Minus (Negative Set) */
bus_state_t op_bmi(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_N, true);
}

/* BPL - Branch if Plus (Negative Clear) */
bus_state_t op_bpl(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_N, false);
}

/* BVC - Branch if Overflow Clear */
bus_state_t op_bvc(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_V, false);
}

/* BVS - Branch if Overflow Set */
bus_state_t op_bvs(bus_state_t pins) {
  trace_operation(__func__);
  return branch_helper(pins, FLAG_V, true);
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "chip/cpu/fam65xx/operations/inc_lint_prevention_footer.hpp"
