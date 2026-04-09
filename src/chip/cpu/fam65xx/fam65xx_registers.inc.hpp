/*
 * fam65xx_registers.inc.hpp - Register File and Width-Aware Accessors
 *
 * Included within the fam65xx_t class definition.  Provides:
 *  - RegisterFile backing store (regs_)
 *  - Width-aware named accessors (get_accumulator, get_sp, …)
 *  - Flag manipulation helpers
 *  - Unified flag-calculation templates keyed on WidthMode
 *  - Register initialisation
 *
 * Direct register access uses regs_[X], regs_[PC], ++regs_[PC], etc.
 */

// ============================================================================
// REGISTER FILE
// ============================================================================

public:
// Data type selection based on CPU capabilities
using data_t =
    std::conditional_t<Traits.has(CPUCoreFlags::C816_16BIT),
                       uint16_t, // 65C816 uses 16-bit data operations
                       uint8_t // All other processors use 8-bit data operations
                       >;

protected:
// Unified backing store (24 bytes; 8-bit CPUs use the first 16)
RegisterFile<24, uint16_t> regs_;

public:
// === Generic register access (for debugger/test harness/system code) ===
template <typename T> T get(RegIdx<T> idx) const { return regs_.get(idx); }
template <typename T, typename U> void set(RegIdx<T> idx, U val) { regs_.set(idx, val); }

/// CpuChipBase override — set program counter from uint32_t.
void set_pc(uint32_t addr) override { regs_[PC] = static_cast<uint16_t>(addr); }

// ========================================================================
// WIDTH-AWARE NAMED ACCESSORS
// ========================================================================

inline data_t get_accumulator() const {
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit())
      return regs_[A16];
  }
  return regs_[A];
}

inline void set_accumulator(data_t value) {
  if constexpr (has_wide_registers()) {
    if (this->is_accumulator_16bit()) {
      regs_[A16] = value;
      return;
    }
  }
  regs_[A] = static_cast<uint8_t>(value & 0xFF);
}

inline data_t get_x_register() const {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit())
      return regs_[X16];
  }
  return regs_[X];
}

inline void set_x_register(data_t value) {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      regs_[X16] = value;
      return;
    }
  }
  regs_[X] = static_cast<uint8_t>(value & 0xFF);
}

inline data_t get_y_register() const {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit())
      return regs_[Y16];
  }
  return regs_[Y];
}

inline void set_y_register(data_t value) {
  if constexpr (has_wide_registers()) {
    if (this->is_index_16bit()) {
      regs_[Y16] = value;
      return;
    }
  }
  regs_[Y] = static_cast<uint8_t>(value & 0xFF);
}

/**
 * Get stack pointer value with automatic 8/16-bit handling
 *
 * Returns correct 16-bit SP for all processors:
 * - 8-bit processors: high byte is always 0x01 (page 1 forced by hardware)
 * - 65C816 native mode: full 16-bit SP value
 * - 65C816 emulation mode: force page 1 (0x01xx) for 6502 compatibility
 */
inline uint16_t get_sp() const {
  if constexpr (has_wide_registers()) {
    if (this->in_emulation_mode())
      return 0x0100 | uint8_t(regs_[SPL]);
  }
  return regs_[SP];
}

// === Stack pointer increment/decrement helpers ===
inline void inc_stack() {
  if constexpr (has_wide_registers()) {
    ++regs_[SP];   // 65C816: Use 16-bit for proper carry handling
  } else {
    ++regs_[S];    // 8-bit CPUs: SP always in page 1
  }
}

inline void dec_stack() {
  if constexpr (has_wide_registers()) {
    --regs_[SP];   // 65C816: Use 16-bit for proper borrow handling
  } else {
    --regs_[S];    // 8-bit CPUs: SP always in page 1
  }
}

private:
// ========================================================================
// FLAG MANIPULATION
// ========================================================================

inline void set_flag(uint8_t flag_mask) {
  // Always use 8-bit P register (PL) for flag operations
  // The E flag (65C816) is in PH and should never be modified by flag ops
  regs_[P] |= flag_mask;
}
inline void clear_flag(uint8_t flag_mask) {
  regs_[P] &= ~flag_mask;
}
inline void update_flags(uint8_t clear_mask, uint8_t set_mask) {
  regs_[P] = uint8_t(uint8_t(regs_[P]) & ~clear_mask) | set_mask;
}
inline void update_flag(uint8_t flag_mask, bool condition) {
  update_flags(flag_mask, static_cast<uint8_t>(condition) * flag_mask);
}

// ========================================================================
// UNIFIED FLAG CALCULATION HELPERS — keyed on WidthMode
// ========================================================================

