/*
 * wide.inc.hpp - 65C816 16-bit Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

// Commented out lint prevention - causing too many issues
// #include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// 65C816 MODE CONTROL OPERATIONS
// ============================================================================

// REP - Reset Processor Status Bits (65C816)
bus_state_t op_rep(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // Fetch immediate operand
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2: {
      // Reset specified status bits (clear bits that are 1 in operand)
      uint8_t mask = this->get(REG_DL);
      if (this->in_emulation_mode()) {
        // In emulation mode, cannot clear M or X flags (bits 5 and 4)
        mask &= ~(FLAG_M | FLAG_X);
      }
      this->set(REG_P, this->get(REG_P) & ~mask);
      this->transition_to_fetch();
      return pins;
    }
    }
    return pins;
  }

  return pins;
}

// SEP - Set Processor Status Bits (65C816)
bus_state_t op_sep(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    // Check emulation mode at runtime - SEP is only valid in native mode
    if (!this->in_emulation_mode()) {
      switch (this->half_cycle) {
      case 0:
        // Fetch immediate operand
        pins = this->bus_setup_read<Addr::PC>(pins);
        return pins;
      case 1:
        /* PHI1: Load data and perform operations */
        this->bus_load_reg(REG_DL, pins);
        this->inc(REG_PC);
        // Set specified status bits (set bits that are 1 in operand)
        this->set(REG_P, this->get(REG_P) | this->get(REG_DL));
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    } else {
      // In emulation mode, SEP becomes a 2-byte NOP - redirect to NOP handler
      this->transition_to_opcode(opcode_info_t{OP::NOP, AM::IMM, OF::NONE});
      return this->call_current_handler(pins);
    }
  }

  return pins;
}

// XCE - Exchange Carry and Emulation flags (65C816)
bus_state_t op_xce(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    // XCE is valid in both native and emulation modes
    switch (this->half_cycle) {
    case 0: {
      // Internal cycle - exchange carry and emulation flags
      // CRITICAL: Work with both 8-bit P and emulation flag separately
      uint16_t p_reg_16 = this->get(REG_P_16);
      bool old_carry = (p_reg_16 & FLAG_C) != 0;
      bool old_emulation = (p_reg_16 & FLAG_E) != 0;

      // Exchange carry flag in 8-bit P register
      if (old_emulation) {
        p_reg_16 |= FLAG_C; // Set carry if was in emulation mode
      } else {
        p_reg_16 &= ~FLAG_C; // Clear carry if was in native mode
      }

      // Exchange emulation flag manually (avoid set_emulation_mode() bugs)
      if (old_carry) {
        p_reg_16 |= FLAG_E; // Set emulation if old carry was set
      } else {
        p_reg_16 &= ~FLAG_E; // Clear emulation if old carry was clear
      }

      this->set(REG_P_16, p_reg_16);
      // Handle mode transition side effects only when switching TO emulation
      // mode
      if (old_carry && !old_emulation) {
        // Switching from native to emulation mode - truncate index registers
        this->set(REG_XH, 0);
        this->set(REG_YH, 0);
        this->set(REG_SPH, 0x01);
        this->set(REG_D_16, 0);
      }

      this->half_cycle++;
      return pins;
    }

    case 1:
      // Complete operation
      this->transition_to_fetch();
      return pins;
    }
  }
  return pins;
}

// ============================================================================
// 65C816 ENHANCED STACK OPERATIONS
// ============================================================================

// PEA - Push Effective Absolute Address (65C816)
bus_state_t op_pea(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read address low byte
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Read address high byte
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABH, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 4:
      // PHI2: Push high byte first
      pins = this->bus_setup_write<Addr::SP>(pins, REG_ABH);
      return pins;
    case 5:
      this->dec(REG_S);
      this->half_cycle++;
      return pins;

    case 6:
      // PHI2: Push low byte
      pins = this->bus_setup_write<Addr::SP>(pins, REG_ABL);
      return pins;
    case 7:
      this->dec(REG_S);
      this->transition_to_fetch();
      return pins;
    }
  }

  // PEA is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// PHB - Push Data Bank Register (65C816)
