/*
 * fam65xx_arithmetic.inc.hpp - Hardware-Accurate BCD Arithmetic Helpers
 *
 * This file contains the perform_* helper methods that implement
 * hardware-accurate arithmetic operations including BCD (Binary Coded Decimal)
 * support for the MOS 65xx family of processors.
 *
 * These methods are extracted from the main fam65xx.hpp file to improve
 * code organization and reduce compilation times.
 *
 * NOTE: This file is meant to be included within the fam65xx_t class definition
 * and assumes access to class members and template parameters.
 */

// ============================================================================
// HARDWARE-ACCURATE BCD ARITHMETIC HELPERS
// ============================================================================

/**
 * Hardware-accurate 6502 BCD addition
 * Based on MAME and floooh implementations with exact hardware timing
 *
 * This implementation matches the actual 6502 silicon behavior including:
 * - Correct N and Z flag behavior in BCD mode
 * - Proper carry handling
 * - Exact overflow flag calculation
 *
 * ADC operation with BCD support
 * Handles both binary and BCD modes with proper flag calculation
 *
 * Template parameter allows compile-time processor-specific optimizations:
 * - NES 6502: BCD disabled, simplified binary-only path
 * - MOS 6502/6510: Full BCD support with hardware-accurate behavior
 * - 65C02: Enhanced BCD with corrected flag behavior
 */
inline void perform_adc(uint8_t operand) {
  const uint8_t old_a = this->get(REG_A);
  const uint8_t carry_in = this->get(REG_P) & FLAG_C;
  const uint16_t full_result = old_a + operand + carry_in;
  const uint8_t result = static_cast<uint8_t>(full_result);

  if constexpr (has_bcd()) {
    if (this->get(REG_P) & FLAG_D) {
      // BCD mode calculation
      uint8_t al = (old_a & 0x0F) + (operand & 0x0F) + carry_in;
      if (al > 9)
        al += 6;

      uint8_t ah = (old_a >> 4) + (operand >> 4) + (al > 0x0F);

      // Calculate flags based on processor type
      uint8_t n_flag, v_flag, z_flag, c_flag;

      if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
        // CMOS processors: All flags from binary result, except when otherwise
        // specified
        if constexpr (Traits.has(CPUCoreFlags::BCD_NMOS_FLAGS)) {
          // Some CMOS processors still use NMOS-style flag calculation
          n_flag = (result != 0) * ((ah << 4) & FLAG_N);
          v_flag = ((~(old_a ^ operand) & (old_a ^ (ah << 4))) >> 1) & FLAG_V;
          if (ah > 9)
            ah += 6;
          z_flag = (result == 0) * FLAG_Z;
          c_flag = (ah > 15) ? FLAG_C : 0;
        } else {
          // Standard CMOS (WDC65C02 and Synertek65C02): Different flag behavior
          // Calculate intermediate BCD result before carry adjustment for flag
          // calculations
          uint8_t intermediate_bcd = (ah << 4) | (al & 0x0F);

          // BCD adjustment for carry calculation
          c_flag = (ah > 9) ? FLAG_C : 0; // C flag based on decimal carry
          if (ah > 9)
            ah += 6;
          uint8_t bcd_result = (ah << 4) | (al & 0x0F);

          // The Synertek 65C02 has unique BCD flag behavior:
          // - V flag: calculated from intermediate BCD (before high nibble
          // adjustment)
          // - Z flag: calculated from BCD result (after adjustments)
          // - N flag: from BCD result
          // - C flag: from decimal carry

          // V flag from intermediate BCD result (hardware-accurate for
          // Synertek)
          v_flag = calc_v_flag_add(old_a, operand, (uint16_t)intermediate_bcd);
          // Z flag from BCD result (hardware-accurate for Synertek)
          z_flag = calc_z_flag(bcd_result);
          // N flag: sign from BCD result
          n_flag = bcd_result & FLAG_N;
        }
      } else {
        // NMOS processors: Complex flag calculation with hardware quirks
        n_flag = (result != 0) * ((ah << 4) & FLAG_N);
        v_flag = ((~(old_a ^ operand) & (old_a ^ (ah << 4))) >> 1) & FLAG_V;
        if (ah > 9)
          ah += 6;
        z_flag = (result == 0) * FLAG_Z;
        c_flag = (ah > 15) ? FLAG_C : 0;
      }

      // Apply result and flags
      this->set(REG_A, (ah << 4) | (al & 0x0F));
      this->set(REG_P,
                (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                    n_flag | v_flag | z_flag | c_flag);
      return;
    }
  }

  // Binary mode
  this->set(REG_A, result);
  this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                       calc_nz_flags<REG_A>(result) |
                       calc_v_flag_add(old_a, operand, result) |
                       calc_c_flag(full_result));
}

