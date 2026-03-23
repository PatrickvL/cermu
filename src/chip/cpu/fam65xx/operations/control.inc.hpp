/*
 * control.inc.hpp - Control Flow Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "chip/cpu/fam65xx/operations/inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// JUMP OPERATIONS
// ============================================================================

/* JMP - Jump
 * Self-addressed (AM::NON) — cases 0-3 perform inline ABS address fetch
 * to avoid adding per-tick overhead to all other opcodes using am_abs.
 * The default case handles chaining from am_ind/am_abi where the target
 * address is already resolved in AB.
 */
bus_state_t op_jmp(bus_state_t pins) {
  trace_operation(__func__);

  // When chained from am_ind/am_abi the target address is already in AB.
  // Only perform the inline ABS fetch (cases 0-3) for the AM::NON path
  // (opcode $4C — absolute JMP with no separate addressing-mode handler).
  if (this->opcode_entry.am_index == to_index(AM::NON)) {
    switch (this->half_cycle) {
    case 0:
      /* PHI2: Read low byte of target address from PC */
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 1:
      /* PHI1: Load low byte */
      this->bus_load_reg(ABL, pins);
      ++regs_[PC];
      this->half_cycle++;
      return pins;

    case 2:
      /* PHI2: Read high byte of target address from PC */
      pins = this->bus_setup_read<Addr::PC>(pins);
      return pins;
    case 3:
      /* PHI1: Load high byte into AB, then jump */
      this->bus_load_reg(ABH, pins);
      break;
    }
  }

  regs_[PC] = regs_[AB];
  this->transition_to_fetch();
  return pins;
}

/* JML - Jump long
 * Addressing modes ABL (0x5C) and ABI (0xDC) handle address calculation.
 * This operation handler only performs the jump once addressing is complete.
 */
bus_state_t op_jml(bus_state_t pins) {
  trace_operation(__func__);
  // Zero-cycle operation: chained from addressing mode's PHI1 phase.
  // Addressing mode already set AB to target address.
  regs_[PC] = regs_[AB];
  this->transition_to_fetch();
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
    this->bus_load_reg(ABL, pins);
    ++regs_[PC];
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
    pins = this->bus_setup_write<Addr::SP>(pins, PCH);
    return pins;
  case 5:
    /* PHI1: Decrement SP */
    this->dec_stack();
    this->half_cycle++;
    return pins;

  case 6:
    /* PHI2: Push PCL (low byte of return address) to stack */
    pins = this->bus_setup_write<Addr::SP>(pins, PCL);
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
    this->bus_load_reg(ABH, pins);  // Load high byte into ABH
    regs_[PC] = regs_[AB];
    this->transition_to_fetch();
  }
  return pins;
}

