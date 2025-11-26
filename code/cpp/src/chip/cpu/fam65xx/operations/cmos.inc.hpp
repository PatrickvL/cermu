/*
 * cmos.inc.hpp - 65C02 Enhanced Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// 65C02 ENHANCED INSTRUCTIONS
// ============================================================================

// BRA - Branch Always (65C02)
bus_state_t op_bra(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    // Read relative offset
    pins = this->/*TODO_READ*/ phi2_read<Addr::PC>(pins, REG_DL);
    
    this->inc(REG_PC);

    // Apply branch offset
    int8_t offset = static_cast<int8_t>(this->get(REG_DL));
    this->set(REG_PC, this->get(REG_PC) + offset);

    transition_to_fetch();
  }
  return pins;
}

// STZ - Store Zero (65C02)
bus_state_t op_stz(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    // Store zero to target address with proper RDY handling
    
    pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB>(pins, 0x00);
    transition_to_fetch();
  }
  return pins;
}

// TRB - Test and Reset Bits (65C02) - Hardware-accurate 3-cycle RMW
bus_state_t op_trb(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    // Hardware-accurate 3-cycle Read-Modify-Write operation
    switch (this->cycle_index) {
    case 0:
      // Cycle 0: Read original value from memory
      pins = this->/*TODO_READ*/ phi2_read<Addr::AB>(pins, REG_DL);
      
      this->cycle_index++;
      return pins;

    case 1:
      // Cycle 1: Dummy write original value back + modify
      
      pins =
          this->/*TODO_WRITE*/ phi2_write<Addr::AB>(pins, this->get(REG_DL));

      uint8_t accumulator = this->get(REG_A);

      // Test bits (set Z flag if A & memory == 0)
      uint8_t test_result = this->get(REG_DL) & accumulator;
      update_flag(FLAG_Z, test_result == 0);

      // Reset bits (memory = memory & ~A)
      this->set(REG_DL, this->get(REG_DL) & ~accumulator);

      this->cycle_index++;
      return pins;

    case 2:
      // Cycle 2: Write modified result back
      
      pins =
          this->/*TODO_WRITE*/ phi2_write<Addr::AB>(pins, this->get(REG_DL));
      transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

// TSB - Test and Set Bits (65C02) - Hardware-accurate 3-cycle RMW
bus_state_t op_tsb(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    // Hardware-accurate 3-cycle Read-Modify-Write operation
    switch (this->cycle_index) {
    case 0:
      // Cycle 0: Read original value from memory
      pins = this->/*TODO_READ*/ phi2_read<Addr::AB>(pins, REG_DL);
      
      this->cycle_index++;
      return pins;

    case 1:
      // Cycle 1: Dummy write original value back + modify
      
      pins =
          this->/*TODO_WRITE*/ phi2_write<Addr::AB>(pins, this->get(REG_DL));

      uint8_t accumulator = this->get(REG_A);

      // Test bits (set Z flag if A & memory == 0)
      uint8_t test_result = this->get(REG_DL) & accumulator;
      update_flag(FLAG_Z, test_result == 0);

      // Set bits (memory = memory | A)
      this->set(REG_DL, this->get(REG_DL) | accumulator);

      this->cycle_index++;
      return pins;

    case 2:
      // Cycle 2: Write modified result back
      
      pins =
          this->/*TODO_WRITE*/ phi2_write<Addr::AB>(pins, this->get(REG_DL));
      transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

// WAI - Wait for Interrupt (65C02)
bus_state_t op_wai(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    // Set wait state
    this->wait_for_interrupt = true;

    // CPU halts until interrupt occurs
    // The tick() function will check this flag
    transition_to_fetch();
  }
  return pins;
}

// STP - Stop (65C02) - 2-byte instruction that reads immediate byte before
// stopping
bus_state_t op_stp(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    // Read immediate byte (required for 2-byte instruction)
    pins = this->/*TODO_READ*/ phi2_read<Addr::PC>(pins, REG_DL);
    
    this->inc(REG_PC);

    // Set stopped state after reading immediate byte
    this->stopped = true;

    // CPU halts until reset
    transition_to_fetch();
  }
  return pins;
}

// PHX - Push X Register (65C02) - Hardware-accurate 3-cycle operation like PHA
bus_state_t op_phx(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    switch (this->cycle_index) {
    case 0:
      /* Dummy cycle for internal operation */
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      
      this->cycle_index++;
      return pins;

    case 1:
      /* PHI2: Write X to stack with processor-specific RDY handling */
      
      pins =
          this->/*TODO_WRITE*/ phi2_write<Addr::SP>(pins, this->get(REG_X));
      this->dec(REG_S);
      transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

// PHY - Push Y Register (65C02) - Hardware-accurate 3-cycle operation like PHA
bus_state_t op_phy(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    switch (this->cycle_index) {
    case 0:
      /* Dummy cycle for internal operation */
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      
      this->cycle_index++;
      return pins;

    case 1:
      /* PHI2: Write Y to stack with processor-specific RDY handling */
      
      pins =
          this->/*TODO_WRITE*/ phi2_write<Addr::SP>(pins, this->get(REG_Y));
      this->dec(REG_S);
      transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

// PLX - Pull X Register (65C02) - Hardware-accurate 4-cycle operation like PLA
bus_state_t op_plx(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    switch (this->cycle_index) {
    case 0:
      /* PHI2: Dummy read from PC */
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      
      this->cycle_index++;
      return pins;

    case 1:
      /* PHI2: Dummy read from current stack pointer, then increment SP */
      pins = this->bus_setup_dummy<Addr::SP>(pins);
      
      /* PHI1: Increment stack pointer */
      this->inc(REG_S);
      this->cycle_index++;
      return pins;

    case 2:
      /* PHI2: Read from incremented stack pointer directly into X (eliminates
       * copy) */
      pins = this->/*TODO_READ*/ phi2_read<Addr::SP>(pins, REG_X);
      
      /* PHI1: Set flags based on X register value */
      update_nz_flags(this->get(REG_X));
      transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

// PLY - Pull Y Register (65C02) - Hardware-accurate 4-cycle operation like PLA
bus_state_t op_ply(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_cmos()) {
    switch (this->cycle_index) {
    case 0:
      /* PHI2: Dummy read from PC */
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      
      this->cycle_index++;
      return pins;

    case 1:
      /* PHI2: Dummy read from current stack pointer, then increment SP */
      pins = this->bus_setup_dummy<Addr::SP>(pins);
      
      /* PHI1: Increment stack pointer */
      this->inc(REG_S);
      this->cycle_index++;
      return pins;

    case 2:
      /* PHI2: Read from incremented stack pointer directly into Y (eliminates
       * copy) */
      pins = this->/*TODO_READ*/ phi2_read<Addr::SP>(pins, REG_Y);
      
      /* PHI1: Set flags based on Y register value */
      update_nz_flags(this->get(REG_Y));
      transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