/**
 * SBC operation with BCD support - Hardware-accurate implementation
 * Based on ProcessorTests validation and actual 65xx silicon behavior
 */
inline void perform_sbc(uint8_t operand) {
  const uint8_t old_a = this->get(REG_A);
  const uint8_t borrow_in =
      (this->get(REG_P) & FLAG_C) ^ 1; // Invert carry for borrow
  const uint16_t full_result = old_a - operand - borrow_in;
  const uint8_t result = static_cast<uint8_t>(full_result);

  if constexpr (has_bcd()) {
    if (this->get(REG_P) & FLAG_D) {
      // Hardware-accurate 6502 BCD subtraction
      // Different algorithms for NMOS vs CMOS processors

      // Calculate flags from binary result (always for V and C)
      uint8_t c_flag = !(full_result & 0x0100) ? FLAG_C : 0;
      uint8_t v_flag = calc_v_flag_sub(old_a, operand, full_result);

      uint8_t bcd_result;

      if constexpr (Traits.is_nmos()) {
        // NMOS BCD algorithm (MOS 6502)
        uint8_t al = (old_a & 0x0F) - (operand & 0x0F) - borrow_in;
        bool low_borrow = false;
        if (al & 0x10) {
          al -= 6;
          low_borrow = true;
        }

        uint8_t ah = (old_a >> 4) - (operand >> 4) - (low_borrow ? 1 : 0);
        if (ah & 0x10) {
          ah -= 6;
        }

        bcd_result = ((ah & 0x0F) << 4) | (al & 0x0F);
      } else {
        // CMOS BCD algorithm - exact match for ProcessorTests ground truth
        // Based on the WDC 65C02 datasheet and verified implementations
        uint16_t bcd_calc_result = old_a - operand - borrow_in + 0x100;

        // Low nibble correction
        if ((old_a & 0x0F) < ((operand & 0x0F) + borrow_in)) {
          bcd_calc_result -= 6;
        }

        // High nibble correction - check for borrow from low nibble
        uint8_t effective_high_operand = (operand >> 4);
        if ((old_a & 0x0F) < ((operand & 0x0F) + borrow_in)) {
          effective_high_operand++;
        }

        if ((old_a >> 4) < effective_high_operand) {
          bcd_calc_result -= 0x60;
        }

        bcd_result = bcd_calc_result & 0xFF;
      }

      // Flag calculation based on processor type
      uint8_t n_flag, z_flag;
      if constexpr (Traits.has(CPUCoreFlags::BCD_NMOS_FLAGS)) {
        // NMOS-style or CMOS with NMOS flags: N,Z from binary result
        n_flag = result & FLAG_N;
        z_flag = calc_z_flag(result);
      } else {
        // Pure CMOS: N,Z from BCD result
        n_flag = bcd_result & FLAG_N;
        z_flag = calc_z_flag(bcd_result);
      }

      // Apply result and flags
      this->set(REG_A, bcd_result);
      this->set(REG_P,
                (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                    n_flag | v_flag | z_flag | c_flag);
      return;
    }
  }

  // Binary mode
  this->set(REG_A, result);
  this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                       calc_nz_flags<REG_A>(result) |
                       calc_v_flag_sub(old_a, operand, full_result) |
                       (!(full_result & 0x0100) ? FLAG_C : 0));
}

/**
 * Compare operation (CMP/CPX/CPY)
 * Optimized implementation with branchless flag calculation
 *
 * CRITICAL: Always use explicit 8-bit comparison (REG_MEM forces 8-bit)
 * even in 65C816 emulation mode where accumulator may be stored as 16-bit
 */
inline void perform_compare(uint8_t reg_value, uint8_t operand) {
  // Update flags using branchless calculations
  // Use REG_MEM template parameter to force 8-bit comparison
  this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                       calc_nzc_flags<REG_MEM>(reg_value, operand));
}