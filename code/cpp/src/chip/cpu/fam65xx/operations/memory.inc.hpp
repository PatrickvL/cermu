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
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (this->has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit LDA
      switch (this->cycle_index) {
      case 0:
        // Read low byte of operand
        pins = this->DEPRECATED_phi2_read_operand(pins, REG_AL);

        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        pins = this->bus_setup_read<Addr::AB>(pins);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
        uint16_t value = this->get(REG_A_16);
        this->update_flag(FLAG_Z, value == 0);
        this->update_flag(FLAG_N, (value & 0x8000) != 0);
        this->transition_to_fetch();
      }
      return pins;
    }
  }

  // Standard 8-bit LDA operation (emulation mode and non-wide CPUs)
  pins = this->DEPRECATED_phi2_read_operand(pins, REG_A);

  // Update N and Z flags
  this->update_nz_flags(this->get(REG_A));

  // Complete instruction
  this->transition_to_fetch();
  return pins;
}

// ============================================================================
// LOAD X REGISTER (LDX)
// ============================================================================

bus_state_t op_ldx(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
  // native code
  if constexpr (this->has_wide_registers()) {
    if (this->is_index_16bit()) {
      // 65C816 native mode, 16-bit X register - perform 16-bit LDX
      switch (this->cycle_index) {
      case 0:
        // Read low byte of operand
        pins = this->DEPRECATED_phi2_read_operand(pins, REG_XL);

        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        pins = this->bus_setup_read<Addr::AB>(pins);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
        uint16_t value = this->get(REG_X_16);
        this->update_flag(FLAG_Z, value == 0);
        this->update_flag(FLAG_N, (value & 0x8000) != 0);
        this->transition_to_fetch();
      }
      return pins;
    }
  }

  // Standard 8-bit LDX operation (emulation mode and non-wide CPUs)
  pins = this->DEPRECATED_phi2_read_operand(pins, REG_X);

  // Update N and Z flags
  this->update_nz_flags(this->get(REG_X));

  // Complete instruction
  this->transition_to_fetch();
  return pins;
}

// ============================================================================
// LOAD Y REGISTER (LDY)
// ============================================================================

bus_state_t op_ldy(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
  // native code
  if constexpr (this->has_wide_registers()) {
    if (this->is_index_16bit()) {
      // 65C816 native mode, 16-bit Y register - perform 16-bit LDY
      switch (this->cycle_index) {
      case 0:
        // Read low byte of operand
        pins = this->DEPRECATED_phi2_read_operand(pins, REG_YL);

        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        pins = this->bus_setup_read<Addr::AB>(pins);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
        uint16_t value = this->get(REG_Y_16);
        this->update_flag(FLAG_Z, value == 0);
        this->update_flag(FLAG_N, (value & 0x8000) != 0);
        this->transition_to_fetch();
      }
      return pins;
    }
  }

  // Standard 8-bit LDY operation (emulation mode and non-wide CPUs)
  pins = this->DEPRECATED_phi2_read_operand(pins, REG_Y);

  // Update N and Z flags
  this->update_nz_flags(this->get(REG_Y));

  // Complete instruction
  this->transition_to_fetch();
  return pins;
}

// ============================================================================
// STORE ACCUMULATOR (STA)
// ============================================================================

bus_state_t op_sta(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (this->has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit STA
      switch (this->cycle_index) {
      case 0:
        // Store low byte of accumulator

        pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_AL));
        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        // Store high byte of accumulator

        pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_AH));
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit STA operation (emulation mode and non-wide CPUs)
  pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_A));
  this->transition_to_fetch();
  return pins;
}

// ============================================================================
// STORE X REGISTER (STX)
// ============================================================================

bus_state_t op_stx(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
  // native code
  if constexpr (this->has_wide_registers()) {
    if (this->is_index_16bit()) {
      // 65C816 native mode, 16-bit X register - perform 16-bit STX
      switch (this->cycle_index) {
      case 0:
        // Store low byte of X register

        pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_XL));
        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        // Store high byte of X register

        pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_XH));
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit STX operation (emulation mode and non-wide CPUs)
  pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_X));
  this->transition_to_fetch();
  return pins;
}

// ============================================================================
// STORE Y REGISTER (STY)
// ============================================================================

