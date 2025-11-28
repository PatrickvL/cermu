/*
 * arithmetic.inc.hpp - Arithmetic Operations for MOS 65xx Family
 *
 * This file contains arithmetic operation implementations (ADC, SBC, CMP, etc.)
 * that are included within the fam65xx_t template class. These operations
 * use the new unified helper functions for optimized performance.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// ADD WITH CARRY (ADC)
// ============================================================================

bus_state_t op_adc(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit ADC
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->inc(REG_AB);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 3: // PHI1 - Load and perform 16-bit ADC
        this->bus_load_operand(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint32_t result = acc + operand + ((this->get(REG_P) & FLAG_C) ? 1 : 0);
        this->set(REG_A_16, result & 0xFFFF);
        this->update_flag(FLAG_C, result > 0xFFFF);
        this->update_flag(FLAG_Z, (result & 0xFFFF) == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->update_flag(FLAG_V,
                          ((acc ^ result) & (operand ^ result) & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit ADC operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    // Setup read from AB (or PC for immediate mode)
    pins = this->bus_setup_read_operand(pins);
    return pins;

  case 1: { // PHI1
    // Load operand and perform ADC
    this->bus_load_operand(REG_DL, pins);
    uint8_t operand = this->get(REG_DL);
    this->perform_adc(operand);

    // CMOS processors need extra cycle in decimal mode
    if constexpr (has_bcd_extra_cycle()) {
      if (this->get(REG_P) & FLAG_D) {
        this->half_cycle++;
        return pins;
      }
      // Complete instruction if no extra cycle needed
      this->transition_to_fetch();
      return pins;
    }
    this->transition_to_fetch();
    return pins;
  }

  case 2: // PHI2 - Extra cycle for CMOS decimal mode
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;

  case 3: // PHI1
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

// ============================================================================
// NO OPERATION (NOP)
// ============================================================================

bus_state_t op_nop(bus_state_t pins) {
  trace_operation(__func__);
  // CRITICAL FIX: 65C816 emulation mode compatibility
  // In emulation mode, 65C816 should behave exactly like 6502, not CMOS
  // This means NO RMW operations should be performed for NOP in emulation mode

  // Check if we should use CMOS RMW behavior
  bool use_cmos_rmw = false;

  // SECONDARY: WDC65C02 neutralized illegal opcodes preserve original
  // addressing timing This code should ONLY execute for non-65C816 CMOS
  // processors or 65C816 in native mode
  if constexpr (has_cmos()) {
    // Check if we're NOT in 65C816 emulation mode
    bool not_in_emulation = true;
    if constexpr (has_wide_registers()) {
      not_in_emulation = !this->in_emulation_mode();
    }

    // Only proceed with RMW if we have RMW flags AND we're not in 65C816
    // emulation mode
    if (not_in_emulation && this->opcode_entry.is_rmw()) {
      use_cmos_rmw = true;
    }
  }

  // Handle CMOS RMW NOP if applicable
  if (use_cmos_rmw) {
    // RMW mode NOP: Perform full read-modify-write cycle but don't modify the
    // value This preserves the bus cycle timing for WDC65C02 neutralized
    // illegal opcodes
    return this->rmw_operation_helper(pins, [this](data_t &value) {
      // NOP operation: read the value but don't modify it
      // This creates the correct bus cycle pattern for WDC65C02 illegal opcodes
      (void)value; // Suppress unused parameter warning
                   // No operation performed - value remains unchanged
    });
  }

  // Regular NOP handling for non-RMW modes using PHI2/PHI1 split
  switch (this->half_cycle) {
  case 0: // PHI2: Bus setup - dummy cycle
    // Set up bus based on addressing mode
    switch (this->opcode_entry.am_index) {
    case to_index(AM::IMM):
      // Immediate: read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      break;
    case to_index(AM::NON):
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      break;
    default:
      // Memory modes: dummy read from AB (address already set up by AM handler)
      pins = this->bus_setup_dummy<Addr::AB>(pins);
      break;
    }
    return pins;
  case 1: // PHI1
    // Increment PC for immediate mode only
    if (this->opcode_entry.am_index == to_index(AM::IMM)) {
      this->inc(REG_PC);
    }
    this->half_cycle++;
    this->transition_to_fetch();
    return pins;
  }

  return pins;
}

// ============================================================================
// SUBTRACT WITH CARRY (SBC)
// ============================================================================

bus_state_t op_sbc(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit SBC
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->inc(REG_AB);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 3: // PHI1 - Load and perform 16-bit SBC
        this->bus_load_operand(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint32_t result = acc - operand - ((this->get(REG_P) & FLAG_C) ? 0 : 1);
        this->set(REG_A_16, result & 0xFFFF);
        this->update_flag(FLAG_C, result <= 0xFFFF);
        this->update_flag(FLAG_Z, (result & 0xFFFF) == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->update_flag(FLAG_V,
                          ((acc ^ operand) & (acc ^ result) & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit SBC operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;

  case 1: { // PHI1
    this->bus_load_operand(REG_DL, pins);
    uint8_t operand = this->get(REG_DL);
    this->perform_sbc(operand);

    // CMOS processors need extra cycle in decimal mode
    if constexpr (has_bcd_extra_cycle()) {
      if (this->get(REG_P) & FLAG_D) {
        this->half_cycle++;
        return pins;
      }
      this->transition_to_fetch();
      return pins;
    }
    this->transition_to_fetch();
    return pins;
  }

  case 2: // PHI2 - Extra cycle for CMOS decimal mode
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;

  case 3: // PHI1
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

// ============================================================================
// COMPARE ACCUMULATOR (CMP)
// ============================================================================

bus_state_t op_cmp(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit CMP
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->inc(REG_AB);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 3: // PHI1 - Load and perform 16-bit CMP
        this->bus_load_operand(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint32_t result = acc - operand;
        this->update_flag(FLAG_C, acc >= operand);
        this->update_flag(FLAG_Z, acc == operand);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit CMP operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;

  case 1: { // PHI1
    this->bus_load_operand(REG_DL, pins);
    uint8_t operand = this->get(REG_DL);
    uint8_t a = this->get(REG_A);
    this->perform_compare(a, operand);
    this->transition_to_fetch();
    return pins;
  }
  }
  return pins;
}

// ============================================================================
// COMPARE X REGISTER (CPX)
// ============================================================================

bus_state_t op_cpx(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
  // native code
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      // 65C816 native mode, 16-bit X register - perform 16-bit CPX
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->inc(REG_AB);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 3: // PHI1 - Load and perform 16-bit CPX
        this->bus_load_operand(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t x = this->get_x_register();
        uint32_t result = x - operand;
        this->update_flag(FLAG_C, x >= operand);
        this->update_flag(FLAG_Z, x == operand);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit CPX operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;

  case 1: { // PHI1
    this->bus_load_operand(REG_DL, pins);
    uint8_t operand = this->get(REG_DL);
    uint8_t x = this->get(REG_X);
    this->perform_compare(x, operand);
    this->transition_to_fetch();
    return pins;
  }
  }
  return pins;
}

// ============================================================================
// COMPARE Y REGISTER (CPY)
// ============================================================================

bus_state_t op_cpy(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
  // native code
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      // 65C816 native mode, 16-bit Y register - perform 16-bit CPY
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->inc(REG_AB);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 3: // PHI1 - Load and perform 16-bit CPY
        this->bus_load_operand(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t y = this->get_y_register();
        uint32_t result = y - operand;
        this->update_flag(FLAG_C, y >= operand);
        this->update_flag(FLAG_Z, y == operand);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit CPY operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;

  case 1: { // PHI1
    this->bus_load_operand(REG_DL, pins);
    uint8_t operand = this->get(REG_DL);
    uint8_t y = this->get(REG_Y);
    this->perform_compare(y, operand);
    this->transition_to_fetch();
    return pins;
  }
  }
  return pins;
}

// ============================================================================
// INCREMENT MEMORY (INC)
// ============================================================================

bus_state_t op_inc(bus_state_t pins) {
  trace_operation(__func__);
  // INC - Increment memory by 1
  // This is a Read-Modify-Write operation
  return this->rmw_operation_helper(pins, [this](data_t &value) {
    // Increment the value
    value++;
    // Update N and Z flags using consolidated helper
    this->update_nz_flags<REG_A>(value);
  });
}

// ============================================================================
// DECREMENT MEMORY (DEC)
// ============================================================================

bus_state_t op_dec(bus_state_t pins) {
  trace_operation(__func__);
  // DEC - Decrement memory by 1
  // This is a Read-Modify-Write operation
  return this->rmw_operation_helper(pins, [this](data_t &value) {
    // Decrement the value
    value--;
    // Update N and Z flags using consolidated helper
    this->update_nz_flags<REG_A>(value);
  });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
