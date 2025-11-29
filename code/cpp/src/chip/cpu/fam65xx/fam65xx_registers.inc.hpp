/*
 * fam65xx_registers.inc.hpp - Register Declarations and Accessors
 *
 * This file contains register declarations and accessor methods for the
 * MOS 65xx family of processors. It handles both 8-bit processors
 * (6502, 6510, 65C02, etc.) and the 16-bit capable 65C816.
 *
 * The register layout and data types are selected at compile time based
 * on the CPU traits, eliminating the need for complex mixin inheritance.
 *
 * NOTE: This file is meant to be included within the fam65xx_t class definition
 *       and provides conditional register layouts using constexpr if.
 */

// ============================================================================
// CONDITIONAL TYPE ALIASES AND REGISTER LAYOUT
// ============================================================================

// Data type selection based on CPU capabilities
using data_t =
    std::conditional_t<Traits.has(CPUCoreFlags::C816_16BIT),
                       uint16_t, // 65C816 uses 16-bit data operations
                       uint8_t // All other processors use 8-bit data operations
                       >;

// Register array with conditional sizing
union {
  uint8_t reg8[Traits.has(CPUCoreFlags::C816_16BIT) ? REG_COUNT_16BIT
                                                    : REG_COUNT_8BIT];
  uint16_t reg16[(Traits.has(CPUCoreFlags::C816_16BIT) ? REG_COUNT_16BIT
                                                       : REG_COUNT_8BIT) /
                 2];
};

// ============================================================================
// REGISTER ACCESSOR METHODS
// ============================================================================

public:
// === Type-safe 8-bit register accessors ===
inline uint8_t get(reg8_t reg) const { return reg8[reg]; }

inline void set(reg8_t reg, uint8_t value) { reg8[reg] = value; }

inline void inc(reg8_t reg) { reg8[reg]++; }

inline void dec(reg8_t reg) { reg8[reg]--; }

// === Type-safe 16-bit register accessors ===
inline uint16_t get(reg16_t reg_pair) const { return reg16[reg_pair]; }

inline void set(reg16_t reg_pair, uint16_t value) { reg16[reg_pair] = value; }

inline void inc(reg16_t reg_pair) { reg16[reg_pair]++; }

inline void dec(reg16_t reg_pair) { reg16[reg_pair]--; }

// ========================================================================
// REGISTER ACCESS METHODS (override mixin methods with constexpr wide
// detection)
// ========================================================================

/**
 * Get accumulator value with automatic 8/16-bit handling
 * Uses constexpr wide check for compile-time optimization
 */
inline data_t get_accumulator() const {
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      return this->get(REG_A_16);
    }
  }
  return this->get(REG_A);
}

/**
 * Set accumulator value with automatic 8/16-bit handling
 * Uses constexpr wide check for compile-time optimization
 */
inline void set_accumulator(data_t value) {
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      this->set(REG_A_16, value);
      return;
    }
  }
  this->set(REG_A, static_cast<uint8_t>(value & 0xFF));
}

/**
 * Get X register value with automatic 8/16-bit handling
 * Uses constexpr wide check for compile-time optimization
 */
inline data_t get_x_register() const {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      return this->get(REG_X_16);
    }
  }
  return this->get(REG_X);
}

/**
 * Set X register value with automatic 8/16-bit handling
 * Uses constexpr wide check for compile-time optimization
 */
inline void set_x_register(data_t value) {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      this->set(REG_X_16, value);
      return;
    }
  }
  this->set(REG_X, static_cast<uint8_t>(value & 0xFF));
}

/**
 * Get Y register value with automatic 8/16-bit handling
 * Uses constexpr wide check for compile-time optimization
 */
inline data_t get_y_register() const {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      return this->get(REG_Y_16);
    }
  }
  return this->get(REG_Y);
}

/**
 * Set Y register value with automatic 8/16-bit handling
 * Uses constexpr wide check for compile-time optimization
 */
inline void set_y_register(data_t value) {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      this->set(REG_Y_16, value);
      return;
    }
  }
  this->set(REG_Y, static_cast<uint8_t>(value & 0xFF));
}

/**
 * Get stack pointer value with automatic 8/16-bit handling
 * Uses constexpr wide check for compile-time optimization
 *
 * Returns correct 16-bit SP for all processors:
 * - 8-bit processors: high byte is always 0x01 (page 1 forced by hardware)
 * - 65C816 native mode: full 16-bit SP value
 * - 65C816 emulation mode: force page 1 (0x01xx) for 6502 compatibility
 */
