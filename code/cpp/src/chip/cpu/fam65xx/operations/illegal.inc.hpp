/*
 * illegal.inc.hpp - Illegal/Undocumented Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// ILLEGAL LOAD/STORE COMBINATIONS
// ============================================================================

bus_state_t op_lax(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // LAX - Load A and X from memory
    // Special case: LAX immediate has unstable behavior - uses (A | 0xEE) &
    // operand
    if (this->opcode_entry.am_index == to_index(AM::IMM)) {
      // LAX immediate - unstable behavior with magic constant
      switch (this->cycle_index) {
      case 0:
        // PHI2: Setup read from PC
        pins = this->bus_setup_read<Addr::PC>(pins);
        return pins;

      case 1:
        // PHI1: Load data, increment PC, and perform operation
        this->bus_load_reg(REG_DL, pins);
        this->inc(REG_PC);
        // Hardware quirk: LAX immediate uses unstable internal state
        // Result is (A | 0xEE) & operand
        uint8_t result = (this->get(REG_A) | 0xEE) & this->get(REG_DL);
        this->set(REG_A, result);
        this->set(REG_X, result);

        this->update_nz_flags<REG_A>(result);
        this->transition_to_fetch();
        return pins;
      }
    } else {
      // LAX memory modes - normal behavior
      switch (this->cycle_index) {
      case 0:
        // PHI2: Setup read from AB
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;

      case 1:
        // PHI1: Load data and perform operation
        this->bus_load_reg(REG_DL, pins);
        this->set(REG_A, this->get(REG_DL));
        this->set(REG_X, this->get(REG_DL));

        this->update_nz_flags<REG_A>(this->get(REG_A));
        this->transition_to_fetch();
        return pins;
      }
    }
  }

  return pins;
}

bus_state_t op_sax(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // Store A AND X to memory with processor-specific RDY handling

    uint8_t result = this->get(REG_A) & this->get(REG_X);
    pins = this->bus_setup_write<Addr::AB>(pins, result);
    this->transition_to_fetch();
  }
  return pins;
}

// ============================================================================
// ILLEGAL READ-MODIFY-WRITE OPERATIONS
// ============================================================================

bus_state_t op_dcp(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // DCP - Decrement memory and compare with A (DEC memory, then CMP A with
    // result) This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](data_t &value) {
      // Perform DEC on memory value
      value--;

      // Perform CMP A with decremented value
      uint16_t result = this->get(REG_A) - static_cast<uint8_t>(value);
      // Set carry flag (CMP uses subtraction semantics: carry = no borrow)
      this->update_flag(FLAG_C, !(result & 0x100));

      // Update N and Z flags based on comparison result
      this->update_nz_flags<REG_A>(static_cast<uint8_t>(result));
    });
  } else {
    return pins;
  }
}

bus_state_t op_isc(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // ISC - Increment memory and subtract from A (INC memory, then SBC A with
    // result) This is a Read-Modify-Write operation - match reference
    // implementation exactly
    return rmw_operation_helper(pins, [this](data_t &value) {
      // Perform INC on memory value
      value++;

      // Perform SBC A with incremented value using BCD-aware function
      // This ensures proper NMOS 6502 BCD behavior including V flag calculation
      perform_sbc(static_cast<uint8_t>(value));
    });
  } else {
    return pins;
  }
}

bus_state_t op_slo(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SLO - Shift Left and OR with A (ASL memory, then ORA A with result)
    // This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](data_t &value) {
      // Perform ASL on memory value
      this->update_flag(FLAG_C, value & 0x80);
      value <<= 1;

      // Perform ORA with accumulator
      this->set(REG_A, this->get(REG_A) | static_cast<uint8_t>(value));
      this->update_nz_flags<REG_A>(this->get(REG_A));
    });
  } else {
    return pins;
  }
}

bus_state_t op_rla(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // RLA - Rotate Left and AND with A (ROL memory, then AND A with result)
    // This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](data_t &value) {
      // Perform ROL on memory value
      uint8_t carry_in = this->get(REG_P) & FLAG_C;
      this->update_flag(FLAG_C, value & 0x80);
      value = (value << 1) | carry_in;

      // Perform AND with accumulator
      this->set(REG_A, this->get(REG_A) & static_cast<uint8_t>(value));
      this->update_nz_flags<REG_A>(this->get(REG_A));
    });
  } else {
    return pins;
  }
}

bus_state_t op_sre(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SRE - Shift Right and EOR with A (LSR memory, then EOR result with A)
    // This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](data_t &value) {
      // Perform LSR on memory value
      this->update_flag(FLAG_C, value & FLAG_C);
      value >>= 1;

      // Perform EOR with accumulator
      this->set(REG_A, this->get(REG_A) ^ static_cast<uint8_t>(value));
      this->update_nz_flags<REG_A>(this->get(REG_A));
    });
  } else {
    return pins;
  }
}

bus_state_t op_rra(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // RRA - Rotate Right and ADC with A (ROR memory, then ADC A with result)
    // This is a Read-Modify-Write operation - match reference implementation
    // exactly
    return rmw_operation_helper(pins, [this](data_t &value) {
      // Perform ROR on memory value - exact reference match
      const uint8_t carry_in = this->get(REG_P) & FLAG_C;
      const uint8_t carry_out = static_cast<uint8_t>(value) & FLAG_C;
      value = (value >> 1) | (carry_in << 7);

      this->update_flag(FLAG_C, carry_out);

      // Perform ADC with A using BCD-aware function
      // This ensures proper NMOS 6502 BCD behavior including V flag calculation
      perform_adc(static_cast<uint8_t>(value));
    });
  } else {
    return pins;
  }
}

// ============================================================================
// PROCESSOR JAM/KILL OPERATION
// ============================================================================

bus_state_t op_jam(bus_state_t pins) {
  trace_operation(__func__);
  // JAM/KIL instruction behavior on 6502:
  // - PC advances to read operand, then resets to opcode address
  // - Performs 3-cycle pattern: opcode read, operand read, operand read
  // - For test compatibility: complete after 3 cycles with PC at opcode address

  switch (this->cycle_index) {
  case 0:
    // PHI2: Read operand from PC+1 (this was PC++ after opcode fetch)
    pins = this->bus_setup_read<Addr::PC>(pins);

    this->cycle_index++;
    return pins;

  case 1:
    pins = this->bus_setup_read<Addr::PC>(pins);
    this->cycle_index++;
    return pins;

  case 2:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABL, pins);
    this->dec(REG_PC); // Go back to opcode address
    this->transition_to_fetch();
  }
  return pins;
}

// ============================================================================
// ILLEGAL ACCUMULATOR OPERATIONS
// ============================================================================

bus_state_t op_anc(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // ANC - AND with carry (AND immediate, then copy N flag to C flag)
    switch (this->cycle_index) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      // PHI1: Load data, increment PC, and perform operation
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      // Perform AND with accumulator
      this->set(REG_A, this->get(REG_A) & this->get(REG_DL));

      // Update N and Z flags
      this->update_nz_flags<REG_A>(this->get(REG_A));

      // Copy N flag to C flag (ANC behavior)
      this->update_flag(FLAG_C, this->get(REG_P) & FLAG_N);
      this->transition_to_fetch();
      return pins;
    }
  }

  return pins;
}

bus_state_t op_arr(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // ARR - AND + ROR with BCD correction in decimal mode (reference
    // implementation)
    switch (this->cycle_index) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      // PHI1: Load data, increment PC, and perform operation
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);

      uint8_t operand = this->get(REG_DL);
      bool carry_in = (this->get(REG_P) & FLAG_C) != 0;

      // Step 1: AND A with operand - save original for BCD checks
      uint8_t original_a = this->get(REG_A) & operand;
      this->set(REG_A, original_a);

      // Step 2: ROR the result (both modes do this)
      uint8_t shifted_a = (this->get(REG_A) >> 1) | (carry_in ? 0x80 : 0);

      // Clear all flags initially
      this->set(REG_P, this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_V | FLAG_C));

      // Set N and Z flags based on shifted result (reference does this first)
      this->update_nz_flags<REG_A>(shifted_a);

      if constexpr (has_bcd()) {
        if (this->get(REG_P) & FLAG_D) {
          // Decimal mode - ARR uses reference algorithm

          // Set V flag based on bit 6 change between original and shifted
          if ((shifted_a ^ this->get(REG_A)) & 0x40) {
            this->set(REG_P, this->get(REG_P) | FLAG_V);

            // BCD correction using ORIGINAL A value for digit checks (reference
            // approach)
            uint8_t result = shifted_a;

            // Low nibble BCD correction - use ORIGINAL A value for threshold
            // check
            if ((this->get(REG_A) & 0x0F) >= 5) {
              result = ((result + 6) & 0x0F) | (result & 0xF0);
            }

            // High nibble BCD correction and carry - use ORIGINAL A value for
            // threshold check
            if ((this->get(REG_A) & 0xF0) >= 0x50) {
              result += 0x60;
              this->set(REG_P, this->get(REG_P) | FLAG_C);
            }

            this->set(REG_A, result);

            // DO NOT update N and Z flags after BCD correction - reference
            // keeps original flags
          } else {
            // Binary mode - special C and V flag behavior
            this->set(REG_A, shifted_a);

            // ARR has special C and V flag behavior:
            // C = bit 6 of result (not the shifted-out bit!)
            // V = bit 6 XOR bit 5 of result
            if (this->get(REG_A) & 0x40) {
              this->set(REG_P, this->get(REG_P) | FLAG_C | FLAG_V);
            }
            if (this->get(REG_A) & 0x20) {
              this->set(REG_P, this->get(REG_P) ^ FLAG_V);
            }
          }
        } else {
          // Binary mode - special C and V flag behavior (for processors without
          // BCD)
          this->set(REG_A, shifted_a);

          // ARR has special C and V flag behavior:
          // C = bit 6 of result (not the shifted-out bit!)
          // V = bit 6 XOR bit 5 of result
          if (this->get(REG_A) & 0x40) {
            this->set(REG_P, this->get(REG_P) | FLAG_C | FLAG_V);
          }
          if (this->get(REG_A) & 0x20) {
            this->set(REG_P, this->get(REG_P) ^ FLAG_V);
          }
        }

        this->transition_to_fetch();
        return pins;
      }
    }
  }

  return pins;
}

bus_state_t op_alr(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // ALR - AND + LSR (AND immediate, then LSR A)
    switch (this->cycle_index) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      // PHI1: Load data, increment PC, and perform operation
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      // Perform AND with accumulator
      this->set(REG_A, this->get(REG_A) & this->get(REG_DL));

      // Perform LSR on accumulator
      this->update_flag(FLAG_C, this->get(REG_A) & 0x01);
      this->set(REG_A, this->get(REG_A) >> 1);

      // Update N and Z flags
      this->update_nz_flags<REG_A>(this->get(REG_A));
      this->transition_to_fetch();
      return pins;
    }
  }

  return pins;
}

// ASR is an alias for ALR (same operation, different names in documentation)
bus_state_t op_asr(bus_state_t pins) { return op_alr(pins); }

// ============================================================================
// ADDITIONAL ILLEGAL OPERATIONS
// ============================================================================

bus_state_t op_xaa(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // XAA - Transfer X AND immediate to A (illegal)
    // Hardware quirk: Uses unstable constant 0xEE like LAX immediate
    switch (this->cycle_index) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      // PHI1: Load data, increment PC, and perform operation
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      // Hardware behavior: (A | 0xEE) & X & operand
      this->set(REG_A, (this->get(REG_A) | 0xEE) & this->get(REG_X) &
                           this->get(REG_DL));
      this->update_nz_flags<REG_A>(this->get(REG_A));
      this->transition_to_fetch();
      return pins;
    }
  }

  return pins;
}

bus_state_t op_sbx(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SBX - Compare X with A AND immediate (illegal) (also called AXS)
    switch (this->cycle_index) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      // PHI1: Load data, increment PC, and perform operation
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      uint8_t temp = this->get(REG_A) & this->get(REG_X);
      uint8_t result = temp - this->get(REG_DL);
      // Update X with result
      this->set(REG_X, result);

      // Set carry flag using standard subtraction semantics (carry = no borrow)
      this->update_flag(FLAG_C, temp >= this->get(REG_DL));

      this->update_nz_flags<REG_X>(result);
      this->transition_to_fetch();
      return pins;
    }
  }

  return pins;
}

bus_state_t op_sha(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHA - Store A & X & (H+1) with address corruption on page cross

    // Calculate value: A & X & (intermediate_high + 1)
    uint8_t data_value =
        this->get(REG_A) & this->get(REG_X) & (this->get(REG_DL) + 1);

    // Apply address corruption on page cross (DL != ABH means page crossed)
    if (this->get(REG_DL) != this->get(REG_ABH)) {
      this->set(REG_ABH, data_value);

      // Set data to write
      this->set(REG_DL, data_value);

      pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_DL));
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

bus_state_t op_shs(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHS - Store A & X & (H+1), Set S to A & X - match reference
    // implementation exactly

    // Calculate A & X first (used for both value and S)
    uint8_t ax = this->get(REG_A) & this->get(REG_X);

    // Calculate value: (A & X) & (intermediate_high + 1)
    // Use DL as intermediate high byte (before page cross correction)
    uint8_t data_value = ax & ((this->get(REG_DL) + 1) & 0xFF);

    // Apply address corruption on page cross (DL != ABH means page crossed)
    if (this->get(REG_DL) != this->get(REG_ABH)) {
      this->set(REG_ABH, data_value);

      // Set data to write
      this->set(REG_DL, data_value);

      // Set stack pointer to A & X (unique to SHS)
      this->set(REG_S, ax);

      pins = this->bus_setup_write<Addr::AB>(pins, this->get(REG_DL));
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

bus_state_t op_shx(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHX - Store X & (H+1) with address corruption - match reference
    // implementation exactly

    // Calculate value: X & (intermediate_high + 1)
    // Use DL as intermediate high byte (before page cross correction)
    uint8_t data_value = this->get(REG_X) & ((this->get(REG_DL) + 1) & 0xFF);

    // Apply address corruption on page cross (DL != ABH means page crossed)
    if (this->get(REG_DL) != this->get(REG_ABH)) {
      this->set(REG_ABH, data_value);

      // Set data to write
      pins = this->bus_setup_write<Addr::AB>(pins, data_value);
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

bus_state_t op_shy(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHY - Store Y & (H+1) with address corruption - match reference
    // implementation exactly

    // Calculate value: Y & (intermediate_high + 1)
    // Use DL as intermediate high byte (before page cross correction)
    uint8_t data_value = this->get(REG_Y) & ((this->get(REG_DL) + 1) & 0xFF);

    // Apply address corruption on page cross (DL != ABH means page crossed)
    if (this->get(REG_DL) != this->get(REG_ABH)) {
      this->set(REG_ABH, data_value);

      // Set data to write
      pins = this->bus_setup_write<Addr::AB>(pins, data_value);
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

bus_state_t op_las(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // LAS - Load A, X, and S with memory AND stack pointer (illegal)
    switch (this->cycle_index) {
    case 0:
      // PHI2: Setup read from AB
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 1:
      // PHI1: Load data and perform operation
      this->bus_load_reg(REG_DL, pins);
      uint8_t result = this->get(REG_DL) & this->get(REG_S);
      this->set(REG_A, result);
      this->set(REG_X, result);
      this->set(REG_S, result);

      this->update_nz_flags<REG_A>(result);
      this->transition_to_fetch();
      return pins;
    }
  }

  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