bus_state_t op_phb(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Push DBR to stack
      pins = this->bus_setup_write<Addr::SP>(pins, REG_DBR);
      return pins;
    case 1:
      this->dec(REG_S);
      this->transition_to_fetch();
      return pins;
    }
  }

  // PHB is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// PHD - Push Direct Page Register (65C816)
bus_state_t op_phd(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Push D register (Direct Page) high byte first
      pins = this->bus_setup_write<Addr::SP>(pins, REG_DPH);
      return pins;
    case 1:
      this->dec(REG_S);
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Push D register (Direct Page) low byte
      pins = this->bus_setup_write<Addr::SP>(pins, REG_DPL);
      return pins;
    case 3:
      this->dec(REG_S);
      this->transition_to_fetch();
      return pins;
    }
  }

  // PHD is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// PHK - Push Program Bank Register (65C816)
bus_state_t op_phk(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // Dummy read from PC (internal operation)
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      return pins;
    case 1:
      this->half_cycle++;
      return pins;

    case 2:
      // Push PBR to stack
      pins = this->bus_setup_write<Addr::SP>(pins, REG_PBR);
      return pins;
    case 3:
      this->dec(REG_S);
      this->transition_to_fetch();
      return pins;
    }
    return pins;
  }

  // PHK is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// PLB - Pull Data Bank Register (65C816)
bus_state_t op_plb(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Dummy read from current PC
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      return pins;
    case 1:
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Dummy read from current SP, then increment SP
      pins = this->bus_setup_dummy<Addr::SP>(pins);
      return pins;
    case 3:
      this->inc(REG_S);
      this->half_cycle++;
      return pins;

    case 4:
      // PHI2: Pull DBR from stack
      pins = this->bus_setup_read<Addr::SP>(pins);
      return pins;
    case 5:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->update_nz_flags(this->get(REG_DBR));
      this->transition_to_fetch();
    }
    return pins;
  }

  // PLB is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// PLD - Pull Direct Page Register (65C816)
bus_state_t op_pld(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Dummy read from current SP, then increment SP for low byte
      pins = this->bus_setup_dummy<Addr::SP>(pins);
      return pins;
    case 1:
      this->inc(REG_S); // Increment for low byte
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Pull D register (Direct Page) low byte first
      pins = this->bus_setup_read<Addr::SP>(pins);
      return pins;
    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DPL, pins);
      this->inc(REG_S); // Increment for high byte
      this->half_cycle++;
      return pins;

    case 4:
      // PHI2: Pull D register (Direct Page) low byte first
      pins = this->bus_setup_read<Addr::SP>(pins);
      return pins;
    case 5:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DPH, pins);
      this->update_nz_flags(
          this->get(REG_DPL)); // Only check low byte for flags
      this->transition_to_fetch();
      return pins;
    }
  }

  // PLD is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// ============================================================================
// 65C816 LONG ADDRESSING OPERATIONS
// ============================================================================

// JSL - Jump to Subroutine Long (65C816)
bus_state_t op_jsl(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // Read address low byte
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // Read address high byte
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABH, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 4:
      // Read bank byte
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 5:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 6:
      // Push program bank register
      // Store PBR in TMP for pushing
      pins = this->bus_setup_write<Addr::SP>(pins, REG_PBR);
      return pins;
    case 7:
      // Push PC high byte (return address - 1)
      this->dec(REG_S);
      this->half_cycle++;
      return pins;

    case 8:
      // Push PC low byte

      pins = this->bus_setup_write<Addr::SP>(pins, REG_PCH);
      return pins;

    case 9:
      this->dec(REG_S);
      this->half_cycle++;
      return pins;

    case 10:
      pins = this->bus_setup_write<Addr::SP>(pins, REG_PCL);
      this->half_cycle++;
      return pins;
    case 11:
      // Set new program counter and bank
      this->set(REG_PC,
                this->get(REG_AB)); // addr_high:addr_low from cases 0-1
      this->set(REG_PBR, this->get(REG_DL)); // bank from case 2
      this->transition_to_fetch();
      return pins;
    }
    return pins;
  }

  // JSL is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// RTL - Return from Subroutine Long (65C816)
