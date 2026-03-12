/*
 * illegal.inc.hpp - Illegal/Undocumented Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "chip/cpu/fam65xx/operations/inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// ILLEGAL LOAD/STORE COMBINATIONS
// ============================================================================

bus_state_t op_lax(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // LAX - Load A and X from memory
    // LAX immediate has unstable behavior — uses (A | magic) & operand.
    // The magic constant varies by chip (empirically observed):
    //   0xEE on NMOS 6502/6510, 0xFF on Ricoh 2A03.
    if (this->opcode_entry.am_index == to_index(AM::IMM)) {
      // LAX immediate — unstable behavior with chip-dependent magic
      switch (this->half_cycle) {
      case 0:
        // PHI2: Setup read from PC
        pins = this->bus_setup_read<Addr::PC>(pins);
        return pins;
      case 1: {
        // PHI1: Load data, increment PC, and perform operation
        uint8_t operand = this->bus_get_data(pins);
        this->inc(REG_PC);
        // The 2A03 empirically yields 0xFF; standard NMOS 6502 yields 0xEE.
        constexpr uint8_t magic = has_apu() ? 0xFF : 0xEE;
        uint8_t result = (this->get(REG_A) | magic) & operand;
        this->set(REG_A, result);
        this->set(REG_X, result);

        this->update_nz_flags<REG_A>(result);
        this->transition_to_fetch();
        return pins;
      }
      }
    } else {
      // LAX memory modes - normal behavior
      switch (this->half_cycle) {
      case 0:
        // PHI2: Setup read from AB
        pins = this->bus_setup_read<Addr::AB>(pins);
        return pins;
      case 1: {
        // PHI1: Load data and perform operation
        uint8_t operand = this->bus_get_data(pins);
        this->set(REG_A, operand);
        this->set(REG_X, operand);
        this->update_nz_flags<REG_A>(this->get(REG_A));
        this->transition_to_fetch();
        return pins;
      }
      }
    }
  }

  return pins;
}

bus_state_t op_sax(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // Store A AND X to memory - PHI2/PHI1 split
    switch (this->half_cycle) {
    case 0: { 
      // PHI2
      uint8_t result = this->get(REG_A) & this->get(REG_X);
      pins = this->bus_setup_write<Addr::AB>(pins, result);
      return pins;
    }
    case 1: // PHI1
      this->transition_to_fetch();
      return pins;
    }
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

  switch (this->half_cycle) {
  case 0:
    // PHI2: Read operand from PC+1 (this was PC++ after opcode fetch)
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;
  case 1:
    this->half_cycle++;
    return pins;

  case 2:
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;
  case 3:
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
    switch (this->half_cycle) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      // PHI1: Load data, increment PC, and perform operation
      uint8_t operand = this->bus_get_data(pins);
      this->inc(REG_PC);
      // Perform AND with accumulator
      this->set(REG_A, this->get(REG_A) & operand);

      // Update N and Z flags
      this->update_nz_flags<REG_A>(this->get(REG_A));

      // Copy N flag to C flag (ANC behavior)
      this->update_flag(FLAG_C, this->get(REG_P) & FLAG_N);
      this->transition_to_fetch();
      return pins;
    }
    }
  }

  return pins;
}

bus_state_t op_arr(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // ARR - AND + ROR with BCD correction in decimal mode (reference
    // implementation)
    switch (this->half_cycle) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      // PHI1: Load data, increment PC, and perform operation
      uint8_t operand = this->bus_get_data(pins);
      this->inc(REG_PC);

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

      // Check if we're in decimal mode (only for processors with BCD support)
      bool decimal_mode = false;
      if constexpr (has_bcd()) {
        decimal_mode = (this->get(REG_P) & FLAG_D) != 0;
      }

      if (decimal_mode) {
        // Decimal mode - ARR with BCD correction (BCD-capable processors only)
        
        // Start with shifted result
        uint8_t result = shifted_a;

        // BCD correction using ORIGINAL A value for nibble checks
        // Low nibble correction
        if ((original_a & 0x0F) >= 5) {
          result = ((result + 6) & 0x0F) | (result & 0xF0);
        }

        // High nibble correction
        if ((original_a & 0xF0) >= 0x50) {
          result += 0x60;
        }

        // Set result in accumulator
        this->set(REG_A, result);

        // V flag: bit 6 changed between original and shifted
        if ((shifted_a ^ original_a) & 0x40) {
          this->set(REG_P, this->get(REG_P) | FLAG_V);
        }

        // C flag: bit 6 of result OR high nibble correction was applied
        if ((original_a & 0xF0) >= 0x50) {
          this->set(REG_P, this->get(REG_P) | FLAG_C);
        }

        // N and Z flags were already set based on shifted_a before BCD correction
        // DO NOT update them again - this matches hardware behavior
      } else {
        // Binary mode - special C and V flag behavior
        // (Used by both BCD-capable processors in binary mode and non-BCD processors)
        this->set(REG_A, shifted_a);

        // ARR has special C and V flag behavior:
        // C = bit 6 of result (not the shifted-out bit!)
        // V = bit 6 XOR bit 5 of result
        if (this->get(REG_A) & 0x40) {
          this->set(REG_P, this->get(REG_P) | FLAG_C);
        }
        if ((this->get(REG_A) & 0x60) == 0x60 || (this->get(REG_A) & 0x60) == 0x00) {
          // V is clear
        } else {
          this->set(REG_P, this->get(REG_P) | FLAG_V);
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
    switch (this->half_cycle) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      // PHI1: Load data, increment PC, and perform operation
      uint8_t operand = bus_get_data(pins);
      this->inc(REG_PC);
      // Perform AND with accumulator
      uint8_t a = this->get(REG_A);
      a &= operand;
      this->update_flag(FLAG_C, a & 0x01);      
      // Perform LSR on accumulator
      a >>= 1;
      this->set(REG_A, a);
      // Update N and Z flags
      this->update_nz_flags<REG_A>(a);
      this->transition_to_fetch();
      return pins;
    }
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
    // Uses same chip-dependent magic as LAX immediate.
    switch (this->half_cycle) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      // PHI1: Load data, increment PC, and perform operation
      uint8_t operand = this->bus_get_data(pins);
      this->inc(REG_PC);
      // (A | magic) & X & operand — same empirical magic as LAX immediate
      constexpr uint8_t magic = has_apu() ? 0xFF : 0xEE;
      this->set(REG_A, (this->get(REG_A) | magic) & this->get(REG_X) &
                           operand);
      this->update_nz_flags<REG_A>(this->get(REG_A));
      this->transition_to_fetch();
      return pins;
    }
    }
  }

  return pins;
}

bus_state_t op_sbx(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SBX - Compare X with A AND immediate (illegal) (also called AXS)
    switch (this->half_cycle) {
    case 0:
      // PHI2: Setup read from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      // PHI1: Load data, increment PC, and perform operation
      uint8_t operand = this->bus_get_data(pins);
      this->inc(REG_PC);
      uint8_t temp = this->get(REG_A) & this->get(REG_X);
      uint8_t result = temp - operand;
      // Update X with result
      this->set(REG_X, result);

      // Set carry flag using standard subtraction semantics (carry = no borrow)
      this->update_flag(FLAG_C, temp >= operand);

      this->update_nz_flags<REG_X>(result);
      this->transition_to_fetch();
      return pins;
    }
    }
  }

  return pins;
}

bus_state_t op_sha(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHA - Store A & X & (H+1) with address corruption on page cross - PHI2/PHI1 split
    switch (this->half_cycle) {
    case 0: {
      // PHI2: Calculate value: A & X & (intermediate_high + 1)
        uint8_t data_value =
            this->get(REG_A) & this->get(REG_X) & (this->get(REG_DL) + 1);

        // Apply address corruption on page cross (DL != ABH means page crossed)
        if (this->get(REG_DL) != this->get(REG_ABH)) {
          this->set(REG_ABH, data_value);
        }

        // Set data to write
        pins = this->bus_setup_write<Addr::AB>(pins, data_value);
        return pins;
      }
    case 1: // PHI1
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

bus_state_t op_shs(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHS - Store A & X & (H+1), Set S to A & X - PHI2/PHI1 split
    switch (this->half_cycle) {
    case 0: {
      // PHI2: Calculate A & X first (used for both value and S)
      uint8_t ax = this->get(REG_A) & this->get(REG_X);
      // Calculate value: (A & X) & (intermediate_high + 1)
      // Use DL as intermediate high byte (before page cross correction)
      uint8_t data_value = ax & ((this->get(REG_DL) + 1) & 0xFF);
      // Apply address corruption on page cross (DL != ABH means page crossed)
      if (this->get(REG_DL) != this->get(REG_ABH)) {
        this->set(REG_ABH, data_value);
      }
      // Set stack pointer to A & X (unique to SHS)
      this->set(REG_S, ax);
      pins = this->bus_setup_write<Addr::AB>(pins, data_value);
      return pins;
    }
    case 1: // PHI1
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

bus_state_t op_shx(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHX - Store X & (H+1) with address corruption - PHI2/PHI1 split
    switch (this->half_cycle) {
    case 0: {
      // PHI2: Calculate value: X & (intermediate_high + 1)
      // Use DL as intermediate high byte (before page cross correction)
      uint8_t data_value = this->get(REG_X) & ((this->get(REG_DL) + 1) & 0xFF);

      // Apply address corruption on page cross (DL != ABH means page crossed)
      if (this->get(REG_DL) != this->get(REG_ABH)) {
        this->set(REG_ABH, data_value);
      }

      // Set data to write
      pins = this->bus_setup_write<Addr::AB>(pins, data_value);
      return pins;
    }
    case 1: // PHI1
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

bus_state_t op_shy(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_illegal_opcodes()) {
    // SHY - Store Y & (H+1) with address corruption - PHI2/PHI1 split
    switch (this->half_cycle) {
    case 0: {
      // PHI2: Calculate value: Y & (intermediate_high + 1)
      // Use DL as intermediate high byte (before page cross correction)
      uint8_t data_value = this->get(REG_Y) & ((this->get(REG_DL) + 1) & 0xFF);
      // Apply address corruption on page cross (DL != ABH means page crossed)
      if (this->get(REG_DL) != this->get(REG_ABH)) {
        this->set(REG_ABH, data_value);
      }
      // Set data to write
      pins = this->bus_setup_write<Addr::AB>(pins, data_value);
      return pins;
    }
    case 1: // PHI1
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
    switch (this->half_cycle) {
    case 0:
      // PHI2: Setup read from AB
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;
    case 1: {
      // PHI1: Load data and perform operation
      uint8_t operand = this->bus_get_data(pins);
      uint8_t result = operand & this->get(REG_S);
      this->set(REG_A, result);
      this->set(REG_X, result);
      this->set(REG_S, result);
      this->update_nz_flags<REG_A>(result);
      this->transition_to_fetch();
      return pins;
    }
    }
  }

  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "chip/cpu/fam65xx/operations/inc_lint_prevention_footer.hpp"
