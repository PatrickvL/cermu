/*
 * transfers.inc.hpp - Register Transfer Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "chip/cpu/fam65xx/operations/inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// REGISTER TRANSFER OPERATIONS
// ============================================================================

/* TAX - Transfer A to X */
bus_state_t op_tax(bus_state_t pins) {
  trace_operation(__func__);
  
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
      
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (!this->in_emulation_mode()) {
          // Native mode: handle M and X flags for register sizes
          bool acc_16bit = this->is_accumulator_16bit();
          bool index_16bit = this->is_index_16bit();

          if (acc_16bit && index_16bit) {
            // 16-bit A to 16-bit X
            uint16_t value = regs_[A16];
            this->set_x_register(value);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
          } else if (acc_16bit && !index_16bit) {
            // 16-bit A to 8-bit X (transfer low byte)
            uint8_t value = regs_[A];
            regs_[X] = value;
            this->update_nz_flags(value);
          } else if (!acc_16bit && index_16bit) {
            // 8-bit A to 16-bit X (zero-extend)
            uint16_t value = regs_[A];
            this->set_x_register(value);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, false); // High bit is always 0
          } else {
            // 8-bit A to 8-bit X
            uint8_t value = regs_[A];
            regs_[X] = value;
            this->update_nz_flags(value);
          }
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit operation
      uint8_t value = regs_[A];
      regs_[X] = value;
      this->update_nz_flags(value);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* TAY - Transfer A to Y */
bus_state_t op_tay(bus_state_t pins) {
  trace_operation(__func__);
  
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
      
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (!this->in_emulation_mode()) {
          // Native mode: handle M and X flags for register sizes
          bool acc_16bit = this->is_accumulator_16bit();
          bool index_16bit = this->is_index_16bit();

          if (acc_16bit && index_16bit) {
            // 16-bit A to 16-bit Y
            uint16_t value = regs_[A16];
            this->set_y_register(value);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
          } else if (acc_16bit && !index_16bit) {
            // 16-bit A to 8-bit Y (transfer low byte)
            uint8_t value = regs_[A];
            regs_[Y] = value;
            this->update_nz_flags(value);
          } else if (!acc_16bit && index_16bit) {
            // 8-bit A to 16-bit Y (zero-extend)
            uint16_t value = regs_[A];
            this->set_y_register(value);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, false); // High bit is always 0
          } else {
            // 8-bit A to 8-bit Y
            uint8_t value = regs_[A];
            regs_[Y] = value;
            this->update_nz_flags(value);
          }
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit operation
      uint8_t value = regs_[A];
      regs_[Y] = value;
      this->update_nz_flags(value);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* TSX - Transfer S to X */
bus_state_t op_tsx(bus_state_t pins) {
  trace_operation(__func__);
  
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
      
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (this->is_index_16bit()) {
          // Native mode, 16-bit X register - transfer 16-bit stack pointer
          uint16_t value = regs_[SP];
          this->set_x_register(value);
          this->update_flag(FLAG_Z, value == 0);
          this->update_flag(FLAG_N, (value & 0x8000) != 0);
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit operation
      uint8_t value = regs_[S];
      regs_[X] = value;
      this->update_nz_flags(value);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* TXA - Transfer X to A */
bus_state_t op_txa(bus_state_t pins) {
  trace_operation(__func__);
  
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
      
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (!this->in_emulation_mode()) {
          // Native mode: handle M and X flags for register sizes
          bool acc_16bit = this->is_accumulator_16bit();
          bool index_16bit = this->is_index_16bit();

          if (acc_16bit && index_16bit) {
            // 16-bit X to 16-bit A
            uint16_t value = this->get_x_register();
            regs_[A16] = value;
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
          } else if (acc_16bit && !index_16bit) {
            // 8-bit X to 16-bit A (zero-extend)
            uint16_t value = regs_[X];
            regs_[A16] = value;
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, false); // High bit is always 0
          } else if (!acc_16bit && index_16bit) {
            // 16-bit X to 8-bit A (transfer low byte)
            uint8_t value = regs_[X];
            regs_[A] = value;
            this->update_nz_flags(value);
          } else {
            // 8-bit X to 8-bit A
            uint8_t value = regs_[X];
            regs_[A] = value;
            this->update_nz_flags(value);
          }
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit operation
      uint8_t value = regs_[X];
      regs_[A] = value;
      this->update_nz_flags(value);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* TXS - Transfer X to S */
bus_state_t op_txs(bus_state_t pins) {
  trace_operation(__func__);
  
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
      
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (!this->in_emulation_mode()) {
          // Native mode - transfer can be 8-bit or 16-bit based on X flag
          if (this->is_index_16bit()) {
            // 16-bit X register - transfer full 16-bit value to stack pointer
            uint16_t value = this->get_x_register();
            regs_[SP] = value;
          } else {
            // 8-bit X register - transfer low byte to stack pointer
            uint8_t value = regs_[X];
            regs_[S] = value;
          }
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit operation
      uint8_t value = regs_[X];
      regs_[S] = value;
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* TYA - Transfer Y to A */
bus_state_t op_tya(bus_state_t pins) {
  trace_operation(__func__);
  
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
      
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (!this->in_emulation_mode()) {
          // Native mode: handle M and X flags for register sizes
          bool acc_16bit = this->is_accumulator_16bit();
          bool index_16bit = this->is_index_16bit();

          if (acc_16bit && index_16bit) {
            // 16-bit Y to 16-bit A
            uint16_t value = this->get_y_register();
            regs_[A16] = value;
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
          } else if (acc_16bit && !index_16bit) {
            // 8-bit Y to 16-bit A (zero-extend)
            uint16_t value = regs_[Y];
            regs_[A16] = value;
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, false); // High bit is always 0
          } else if (!acc_16bit && index_16bit) {
            // 16-bit Y to 8-bit A (transfer low byte)
            uint8_t value = regs_[Y];
            regs_[A] = value;
            this->update_nz_flags(value);
          } else {
            // 8-bit Y to 8-bit A
            uint8_t value = regs_[Y];
            regs_[A] = value;
            this->update_nz_flags(value);
          }
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit operation
      uint8_t value = regs_[Y];
      regs_[A] = value;
      this->update_nz_flags(value);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

// ============================================================================
// REGISTER INCREMENT/DECREMENT OPERATIONS
// ============================================================================

/* INX - Increment X */
bus_state_t op_inx(bus_state_t pins) {
  trace_operation(__func__);
  
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (this->is_index_16bit()) {
          // Native mode, 16-bit X register - perform 16-bit INX
          uint16_t value = this->get_x_register();
          value++;
          this->set_x_register(value);
          // Update flags for 16-bit operation
          this->update_flag(FLAG_Z, value == 0);
          this->update_flag(FLAG_N, (value & 0x8000) != 0);
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit INX operation
      ++regs_[X];
      this->update_nz_flags(regs_[X]);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* INY - Increment Y */
bus_state_t op_iny(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (this->is_index_16bit()) {
          // Native mode, 16-bit Y register - perform 16-bit INY
          uint16_t value = this->get_y_register();
          value++;
          this->set_y_register(value);
          // Update flags for 16-bit operation
          this->update_flag(FLAG_Z, value == 0);
          this->update_flag(FLAG_N, (value & 0x8000) != 0);
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit INY operation
      ++regs_[Y];
      this->update_nz_flags(regs_[Y]);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* DEX - Decrement X */
bus_state_t op_dex(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (this->is_index_16bit()) {
          // Native mode, 16-bit X register - perform 16-bit DEX
          uint16_t value = this->get_x_register();
          value--;
          this->set_x_register(value);
          // Update flags for 16-bit operation
          this->update_flag(FLAG_Z, value == 0);
          this->update_flag(FLAG_N, (value & 0x8000) != 0);
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit DEX operation
      --regs_[X];
      this->update_nz_flags(regs_[X]);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

/* DEY - Decrement Y */
bus_state_t op_dey(bus_state_t pins) {
  trace_operation(__func__);
  switch (this->half_cycle) {
    case 0: // PHI2: Bus setup - dummy cycle for non-optimized CPUs
      // OPTIMIZED_CYCLES: transition_to_opcode sets half_cycle=1, skipping this
      return this->bus_setup_dummy<Addr::PC>(pins);
    case 1: // PHI1: Operation
      if constexpr (has_wide_registers()) {
        if (this->is_index_16bit()) {
          // Native mode, 16-bit Y register - perform 16-bit DEY
          uint16_t value = this->get_y_register();
          value--;
          this->set_y_register(value);
          // Update flags for 16-bit operation
          this->update_flag(FLAG_Z, value == 0);
          this->update_flag(FLAG_N, (value & 0x8000) != 0);
          this->transition_to_fetch();
          return pins;
        }
      }
      // Standard 8-bit DEY operation
      --regs_[Y];
      this->update_nz_flags(regs_[Y]);
      this->transition_to_fetch();
      return pins;
  }
  
  return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "chip/cpu/fam65xx/operations/inc_lint_prevention_footer.hpp"