inline uint16_t get_sp() const {
  if constexpr (has_wide_registers()) {
    // 65C816: Runtime check for emulation mode
    if (this->in_emulation_mode()) {
      // Emulation mode: Force SP to page 1 (0x01xx) for 6502 compatibility
      return 0x0100 | this->get(REG_SPL);
    }
  }
  // Native mode: Use full 16-bit SP (can be anywhere in bank 0)
  // Non-65C816: Always uses page 1 (like hardware, high byte REG_SPH is fixed to 0x01)
  return this->get(REG_SP);
}

private:
// ========================================================================
// HELPER FUNCTIONS (needed by operation files)
// ========================================================================

// === Building blocks ===
inline void set_flag(data_t flag_mask) {
  if constexpr (has_wide_registers()) {
    // 65C816: Use 16-bit P register (data_t is uint16_t)
    this->set(REG_P_16, this->get(REG_P_16) | flag_mask);
  } else {
    // Non-65C816 processors: Use 8-bit P register (data_t is uint8_t)
    this->set(REG_P, this->get(REG_P) | flag_mask);
  }
}
inline void clear_flag(data_t flag_mask) {
  if constexpr (has_wide_registers()) {
    // 65C816: Use 16-bit P register (data_t is uint16_t)
    this->set(REG_P_16, this->get(REG_P_16) & ~flag_mask);
  } else {
    // Non-65C816 processors: Use 8-bit P register (data_t is uint8_t)
    this->set(REG_P, this->get(REG_P) & ~flag_mask);
  }
}
// === Foundation: Single memory write ===
inline void update_flags(data_t clear_mask, data_t set_mask) {
  if constexpr (has_wide_registers()) {
    // 65C816: Use 16-bit P register (data_t is uint16_t)
    this->set(REG_P_16, (this->get(REG_P_16) & ~clear_mask) | set_mask);
  } else {
    // Non-65C816 processors: Use 8-bit P register (data_t is uint8_t)
    this->set(REG_P, (this->get(REG_P) & ~clear_mask) | set_mask);
  }
}
inline void update_flag(data_t flag_mask, bool condition) {
  update_flags(flag_mask, static_cast<data_t>(condition) * flag_mask);
}

// ========================================================================
// UNIFIED FLAG CALCULATION HELPERS WITH AUTOMATIC WIDTH DETECTION
// ========================================================================

/**
 * Unified N flag calculation with automatic width detection
 * Uses template parameter for compile-time register type detection
 */
template <reg8_t reg_type = REG_A>
inline uint8_t calc_n_flag(data_t value) const {
  if (is_register_16bit<reg_type>()) {
    return (value & 0x8000) ? FLAG_N : 0;
  } else {
    return (static_cast<uint8_t>(value) & 0x80) ? FLAG_N : 0;
  }
}

/**
 * Unified Z flag calculation with automatic width detection
 */
template <reg8_t reg_type = REG_A>
inline uint8_t calc_z_flag(data_t value) const {
  if (is_register_16bit<reg_type>()) {
    return (value == 0) ? FLAG_Z : 0;
  } else {
    return (static_cast<uint8_t>(value) == 0) ? FLAG_Z : 0;
  }
}

/**
 * Branchless C flag calculation from 16-bit result
 * Extract carry bit directly from bit 8
 */
inline uint8_t calc_c_flag(uint16_t result) { return (result >> 8) & FLAG_C; }

/**
 * Branchless V flag calculation for addition
 * Hardware-accurate overflow detection using XOR logic
 */
inline uint8_t calc_v_flag_add(uint8_t a, uint8_t b, uint16_t result) {
  return (((a ^ result) & (b ^ result)) >> 1) & FLAG_V;
}

/**
 * Branchless V flag calculation for subtraction
 * Hardware-accurate overflow detection for SBC/CMP operations
 */
inline uint8_t calc_v_flag_sub(uint8_t a, uint8_t b, uint16_t result) {
  return (((a ^ b) & (a ^ result)) >> 1) & FLAG_V;
}

/**
 * Unified NZ flag calculation with automatic width detection
 * Replaces all calc_nz_flags variants for maximum deduplication
 */
template <reg8_t reg_type = REG_A>
inline uint8_t calc_nz_flags(data_t value) const {
  return calc_n_flag<reg_type>(value) | calc_z_flag<reg_type>(value);
}

/**
 * Unified NZC flag calculation for compare operations with width detection
 * Replaces calc_nzc_flags variants
 */