bus_state_t op_sty(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit index registers (X=0) - nested
  // native code
  if constexpr (this->has_wide_registers()) {
    if (this->is_index_16bit()) {
      // 65C816 native mode, 16-bit Y register - perform 16-bit STY
      switch (this->cycle_index) {
      case 0:
        // Store low byte of Y register

        pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_YL));
        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        // Store high byte of Y register

        pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_YH));
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }

  // Standard 8-bit STY operation (emulation mode and non-wide CPUs)
  pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_Y));
  this->transition_to_fetch();
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
  if constexpr (this->has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit AND
      switch (this->cycle_index) {
      case 0:
        // Read low byte of operand
        pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        pins = this->bus_setup_read<Addr::AB>(pins);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint16_t result = acc & operand;
        this->set(REG_A_16, result);
        this->update_flag(FLAG_Z, result == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
      }
      return pins;
    }
  }

  // Standard 8-bit AND operation (emulation mode and non-wide CPUs)
  pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

  // Perform AND operation
  this->set(REG_A, this->get(REG_A) & this->get(REG_DL));

  // Update N and Z flags
  this->update_nz_flags(this->get(REG_A));

  // Complete instruction
  this->transition_to_fetch();
  return pins;
}

// OR with Accumulator
bus_state_t op_ora(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (this->has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit ORA
      switch (this->cycle_index) {
      case 0:
        // Read low byte of operand
        pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        pins = this->bus_setup_read<Addr::AB, Bank::PBR>(pins);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_AB);
        uint16_t acc = this->get(REG_A_16);
        uint16_t result = acc | operand;
        this->set(REG_A_16, result);
        this->update_flag(FLAG_Z, result == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
      }
      return pins;
    }
  }

  // Standard 8-bit ORA operation (emulation mode and non-wide CPUs)
  pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

  // Perform OR operation
  this->set(REG_A, this->get(REG_A) | this->get(REG_DL));

  // Update N and Z flags
  this->update_nz_flags(this->get(REG_A));

  // Complete instruction
  this->transition_to_fetch();
  return pins;
}

// Exclusive OR with Accumulator
bus_state_t op_eor(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65C816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (this->has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65C816 native mode, 16-bit accumulator - perform 16-bit EOR
      switch (this->cycle_index) {
      case 0:
        // Read low byte of operand
        pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

        // For 65C816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        pins = this->bus_setup_read<Addr::AB>(pins);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
        this->set(REG_ABL, this->get(REG_DL));
        uint16_t operand = this->get(REG_ABL);
        uint16_t acc = this->get(REG_A_16);
        uint16_t result = acc ^ operand;
        this->set(REG_A_16, result);
        this->update_flag(FLAG_Z, result == 0);
        this->update_flag(FLAG_N, (result & 0x8000) != 0);
        this->transition_to_fetch();
      }
      return pins;
    }
  }

  // Standard 8-bit EOR operation (emulation mode and non-wide CPUs)
  pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

  // Perform EOR operation
  this->set(REG_A, this->get(REG_A) ^ this->get(REG_DL));

  // Update N and Z flags
  this->update_nz_flags(this->get(REG_A));

  // Complete instruction
  this->transition_to_fetch();
  return pins;
}

// ============================================================================
// BIT TEST OPERATION
// ============================================================================

bus_state_t op_bit(bus_state_t pins) {
  trace_operation(__func__);
  // Check for 65816 native mode with 16-bit accumulator (M=0) - nested native
  // code
  if constexpr (this->has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      // 65816 native mode, 16-bit accumulator - read 2 bytes
      switch (this->cycle_index) {
      case 0:
        // Read low byte of operand
        pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

        // For 65816 native mode, increment address bus with bank handling
        this->inc(REG_AB);
        return pins;

      case 1:
        pins = this->bus_setup_read<Addr::AB>(pins);
        this->cycle_index++;
        return pins;

      case 2:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
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
      }
      return pins;
    }
  }

  // Standard 8-bit BIT operation (emulation mode and non-wide CPUs)
  pins = this->DEPRECATED_phi2_read_operand(pins, REG_DL);

  uint8_t operand = this->get(REG_DL);
  uint8_t result = this->get(REG_A) & operand;

  // BIT immediate (65C02) only affects Z flag - N and V are NOT affected
  // BIT memory affects N, V, and Z flags normally
  if (this->opcode_entry.am_index == to_index(AM::IMM)) {
    // BIT immediate: only update Z flag
    this->update_flag(FLAG_Z, result == 0);
    else {
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
  }
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