/* RTS - Return from Subroutine */
bus_state_t op_rts(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
  case 0:
    /* PHI2 T1: Dummy read from PC+1 (discarded) */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1 T1: No side effects */
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2 T2: Read from $0100+S (dummy — value discarded, bus shows old SP) */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 3:
    /* PHI1 T2: Increment SP */
    this->inc_stack();
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2 T3: Pull PCL from stack ($0100+S) */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 5:
    /* PHI1 T3: Load PCL and increment SP */
    this->bus_load_reg(PCL, pins);
    this->inc_stack();
    this->half_cycle++;
    return pins;

  case 6:
    /* PHI2 T4: Pull PCH from stack ($0100+S) */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 7:
    /* PHI1 T4: Load PCH */
    this->bus_load_reg(PCH, pins);
    this->half_cycle++;
    return pins;

  case 8:
    /* PHI2 T5: Dummy read from reconstructed PC address */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 9:
    /* PHI1 T5: Increment PC (fix JSR's PC-1 push) and transition */
    ++regs_[PC];
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
        /* CRITICAL: Only set interrupt type to BRK if NO interrupt is active
         * When hardware IRQ/NMI hijacks BRK execution, keep the original interrupt type
         * Only increment PC for software BRK (skip signature byte)
         * Hardware IRQ/NMI must NOT advance PC - the interrupted instruction
         * must re-execute after RTI
         */
        if (this->brk_is_software_) {
          ++regs_[PC]; // BRK only: skip signature byte
          if (this->active_interrupt == FAM65XX_INT_NONE) {
            this->active_interrupt = FAM65XX_INT_BRK;
          }
        }
        this->half_cycle++;
        return pins;

      case 2:
        // PHI2: Push program bank register
        // RESET suppresses writes — R/W held high (read) during stack pushes
        if (this->active_interrupt == FAM65XX_INT_RESET)
          pins = this->bus_setup_dummy<Addr::SP>(pins);
        else
          pins = this->bus_setup_write<Addr::SP>(pins, PBR);
        return pins;
      case 3:
        this->dec_stack();
        this->half_cycle++;
        return pins;

      case 4:
        // PHI2: Push PC high byte
        if (this->active_interrupt == FAM65XX_INT_RESET)
          pins = this->bus_setup_dummy<Addr::SP>(pins);
        else
          pins = this->bus_setup_write<Addr::SP>(pins, PCH);
        return pins;
      case 5:
        this->dec_stack();
        this->half_cycle++;
        return pins;

      case 6:
        // PHI2: Push PC low byte
        if (this->active_interrupt == FAM65XX_INT_RESET)
          pins = this->bus_setup_dummy<Addr::SP>(pins);
        else
          pins = this->bus_setup_write<Addr::SP>(pins, PCL);
        return pins;
      case 7:
        this->dec_stack();
        this->half_cycle++;
        return pins;

      case 8:
        // PHI2: Push processor status register (no B flag in native mode)
        if (this->active_interrupt == FAM65XX_INT_RESET)
          pins = this->bus_setup_dummy<Addr::SP>(pins);
        else
          pins = this->bus_setup_write<Addr::SP>(pins, P);
        return pins;
      case 9:
        this->dec_stack();
        this->set_flag(FLAG_I);   // Disable interrupts
        this->clear_flag(FLAG_D); // Clear decimal mode
        regs_[AB] = this->get_vector_addr();
        this->half_cycle++;
        return pins;

      case 10:
        // Read interrupt vector low byte (always from bank 0 via ZBR)
        pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
        return pins;
      case 11:
        this->bus_load_reg(PCL, pins);
        ++regs_[AB];
        this->half_cycle++;
        return pins;
    
      case 12:
        // Read interrupt vector high byte (always from bank 0 via ZBR)
        pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
        return pins;
      case 13:
        this->bus_load_reg(PCH, pins);
        regs_[PBR] = 0; // Clear PBR for interrupt vectors
        // DON'T clear active_interrupt here - keep it set so nested interrupts are blocked
        // It will be cleared by RTI when the interrupt handler completes
        this->transition_to_fetch();
        return pins;
      }
      return pins;
    }
  }
  
  // 6502/65C02/65C816 Emulation Mode: Standard 3-byte stack frame (7 cycles total)
  //
  // T0: Opcode fetch (handled by dispatch, not here)
  // T1: Read signature byte, set interrupt type
  // T2: Push PCH to stack
  // T3: Push PCL to stack
  // T4: Push P|B|U to stack
  // T5: Read interrupt vector low byte
  // T6: Read interrupt vector high byte
  //
  switch (this->half_cycle) {
  case 0:
    /* PHI2: T1 — Read signature byte (BRK has optional signature byte) */
    pins = this->bus_setup_dummy<Addr::PC>(pins);
    return pins;
  case 1:
    /* PHI1: Set interrupt type and optionally increment PC */
    /* CRITICAL: Only set interrupt type to BRK if NO interrupt is active
     * When hardware IRQ/NMI hijacks BRK execution, keep the original interrupt type
     * Only increment PC for software BRK (skip signature byte)
     * Hardware IRQ/NMI must NOT advance PC - the interrupted instruction
     * must re-execute after RTI
     */
    if (this->brk_is_software_) {
      ++regs_[PC]; // BRK only: skip signature byte
      // Set active_interrupt to BRK only if NMI hasn't already hijacked
      // the vector.  If NMI overrode active_interrupt between dispatch and
      // here (case 0 PHI2), we keep NMI so the NMI vector is used.
      if (this->active_interrupt == FAM65XX_INT_NONE) {
        this->active_interrupt = FAM65XX_INT_BRK;
      }
    }
    /* Higher priority interrupts (NMI, RESET, IRQ) should NOT be overridden */
    this->half_cycle++;
    return pins;

  case 2:
    /* PHI2: T2 — Push PCH to stack */
    /* RESET suppresses writes — R/W held high (read) during stack pushes */
    if (this->active_interrupt == FAM65XX_INT_RESET)
      pins = this->bus_setup_dummy<Addr::SP>(pins);
    else
      pins = this->bus_setup_write<Addr::SP>(pins, PCH);
    return pins;
  case 3:
    /* PHI1: Decrement SP */
    this->dec_stack();
    this->half_cycle++;
    return pins;

  case 4:
    /* PHI2: T3 — Push PCL to stack */
    if (this->active_interrupt == FAM65XX_INT_RESET)
      pins = this->bus_setup_dummy<Addr::SP>(pins);
    else
      pins = this->bus_setup_write<Addr::SP>(pins, PCL);
    return pins;
  case 5: {
    /* PHI1: Decrement SP and prepare status register for stack push */
    this->dec_stack();
    /* CRITICAL FIX: B flag distinguishes BRK from hardware interrupts
     * - BRK instruction (software): B flag SET (active_interrupt == FAM65XX_INT_BRK)
     * - Hardware IRQ/NMI: B flag CLEAR (active_interrupt == FAM65XX_INT_IRQ/NMI)
     * The KERNAL ROM tests this flag to route to correct handler vector:
     * - B flag SET: JMP ($0316) - BRK handler
     * - B flag CLEAR: JMP ($0314) - IRQ handler
     */
    uint8_t status_flags = regs_[P] | FLAG_U;  // U flag always set
    if (this->brk_is_software_) {
      status_flags |= FLAG_B;  // Set B flag only for actual BRK instruction
    }
    // For hardware IRQ/NMI: B flag remains clear (not set)
    regs_[DL] = status_flags;
    this->half_cycle++;
    return pins;
  }

  case 6:
    /* PHI2: T4 — Push P|B|U to stack (B flag set for BRK) */
    if (this->active_interrupt == FAM65XX_INT_RESET)
      pins = this->bus_setup_dummy<Addr::SP>(pins);
    else
      pins = this->bus_setup_write<Addr::SP>(pins, DL);
    return pins;
  case 7:
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
    
    // CRITICAL FIX: Clear interrupt shift register immediately after setting I flag
    // This prevents the shift register from accumulating more interrupt samples
    // while we're reading the vector (cycles 8-11). Without this, IRQs sampled
    // during vector read will trigger immediately after BRK completes.
    this->interrupt_shift_register = 0;

    // NMI VECTOR HIJACKING — check at vector-determination cycle.
    //
    // On real 6502 hardware the vector address MUX checks the NMI internal
    // edge-detect flip-flop at T4 (the cycle where the vector address is
    // loaded).  However, the flip-flop state visible to the MUX is sampled
    // from the PREVIOUS cycle's edge detector output — edges detected
    // during T4 itself are too late.
    //
    // In our model, sample_nmi_pin() runs between PHI2 and PHI1 of each
    // cycle (including T4).  nmi_edge_latch is updated immediately, so it
    // reflects T4's edge.  nmi_output_latch_ is updated at each PHI2 from
    // the current nmi_edge_latch, giving it a 1-cycle pipeline delay.
    // At case 7 (T4 PHI1), nmi_output_latch_ was last written at case 6
    // (T4 PHI2) using the edge_latch set by T3's sample_nmi_pin — matching
    // the real hardware's T0-T3 detection window (5 in-BRK cycles total
    // with deferred hijack's T0 dummy fetch).
    if constexpr (has_nmi_line()) {
      bool nmi_pending = (this->nmi_output_latch_ != false);
      if (nmi_pending && this->active_interrupt != FAM65XX_INT_NMI) {
        this->active_interrupt = FAM65XX_INT_NMI;
      }
    }

    // Clear NMI edge latch ONLY when NMI is actually being serviced.
    // On real 6502 hardware the edge-detect flip-flop is cleared during the
    // NMI vector fetch — NOT during IRQ or BRK sequences.  Clearing it
    // unconditionally causes NMI deadlocks: if an NMI falling edge arrives
    // during an IRQ's op_brk sequence, the latch gets erased, the CPU never
    // detects the NMI, and the NMI line stays LOW permanently (CIA ICR never
    // read → pending_bus_lines never released).
    if constexpr (has_nmi_line()) {
      if (this->active_interrupt == FAM65XX_INT_NMI) {
        this->nmi_edge_latch = 0;
        this->nmi_output_latch_ = false;
      }
    }
    // Reset NMI edge detection using INVERTED convention:
    // Pin HIGH (inactive) → inverted = 0, Pin LOW (asserted) → inverted = 1
    this->nmi_prev = (pins & FAM65XX_NMI) ? 0 : 1;
    
    regs_[AB] = this->get_vector_addr();
    this->half_cycle++;
    return pins;

  case 8:
    /* PHI2: T5 — Read interrupt vector low byte (always from bank 0 via ZBR for 65C816) */
    pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
    return pins;
  case 9:
    /* PHI1: Load vector low byte into PCL, then increment vector address in AB */
    this->bus_load_reg(PCL, pins);
    ++regs_[AB];
    this->half_cycle++;
    return pins;

  case 10:
    /* PHI2: T6 — Read interrupt vector high byte (always from bank 0 via ZBR for 65C816) */
    pins = this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
    return pins;
  case 11:
    /* PHI1: Construct PC from vector bytes and clear PBR if needed */
    this->bus_load_reg(PCH, pins);
    /* 65C816: Clear PBR for interrupt vectors in emulation mode */
    if constexpr (has_wide_registers()) {
      regs_[PBR] = 0;
    }
    
    // CRITICAL FIX: Clear active_interrupt NOW, before starting the handler!
    // This allows the interrupt handler to:
    // 1. Execute BRK instructions (used by BASIC ROM for error handling)
    // 2. Be interrupted by higher-priority interrupts (NMI)
    // 3. Properly nest interrupt handling
    // The I flag (set at cycle 9) blocks new IRQs, but not NMI or software BRK.
    this->active_interrupt = FAM65XX_INT_NONE;
    
    // Reset the NMI serviceability pipeline.  The edge latch is PRESERVED so a
    // pending NMI will still fire, but the output latch is cleared to add one
    // fresh pipeline cycle before NMI becomes serviceable.  Without this, an NMI
    // edge detected during the vector-read cycles (T5/T6) would have already
    // propagated through the pipeline and fire immediately at the next fetch
    // boundary — 1 cycle too early.  On real hardware, NMI detected at T5 of
    // BRK fires after the FIRST instruction of the handler, not before it.
    if constexpr (has_nmi_line()) {
      this->nmi_output_latch_ = false;
    }
    
    // NOTE: Shift register was already cleared at cycle 9 after setting I flag
    
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
        regs_[P] = new_p | FLAG_B | FLAG_U;
      } else {
        // Native mode: load value as-is (B becomes X flag, U becomes M flag)
        uint8_t old_p = regs_[P];
        regs_[P] = new_p;
        
        // X flag (bit 4): When switching from 16-bit to 8-bit index mode, clear high bytes
        if ((new_p & FLAG_X) && !(old_p & FLAG_X)) {
          // Switching index registers from 16-bit to 8-bit: clear XH and YH
          regs_[XH] = 0x00;
          regs_[YH] = 0x00;
        }
      }
    } else {
      // 6502/6510/65C02: Mask off bit 4 (B is phantom), set bit 5 (U always 1)
      regs_[P] = (new_p & ~FLAG_B) | FLAG_U;
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
    this->bus_load_reg(PCL, pins);
    this->inc_stack();
    this->half_cycle++;
    return pins;

  case 8:
    /* PHI2: Pull PCH from stack */
    pins = this->bus_setup_read<Addr::SP>(pins);
    return pins;
  case 9:
    /* PHI1: Load PCH, clear active_interrupt, and transition */
    this->bus_load_reg(PCH, pins);
    // Clear active_interrupt when RTI completes - this re-enables interrupt detection
    this->active_interrupt = FAM65XX_INT_NONE;
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

#include "chip/cpu/fam65xx/operations/inc_lint_prevention_footer.hpp"