template <WidthMode WM = WidthMode::ACC>
inline uint8_t calc_n_flag(data_t value) const {
  if (is_register_16bit<WM>()) {
    return (value & 0x8000) ? FLAG_N : 0;
  } else {
    return (static_cast<uint8_t>(value) & 0x80) ? FLAG_N : 0;
  }
}

template <WidthMode WM = WidthMode::ACC>
inline uint8_t calc_z_flag(data_t value) const {
  if (is_register_16bit<WM>()) {
    return (value == 0) ? FLAG_Z : 0;
  } else {
    return (static_cast<uint8_t>(value) == 0) ? FLAG_Z : 0;
  }
}

// Branchless C flag calculation from 16-bit result
inline uint8_t calc_c_flag(uint16_t result) { return (result >> 8) & FLAG_C; }

// Hardware-accurate V flag for ADC
inline uint8_t calc_v_flag_add(uint8_t a, uint8_t b, uint16_t result) {
  return (((a ^ result) & (b ^ result)) >> 1) & FLAG_V;
}

// Hardware-accurate V flag for SBC
inline uint8_t calc_v_flag_sub(uint8_t a, uint8_t b, uint16_t result) {
  return (((a ^ b) & (a ^ result)) >> 1) & FLAG_V;
}

template <WidthMode WM = WidthMode::ACC>
inline uint8_t calc_nz_flags(data_t value) const {
  return calc_n_flag<WM>(value) | calc_z_flag<WM>(value);
}

template <WidthMode WM = WidthMode::ACC>
inline uint8_t calc_nzc_flags(data_t minuend, data_t subtrahend) {
  if (is_register_16bit<WM>()) {
    uint32_t result = minuend - subtrahend;
    return ((result & 0x8000) ? FLAG_N : 0) |
           ((result & 0xFFFF) == 0 ? FLAG_Z : 0) |
           (minuend >= subtrahend ? FLAG_C : 0);
  } else {
    // 8-bit comparison - mask inputs to 8-bit for proper comparison semantics
    // This is critical for 65C816 where data_t is uint16_t but comparison must use 8-bit values
    uint8_t min8 = static_cast<uint8_t>(minuend & 0xFF);
    uint8_t sub8 = static_cast<uint8_t>(subtrahend & 0xFF);
    uint8_t result = min8 - sub8;
    return calc_n_flag<WM>(result) | calc_z_flag<WM>(result) |
           (min8 >= sub8 ? FLAG_C : 0);
  }
}

// ========================================================================
// UNIFIED UPDATE OPERATIONS
// ========================================================================

inline void update_c_flag(uint8_t value, uint8_t bit_position) {
  update_flag(FLAG_C, (value >> bit_position) & FLAG_C);
}

template <WidthMode WM = WidthMode::ACC> inline void update_nz_flags(data_t value) {
  update_flags(FLAG_N | FLAG_Z, calc_nz_flags<WM>(value));
}

template <WidthMode WM = WidthMode::ACC>
inline void update_nzc_flags(data_t value, uint8_t carry_flag) {
  update_flags(FLAG_N | FLAG_Z | FLAG_C,
               calc_nz_flags<WM>(value) | carry_flag);
}

// ========================================================================
// LEGACY COMPATIBILITY (8-bit only versions for explicit 8-bit operations)
// ========================================================================

inline uint8_t calc_n_flag_8bit(uint8_t value) const { return value & FLAG_N; }

inline uint8_t calc_z_flag_8bit(uint8_t value) const {
  return (value == 0) * FLAG_Z;
}

inline uint8_t calc_nz_flags_8bit(uint8_t value) const {
  return calc_n_flag_8bit(value) | calc_z_flag_8bit(value);
}

inline void update_nz_flags_8bit(uint8_t value) {
  update_flags(FLAG_N | FLAG_Z, calc_nz_flags_8bit(value));
}

inline void update_nz_flags_16bit(uint16_t value) {
  uint8_t n_flag = (value & 0x8000) ? FLAG_N : 0;
  uint8_t z_flag = (value == 0) ? FLAG_Z : 0;
  update_flags(FLAG_N | FLAG_Z, n_flag | z_flag);
}

// === Register initialization ===
void init_registers() {
  regs_.clear();

  if constexpr (has_wide_registers()) {
    regs_[SP] = 0x01FF;
    regs_[P16] = FLAG_E | FLAG_I | FLAG_M | FLAG_X;
    regs_[PBR] = 0x00;
    regs_[DBR] = 0x00;
    regs_[ZBR] = 0x00;
  } else {
    regs_[SP] = 0x0100;  // S=$00, page byte=$01
    regs_[P] = FLAG_I;
  }
}

public:
/*
 * End of fam65xx_registers.inc.hpp
 */