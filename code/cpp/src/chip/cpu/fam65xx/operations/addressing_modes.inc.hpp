/*
 * addressing_modes.inc.hpp - Addressing Mode Handlers for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// ADDRESSING MODE HANDLERS
// ============================================================================
//
// NOTE: Immediate mode (AM::IMM) no longer needs a handler!
// The bus_setup_read_operand() helper in fam65xx.hpp handles immediate mode
// automatically by checking opcode_entry.am_index and reading from PC instead
// of AB. Operations go directly to execution for immediate mode.

// Zero Page addressing: $nn (cycle-accurate)
bus_state_t am_zp(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Set up bus read for zero page address from PC */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load zero page address and set up address registers */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Zero Page,X addressing: $nn,X (cycle-accurate)
bus_state_t am_zpx(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Set up bus read for base address from PC */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load base address and set up zero page */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Dummy read from AB while adding index */
    pins = this->bus_setup_dummy<Addr::AB>(pins);
    return pins;

  case 3:
    /* PHI1: Add index to ABL address (wraps in zero page) */
    this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_X));
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Zero Page,Y addressing: $nn,Y (cycle-accurate)
bus_state_t am_zpy(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Set up bus read for base address from PC */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load base address and set up zero page */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Dummy read from AB while adding index */
    pins = this->bus_setup_dummy<Addr::AB>(pins);
    return pins;

  case 3:
    /* PHI1: Add index to AB address (wraps in zero page) */
    this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_Y));
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Absolute addressing: $nnnn (cycle-accurate)
bus_state_t am_abs(bus_state_t pins) {
  trace_addressing_mode(__func__);
  // Check for 65C816 PEA (0xF4) in emulation mode - redirect to ZPX addressing
  if constexpr (has_wide_registers()) {
    if (this->in_emulation_mode() && this->get(REG_IR) == 0xF4) {
      // PEA in emulation mode should behave as NOP zp,X
      this->transition_to_opcode(opcode_info_t{OP::NOP, AM::ZPX, OF::NONE});
      return this->call_current_handler(pins);
    }
  }

  switch (this->half_cycle) {
  case 0:
    /* PHI2: Set up bus read for low byte from PC */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load low byte */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Set up bus read for high byte from PC */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 3:
    /* PHI1: Load high byte and transition */
    this->bus_load_reg(REG_ABH, pins);
    this->inc(REG_PC);
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Absolute,X addressing: $nnnn,X (cycle-accurate with page crossing)
bus_state_t am_abx(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Read low byte from PC
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_DL, pins);
    this->inc(REG_PC);
    this->half_cycle++;
    return pins;

  case 2:
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 3: {
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABH, pins);  // Load high byte into ABH
    this->inc(REG_PC);
    this->set(REG_DL, this->get(REG_ABH));
    uint16_t base = this->get(REG_AB);
    uint16_t effective = base + this->get(REG_X);
    this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_X));
    bool needs_penalty =
        this->page_crossed(base, effective) ||            // Page crossing
        (this->opcode_entry.flags & to_index(OF::RMW)) || // RMW operations
        (this->opcode_entry.flags &
         to_index(OF::ILLEGAL_STORE)) || // SHY illegal store
        !(this->opcode_entry.flags &
          to_index(OF::SKIP_PAGE)); // No skip allowed
    if (needs_penalty) {
      this->half_cycle++;
    } else {
      this->set(REG_AB, effective);
      this->transition_to_operation();
    }
    return pins;
  }

  case 4:
    /* PHI2: Page cross penalty - read from wrong address */
    pins = this->bus_setup_dummy<Addr::AB>(pins);
    return pins;

  case 5:
    /* PHI1: Correct final address */
    // ABL has X added, ABH is unchanged from original. Subtract X from ABL to
    // restore original base
    this->set(REG_ABL, this->get(REG_ABL) - this->get(REG_X));
    // Now AB has original base address, add X to full 16-bit AB for correct
    // effective address with carry
    this->set(REG_AB, this->get(REG_AB) + this->get(REG_X));
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Absolute,Y addressing: $nnnn,Y (cycle-accurate with page crossing)
bus_state_t am_aby(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Read low byte from PC
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->half_cycle++;
    return pins;

  case 2:
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 3: {
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABH, pins);  // Load high byte into ABH
    this->inc(REG_PC);
    this->set(REG_DL, this->get(REG_ABH));
    uint16_t base = this->get(REG_AB);
    uint16_t effective = base + this->get(REG_Y);
    this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_Y));
    bool needs_penalty =
        this->page_crossed(base, effective) ||            // Page crossing
        (this->opcode_entry.flags & to_index(OF::RMW)) || // RMW operations
        (this->opcode_entry.flags &
         to_index(OF::ILLEGAL_STORE)) || // SHA illegal store
        !(this->opcode_entry.flags &
          to_index(OF::SKIP_PAGE)); // No skip allowed
    if (needs_penalty) {
      this->half_cycle++;
    } else {
      this->set(REG_AB, effective);
      this->transition_to_operation();
    }
    return pins;
  }

  case 4:
    /* PHI2: Page cross penalty - read from wrong address */
    pins = this->bus_setup_dummy<Addr::AB>(pins);
    return pins;

  case 5:
    /* PHI1: Correct final address */
    // ABL has Y added, ABH is unchanged from original. Subtract Y from ABL to
    // restore original base
    this->set(REG_ABL, this->get(REG_ABL) - this->get(REG_Y));
    // Now AB has original base address, add Y to full 16-bit AB for correct
    // effective address with carry
    this->set(REG_AB, this->get(REG_AB) + this->get(REG_Y));
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Indirect addressing: ($nnnn) - Used only by JMP instruction
bus_state_t am_ind(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    // PHI2: Read low byte of pointer address from PC
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->half_cycle++;
    return pins;

  case 2:
    // PHI2: Read high byte of pointer address from PC
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 3:
    /* PHI1: Load high byte of pointer and assemble pointer address */
    this->bus_load_reg(REG_ABH, pins);
    this->inc(REG_PC);
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Read low byte of target address from pointer */
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 5:
    /* PHI1: Load low byte and save to TMP */
    this->bus_load_reg(REG_AH, pins);  // Save low byte to AH (unused on 6502)
    this->set(REG_ABL, this->get(REG_ABL) + 1);
    this->half_cycle++;
    return pins;

  case 6:
    /* PHI2: Read high byte of target address from pointer+1 */
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 7:
    /* PHI1: Assemble final address from TMP (low) and bus data (high) */
    this->bus_load_reg(REG_ABH, pins);  // High byte to ABH
    this->set(REG_ABL, this->get(REG_AH)); // Low byte from AH to ABL
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Indexed Indirect addressing: ($nn,X)
bus_state_t am_inx(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    /* Read pointer from PC */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
    this->half_cycle++;
    return pins;

  case 2:
    /* Dummy read from AB (before adding X) */
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 3:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_DL, pins);
    this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_X));
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Read low byte of target from AB+X */
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 5:
    /* PHI1: Load low byte and save to TMP */
    this->bus_load_reg(REG_AH, pins);  // Save low byte to AH (unused on 6502)
    this->set(REG_ABL, this->get(REG_ABL) + 1);
    this->half_cycle++;
    return pins;

  case 6:
    /* PHI2: Read high byte of target from AB+X+1 */
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 7:
    /* PHI1: Assemble final address from TMP (low) and bus data (high) */
    this->bus_load_reg(REG_ABH, pins);  // High byte to ABH
    this->set(REG_ABL, this->get(REG_AH)); // Low byte from AH to ABL
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Indirect Indexed addressing: ($nn),Y
bus_state_t am_iny(bus_state_t pins) {
  trace_addressing_mode(__func__);
  switch (this->half_cycle) {
  case 0:
    /* Read pointer from PC */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Read low byte of target from ZP */
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 3:
    /* PHI1: Load low byte and save to TMP */
    this->bus_load_reg(REG_AH, pins);  // Save low byte to AH (unused on 6502)
    this->set(REG_ABL, this->get(REG_ABL) + 1); // Increment zero page pointer
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Read high byte of target from ZP+1 */
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 5: {
    /* PHI1: Load high byte and assemble base address */
    this->bus_load_reg(REG_ABH, pins);  // High byte to ABH
    this->set(REG_ABL, this->get(REG_AH)); // Low byte from AH to ABL
    uint16_t base_addr = this->get(REG_AB);
    uint16_t final_addr = base_addr + this->get(REG_Y);
    /* Store intermediate high byte in DL for illegal opcodes AFTER setting up
     * AB */
    this->set(REG_DL, this->get(REG_ABH));
    /* Add index to low byte only (creates intermediate "wrong" address for
     * page cross) */
    this->set(REG_ABL, this->get(REG_ABL) + this->get(REG_Y));
    /* Check if penalty cycle is needed */
    bool needs_penalty =
        this->page_crossed(base_addr, final_addr) ||      // Page crossing
        (this->opcode_entry.flags & to_index(OF::RMW)) || // RMW operations
        (this->opcode_entry.flags &
         to_index(OF::ILLEGAL_STORE)) || // SHA illegal store
        !(this->opcode_entry.flags &
          to_index(
              OF::SKIP_PAGE)); // Store operations and others that can't skip
    if (needs_penalty) {
      /* Page crossing, RMW, or illegal store - need penalty cycle with
       * intermediate address */
      this->half_cycle++;
    } else {
      /* No page cross, not RMW, and not illegal store - can skip penalty, set
       * correct address */
      this->set(REG_AB, final_addr);
      this->transition_to_operation();
    }
    return pins;
  }

  case 6:
    /* PHI2: Page cross penalty - dummy read from wrong address */
    pins = this->bus_setup_dummy<Addr::AB>(pins);
    return pins;

  case 7:
    /* PHI1: Correct final address calculation */
    /* DL contains intermediate high byte from case 5 */
    /* Current AB has intermediate address: orig_high:(base_low + Y) */
    /* We need: (orig_high:(base_low)) + Y */
    this->set(REG_ABH, this->get(REG_DL)); /* Restore original high byte */
    this->set(REG_ABL, this->get(REG_ABL) -
                           this->get(REG_Y)); /* Recover original base low */
    this->set(REG_AB,
              this->get(REG_AB) +
                  this->get(REG_Y)); /* Calculate correct final with carry */
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// 65C02 Enhanced Addressing Modes (conditional compilation)

// Zero Page Indirect addressing: ($nn) - 65C02 only
bus_state_t am_zpi(bus_state_t pins) {
  trace_addressing_mode(__func__);
  // Check for 65C816 PEI (0xD4) in emulation mode - redirect to ZPX addressing
  if constexpr (has_wide_registers()) {
    if (this->in_emulation_mode() && this->get(REG_IR) == 0xD4) {
      // PEI in emulation mode should behave as NOP zp,X
      this->transition_to_opcode(opcode_info_t{OP::NOP, AM::ZPX, OF::NONE});
      return this->call_current_handler(pins);
    }
  }

  // CRITICAL FIX: Remove constexpr conditional - all CMOS processors should
  // support ZPI The constexpr condition was preventing proper execution
  switch (this->half_cycle) {
  case 0:
    // PHI2: Read zero page address from PC
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;

  case 1:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_DL, pins);
    this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
    this->inc(REG_PC);
    this->half_cycle++;
    return pins;

  case 2:
    // PHI2: Read low byte of target address from zero page
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 3:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_DL, pins);
    this->set(REG_ABL, this->get(REG_ABL) + 1);
    this->half_cycle++;
    return pins;

  case 4:
    pins = this->bus_setup_read<Addr::AB>(pins);
    return pins;

  case 5:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_DL, pins);
    this->set(REG_ABL, this->get(REG_DL)); // Low byte from cycle 1
    this->transition_to_operation();
    return pins;
  }
  return pins;
}