template <reg8_t reg_type = REG_A>
inline uint8_t calc_nzc_flags(data_t minuend, data_t subtrahend) {
  if (is_register_16bit<reg_type>()) {
    // 16-bit comparison
    uint32_t result = minuend - subtrahend;
    return ((result & 0x8000) ? FLAG_N : 0) |
           ((result & 0xFFFF) == 0 ? FLAG_Z : 0) |
           (minuend >= subtrahend ? FLAG_C : 0);
  } else {
    // 8-bit comparison
    uint8_t result_8bit =
        static_cast<uint8_t>(minuend) - static_cast<uint8_t>(subtrahend);
    // For 8-bit subtraction: C=1 if no borrow (minuend >= subtrahend)
    // This is equivalent to checking if bit 8 of the 16-bit result is 0
    uint8_t carry = (static_cast<uint8_t>(minuend) >= static_cast<uint8_t>(subtrahend)) ? FLAG_C : 0;
    return calc_n_flag<reg_type>(result_8bit) |
           calc_z_flag<reg_type>(result_8bit) |
           carry;
  }
}

// ========================================================================
// UNIFIED UPDATE OPERATIONS WITH AUTOMATIC WIDTH DETECTION
// ========================================================================

inline void update_c_flag(uint8_t value, uint8_t bit_position) {
  update_flag(FLAG_C, (value >> bit_position) & FLAG_C);
}

/**
 * Unified NZ flags update with automatic width detection
 * Replaces all update_nz_flags variants for maximum deduplication
 */
template <reg8_t reg_type = REG_A> inline void update_nz_flags(data_t value) {
  update_flags(FLAG_N | FLAG_Z, calc_nz_flags<reg_type>(value));
}

/**
 * Unified NZC flags update with automatic width detection
 * Replaces all update_nzc_flags variants for maximum deduplication
 */
template <reg8_t reg_type = REG_A>
inline void update_nzc_flags(data_t value, uint8_t carry_flag) {
  update_flags(FLAG_N | FLAG_Z | FLAG_C,
               calc_nz_flags<reg_type>(value) | carry_flag);
}

// ========================================================================
// LEGACY COMPATIBILITY (8-bit only versions for explicit 8-bit operations)
// ========================================================================

/**
 * Legacy 8-bit only N flag calculation (for explicit 8-bit contexts)
 */
inline uint8_t calc_n_flag_8bit(uint8_t value) const { return value & FLAG_N; }

/**
 * Legacy 8-bit only Z flag calculation (for explicit 8-bit contexts)
 */
inline uint8_t calc_z_flag_8bit(uint8_t value) const {
  return (value == 0) * FLAG_Z;
}

/**
 * Legacy 8-bit only NZ flag calculation (for explicit 8-bit contexts)
 */
inline uint8_t calc_nz_flags_8bit(uint8_t value) const {
  return calc_n_flag_8bit(value) | calc_z_flag_8bit(value);
}

/**
 * Legacy 8-bit only NZ flags update (for explicit 8-bit contexts)
 */
inline void update_nz_flags_8bit(uint8_t value) {
  update_flags(FLAG_N | FLAG_Z, calc_nz_flags_8bit(value));
}

/**
 * Explicit 16-bit NZ flags update (for 16-bit memory operations)
 */
inline void update_nz_flags_16bit(uint16_t value) {
  // N flag: bit 15 for 16-bit values
  uint8_t n_flag = (value & 0x8000) ? FLAG_N : 0;
  // Z flag: zero if entire 16-bit value is 0
  uint8_t z_flag = (value == 0) ? FLAG_Z : 0;
  update_flags(FLAG_N | FLAG_Z, n_flag | z_flag);
}

// === Register initialization ===
void init_registers() {
  // Clear all registers to zero
  std::memset(&reg8, 0, sizeof(reg8));

  // Initialize stack pointer to 0x01FF for all processors
  // - 6502/6510/65C02: SP always on page 1 (0x0100-0x01FF)
  // - 65C816 emulation mode: SP forced to page 1 for compatibility
  // - 65C816 native mode: SP can be set anywhere, but starts at 0x01FF
  set(REG_SP, 0x01FF);

  // Initialize P register based on CPU type
  if constexpr (has_wide_registers()) {
    // 65C816 starts in emulation mode with E=1, M=1, X=1 flags
    // CRITICAL: Must set FLAG_E in the 16-bit P register for proper emulation mode
    set(REG_P_16, FLAG_E | FLAG_I | FLAG_M | FLAG_X);
    set(REG_PBR, 0x00); // Program bank register
    set(REG_DBR, 0x00); // Data bank register
    set(REG_ZBR, 0x00); // Zero page bank register
  } else {
    // Standard 8-bit processors: Only interrupt disable flag
    set(REG_P, FLAG_I);
  }
}

public:
/*
 * End of fam65xx_registers.inc.hpp
 */