bus_state_t op_rtl(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // Dummy read from current SP, then increment SP for PCL
      pins = this->bus_setup_dummy<Addr::SP>(pins);
      return pins;
    case 1:
      this->inc(REG_S);
      this->half_cycle++;
      return pins;

    case 2:
      // Pull PC low byte
      pins = this->bus_setup_read<Addr::SP>(pins);
      return pins;
    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_PCL, pins);
      this->inc(REG_S); // Increment for PCH
      this->half_cycle++;
      return pins;

    case 4:
      // Pull PC high byte
      pins = this->bus_setup_read<Addr::SP>(pins);
      return pins;
    case 5:
      this->bus_load_reg(REG_PCH, pins);
      this->inc(REG_S); // Increment for PBR
      this->half_cycle++;
      return pins;

    case 6:
      // PHI2: Pull program bank register
      pins = this->bus_setup_read<Addr::SP>(pins);
      return pins;
    case 7:
      // PHI1: Load PBR, increment PC, and complete
      this->bus_load_reg(REG_PBR, pins);
      this->inc(REG_PC);
      this->transition_to_fetch();
      return pins;
    }
    return pins;
  }

  // RTL is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// PER - Push Effective Relative Address (65C816)
bus_state_t op_per(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    // Check for emulation mode - PER is not available in emulation mode
    if (this->in_emulation_mode()) {
      // In emulation mode, PER behaves as NOP (no operation)
      this->transition_to_fetch();
      return pins;
    }

    switch (this->half_cycle) {
    case 0:
      // Read relative offset low byte
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // Read relative offset high byte
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 3: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABH, pins);
      this->inc(REG_PC);
      uint16_t effective_addr = this->get(REG_PC) + static_cast<int16_t>(this->get(REG_AB));
      this->set(REG_AB, effective_addr);
      this->half_cycle++;
      return pins;
    }

    case 4:
      // Push high byte of effective address
      pins = this->bus_setup_write<Addr::SP>(pins, REG_ABH);
      return pins;
    case 5:
      this->dec(REG_S);
      this->half_cycle++;
      return pins;

    case 6:
      // Push low byte of effective address
      pins = this->bus_setup_write<Addr::SP>(pins, REG_ABL);
      return pins;
    case 7:
      this->dec(REG_S);
      this->transition_to_fetch();
      return pins;
    }
    return pins;
  }

  // PER is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// PEI - Push Effective Indirect Address (65C816)
bus_state_t op_pei(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // Read zero page address
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      uint16_t dp_addr = this->get(REG_D_16) + this->get(REG_DL);
      this->set(REG_AB, dp_addr);
      this->half_cycle++;
      return pins;
    }

    case 2:
      // Read low byte of indirect address
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;
    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_AB);
      this->half_cycle++;
      return pins;

    case 4:
      // Read high byte of indirect address
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;
    case 5:
      this->bus_load_reg(REG_ABL, pins);
      this->half_cycle++;
      return pins;

    case 6:
      // Push high byte of effective address
      pins = this->bus_setup_write<Addr::SP>(pins, REG_ABH);
      return pins;
    case 7:
      this->dec(REG_S);
      this->half_cycle++;
      return pins;

    case 8:
      // Push low byte of effective address
      pins = this->bus_setup_write<Addr::SP>(pins, REG_ABL);
      return pins;
    case 9:
      this->dec(REG_S);
      this->transition_to_fetch();
      return pins;
    }
    return pins;
  }

  // PEI is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// XBA - Exchange B and A (65C816)