// Absolute Indexed Indirect addressing: ($nnnn,X) - 65C02 JMP only
bus_state_t am_abi(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_cmos()) {
    // WDC 65C02 absolute indexed indirect: JMP (abs,X)
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read low byte of base address from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Read high byte of base address from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->set(REG_AB, this->get(REG_AB) + this->get(REG_X));
      this->half_cycle++;
      return pins;

    case 4:
      // PHI2: Read low byte of target address from (base+X)
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 5:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_AB);
      this->half_cycle++;
      return pins;

    case 6:
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 7:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->set(REG_ABL, this->get(REG_DL)); // Low byte from cycle 2
      this->transition_to_operation();
    }
  }
  return pins;
}

// 65C816 Enhanced Addressing Modes (conditional compilation)

// Direct Page addressing: dp (65C816)
bus_state_t am_dp(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      uint16_t dp_addr = this->get(REG_D) + this->get(REG_DL);
      this->set(REG_ABL, dp_addr & 0xFF);
      this->set(REG_ABH, (dp_addr >> 8) & 0xFF);
      if ((this->get(REG_D) & 0xFF) != 0x00) {
        this->half_cycle++;
      } else {
        this->transition_to_operation();
      }
      return pins;
    }

    case 2:
      // PHI2: Direct Page penalty cycle

      this->transition_to_operation();
      return pins;
    }
    return pins;
  }

  // Fall back to zero page on emulation mode and non-wide processors
  return am_zp(pins);
}

