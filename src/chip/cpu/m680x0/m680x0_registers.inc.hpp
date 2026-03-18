// m680x0_registers.inc.hpp — Register access helpers (included mid-class)
//
// Provides typed accessors for the M680x0 register file.
// This file is #included inside the m680x0_t template class body.

// ========================================================================
// Data Register Access
// ========================================================================

inline uint32_t get_d(uint8_t idx) const { return regs_.d[idx & 7]; }
inline uint16_t get_d_w(uint8_t idx) const { return static_cast<uint16_t>(regs_.d[idx & 7]); }
inline uint8_t  get_d_b(uint8_t idx) const { return static_cast<uint8_t>(regs_.d[idx & 7]); }

inline void set_d(uint8_t idx, uint32_t val) { regs_.d[idx & 7] = val; }
inline void set_d_w(uint8_t idx, uint16_t val) {
    regs_.d[idx & 7] = (regs_.d[idx & 7] & 0xFFFF0000) | val;
}
inline void set_d_b(uint8_t idx, uint8_t val) {
    regs_.d[idx & 7] = (regs_.d[idx & 7] & 0xFFFFFF00) | val;
}

// ========================================================================
// Address Register Access
// ========================================================================

inline uint32_t get_a(uint8_t idx) const { return regs_.a[idx & 7]; }

inline void set_a(uint8_t idx, uint32_t val) { regs_.a[idx & 7] = val; }

// ========================================================================
// Stack Pointer Management
// ========================================================================
// A7 is the active stack pointer.  On supervisor/user mode switch,
// A7 is swapped with USP or SSP as appropriate.

inline uint32_t get_usp() const { return regs_.usp; }
inline uint32_t get_ssp() const { return regs_.ssp; }

inline void set_usp(uint32_t val) { regs_.usp = val; }
inline void set_ssp(uint32_t val) { regs_.ssp = val; }

/// Keep A7 ↔ SSP/USP in sync after direct A7 modifications (e.g. EXG)
inline void sync_sp() {
    if (regs_.sr & SRBits::S) {
        regs_.ssp = regs_.a[7];
    } else {
        regs_.usp = regs_.a[7];
    }
}

/// Swap A7 with USP when entering supervisor mode
inline void enter_supervisor() {
    if (!(regs_.sr & SRBits::S)) {
        regs_.usp = regs_.a[7];                 // Save user SP
        regs_.a[7] = regs_.ssp;                 // Load supervisor SP
        regs_.sr |= SRBits::S;
    }
}

/// Swap A7 with SSP when returning to user mode
inline void leave_supervisor() {
    if (regs_.sr & SRBits::S) {
        regs_.ssp = regs_.a[7];                 // Save supervisor SP
        regs_.a[7] = regs_.usp;                 // Load user SP
        regs_.sr &= ~SRBits::S;
    }
}

/// Full SR write with mode switch handling
inline void set_sr(uint16_t new_sr) {
    // Mask to valid SR bits for this CPU model
    if constexpr (!has_bit_fields()) {
        // MC68000/010: only T1, S, IPM, CCR valid
        new_sr &= SRBits::SR_MASK;
    }
    bool was_super = (regs_.sr & SRBits::S) != 0;
    bool now_super = (new_sr  & SRBits::S) != 0;
    if (was_super && !now_super) {
        // Supervisor → User
        regs_.ssp = regs_.a[7];
        regs_.a[7] = regs_.usp;
    } else if (!was_super && now_super) {
        // User → Supervisor
        regs_.usp = regs_.a[7];
        regs_.a[7] = regs_.ssp;
    }
    regs_.sr = new_sr;
}

// ========================================================================
// CCR access
// ========================================================================

inline uint8_t  get_ccr() const { return static_cast<uint8_t>(regs_.sr & Flags::CCR_MASK); }
inline void set_ccr(uint8_t ccr) {
    regs_.sr = (regs_.sr & ~static_cast<uint16_t>(Flags::CCR_MASK)) | (ccr & Flags::CCR_MASK);
}

// ========================================================================
// Sized register read/write (dispatches by OpSize)
// ========================================================================

inline uint32_t read_dn(uint8_t idx, OpSize sz) const {
    switch (sz) {
        case OpSize::Byte: return get_d_b(idx);
        case OpSize::Word: return get_d_w(idx);
        case OpSize::Long: return get_d(idx);
    }
    return 0;
}

inline void write_dn(uint8_t idx, uint32_t val, OpSize sz) {
    switch (sz) {
        case OpSize::Byte: set_d_b(idx, static_cast<uint8_t>(val)); break;
        case OpSize::Word: set_d_w(idx, static_cast<uint16_t>(val)); break;
        case OpSize::Long: set_d(idx, val); break;
    }
}

// ========================================================================
// Size helpers
// ========================================================================

static constexpr uint32_t size_mask(OpSize sz) {
    switch (sz) {
        case OpSize::Byte: return 0xFF;
        case OpSize::Word: return 0xFFFF;
        case OpSize::Long: return 0xFFFFFFFF;
    }
    return 0;
}

static constexpr uint32_t msb_mask(OpSize sz) {
    switch (sz) {
        case OpSize::Byte: return 0x80;
        case OpSize::Word: return 0x8000;
        case OpSize::Long: return 0x80000000;
    }
    return 0;
}

static constexpr uint8_t size_bytes(OpSize sz) {
    switch (sz) {
        case OpSize::Byte: return 1;
        case OpSize::Word: return 2;
        case OpSize::Long: return 4;
    }
    return 0;
}
