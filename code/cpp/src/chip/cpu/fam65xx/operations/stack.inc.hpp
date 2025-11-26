/*
 * stack.inc.hpp - Stack Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// STACK OPERATIONS
// ============================================================================

/* PHA - Push Accumulator */
bus_state_t op_pha(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->this->is_accumulator_16bit()) {
      // Native mode, 16-bit accumulator - perform 16-bit PHA
      switch (this->cycle_index) {
      case 0:
        /* Dummy cycle for internal operation */
        pins = this->bus_setup_dummy<Addr::PC>(pins);

        return pins;

      case 1:
        /* PHI2: Write high byte of A to stack */

        pins = this->bus_setup_write<Addr::SP>(pins, this->get(REG_AH));
        this->dec(REG_S);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI2: Write low byte of A to stack */

        pins = this->bus_setup_write<Addr::SP>(pins, this->get(REG_AL));
        this->dec(REG_S);
        this->this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit PHA operation (emulation mode and non-wide CPUs)
  switch (this->cycle_index) {
  case 0:
    /* Dummy cycle for internal operation */
    pins = this->bus_setup_dummy<Addr::PC>(pins);

    return pins;

  case 1:
    /* PHI2: Write A to stack with processor-specific RDY handling */

    pins = this->bus_setup_write<Addr::SP>(pins, this->get(REG_A));
    this->dec(REG_S);
    this->this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* PHP - Push Processor Status */
bus_state_t op_php(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->cycle_index) {
  case 0:
    /* Dummy cycle for internal operation */
    pins = this->bus_setup_dummy<Addr::PC>(pins);

    this->set(REG_DL, this->get(REG_P) | FLAG_B | FLAG_U);
    return pins;

  case 1:
    /* PHI2: Write P|B|U to stack with processor-specific RDY handling */

    pins = this->bus_setup_write<Addr::SP>(pins, this->get(REG_DL));
    this->dec(REG_S);
    this->this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* PLA - Pull Accumulator */
bus_state_t op_pla(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->this->is_accumulator_16bit()) {
      // Native mode, 16-bit accumulator - perform 16-bit PLA
      switch (this->cycle_index) {
      case 0:
        /* PHI2: Dummy read from PC */
        pins = this->bus_setup_dummy<Addr::PC>(pins);
        return pins;

      case 1:
        /* PHI1: No operation - just increment cycle */
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI2: Dummy read from current stack pointer */
        pins = this->bus_setup_dummy<Addr::SP>(pins);
        return pins;

      case 3:
        /* PHI1: Increment stack pointer */
        this->inc(REG_S);
        this->cycle_index++;
        return pins;

      case 4:
        /* PHI2: Set up read for accumulator low byte from stack */
        pins = this->bus_setup_read<Addr::SP>(pins);
        return pins;

      case 5:
        /* PHI1: Load low byte from bus and increment SP */
        this->bus_load_reg(REG_AL, pins);
        this->inc(REG_S);
        this->cycle_index++;
        return pins;

      case 6:
        /* PHI2: Set up read for accumulator high byte from stack */
        pins = this->bus_setup_read<Addr::SP>(pins);
        return pins;

      case 7:
        /* PHI1: Load high byte from bus and update flags */
        this->bus_load_reg(REG_AH, pins);
        uint16_t value = this->get(REG_A_16);
        this->update_flag(FLAG_Z, value == 0);
        this->update_flag(FLAG_N, (value & 0x8000) != 0);
        this->this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit PLA operation (emulation mode and non-wide CPUs)
  switch (this->cycle_index) {
  case 0:
    /* PHI2: Dummy read from PC */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: No operation - just increment cycle */
    this->cycle_index++;
    return pins;

  case 2:
    /* PHI2: Dummy read from current stack pointer */
    pins = this->bus_setup_dummy<Addr::SP>(pins);
    return pins;

  case 3:
    /* PHI1: Increment stack pointer */
    this->inc(REG_S);
    this->cycle_index++;
    return pins;

  case 4:
    /* PHI2: Set up bus read from incremented stack pointer */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;

  case 5:
    /* PHI1: Load accumulator from bus and set flags */
    this->bus_load_reg(REG_A, pins);
    this->update_nz_flags(this->get(REG_A));
    this->this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* PLP - Pull Processor Status */
bus_state_t op_plp(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->cycle_index) {
  case 0:
    /* PHI2: Dummy read from PC */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: No operation - just increment cycle */
    this->cycle_index++;
    return pins;

  case 2:
    /* PHI2: Dummy read from current stack pointer */
    pins = this->bus_setup_dummy<Addr::SP>(pins);
    return pins;

  case 3:
    /* PHI1: Increment stack pointer */
    this->inc(REG_S);
    this->cycle_index++;
    return pins;

  case 4:
    /* PHI2: Set up bus read for status byte from stack */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;

  case 5:
    /* PHI1: Load status byte from bus into P (clear B, set U) */
    this->bus_load_reg(REG_DL, pins);
    this->set(REG_P, (this->get(REG_DL) & ~FLAG_B) | FLAG_U);
    this->this->transition_to_fetch();
    return pins;
  }
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