// Direct Page,X addressing: dp,X (65C816)
bus_state_t am_dpx(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    // Check for emulation mode - fall back to zero page,X behavior
    if (this->in_emulation_mode()) {
      return am_zpx(pins);
    }

    // Native mode: True Direct Page,X addressing
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read Direct Page offset from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      uint16_t dp_addr = this->get(REG_D) + this->get(REG_DL);
      this->set(REG_ABL, dp_addr & 0xFF);
      this->set(REG_ABH, (dp_addr >> 8) & 0xFF);
      if ((this->get(REG_D) & 0xFF) != 0x00) {
        this->half_cycle++;
      } else {
        this->half_cycle = 4; // Skip penalty cycle
      }
      return pins;
    }

    case 2:
      // PHI2: Direct Page penalty cycle
      this->half_cycle++;
      return pins;

    case 3:
      // PHI1: Direct Page penalty cycle
      this->half_cycle++;
      return pins;

    case 4: {
      // PHI2: Dummy read from Direct Page address while adding X
      pins = this->bus_setup_dummy<Addr::AB>(pins);

      // PHI1: Add X register to Direct Page address (wraps within bank $00)
      uint16_t base_addr = this->get(REG_AB);
      uint16_t x_val = this->get_x_register();
      uint16_t final_addr = (base_addr + x_val) & 0xFFFF;

      this->set(REG_AB, final_addr);
      this->transition_to_operation();
      return pins;
    }
    }
    return pins;
  }

  // Fall back to zero page,X on non-wide processors
  return am_zpx(pins);
}

