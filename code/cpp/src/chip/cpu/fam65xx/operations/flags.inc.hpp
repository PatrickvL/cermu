/*
 * flags.inc.hpp - Flag Manipulation Operations for MOS 65xx Family
 *
 * This file contains flag manipulation operation implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// FLAG MANIPULATION OPERATIONS
// ============================================================================

/* CLC - Clear Carry Flag */
bus_state_t op_clc(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Dummy read from PC
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    // PHI1: Clear carry flag
    clear_flag(FLAG_C);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* SEC - Set Carry Flag */
bus_state_t op_sec(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Dummy read from PC
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    // PHI1: Set carry flag
    set_flag(FLAG_C);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* CLI - Clear Interrupt Disable Flag */
bus_state_t op_cli(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Dummy read from PC
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    // PHI1: Clear interrupt disable
    clear_flag(FLAG_I);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* SEI - Set Interrupt Disable Flag */
bus_state_t op_sei(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Dummy read from PC
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    // PHI1: Set interrupt disable
    set_flag(FLAG_I);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* CLD - Clear Decimal Mode Flag */
bus_state_t op_cld(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Dummy read from PC
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    // PHI1: Clear decimal mode
    clear_flag(FLAG_D);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* SED - Set Decimal Mode Flag */
bus_state_t op_sed(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Dummy read from PC
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    // PHI1: Set decimal mode
    set_flag(FLAG_D);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* CLV - Clear Overflow Flag */
bus_state_t op_clv(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Dummy read from PC
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    // PHI1: Clear overflow
    clear_flag(FLAG_V);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
