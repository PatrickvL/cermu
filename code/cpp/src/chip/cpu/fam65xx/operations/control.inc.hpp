/*
 * control.inc.hpp - Control Flow Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// JUMP OPERATIONS
// ============================================================================

/* JMP - Jump
 * All addressing modes (ABS, IND, ABI) are now handled by proper addressing
 * mode handlers. This operation handler only performs the jump once addressing
 * is complete.
 */
bus_state_t op_jmp(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
    case 0: // PHI2: Dummy bus cycle
      // Addressing mode has already set up AB register with target address
      return pins;
    case 1: // PHI1: Set PC and transition
      this->set(REG_PC, this->get(REG_AB));
      this->transition_to_fetch();
      return pins;
  }
  return pins;
}

/* JML - Jump long
 * All addressing modes (ABS, IND, ABI) are now handled by proper addressing
 * mode handlers. This operation handler only performs the jump once addressing
 * is complete.
 */
bus_state_t op_jml(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
    case 0: // PHI2: Dummy bus cycle
      // Addressing mode has already set up AB register with target address
      return pins;
    case 1: // PHI1: Set PC and transition
      this->set(REG_PC, this->get(REG_AB));
      this->transition_to_fetch();
      return pins;
  }
  return pins;
}

/* JSR - Jump to Subroutine */
bus_state_t op_jsr(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Read low byte of target address from PC directly to ABL */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: Load data and perform operations */
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Dummy read from stack pointer (internal operation) */
    pins = this->bus_setup_dummy<Addr::SP>(pins);
    return pins;
  case 3:
    /* PHI1: Increment cycle index */
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Push PCH (high byte of return address) to stack */
    pins = this->bus_setup_write<Addr::SP>(pins, REG_PCH);
    return pins;
  case 5:
    /* PHI1: Decrement SP */
    this->dec_stack();
    this->half_cycle++;
    return pins;

  case 6:
    /* PHI2: Push PCL (low byte of return address) to stack */
    pins = this->bus_setup_write<Addr::SP>(pins, REG_PCL);
    return pins;
  case 7:
    /* PHI1: Decrement SP */
    this->dec_stack();
    this->half_cycle++;
    return pins;

  case 8:
    /* PHI2: Read high byte of target address */
    pins = this->bus_setup_read<Addr::PC>(pins);
    return pins;
  case 9:
    /* PHI1: Load high byte, set PC to target address, and transition */
    this->bus_load_reg(REG_ABH, pins);  // Load high byte into ABH
    this->set(REG_PC, this->get(REG_AB));
    this->transition_to_fetch();
  }
  return pins;
}