// Direct Page,Y addressing: dp,Y (65C816)
bus_state_t am_dpy(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    // TODO : Implement similarly to am_dpx
  }

  // Fall back to zero page,Y on non-wide processors
  return am_zpy(pins);
}

// Direct Page Indirect addressing: [dp] (65C816)
bus_state_t am_dpi(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    // TODO : Implement similarly to am_dpil
  }

  return am_zpi(pins);
}

// Direct Page Indirect Long addressing: [dp.l] (65C816)
bus_state_t am_dpil(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read Direct Page offset from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      uint16_t dp_addr = this->get(REG_D) + this->get(REG_DL);
      this->set(REG_ABL, dp_addr & 0xFF);
      this->set(REG_ABH, (dp_addr >> 8) & 0xFF);
      this->half_cycle++;
      return pins;
    }

    case 2:
      // PHI2: Read low byte of target address from Direct Page
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 3: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      uint16_t next_addr = this->get(REG_AB) + 1;
      this->set(REG_AB, next_addr);
      this->half_cycle++;
      return pins;
    }

    case 4:
      // PHI2: Read middle byte of target address
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 5: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      uint16_t next_addr = this->get(REG_AB) + 1;
      this->set(REG_AB, next_addr);
      this->half_cycle++;
      return pins;
    }

    case 6:
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 7: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      uint8_t low_byte = this->get(REG_DL);
      uint8_t mid_byte = this->get(REG_ABL);
      uint8_t bank_byte = this->get(REG_ABH);
      this->set(REG_ABL, low_byte);
      this->set(REG_ABH, mid_byte);
      this->set(REG_DL, bank_byte); // Bank byte for memory system
      if ((this->get(REG_D) & 0xFF) != 0x00) {
        this->half_cycle++;
      } else {
        this->transition_to_operation();
      }
      return pins;
    }

    case 8:
      // PHI2: Direct Page penalty cycle

      this->transition_to_operation();
      return pins;
    }
    return pins;
  }

  // Should not be called on non-wide processors
  return pins;
}

// Direct Page Indirect Long,Y addressing: [dp],Y (65C816)
bus_state_t am_dpily(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read Direct Page offset from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      uint16_t dp_addr = this->get(REG_D) + this->get(REG_DL);
      this->set(REG_ABL, dp_addr & 0xFF);
      this->set(REG_ABH, (dp_addr >> 8) & 0xFF);
      this->half_cycle++;
      return pins;
    }

    case 2:
      // PHI2: Read low byte of base address from Direct Page
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 3: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      uint16_t next_addr = this->get(REG_AB) + 1;
      this->set(REG_AB, next_addr);
      this->half_cycle++;
      return pins;
    }

    case 4:
      // PHI2: Read middle byte of base address
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 5: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      uint16_t next_addr = this->get(REG_AB) + 1;
      this->set(REG_AB, next_addr);
      this->half_cycle++;
      return pins;
    }

    case 6:
      pins = this->bus_setup_read<Addr::AB>(pins);
      return pins;

    case 7: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      uint8_t low_byte = this->get(REG_DL);
      uint8_t mid_byte = this->get(REG_ABL);
      uint8_t bank_byte = this->get(REG_ABH);
      uint16_t base_addr = (mid_byte << 8) | low_byte;
      uint16_t y_val = this->get_y_register();
      uint16_t final_addr = base_addr + y_val;
      this->set(REG_ABL, final_addr & 0xFF);
      this->set(REG_ABH, (final_addr >> 8) & 0xFF);
      this->set(REG_DL, bank_byte); // Bank byte for memory system
      if ((this->get(REG_D) & 0xFF) != 0x00) {
        this->half_cycle++;
      } else {
        this->transition_to_operation();
      }
      return pins;
    }

    case 8:
      // PHI2: Direct Page penalty cycle

      this->transition_to_operation();
      return pins;
    }
    return pins;
  }

  // Should not be called on non-wide processors
  return pins;
}

