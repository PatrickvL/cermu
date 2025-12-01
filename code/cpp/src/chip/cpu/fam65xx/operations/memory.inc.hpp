/*
 * memory.inc.hpp - Memory Load/Store Operations for MOS 65xx Family
 *
 * This file contains load and store operation implementations that are
 * included within the fam65xx_t template class. These operations handle
 * data movement between registers and memory.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// LOAD ACCUMULATOR (LDA)
// ============================================================================

bus_state_t op_lda(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0: // PHI2 - Read low byte
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 1: // PHI1 - Load and increment
    // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
    // code
    this->bus_load_operand(REG_AL, pins);
    if constexpr (has_wide_registers()) {
      if (this->is_accumulator_16bit()) {
          this->half_cycle++;
          return pins;
      }
    }
    // Standard 8-bit LDA operation (emulation mode and non-wide CPUs)
    this->update_nz_flags(this->get(REG_A));
    this->transition_to_fetch();
    return pins;

  case 2: // PHI2 - Read high byte
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 3: // PHI1 - Load and update flags
    this->bus_load_operand(REG_AH, pins);
    uint16_t value = this->get(REG_A_16);
    this->update_flag(FLAG_Z, value == 0);
    this->update_flag(FLAG_N, (value & 0x8000) != 0);
    this->transition_to_fetch();
    return pins;
  }

  return pins;
}

// ============================================================================
// LOAD X REGISTER (LDX)
// ============================================================================

bus_state_t op_ldx(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0: // PHI2 - Read low byte
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 1: // PHI1 - Load and increment
    this->bus_load_operand(REG_XL, pins);
    // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
    // native code
    if constexpr (has_wide_registers()) {
      if (this->is_index_16bit()) {
        // 65C816 native mode, 16-bit X register - perform 16-bit LDX
        this->half_cycle++;
        return pins;
      }
    }
    // Standard 8-bit LDX operation (emulation mode and non-wide CPUs)
    this->update_nz_flags(this->get(REG_X));
    this->transition_to_fetch();
    return pins;

  case 2: // PHI2 - Read high byte
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 3: // PHI1 - Load and update flags
    this->bus_load_operand(REG_XH, pins);
    uint16_t value = this->get(REG_X_16);
    this->update_flag(FLAG_Z, value == 0);
    this->update_flag(FLAG_N, (value & 0x8000) != 0);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

// ============================================================================
// LOAD Y REGISTER (LDY)
// ============================================================================

bus_state_t op_ldy(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0: // PHI2 - Read low byte
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 1: // PHI1 - Load and increment
    this->bus_load_operand(REG_YL, pins);
    // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
    // native code
    if constexpr (has_wide_registers()) {
      if (this->is_index_16bit()) {
        // 65C816 native mode, 16-bit Y register - perform 16-bit LDY
        this->half_cycle++;
        return pins;
      }
    }
    // Standard 8-bit LDY operation (emulation mode and non-wide CPUs)
    this->update_nz_flags(this->get(REG_Y));
    this->transition_to_fetch();
    return pins;

  case 2: // PHI2 - Read high byte
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 3: // PHI1 - Load and update flags
    this->bus_load_operand(REG_YH, pins);
    uint16_t value = this->get(REG_Y_16);
    this->update_flag(FLAG_Z, value == 0);
    this->update_flag(FLAG_N, (value & 0x8000) != 0);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

// ============================================================================
// STORE ACCUMULATOR (STA)
// ============================================================================

bus_state_t op_sta(bus_state_t pins) {
  switch (this->half_cycle) {
    case 0:
      pins = this->bus_setup_write<Addr::AB>(pins, REG_AL);
      return pins;
    case 1:
      if (this->is_accumulator_16bit()) {
        this->inc(REG_AB);
      } else {
        this->transition_to_fetch();
      }
      return pins;

    case 2:
      pins = this->bus_setup_write<Addr::AB>(pins, REG_AH);
      return pins;
    case 3:
      this->transition_to_fetch();
      return pins;
  }
  return pins;
}

// Minimal STX: store X at resolved address
bus_state_t op_stx(bus_state_t pins) {
  switch (this->half_cycle) {
    case 0:
      pins = this->bus_setup_write<Addr::AB>(pins, REG_XL);
      return pins;
    case 1:
     if (this->is_index_16bit()) {
        this->inc(REG_AB);
     } else {
        this->transition_to_fetch();
     }
     return pins;

    case 2:
      pins = this->bus_setup_write<Addr::AB>(pins, REG_XH);
      return pins;
    case 3:
      this->transition_to_fetch();
      return pins;
  }
  return pins;
}

// ============================================================================
// STORE Y REGISTER (STY)
// ============================================================================

// Minimal STY: store Y at resolved address
bus_state_t op_sty(bus_state_t pins) {
    switch (this->half_cycle) {
        case 0:
            pins = this->bus_setup_write<Addr::AB>(pins, REG_YL);
            return pins;
        case 1:
            if (this->is_index_16bit()) {
              this->inc(REG_AB);
            } else {
                this->transition_to_fetch();
            }
            return pins;
        case 2:
            pins = this->bus_setup_write<Addr::AB>(pins, REG_YH);
            return pins;
        case 3:
            this->transition_to_fetch();
            return pins;
    }
    return pins;
}

// ============================================================================
// LOGIC OPERATIONS
// ============================================================================

// AND with Accumulator
bus_state_t op_and(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit AND
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 3: { // PHI1 - Load and perform 16-bit AND
        this->bus_load_operand(REG_ABH, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint16_t result = acc & operand;
        this->set(REG_A_16, result);
        this->update_flag(FLAG_Z, result == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      }
      return pins;
    }
  }

  // Standard 8-bit AND operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 1: { // PHI1
    uint8_t operand = this->bus_get_operand(pins);
    this->set(REG_A, this->get(REG_A) & operand);
    this->update_nz_flags(this->get(REG_A));
    this->transition_to_fetch();
    return pins;
  }
  }
  return pins;
}

// OR with Accumulator
bus_state_t op_ora(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit ORA
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 3: { // PHI1 - Load and perform 16-bit ORA
        this->bus_load_operand(REG_ABH, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint16_t result = acc | operand;
        this->set(REG_A_16, result);
        this->update_flag(FLAG_Z, result == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      }
      return pins;
    }
  }

  // Standard 8-bit ORA operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 1: { // PHI1
    uint8_t operand = this->bus_get_operand(pins);
    this->set(REG_A, this->get(REG_A) | operand);
    this->update_nz_flags(this->get(REG_A));
    this->transition_to_fetch();
    return pins;
  }
  }
  return pins;
}

// Exclusive OR with Accumulator
bus_state_t op_eor(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit EOR
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 3: // PHI1 - Load and perform 16-bit EOR
        this->bus_load_operand(REG_ABH, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint16_t result = acc ^ operand;
        this->set(REG_A_16, result);
        this->update_flag(FLAG_Z, result == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit EOR operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 1: { // PHI1
    uint8_t operand = this->bus_get_operand(pins);
    this->set(REG_A, this->get(REG_A) ^ operand);
    this->update_nz_flags(this->get(REG_A));
    this->transition_to_fetch();
    return pins;
  }
  }
  return pins;
}

// ============================================================================
// BIT TEST OPERATION
// ============================================================================

bus_state_t op_bit(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65816 native mode, 16-bit accumulator - read 2 bytes
      switch (this->half_cycle) {
      case 0: // PHI2 - Read low byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 1: // PHI1 - Load and increment
        this->bus_load_operand(REG_DL, pins);
        this->half_cycle++;
        return pins;

      case 2: // PHI2 - Read high byte
        pins = this->bus_setup_read_operand(pins);
        return pins;
      case 3: { // PHI1 - Load and perform 16-bit BIT
        this->bus_load_operand(REG_ABH, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint16_t result = acc & operand;
        uint8_t high_byte = this->get(REG_ABH);
        this->update_flag(FLAG_Z, result == 0); // Z = 1 if (A & operand) == 0
        this->update_flag(FLAG_V,
                          (high_byte & 0x40) != 0); // V = bit 6 of HIGH byte
        this->update_flag(FLAG_N,
                          (high_byte & 0x80) != 0); // N = bit 7 of HIGH byte
        this->transition_to_fetch();
        return pins;
      }
      }
      return pins;
    }
  }

  // Standard 8-bit BIT operation (emulation mode and non-wide CPUs)
  switch (this->half_cycle) {
  case 0: // PHI2
    pins = this->bus_setup_read_operand(pins);
    return pins;
  case 1: { // PHI1
    uint8_t operand = this->bus_get_operand(pins);
    uint8_t result = this->get(REG_A) & operand;
    // BIT immediate (65C02) only affects Z flag - N and V are NOT affected
    // BIT memory affects N, V, and Z flags normally
    if (this->opcode_entry.am_index == to_index(AM::IMM)) {
      // BIT immediate: only update Z flag
      this->update_flag(FLAG_Z, result == 0);
    } else {
      // BIT memory: update N, V, and Z flags
      // N = bit 7 of operand (copy bit 7 directly)
      // V = bit 6 of operand (copy bit 6 directly)
      // Z = result of A & operand (set if result is zero)
      // Extract N and V flags from operand in one operation (more efficient)
      uint8_t flags_from_operand = operand & (FLAG_N | FLAG_V);
      this->update_flags(FLAG_N | FLAG_V | FLAG_Z,
                         flags_from_operand | this->calc_z_flag(result));
    }
    // Complete instruction
    this->transition_to_fetch();
    return pins;
  }
  }
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