/* RTS - Return from Subroutine */
bus_state_t op_rts(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Dummy read from PC */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: Increment SP */
    this->inc_stack();
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Pull PCL from stack */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 3:
    /* PHI1: Load PCL and increment SP */
    this->bus_load_reg(REG_PCL, pins);
    this->inc_stack();
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Pull PCH from stack */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 5:
    /* PHI1: Load PCH */
    this->bus_load_reg(REG_PCH, pins);
    this->half_cycle++;
    return pins;
    
  case 6:
    /* PHI2: Dummy read from reconstructed address  */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 7:
    /* PHI1: Increment PC and transition */
    this->inc(REG_PC);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

// ============================================================================
// INTERRUPT OPERATIONS
// ============================================================================

/* Helper function to get interrupt vector address based on active_interrupt and
 * CPU type
 *
 * Uses direct enum indexing for clean, efficient vector determination.
 * Higher priority interrupts use higher enum values in active_interrupt.
 *
 * INTERRUPT TYPES AND VECTOR TABLE LAYOUT:
 * ========================================
 * Enum | Interrupt Type      | Priority | 6502/65C02 | 65C816 Emul | 65C816
 * Native
 * -----|---------------------|----------|------------|-------------|---------------
 *  0   | FAM65XX_INT_NONE    |  N/A     |   0xFFFE   |   0xFFFE    |    0xFFE6
 *  1   | FAM65XX_INT_BRK     |  Lowest  |   0xFFFE   |   0xFFFE    |    0xFFE6
 *  2   | FAM65XX_INT_IRQ     |    ↑     |   0xFFFE   |   0xFFFE    |    0xFFEE
 *  3   | FAM65XX_INT_COP#    |    |     |   N/A*     |   0xFFF4    |    0xFFE4
 *  4   | FAM65XX_INT_NMI     |    |     |   0xFFFA   |   0xFFFA    |    0xFFEA
 *  5   | FAM65XX_INT_ABORT#  |    ↓     |   N/A*     |   0xFFF8    |    0xFFE8
 *  6   | FAM65XX_INT_RESET   | Highest  |   0xFFFC   |   0xFFFC    |    0xFFFC
 *
 * # = Only available on 65C816 CPU's
 * * = Not supported on this CPU, conditional compilation prevents usage
 *
 * LOOKUP METHOD:
 * ==============
 * The active_interrupt enum serves directly as an index into vector tables.
 * This eliminates the need for bit manipulation and provides clean lookup.
 */
uint16_t get_vector_addr() const {
  // Vector lookup tables indexed by interrupt enum values
  static constexpr uint16_t standard_vectors[7] = {
      0xFFFE, // FAM65XX_INT_NONE: Default to BRK/IRQ vector
      0xFFFE, // FAM65XX_INT_BRK: Software interrupt and default/fallback
      0xFFFE, // FAM65XX_INT_IRQ: Hardware interrupt
      0xFFF4, // FAM65XX_INT_COP: CoProcessor (65C816 emulation mode)
      0xFFFA, // FAM65XX_INT_NMI: Non-maskable interrupt
      0xFFF8, // FAM65XX_INT_ABORT: Memory abort (65C816 emulation mode)
      0xFFFC  // FAM65XX_INT_RESET: Reset vector
  };

  static constexpr uint16_t native_65C816_vectors[7] = {
      0xFFE6, // FAM65XX_INT_NONE: Default to BRK vector (native mode)
      0xFFE6, // FAM65XX_INT_BRK: Software interrupt (native mode)
      0xFFEE, // FAM65XX_INT_IRQ: Hardware interrupt (native mode)
      0xFFE4, // FAM65XX_INT_COP: CoProcessor (native mode)
      0xFFEA, // FAM65XX_INT_NMI: Non-maskable interrupt (native mode)
      0xFFE8, // FAM65XX_INT_ABORT: Memory abort (native mode)
      0xFFFC  // FAM65XX_INT_RESET: Reset vector (same in both modes)
  };

  // Use active_interrupt directly as table index
  const int interrupt_index = static_cast<int>(this->active_interrupt);

  // 65C816 native mode uses different vectors
  if constexpr (has_wide_registers()) {
    if (!this->in_emulation_mode()) {
      return native_65C816_vectors[interrupt_index];
    }
  }

  return standard_vectors[interrupt_index];
}

/* BRK - Break (Software Interrupt)
 * 
 * Unified implementation handling both native and emulation modes.
 * Native mode (65C816): Pushes 4 bytes (PBR, PCH, PCL, P) - uses all cycles
 * Emulation mode: Pushes 3 bytes (PCH, PCL, P|B) - skips PBR push cycles
 */
bus_state_t op_brk(bus_state_t pins) {
  trace_operation(__func__);
  
  if constexpr (has_wide_registers()) {
    // 65C816: Check if we're in native mode
    if (!this->in_emulation_mode()) {
      // Native mode: 4-byte stack frame (PBR, PCH, PCL, P)
      switch (this->half_cycle) {
      case 0:
        // PHI2: Read signature byte (ignored, but must be read for timing)
        pins = this->bus_setup_dummy<Addr::PC>(pins);
        return pins;
      case 1:
        this->inc(REG_PC);
        // Set interrupt type to BRK (unconditionally for software interrupt)
        // Only higher priority interrupts (NMI, RESET) should override BRK
        if (this->active_interrupt < FAM65XX_INT_COP) {
          this->active_interrupt = FAM65XX_INT_BRK;
        }
        this->half_cycle++;
        return pins;

      case 2:
        // PHI2: Push program bank register
        pins = this->bus_setup_write<Addr::SP>(pins, REG_PBR);
        return pins;
      case 3:
        this->dec_stack();
        this->half_cycle++;
        return pins;

      case 4:
        // PHI2: Push PC high byte
        pins = this->bus_setup_write<Addr::SP>(pins, REG_PCH);
        return pins;
      case 5:
        this->dec_stack();
        this->half_cycle++;
        return pins;

      case 6:
        // PHI2: Push PC low byte
        pins = this->bus_setup_write<Addr::SP>(pins, REG_PCL);
        return pins;
      case 7:
        this->dec_stack();
        this->half_cycle++;
        return pins;

      case 8:
        // PHI2: Push processor status register (no B flag in native mode)
        pins = this->bus_setup_write<Addr::SP>(pins, REG_P);
        return pins;
      case 9:
        this->dec_stack();
        this->set_flag(FLAG_I);   // Disable interrupts
        this->clear_flag(FLAG_D); // Clear decimal mode
        this->set(REG_AB, this->get_vector_addr());
        this->half_cycle++;
        return pins;

      case 10:
        // Read interrupt vector low byte (always from bank 0 via ZBR)
        pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
        return pins;
      case 11:
        this->bus_load_reg(REG_PCL, pins);
        this->inc(REG_AB);
        this->half_cycle++;
        return pins;
    
      case 12:
        // Read interrupt vector high byte (always from bank 0 via ZBR)
        pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
        return pins;
      case 13:
        this->bus_load_reg(REG_PCH, pins);
        this->set(REG_PBR, 0); // Clear PBR for interrupt vectors
        this->active_interrupt = FAM65XX_INT_NONE;
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }
  
  // 6502/65C02/65C816 Emulation Mode: Standard 3-byte stack frame (7 cycles total)
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Dummy read from PC+1 (BRK has optional signature byte) */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: Increment PC and set interrupt type */
    this->inc(REG_PC);
    /* Set interrupt type to BRK (unconditionally for software interrupt) */
    /* Only higher priority interrupts (NMI, RESET) should override BRK */
    if (this->active_interrupt < FAM65XX_INT_COP) {
      this->active_interrupt = FAM65XX_INT_BRK;
    }
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Dummy internal operation cycle (stack pointer setup) */
    pins = this->bus_setup_dummy<Addr::SP>(pins);
    return pins;
  case 3:
    /* PHI1: Internal operation */
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Push PCH to stack */
    pins = this->bus_setup_write<Addr::SP>(pins, REG_PCH);
    return pins;
  case 5:
    /* PHI1: Decrement SP */
    this->dec_stack();
    this->half_cycle++;
    return pins;

  case 6:
    /* PHI2: Push PCL to stack */
    pins = this->bus_setup_write<Addr::SP>(pins, REG_PCL);
    return pins;
  case 7:
    /* PHI1: Decrement SP */
    this->dec_stack();
    this->set(REG_DL, this->get(REG_P) | FLAG_B | FLAG_U);
    this->half_cycle++;
    return pins;

  case 8:
    /* PHI2: Push P|B|U to stack (B flag set for BRK) */
    pins = this->bus_setup_write<Addr::SP>(pins, REG_DL);
    return pins;
  case 9:
    /* PHI1: Decrement SP, set interrupt flags, get vector address */
    this->dec_stack();
    /* Set interrupt disable flag - processor specific behavior */
    if constexpr (has_nmos_bugs()) {
      /* NMOS 6502 always sets I flag on BRK */
      set_flag(FLAG_I);
    } else {
      /* CMOS 65C02 sets I flag and clears D flag on BRK/IRQ/NMI */
      set_flag(FLAG_I);
      clear_flag(FLAG_D);
    }
    this->set(REG_AB, this->get_vector_addr());
    this->half_cycle++;
    return pins;

  case 10:
    /* PHI2: Read interrupt vector low byte (always from bank 0 via ZBR for 65C816) */
    pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
    return pins;
  case 11:
    /* PHI1: Load vector low byte into PCL, then increment vector address in AB */
    this->bus_load_reg(REG_PCL, pins);
    this->inc(REG_AB);
    this->half_cycle++;
    return pins;

  case 12:
    /* PHI2: Read interrupt vector high byte (always from bank 0 via ZBR for 65C816) */
    pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
    return pins;
  case 13:
    /* PHI1: Construct PC from vector bytes and clear PBR if needed */
    this->bus_load_reg(REG_PCH, pins);
    /* 65C816: Clear PBR for interrupt vectors in emulation mode */
    if constexpr (has_wide_registers()) {
      this->set(REG_PBR, 0);
    }
    this->active_interrupt = FAM65XX_INT_NONE;
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

/* RTI - Return from Interrupt */
bus_state_t op_rti(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2: Dummy read from PC */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: Dummy read from current stack pointer */
    pins = this->bus_setup_dummy<Addr::SP>(pins);
    return pins;
  case 3:
    /* PHI1: Increment SP */
    this->inc_stack();
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: Pull P from stack (clear B, set U) */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 5: {
    /* PHI1: Load processor status from stack
     * 6502/65C02: Bit 5 (U) always 1, bit 4 (B) is NOT a real flag - mask it off
     * 65C816 emulation: Set both bits 4 and 5 (B and U flags always 1)
     * 65C816 native: Load all bits as-is (bits 4 and 5 have different meanings: X and M)
     */
    uint8_t new_p = this->bus_get_data(pins);
    if constexpr (has_wide_registers()) {
      if (this->in_emulation_mode()) {
        // 65C816 emulation mode: set both FLAG_B (bit 4) and FLAG_U (bit 5)
        this->set(REG_P, new_p | FLAG_B | FLAG_U);
      } else {
        // Native mode: load value as-is (B becomes X flag, U becomes M flag)
        uint8_t old_p = this->get(REG_P);
        this->set(REG_P, new_p);
        
        // X flag (bit 4): When switching from 16-bit to 8-bit index mode, clear high bytes
        if ((new_p & FLAG_X) && !(old_p & FLAG_X)) {
          // Switching index registers from 16-bit to 8-bit: clear XH and YH
          this->set(REG_XH, 0x00);
          this->set(REG_YH, 0x00);
        }
      }
    } else {
      // 6502/6510/65C02: Mask off bit 4 (B is phantom), set bit 5 (U always 1)
      this->set(REG_P, (new_p & ~FLAG_B) | FLAG_U);
    }
    this->inc_stack();
    this->half_cycle++;
    return pins;
  }

  case 6:
    /* PHI2: Pull PCL from stack */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 7:
    /* PHI1: Load PCL and increment SP */
    this->bus_load_reg(REG_PCL, pins);
    this->inc_stack();
    this->half_cycle++;
    return pins;

  case 8:
    /* PHI2: Pull PCH from stack */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 9:
    /* PHI1: Load PCH and transition */
    this->bus_load_reg(REG_PCH, pins);
    this->transition_to_fetch();
    return pins;
  }
  return pins;
}

// ============================================================================
// 65C02 ENHANCED CONTROL OPERATIONS
// ============================================================================
// NOTE: 65C816 long operations (op_jsl, op_rtl) are implemented in wide.inc.hpp
// NOTE: 65C02 branch always (op_bra) is implemented in cmos.inc.hpp
// ============================================================================

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
