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
    if (this->is_accumulator_16bit()) {
      // Native mode, 16-bit accumulator - perform 16-bit PHA
      switch (this->half_cycle) {
      case 0:
        /* Dummy cycle for internal operation */
        pins = this->bus_setup_dummy<Addr::PC>(pins);
        return pins;
      case 1:
        /* PHI1: Increment cycle */
        this->half_cycle++;
        return pins;

      case 2:
        /* PHI2: Write high byte of A to stack */
        pins = this->bus_setup_write<Addr::SP>(pins, REG_AH);
        return pins;
      case 3:
        this->dec(REG_S);
        this->half_cycle++;
        return pins;

      case 4:
        /* PHI2: Write low byte of A to stack */
        pins = this->bus_setup_write<Addr::SP>(pins, REG_AL);
        return pins;
      case 5:
        this->dec(REG_S);
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit PHA operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Dummy read from PC+1 */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: Increment cycle */
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Write A to stack */
    pins = this->bus_setup_write<Addr::SP>(pins, REG_A);
    return pins;
  case 3:
    /* PHI1: Decrement SP and transition */
    this->dec(REG_S);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* PHP - Push Processor Status */
bus_state_t op_php(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Dummy read from PC+1 */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: Prepare status byte and increment cycle */
    this->set(REG_DL, this->get(REG_P) | FLAG_B | FLAG_U);
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Write P|B|U to stack */
    pins = this->bus_setup_write<Addr::SP>(pins, REG_DL);
    return pins;
  case 3:
    /* PHI1: Decrement SP and transition */
    this->dec(REG_S);
    this->transition_to_fetch();
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
    if (this->is_accumulator_16bit()) {
      // Native mode, 16-bit accumulator - perform 16-bit PLA
      switch (this->half_cycle) {
      case 0:
        /* PHI2: Dummy read from PC */
        pins = this->bus_setup_dummy<Addr::PC>(pins);
        return pins;
      case 1:
        /* PHI1: No operation - just increment cycle */
        this->half_cycle++;
        return pins;

      case 2:
        /* PHI2: Dummy read from current stack pointer */
        pins = this->bus_setup_dummy<Addr::SP>(pins);
        return pins;
      case 3:
        /* PHI1: Increment stack pointer */
        this->inc(REG_S);
        this->half_cycle++;
        return pins;

      case 4:
        /* PHI2: Set up read for accumulator low byte from stack */
        pins = this->bus_setup_read<Addr::SP>(pins);
        return pins;
      case 5:
        /* PHI1: Load low byte from bus and increment SP */
        this->bus_load_reg(REG_AL, pins);
        this->inc(REG_S);
        this->half_cycle++;
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
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit PLA operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Dummy read from PC */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: No operation - just increment cycle */
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Dummy read from current stack pointer */
    pins = this->bus_setup_dummy<Addr::SP>(pins);
    return pins;
  case 3:
    /* PHI1: Increment stack pointer */
    this->inc(REG_S);
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Set up bus read from incremented stack pointer */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 5:
    /* PHI1: Load accumulator from bus and set flags */
    this->bus_load_reg(REG_A, pins);
    this->update_nz_flags(this->get(REG_A));
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* PLP - Pull Processor Status */
bus_state_t op_plp(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Dummy read from PC */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: No operation - just increment cycle */
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Dummy read from current stack pointer */
    pins = this->bus_setup_dummy<Addr::SP>(pins);
    return pins;
  case 3:
    /* PHI1: Increment stack pointer */
    this->inc(REG_S);
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Set up bus read for status byte from stack */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 5:
    /* PHI1: Load status byte from bus into P
     * 6502/65C02: Bit 5 (U) always 1, bit 4 (B) is NOT a real flag - mask it off
     * 65C816 emulation: Set both bits 4 and 5 (B and U flags always 1)
     * 65C816 native: Load all bits as-is (bits 4 and 5 have different meanings: X and M)
     */
    this->bus_load_reg(REG_DL, pins);
    if constexpr (has_wide_registers()) {
      if (this->in_emulation_mode()) {
        // 65C816 emulation mode: set both FLAG_B (bit 4) and FLAG_U (bit 5)
        this->set(REG_P, this->get(REG_DL) | FLAG_B | FLAG_U);
      } else {
        // Native mode: load value as-is (B becomes X flag, U becomes M flag)
        this->set(REG_P, this->get(REG_DL));
      }
    } else {
      // 6502/6510/65C02: Mask off bit 4 (B is phantom), set bit 5 (U always 1)
      this->set(REG_P, (this->get(REG_DL) & ~FLAG_B) | FLAG_U);
    }
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