bus_state_t op_xba(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    // XBA is valid in both native and emulation modes
    switch (this->half_cycle) {
    case 0:
      // Internal cycle - exchange high and low bytes of accumulator
      {
        uint16_t acc = this->get(REG_C); // Get 16-bit accumulator
        uint16_t swapped = ((acc & 0x00FF) << 8) | ((acc & 0xFF00) >> 8);
        this->set(REG_C, swapped);
        // Update N,Z flags based on new low byte
        this->update_nz_flags(swapped & 0xFF);
      }
      this->half_cycle++;
      return pins;

    case 1:
      // Complete operation
      this->transition_to_fetch();
      return pins;
    }
    return pins;
  }

  return pins;
}

// MVN - Move Negative (65C816)
bus_state_t op_mvn(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read destination bank
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DBR, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Read source bank
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_SBR, pins);
      this->inc(REG_PC);
      this->set(REG_AB, this->get(REG_X));
      this->half_cycle++;
      return pins;

    case 4:
      // PHI2: Read from source address (bank:X)
      pins = this->bus_setup_read<Addr::AB, Bank::SBR>(pins);
      return pins;
    case 5:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->set(REG_AB, this->get(REG_Y));
      this->half_cycle++;
      return pins;

    case 6:
      // PHI2: Write to destination address (bank:Y)
      pins = this->bus_setup_write<Addr::AB, Bank::DBR>(pins, REG_DL);
      return pins;
    case 7:
      // Increment X and Y
      this->inc(REG_X);
      this->inc(REG_Y);
      // Decrement C (transfer count)
      this->dec(REG_C);
      // Check if more bytes to transfer
      if (this->get(REG_C) != 0xFFFF) {
        this->set(REG_AB, this->get(REG_X));
        // Continue transfer - go back to cycle 4
        this->half_cycle = 4;
      } else {
        // Transfer complete
        this->transition_to_fetch();
      }
      return pins;
    }
  }
  return pins;
}

// MVP - Move Positive (65C816)
bus_state_t op_mvp(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read destination bank
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Read source bank
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_SBR, pins);
      this->inc(REG_PC);
      this->set(REG_DBR, this->get(REG_DL)); // destination bank from case 1
      this->set(REG_AB, this->get(REG_X));
      this->half_cycle++;
      return pins;

    case 4:
      // PHI2: Read from source address (bank:X)
      pins = this->bus_setup_read<Addr::AB, Bank::SBR>(pins);
      return pins;
    case 5:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->set(REG_AB, this->get(REG_Y));
      this->half_cycle++;
      return pins;

    case 6:
      // PHI2: Write to destination address (bank:Y)
      pins = this->bus_setup_write<Addr::AB, Bank::DBR>(pins, REG_DL);
      return pins;
    case 7:
      // Decrement X and Y (move in opposite direction from MVN)
      this->dec(REG_X);
      this->dec(REG_Y);
      // Decrement C (transfer count)
      this->dec(REG_C);
      // Check if more bytes to transfer
      if (this->get(REG_C) != 0xFFFF) {
        this->set(REG_AB, this->get(REG_X));
        // Continue transfer - go back to cycle 4
        this->half_cycle = 4;
      } else {
        // Transfer complete
        this->transition_to_fetch();
      }
      return pins;
    }
    return pins;
  }

  // MVP is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// COP - Co-processor Instruction (65C816)
// Forwards to BRK after setting COP interrupt type
bus_state_t op_cop(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    // Set up interrupt type to COP (only difference from BRK)
    this->active_interrupt = FAM65XX_INT_COP;
    // Redirect handler to BRK for all cycles
    this->current_handler = &fam65xx_t::op_brk;
    return this->op_brk(pins);
  }

  // COP is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

// WDM - WDM Reserved Instruction (65C816)
bus_state_t op_wdm(bus_state_t pins) {
  trace_operation(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // Read and ignore operand byte for timing compatibility
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      return pins;
    case 1:
      this->inc(REG_PC);
      // WDM is essentially a 2-byte NOP - do nothing else
      this->transition_to_fetch();
      return pins;
    }
  }

  // WDM is illegal on non-wide CPUs - acts as NOP
  this->transition_to_fetch();
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