// Absolute Long addressing: $nnnnnn (65C816)
bus_state_t am_abl(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    // Check for 65C816 JSL (0x22) in emulation mode - redirect to NOP IMM
    // addressing
    if (this->in_emulation_mode() && this->get(REG_IR) == 0x22) {
      // PEI in emulation mode should behave as NOP zp,X
      this->transition_to_opcode(opcode_info_t{OP::NOP, AM::IMM, OF::NONE});
      return this->call_current_handler(pins);
    }

    switch (this->half_cycle) {
    case 0:
      // PHI2: Read low byte from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Read middle byte from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 4:
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 5:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      this->transition_to_operation();
    }
    return pins;
  }

  // Should not be called on non-wide processors
  return pins;
}

// Absolute Long,X addressing: $nnnnnn,X (65C816)
bus_state_t am_ablx(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read low byte from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2:
      // PHI2: Read middle byte from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 4:
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 5: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      uint16_t base_addr = this->get(REG_AB);
      uint16_t x_val = this->get_x_register();
      uint16_t final_addr = base_addr + x_val;
      this->set(REG_AB, final_addr);
      this->transition_to_operation();
      return pins;
    }
    }
    return pins;
  }

  // Should not be called on non-wide processors
  return pins;
}

// Stack Relative addressing: sr,S (65C816)
bus_state_t am_sr(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read stack offset from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2: {
      // PHI2: Dummy internal operation cycle

      // Calculate stack address: $00:(S + offset)
      uint16_t stack_addr = this->get(REG_SP) + this->get(REG_DL);
      this->set(REG_ABL, stack_addr & 0xFF);
      this->set(REG_ABH, (stack_addr >> 8) & 0xFF);

      this->transition_to_operation();
      return pins;
    }
    }
    return pins;
  }

  // Should not be called on non-wide processors
  return pins;
}

// Stack Relative Indirect Indexed: (sr,S),Y (65C816)
bus_state_t am_sri(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_wide_registers()) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read stack offset from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABL, pins);
      this->inc(REG_PC);
      this->half_cycle++;
      return pins;

    case 2: {
      // PHI2: Dummy internal operation cycle

      // Calculate stack pointer address: $00:(S + offset)
      uint16_t stack_addr = this->get(REG_SP) + this->get(REG_DL);
      this->set(REG_ABL, stack_addr & 0xFF);
      this->set(REG_ABH, (stack_addr >> 8) & 0xFF);
      return pins;
    }

    case 3:
      // PHI2: Read low byte of pointer from stack
      pins = this->bus_setup_read<Addr::AB>(pins);
      this->half_cycle++;
      return pins;

    case 4: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      uint16_t next_addr = this->get(REG_AB) + 1;
      this->set(REG_AB, next_addr);
      this->half_cycle++;
      return pins;
    }

    case 5:
      pins = this->bus_setup_read<Addr::AB>(pins);
      this->half_cycle++;
      return pins;

    case 6: {
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->set(REG_ABL, this->get(REG_DL)); // Low byte from cycle 2
      uint16_t base_addr = this->get(REG_AB);
      uint16_t y_val = this->get_y_register();
      uint16_t final_addr = base_addr + y_val;
      this->set(REG_AB, final_addr);
      this->transition_to_operation();
      return pins;
    }
    }
    return pins;
  }

  // Should not be called on non-wide processors
  return pins;
}

// Zero Page Relative Addressing: For BBR/BBS instructions ($nn,$offset)
bus_state_t am_zpr(bus_state_t pins) {
  trace_addressing_mode(__func__);
  if constexpr (has_bit_manipulation()) {
    // BBR/BBS instructions: $nn,$offset
    switch (this->half_cycle) {
    case 0:
      // PHI2: Read zero page address from PC
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 1:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_DL, pins);
      this->inc(REG_PC);
      this->set(REG_ABH, 0x00); // High byte is always 0 for zero page
      this->half_cycle++;
      return pins;

    case 2:
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;

    case 3:
      /* PHI1: Load data and perform operations */
      this->bus_load_reg(REG_ABH, pins);
      this->inc(REG_PC);
      this->transition_to_operation();
    }
  }
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
