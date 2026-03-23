// ========================================================================
// mc6809_base_ops.inc.hpp — Page 1 instruction handlers
// ========================================================================
//
// All Page 1 (no prefix) opcode handlers for the MC6809 family.
// Included inside mc6809_t<Traits> class body.
//
// Instruction handlers are multi-cycle FSMs using switch(step_++).
// Each case corresponds to one bus cycle.
// ========================================================================

// ========================================================================
// ADDRESSING MODE HELPERS
// ========================================================================
// These resolve the effective address (ea_) for Direct, Extended, and
// Indexed addressing modes, then call the operation handler.

// --- Direct addressing: EA = DP:postbyte ---

/// Generic Direct-mode read-modify-write template
/// rmw_fn: pointer to member function that takes (bus_state_t, uint8_t) -> uint8_t
bus_state_t addr_direct_read(bus_state_t pins) {
    switch (step_++) {
    case 0:
        return bus_setup_read(pins, regs_[PC]);  // Fetch offset byte
    case 1:
        ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, ea_);  // Read from EA
    case 2:
        data_lo_ = bus_read_data(pins);
        return pins;  // Data available in data_lo_
    default:
        return pins;
    }
}

/// Direct addressing: compute EA, return at step where data can be read
bus_state_t addr_direct_ea(bus_state_t pins) {
    switch (step_++) {
    case 0:
        return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins);
        regs_[PC]++;
        return pins;
    default:
        return pins;
    }
}

// --- Extended addressing: EA = next two bytes ---

bus_state_t addr_extended_ea(bus_state_t pins) {
    switch (step_++) {
    case 0:
        return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        ea_ |= bus_read_data(pins);
        regs_[PC]++;
        return pins;
    default:
        return pins;
    }
}

// --- Indexed addressing: EA from post-byte ---

bus_state_t addr_indexed_ea(bus_state_t pins) {
    switch (step_++) {
    case 0:
        return bus_setup_read(pins, regs_[PC]);  // Fetch post-byte
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;

        if (!(postbyte_ & 0x80)) {
            // 5-bit signed offset: bits 4-0, sign-extended
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;  // Sign extend
            uint8_t reg_idx = (postbyte_ >> 5) & 0x03;
            ea_ = static_cast<uint16_t>(get_index_reg(reg_idx) + off5);
            return pins;  // EA ready, no more bus cycles needed
        }

        // Complex indexed mode — decode
        uint8_t reg_idx = (postbyte_ >> 5) & 0x03;
        uint16_t reg_val = get_index_reg(reg_idx);
        bool indirect = (postbyte_ & 0x10) != 0;
        uint8_t mode = postbyte_ & 0x0F;

        switch (mode) {
        case 0x00: // ,R+
            ea_ = reg_val;
            set_index_reg(reg_idx, reg_val + 1);
            return pins;
        case 0x01: // ,R++
            ea_ = reg_val;
            set_index_reg(reg_idx, reg_val + 2);
            if (indirect) {
                step_ = 10;  // Need to fetch indirect
                return bus_setup_read(pins, ea_);
            }
            return pins;
        case 0x02: // ,-R
            set_index_reg(reg_idx, reg_val - 1);
            ea_ = get_index_reg(reg_idx);
            return pins;
        case 0x03: // ,--R
            set_index_reg(reg_idx, reg_val - 2);
            ea_ = get_index_reg(reg_idx);
            if (indirect) {
                step_ = 10;
                return bus_setup_read(pins, ea_);
            }
            return pins;
        case 0x04: // ,R (no offset)
            ea_ = reg_val;
            if (indirect) {
                step_ = 10;
                return bus_setup_read(pins, ea_);
            }
            return pins;
        case 0x05: // B,R
            ea_ = static_cast<uint16_t>(reg_val + static_cast<int8_t>(regs_[B]));
            if (indirect) {
                step_ = 10;
                return bus_setup_read(pins, ea_);
            }
            return pins;
        case 0x06: // A,R
            ea_ = static_cast<uint16_t>(reg_val + static_cast<int8_t>(regs_[A]));
            if (indirect) {
                step_ = 10;
                return bus_setup_read(pins, ea_);
            }
            return pins;
        case 0x08: // n8,R — 8-bit signed offset
            step_ = 20;  // Need to fetch offset byte
            return bus_setup_read(pins, regs_[PC]);
        case 0x09: // n16,R — 16-bit offset
            step_ = 30;  // Need to fetch 2 offset bytes
            return bus_setup_read(pins, regs_[PC]);
        case 0x0B: // D,R
            ea_ = static_cast<uint16_t>(reg_val + static_cast<int16_t>(regs_[D]));
            if (indirect) {
                step_ = 10;
                return bus_setup_read(pins, ea_);
            }
            return pins;
        case 0x0C: // n8,PCR — 8-bit PC relative
            step_ = 40;
            return bus_setup_read(pins, regs_[PC]);
        case 0x0D: // n16,PCR — 16-bit PC relative
            step_ = 50;
            return bus_setup_read(pins, regs_[PC]);
        case 0x0F: // Extended indirect: [n16]
            if (indirect) {
                step_ = 50;  // Reuse n16,PCR path then indirect
                return bus_setup_read(pins, regs_[PC]);
            }
            ea_ = reg_val;
            return pins;
        default:
            // Undocumented indexed modes
            ea_ = reg_val;
            return pins;
        }
    }

    // --- Continuation states for multi-byte indexed modes ---

    // States 10-11: Indirect fetch
    case 10:
        data_hi_ = bus_read_data(pins);
        return bus_setup_read(pins, ea_ + 1);
    case 11:
        ea_ = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        return pins;

    // States 20-21: 8-bit offset fetch
    case 20: {
        int8_t off8 = static_cast<int8_t>(bus_read_data(pins));
        regs_[PC]++;
        uint8_t reg_idx = (postbyte_ >> 5) & 0x03;
        ea_ = static_cast<uint16_t>(get_index_reg(reg_idx) + off8);
        if (postbyte_ & 0x10) {
            step_ = 10;
            return bus_setup_read(pins, ea_);
        }
        return pins;
    }

    // States 30-32: 16-bit offset fetch
    case 30:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 31: {
        int16_t off16 = static_cast<int16_t>((static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins));
        regs_[PC]++;
        uint8_t reg_idx = (postbyte_ >> 5) & 0x03;
        ea_ = static_cast<uint16_t>(get_index_reg(reg_idx) + off16);
        if (postbyte_ & 0x10) {
            step_ = 10;
            return bus_setup_read(pins, ea_);
        }
        return pins;
    }

    // States 40-41: 8-bit PC-relative
    case 40: {
        int8_t off8 = static_cast<int8_t>(bus_read_data(pins));
        regs_[PC]++;
        ea_ = static_cast<uint16_t>(regs_[PC] + off8);
        if (postbyte_ & 0x10) {
            step_ = 10;
            return bus_setup_read(pins, ea_);
        }
        return pins;
    }

    // States 50-52: 16-bit PC-relative or extended indirect
    case 50:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 51: {
        int16_t off16 = static_cast<int16_t>((static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins));
        regs_[PC]++;
        // Check if this is extended indirect ($9F post-byte)
        if ((postbyte_ & 0x1F) == 0x1F) {
            ea_ = static_cast<uint16_t>((static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins));
            // Extended indirect always does indirect lookup
            step_ = 10;
            return bus_setup_read(pins, ea_);
        }
        ea_ = static_cast<uint16_t>(regs_[PC] + off16);
        if (postbyte_ & 0x10) {
            step_ = 10;
            return bus_setup_read(pins, ea_);
        }
        return pins;
    }

    default:
        return pins;
    }
}

// ========================================================================
// MACRO HELPERS for generating addressing-mode variants
// ========================================================================
// These macros generate the 4 addressing-mode variants (immediate, direct,
// indexed, extended) for common 8-bit and 16-bit register operations.

// ========================================================================
// DIRECT MODE READ-MODIFY-WRITE OPERATIONS
// ========================================================================

#define MC6809_DEFINE_RMW_DIRECT(name, alu_op) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 2: \
            data_lo_ = alu_op(bus_read_data(pins)); \
            return bus_setup_write(pins, ea_, data_lo_); \
        case 3: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_DEFINE_RMW_INDEXED(name, alu_op) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return addr_indexed_ea(pins); \
        default: \
            if (step_ <= 2) return addr_indexed_ea(pins); \
            if (step_ == 3) return bus_setup_read(pins, ea_); \
            if (step_ == 4) { \
                data_lo_ = alu_op(bus_read_data(pins)); \
                return bus_setup_write(pins, ea_, data_lo_); \
            } \
            transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_DEFINE_RMW_EXTENDED(name, alu_op) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 3: \
            data_lo_ = alu_op(bus_read_data(pins)); \
            return bus_setup_write(pins, ea_, data_lo_); \
        case 4: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

// Generate RMW instructions for all 3 memory addressing modes
MC6809_DEFINE_RMW_DIRECT(neg, alu_neg8)
MC6809_DEFINE_RMW_DIRECT(com, alu_com8)
MC6809_DEFINE_RMW_DIRECT(lsr, alu_lsr8)
MC6809_DEFINE_RMW_DIRECT(ror, alu_ror8)
MC6809_DEFINE_RMW_DIRECT(asr, alu_asr8)
MC6809_DEFINE_RMW_DIRECT(asl, alu_asl8)
MC6809_DEFINE_RMW_DIRECT(rol, alu_rol8)
MC6809_DEFINE_RMW_DIRECT(dec, alu_dec8)
MC6809_DEFINE_RMW_DIRECT(inc, alu_inc8)

MC6809_DEFINE_RMW_EXTENDED(neg, alu_neg8)
MC6809_DEFINE_RMW_EXTENDED(com, alu_com8)
MC6809_DEFINE_RMW_EXTENDED(lsr, alu_lsr8)
MC6809_DEFINE_RMW_EXTENDED(ror, alu_ror8)
MC6809_DEFINE_RMW_EXTENDED(asr, alu_asr8)
MC6809_DEFINE_RMW_EXTENDED(asl, alu_asl8)
MC6809_DEFINE_RMW_EXTENDED(rol, alu_rol8)
MC6809_DEFINE_RMW_EXTENDED(dec, alu_dec8)
MC6809_DEFINE_RMW_EXTENDED(inc, alu_inc8)

// Indexed RMW — uses a multi-state approach since indexed EA resolution
// takes variable cycles. We use a simpler single-handler approach.
bus_state_t op_neg_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_neg8(v); }); }
bus_state_t op_com_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_com8(v); }); }
bus_state_t op_lsr_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_lsr8(v); }); }
bus_state_t op_ror_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_ror8(v); }); }
bus_state_t op_asr_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_asr8(v); }); }
bus_state_t op_asl_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_asl8(v); }); }
bus_state_t op_rol_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_rol8(v); }); }
bus_state_t op_dec_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_dec8(v); }); }
bus_state_t op_inc_indexed(bus_state_t pins) { return op_rmw_indexed(pins, [this](uint8_t v){ return alu_inc8(v); }); }

/// Generic indexed RMW handler using a staged approach
template <typename Fn>
bus_state_t op_rmw_indexed(bus_state_t pins, Fn alu_fn) {
    // Phase 1: resolve indexed EA
    // Phase 2: read from EA, apply ALU, write back
    // Use indexed_phase_ to track which phase we're in.
    //
    // Simple implementation: we feed addr_indexed_ea until ea_ is resolved,
    // then read-modify-write.

    switch (step_++) {
    case 0:
        indexed_phase_ = 0;
        return bus_setup_read(pins, regs_[PC]);
    default: {
        // Check if EA is resolved by trying the indexed state machine.
        // This is a simplified approach — the addr_indexed_ea function
        // handles its own step counting via its own switch. But since
        // we share step_ with it, we need a different approach.
        //
        // For now, use a simpler inline approach:
        if (indexed_phase_ == 0) {
            // Still resolving EA
            bus_state_t result = addr_indexed_ea(pins);
            // Check if EA is now resolved (addr_indexed_ea returns
            // without setting up another bus read when done)
            if (step_ > 60) {  // Failsafe
                transition_to_fetch();
                return pins;
            }
            // If ea_ was set and we're past the initial fetch, proceed
            // This is a heuristic; the proper implementation will use
            // a dedicated indexed resolver
            indexed_phase_ = 1;
            return bus_setup_read(pins, ea_);
        }
        if (indexed_phase_ == 1) {
            data_lo_ = alu_fn(bus_read_data(pins));
            indexed_phase_ = 2;
            return bus_setup_write(pins, ea_, data_lo_);
        }
        transition_to_fetch();
        return pins;
    }
    }
}

uint8_t indexed_phase_ = 0;

// ========================================================================
// TST — Test (read-only, no writeback)
// ========================================================================

bus_state_t op_tst_direct(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, ea_);
    case 2:
        alu_tst8(bus_read_data(pins));
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_tst_indexed(bus_state_t pins) {
    // Simplified — resolve EA then read and test
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        // Simple 5-bit offset case
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
            return bus_setup_read(pins, ea_);
        }
        // Complex modes — simplified for now
        ea_ = resolve_indexed_simple(postbyte_, pins);
        return bus_setup_read(pins, ea_);
    }
    case 2:
        alu_tst8(bus_read_data(pins));
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_tst_extended(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        ea_ |= bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, ea_);
    case 3:
        alu_tst8(bus_read_data(pins));
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// CLR — Clear memory location
// ========================================================================

bus_state_t op_clr_direct(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, ea_);  // Dummy read
    case 2:
        alu_clr8();
        return bus_setup_write(pins, ea_, 0);
    case 3:
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_clr_indexed(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
            return bus_setup_read(pins, ea_);
        }
        ea_ = resolve_indexed_simple(postbyte_, pins);
        return bus_setup_read(pins, ea_);
    }
    case 2:
        alu_clr8();
        return bus_setup_write(pins, ea_, 0);
    case 3:
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_clr_extended(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        ea_ |= bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, ea_);
    case 3:
        alu_clr8();
        return bus_setup_write(pins, ea_, 0);
    case 4:
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// JMP — Jump
// ========================================================================

bus_state_t op_jmp_direct(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        regs_[PC] = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_jmp_indexed(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
            regs_[PC] = ea_;
            transition_to_fetch();
            return pins;
        }
        ea_ = resolve_indexed_simple(postbyte_, pins);
        regs_[PC] = ea_;
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_jmp_extended(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        ea_ |= bus_read_data(pins);
        regs_[PC] = ea_;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// Simplified indexed EA resolver (for simple modes, deferred complex modes)
// ========================================================================

uint16_t resolve_indexed_simple(uint8_t pb, bus_state_t /*pins*/) {
    uint8_t reg_idx = (pb >> 5) & 0x03;
    uint16_t reg_val = get_index_reg(reg_idx);
    uint8_t mode = pb & 0x0F;

    switch (mode) {
    case 0x00: { uint16_t ea = reg_val; set_index_reg(reg_idx, reg_val + 1); return ea; }
    case 0x01: { uint16_t ea = reg_val; set_index_reg(reg_idx, reg_val + 2); return ea; }
    case 0x02: set_index_reg(reg_idx, reg_val - 1); return get_index_reg(reg_idx);
    case 0x03: set_index_reg(reg_idx, reg_val - 2); return get_index_reg(reg_idx);
    case 0x04: return reg_val;
    case 0x05: return static_cast<uint16_t>(reg_val + static_cast<int8_t>(regs_[B]));
    case 0x06: return static_cast<uint16_t>(reg_val + static_cast<int8_t>(regs_[A]));
    case 0x0B: return static_cast<uint16_t>(reg_val + static_cast<int16_t>(regs_[D]));
    default: return reg_val;
    }
}

// ========================================================================
// 8-BIT REGISTER/MEMORY OPERATIONS (SUB, CMP, SBC, AND, BIT, LD, ST, 
//                                    EOR, ADC, OR, ADD)
// ========================================================================

// Helper macro for immediate mode 8-bit operations
#define MC6809_OP_IMM8(name, reg, op) \
    bus_state_t op_##name##_imm(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            reg = op(reg, bus_read_data(pins)); \
            regs_[PC]++; \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

// CMP and BIT variants (don't store result)
#define MC6809_OP_CMP_IMM8(name, reg, op) \
    bus_state_t op_##name##_imm(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            op(reg, bus_read_data(pins)); \
            regs_[PC]++; \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

// Helper macro for direct mode 8-bit operations
#define MC6809_OP_DIRECT8(name, reg, op) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 2: \
            reg = op(reg, bus_read_data(pins)); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_CMP_DIRECT8(name, reg, op) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 2: \
            op(reg, bus_read_data(pins)); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

// Helper macro for indexed mode 8-bit operations (simplified)
#define MC6809_OP_INDEXED8(name, reg, op) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
                return bus_setup_read(pins, ea_); \
            } \
            ea_ = resolve_indexed_simple(postbyte_, pins); \
            return bus_setup_read(pins, ea_); \
        } \
        case 2: \
            reg = op(reg, bus_read_data(pins)); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_CMP_INDEXED8(name, reg, op) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
                return bus_setup_read(pins, ea_); \
            } \
            ea_ = resolve_indexed_simple(postbyte_, pins); \
            return bus_setup_read(pins, ea_); \
        } \
        case 2: \
            op(reg, bus_read_data(pins)); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

// Helper macro for extended mode 8-bit operations
#define MC6809_OP_EXTENDED8(name, reg, op) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 3: \
            reg = op(reg, bus_read_data(pins)); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_CMP_EXTENDED8(name, reg, op) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 3: \
            op(reg, bus_read_data(pins)); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

// ALU helper wrappers that match the (reg, operand) -> result signature
inline uint8_t wrap_sub8(uint8_t a, uint8_t b) { return alu_sub8(a, b, false); }
inline uint8_t wrap_sbc8(uint8_t a, uint8_t b) { return alu_sub8(a, b, true); }
inline uint8_t wrap_add8(uint8_t a, uint8_t b) { return alu_add8(a, b, false); }
inline uint8_t wrap_adc8(uint8_t a, uint8_t b) { return alu_add8(a, b, true); }

// Generate all A-register operations
MC6809_OP_IMM8(suba, regs_[A], wrap_sub8)
MC6809_OP_IMM8(sbca, regs_[A], wrap_sbc8)
MC6809_OP_IMM8(anda, regs_[A], alu_and8)
MC6809_OP_IMM8(eora, regs_[A], alu_eor8)
MC6809_OP_IMM8(adca, regs_[A], wrap_adc8)
MC6809_OP_IMM8(ora,  regs_[A], alu_or8)
MC6809_OP_IMM8(adda, regs_[A], wrap_add8)
MC6809_OP_CMP_IMM8(cmpa, regs_[A], alu_cmp8)
MC6809_OP_CMP_IMM8(bita, regs_[A], alu_bit8)

MC6809_OP_DIRECT8(suba, regs_[A], wrap_sub8)
MC6809_OP_DIRECT8(sbca, regs_[A], wrap_sbc8)
MC6809_OP_DIRECT8(anda, regs_[A], alu_and8)
MC6809_OP_DIRECT8(eora, regs_[A], alu_eor8)
MC6809_OP_DIRECT8(adca, regs_[A], wrap_adc8)
MC6809_OP_DIRECT8(ora,  regs_[A], alu_or8)
MC6809_OP_DIRECT8(adda, regs_[A], wrap_add8)
MC6809_OP_CMP_DIRECT8(cmpa, regs_[A], alu_cmp8)
MC6809_OP_CMP_DIRECT8(bita, regs_[A], alu_bit8)

MC6809_OP_INDEXED8(suba, regs_[A], wrap_sub8)
MC6809_OP_INDEXED8(sbca, regs_[A], wrap_sbc8)
MC6809_OP_INDEXED8(anda, regs_[A], alu_and8)
MC6809_OP_INDEXED8(eora, regs_[A], alu_eor8)
MC6809_OP_INDEXED8(adca, regs_[A], wrap_adc8)
MC6809_OP_INDEXED8(ora,  regs_[A], alu_or8)
MC6809_OP_INDEXED8(adda, regs_[A], wrap_add8)
MC6809_OP_CMP_INDEXED8(cmpa, regs_[A], alu_cmp8)
MC6809_OP_CMP_INDEXED8(bita, regs_[A], alu_bit8)

MC6809_OP_EXTENDED8(suba, regs_[A], wrap_sub8)
MC6809_OP_EXTENDED8(sbca, regs_[A], wrap_sbc8)
MC6809_OP_EXTENDED8(anda, regs_[A], alu_and8)
MC6809_OP_EXTENDED8(eora, regs_[A], alu_eor8)
MC6809_OP_EXTENDED8(adca, regs_[A], wrap_adc8)
MC6809_OP_EXTENDED8(ora,  regs_[A], alu_or8)
MC6809_OP_EXTENDED8(adda, regs_[A], wrap_add8)
MC6809_OP_CMP_EXTENDED8(cmpa, regs_[A], alu_cmp8)
MC6809_OP_CMP_EXTENDED8(bita, regs_[A], alu_bit8)

// Generate all B-register operations
MC6809_OP_IMM8(subb, regs_[B], wrap_sub8)
MC6809_OP_IMM8(sbcb, regs_[B], wrap_sbc8)
MC6809_OP_IMM8(andb, regs_[B], alu_and8)
MC6809_OP_IMM8(eorb, regs_[B], alu_eor8)
MC6809_OP_IMM8(adcb, regs_[B], wrap_adc8)
MC6809_OP_IMM8(orb,  regs_[B], alu_or8)
MC6809_OP_IMM8(addb, regs_[B], wrap_add8)
MC6809_OP_CMP_IMM8(cmpb, regs_[B], alu_cmp8)
MC6809_OP_CMP_IMM8(bitb, regs_[B], alu_bit8)

MC6809_OP_DIRECT8(subb, regs_[B], wrap_sub8)
MC6809_OP_DIRECT8(sbcb, regs_[B], wrap_sbc8)
MC6809_OP_DIRECT8(andb, regs_[B], alu_and8)
MC6809_OP_DIRECT8(eorb, regs_[B], alu_eor8)
MC6809_OP_DIRECT8(adcb, regs_[B], wrap_adc8)
MC6809_OP_DIRECT8(orb,  regs_[B], alu_or8)
MC6809_OP_DIRECT8(addb, regs_[B], wrap_add8)
MC6809_OP_CMP_DIRECT8(cmpb, regs_[B], alu_cmp8)
MC6809_OP_CMP_DIRECT8(bitb, regs_[B], alu_bit8)

MC6809_OP_INDEXED8(subb, regs_[B], wrap_sub8)
MC6809_OP_INDEXED8(sbcb, regs_[B], wrap_sbc8)
MC6809_OP_INDEXED8(andb, regs_[B], alu_and8)
MC6809_OP_INDEXED8(eorb, regs_[B], alu_eor8)
MC6809_OP_INDEXED8(adcb, regs_[B], wrap_adc8)
MC6809_OP_INDEXED8(orb,  regs_[B], alu_or8)
MC6809_OP_INDEXED8(addb, regs_[B], wrap_add8)
MC6809_OP_CMP_INDEXED8(cmpb, regs_[B], alu_cmp8)
MC6809_OP_CMP_INDEXED8(bitb, regs_[B], alu_bit8)

MC6809_OP_EXTENDED8(subb, regs_[B], wrap_sub8)
MC6809_OP_EXTENDED8(sbcb, regs_[B], wrap_sbc8)
MC6809_OP_EXTENDED8(andb, regs_[B], alu_and8)
MC6809_OP_EXTENDED8(eorb, regs_[B], alu_eor8)
MC6809_OP_EXTENDED8(adcb, regs_[B], wrap_adc8)
MC6809_OP_EXTENDED8(orb,  regs_[B], alu_or8)
MC6809_OP_EXTENDED8(addb, regs_[B], wrap_add8)
MC6809_OP_CMP_EXTENDED8(cmpb, regs_[B], alu_cmp8)
MC6809_OP_CMP_EXTENDED8(bitb, regs_[B], alu_bit8)

// Cleanup macros
#undef MC6809_OP_IMM8
#undef MC6809_OP_CMP_IMM8
#undef MC6809_OP_DIRECT8
#undef MC6809_OP_CMP_DIRECT8
#undef MC6809_OP_INDEXED8
#undef MC6809_OP_CMP_INDEXED8
#undef MC6809_OP_EXTENDED8
#undef MC6809_OP_CMP_EXTENDED8

// ========================================================================
// LDA/LDB — Load register (immediate sets flags)
// ========================================================================

bus_state_t op_lda_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        regs_[A] = bus_read_data(pins);
        regs_[PC]++;
        set_nz8(regs_[A]);
        set_flag(Flags::V, false);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_ldb_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        regs_[B] = bus_read_data(pins);
        regs_[PC]++;
        set_nz8(regs_[B]);
        set_flag(Flags::V, false);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// LD/ST macros for direct/indexed/extended
#define MC6809_OP_LD_DIRECT8(name, reg) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 2: \
            reg = bus_read_data(pins); \
            set_nz8(reg); \
            set_flag(Flags::V, false); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_LD_INDEXED8(name, reg) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
                return bus_setup_read(pins, ea_); \
            } \
            ea_ = resolve_indexed_simple(postbyte_, pins); \
            return bus_setup_read(pins, ea_); \
        } \
        case 2: \
            reg = bus_read_data(pins); \
            set_nz8(reg); \
            set_flag(Flags::V, false); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_LD_EXTENDED8(name, reg) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 3: \
            reg = bus_read_data(pins); \
            set_nz8(reg); \
            set_flag(Flags::V, false); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_ST_DIRECT8(name, reg) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            set_nz8(reg); \
            set_flag(Flags::V, false); \
            return bus_setup_write(pins, ea_, reg); \
        case 2: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_ST_INDEXED8(name, reg) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
            } else { \
                ea_ = resolve_indexed_simple(postbyte_, pins); \
            } \
            set_nz8(reg); \
            set_flag(Flags::V, false); \
            return bus_setup_write(pins, ea_, reg); \
        } \
        case 2: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_ST_EXTENDED8(name, reg) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            set_nz8(reg); \
            set_flag(Flags::V, false); \
            return bus_setup_write(pins, ea_, reg); \
        case 3: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_LD_DIRECT8(lda, regs_[A])
MC6809_OP_LD_DIRECT8(ldb, regs_[B])
MC6809_OP_LD_INDEXED8(lda, regs_[A])
MC6809_OP_LD_INDEXED8(ldb, regs_[B])
MC6809_OP_LD_EXTENDED8(lda, regs_[A])
MC6809_OP_LD_EXTENDED8(ldb, regs_[B])

MC6809_OP_ST_DIRECT8(sta, regs_[A])
MC6809_OP_ST_DIRECT8(stb, regs_[B])
MC6809_OP_ST_INDEXED8(sta, regs_[A])
MC6809_OP_ST_INDEXED8(stb, regs_[B])
MC6809_OP_ST_EXTENDED8(sta, regs_[A])
MC6809_OP_ST_EXTENDED8(stb, regs_[B])

#undef MC6809_OP_LD_DIRECT8
#undef MC6809_OP_LD_INDEXED8
#undef MC6809_OP_LD_EXTENDED8
#undef MC6809_OP_ST_DIRECT8
#undef MC6809_OP_ST_INDEXED8
#undef MC6809_OP_ST_EXTENDED8

// ========================================================================
// 16-BIT OPERATIONS (SUBD, ADDD, LDD, STD, LDX, STX, LDU, STU, CMPX)
// ========================================================================

// --- Immediate 16-bit ---

bus_state_t op_subd_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        regs_[D] = alu_sub16(regs_[D], val);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_addd_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        regs_[D] = alu_add16(regs_[D], val);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_cmpx_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_cmp16(regs_[X], val);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_ldd_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        regs_[A] = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        regs_[B] = bus_read_data(pins);
        regs_[PC]++;
        alu_ld16_flags(regs_[D]);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_ldx_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        regs_[X] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_ld16_flags(regs_[X]);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_ldu_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        regs_[U] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_ld16_flags(regs_[U]);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// --- Direct 16-bit --- (macro-generated)
#define MC6809_OP_DIRECT16(name, body) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 2: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 3: { \
            uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            body; \
            transition_to_fetch(); \
            return pins; \
        } \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_DIRECT16(subd, regs_[D] = alu_sub16(regs_[D], val))
MC6809_OP_DIRECT16(addd, regs_[D] = alu_add16(regs_[D], val))
MC6809_OP_DIRECT16(cmpx, alu_cmp16(regs_[X], val))
MC6809_OP_DIRECT16(ldd,  regs_[D] = val; alu_ld16_flags(regs_[D]))
MC6809_OP_DIRECT16(ldx,  regs_[X] = val; alu_ld16_flags(regs_[X]))
MC6809_OP_DIRECT16(ldu,  regs_[U] = val; alu_ld16_flags(regs_[U]))

#undef MC6809_OP_DIRECT16

// Store 16-bit direct
#define MC6809_OP_ST_DIRECT16(name, reg) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            alu_ld16_flags(reg); \
            return bus_setup_write(pins, ea_, reg >> 8); \
        case 2: \
            return bus_setup_write(pins, ea_ + 1, reg & 0xFF); \
        case 3: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_ST_DIRECT16(std, regs_[D])
MC6809_OP_ST_DIRECT16(stx, regs_[X])
MC6809_OP_ST_DIRECT16(stu, regs_[U])

#undef MC6809_OP_ST_DIRECT16

// --- Indexed 16-bit --- (simplified)
#define MC6809_OP_INDEXED16(name, body) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
            } else { \
                ea_ = resolve_indexed_simple(postbyte_, pins); \
            } \
            return bus_setup_read(pins, ea_); \
        } \
        case 2: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 3: { \
            uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            body; \
            transition_to_fetch(); \
            return pins; \
        } \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_INDEXED16(subd, regs_[D] = alu_sub16(regs_[D], val))
MC6809_OP_INDEXED16(addd, regs_[D] = alu_add16(regs_[D], val))
MC6809_OP_INDEXED16(cmpx, alu_cmp16(regs_[X], val))
MC6809_OP_INDEXED16(ldd,  regs_[D] = val; alu_ld16_flags(regs_[D]))
MC6809_OP_INDEXED16(ldx,  regs_[X] = val; alu_ld16_flags(regs_[X]))
MC6809_OP_INDEXED16(ldu,  regs_[U] = val; alu_ld16_flags(regs_[U]))

#undef MC6809_OP_INDEXED16

// Store 16-bit indexed
#define MC6809_OP_ST_INDEXED16(name, reg) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
            } else { \
                ea_ = resolve_indexed_simple(postbyte_, pins); \
            } \
            alu_ld16_flags(reg); \
            return bus_setup_write(pins, ea_, reg >> 8); \
        } \
        case 2: \
            return bus_setup_write(pins, ea_ + 1, reg & 0xFF); \
        case 3: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_ST_INDEXED16(std, regs_[D])
MC6809_OP_ST_INDEXED16(stx, regs_[X])
MC6809_OP_ST_INDEXED16(stu, regs_[U])

#undef MC6809_OP_ST_INDEXED16

// --- Extended 16-bit ---
#define MC6809_OP_EXTENDED16(name, body) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 3: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 4: { \
            uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            body; \
            transition_to_fetch(); \
            return pins; \
        } \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_EXTENDED16(subd, regs_[D] = alu_sub16(regs_[D], val))
MC6809_OP_EXTENDED16(addd, regs_[D] = alu_add16(regs_[D], val))
MC6809_OP_EXTENDED16(cmpx, alu_cmp16(regs_[X], val))
MC6809_OP_EXTENDED16(ldd,  regs_[D] = val; alu_ld16_flags(regs_[D]))
MC6809_OP_EXTENDED16(ldx,  regs_[X] = val; alu_ld16_flags(regs_[X]))
MC6809_OP_EXTENDED16(ldu,  regs_[U] = val; alu_ld16_flags(regs_[U]))

#undef MC6809_OP_EXTENDED16

// Store 16-bit extended
#define MC6809_OP_ST_EXTENDED16(name, reg) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            alu_ld16_flags(reg); \
            return bus_setup_write(pins, ea_, reg >> 8); \
        case 3: \
            return bus_setup_write(pins, ea_ + 1, reg & 0xFF); \
        case 4: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_ST_EXTENDED16(std, regs_[D])
MC6809_OP_ST_EXTENDED16(stx, regs_[X])
MC6809_OP_ST_EXTENDED16(stu, regs_[U])

#undef MC6809_OP_ST_EXTENDED16

// ========================================================================
// BRANCH INSTRUCTIONS
// ========================================================================

/// Short branch (BRA, BRN, BHI, BLS, BCC, BCS, BNE, BEQ, etc.)
bus_state_t op_bra_short(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        int8_t offset = static_cast<int8_t>(bus_read_data(pins));
        regs_[PC]++;
        if (eval_cc(opcode_))
            regs_[PC] = static_cast<uint16_t>(regs_[PC] + offset);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

/// LBRA — Long branch (always taken)
bus_state_t op_lbra(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        int16_t offset = static_cast<int16_t>((static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins));
        regs_[PC]++;
        regs_[PC] = static_cast<uint16_t>(regs_[PC] + offset);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

/// BSR — Branch to subroutine (short)
bus_state_t op_bsr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        int8_t offset = static_cast<int8_t>(bus_read_data(pins));
        regs_[PC]++;
        ea_ = static_cast<uint16_t>(regs_[PC] + offset);
        return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    }
    case 2:
        return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 3:
        regs_[PC] = ea_;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

/// LBSR — Long branch to subroutine
bus_state_t op_lbsr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        int16_t offset = static_cast<int16_t>((static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins));
        regs_[PC]++;
        ea_ = static_cast<uint16_t>(regs_[PC] + offset);
        return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    }
    case 3:
        return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 4:
        regs_[PC] = ea_;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// JSR — Jump to subroutine
// ========================================================================

bus_state_t op_jsr_direct(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    case 2:
        return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 3:
        regs_[PC] = ea_;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_jsr_indexed(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
        } else {
            ea_ = resolve_indexed_simple(postbyte_, pins);
        }
        return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    }
    case 2:
        return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 3:
        regs_[PC] = ea_;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_jsr_extended(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        ea_ |= bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    case 3:
        return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 4:
        regs_[PC] = ea_;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// LEA — Load Effective Address
// ========================================================================

bus_state_t op_leax(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
        } else {
            ea_ = resolve_indexed_simple(postbyte_, pins);
        }
        regs_[X] = ea_;
        set_flag(Flags::Z, regs_[X] == 0);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_leay(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
        } else {
            ea_ = resolve_indexed_simple(postbyte_, pins);
        }
        regs_[Y] = ea_;
        set_flag(Flags::Z, regs_[Y] == 0);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_leas(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
        } else {
            ea_ = resolve_indexed_simple(postbyte_, pins);
        }
        regs_[S] = ea_;
        nmi_armed_ = true;  // Loading S arms NMI
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_leau(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
        } else {
            ea_ = resolve_indexed_simple(postbyte_, pins);
        }
        regs_[U] = ea_;
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// STACK OPERATIONS (PSH/PUL)
// ========================================================================

bus_state_t op_pshs(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);  // Fetch postbyte
    case 1:
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        data_lo_ = 0;  // Push phase: walk bits 7→0
        return bus_internal(pins);
    default: {
        // Push registers in order: PC(7), U(6), Y(5), X(4), DP(3), B(2), A(1), CC(0)
        // 16-bit regs require two bus writes (hi byte first, then lo byte)
        while (data_lo_ < 12) {  // Max 12 push phases (8 bits, 16-bit regs = 2 each)
            uint8_t phase = data_lo_++;
            switch (phase) {
            case 0:  if (postbyte_ & 0x80) return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
                     break;
            case 1:  if (postbyte_ & 0x80) return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
                     break;
            case 2:  if (postbyte_ & 0x40) return bus_setup_write(pins, --regs_[S], regs_[U] & 0xFF);
                     break;
            case 3:  if (postbyte_ & 0x40) return bus_setup_write(pins, --regs_[S], regs_[U] >> 8);
                     break;
            case 4:  if (postbyte_ & 0x20) return bus_setup_write(pins, --regs_[S], regs_[Y] & 0xFF);
                     break;
            case 5:  if (postbyte_ & 0x20) return bus_setup_write(pins, --regs_[S], regs_[Y] >> 8);
                     break;
            case 6:  if (postbyte_ & 0x10) return bus_setup_write(pins, --regs_[S], regs_[X] & 0xFF);
                     break;
            case 7:  if (postbyte_ & 0x10) return bus_setup_write(pins, --regs_[S], regs_[X] >> 8);
                     break;
            case 8:  if (postbyte_ & 0x08) return bus_setup_write(pins, --regs_[S], regs_[DP]);
                     break;
            case 9:  if (postbyte_ & 0x04) return bus_setup_write(pins, --regs_[S], regs_[B]);
                     break;
            case 10: if (postbyte_ & 0x02) return bus_setup_write(pins, --regs_[S], regs_[A]);
                     break;
            case 11: if (postbyte_ & 0x01) return bus_setup_write(pins, --regs_[S], regs_[CC]);
                     break;
            }
        }
        transition_to_fetch();
        return pins;
    }
    }
}

bus_state_t op_puls(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);  // Fetch postbyte
    case 1:
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        data_lo_ = 0;  // Pull phase
        return bus_internal(pins);
    default: {
        // Process result from previous read, then start next read
        // Pull order: CC(0), A(1), B(2), DP(3), X(4), Y(5), U(6), PC(7)
        // 16-bit registers: read hi byte first, then lo byte
        //
        // We use a two-phase approach: even phases start a read,
        // odd phases consume the result and start the next read or finish.
        // But since step_ advances each call, we need to handle both
        // initiating reads and consuming their results.
        //
        // Simpler: use data_lo_ as a phase counter that tracks which
        // register byte to pull next. Each case either reads a result
        // from the previous cycle or starts a new read.
        //
        // On entry to default (step_>=2), we need to start pulls.
        // data_lo_ tracks current pull phase.

        // First call at step_==2: start the first read
        // Subsequent calls: consume previous read and start next

        // Phase state machine:
        // 0: start CC read (if needed)
        // 1: consume CC, start A read (if needed)
        // 2: consume A, start B read, etc.
        // ...
        // We handle this by having the previous step's result consumed
        // at the beginning of the current step.

        // Actually, let's use a different approach: track byte index
        // through a flattened list of bytes to pull.
        // On each tick: if we set up a read last tick, consume it.
        // Then set up the next read or finish.

        // Use offset_ to remember if we have pending data
        // Use ea_ to track what we're reading

        // Simplest correct approach: walk through bits, one read per tick.
        // Each case returns after setting up a read; next entry consumes it.

        while (data_lo_ < 12) {
            uint8_t phase = data_lo_++;
            switch (phase) {
            case 0:  if (postbyte_ & 0x01) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 1:  if (postbyte_ & 0x01) { regs_[CC] = bus_read_data(pins); }
                     if (postbyte_ & 0x02) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 2:  if (postbyte_ & 0x02) { regs_[A] = bus_read_data(pins); }
                     if (postbyte_ & 0x04) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 3:  if (postbyte_ & 0x04) { regs_[B] = bus_read_data(pins); }
                     if (postbyte_ & 0x08) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 4:  if (postbyte_ & 0x08) { regs_[DP] = bus_read_data(pins); }
                     if (postbyte_ & 0x10) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 5:  if (postbyte_ & 0x10) { regs_[X] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[S]++); }
                     break;
            case 6:  if (postbyte_ & 0x10) { regs_[X] |= bus_read_data(pins); }
                     if (postbyte_ & 0x20) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 7:  if (postbyte_ & 0x20) { regs_[Y] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[S]++); }
                     break;
            case 8:  if (postbyte_ & 0x20) { regs_[Y] |= bus_read_data(pins); }
                     if (postbyte_ & 0x40) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 9:  if (postbyte_ & 0x40) { regs_[U] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[S]++); }
                     break;
            case 10: if (postbyte_ & 0x40) { regs_[U] |= bus_read_data(pins); }
                     if (postbyte_ & 0x80) return bus_setup_read(pins, regs_[S]++);
                     break;
            case 11: if (postbyte_ & 0x80) { regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[S]++); }
                     break;
            }
        }
        // Final byte of PC if we were pulling it
        if (postbyte_ & 0x80) { regs_[PC] |= bus_read_data(pins); }
        transition_to_fetch();
        return pins;
    }
    }
}

bus_state_t op_pshu(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);  // Fetch postbyte
    case 1:
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        data_lo_ = 0;
        return bus_internal(pins);
    default: {
        // Push order: PC(7), S(6), Y(5), X(4), DP(3), B(2), A(1), CC(0)
        // Uses U stack instead of S; bit 6 = S (not U)
        while (data_lo_ < 12) {
            uint8_t phase = data_lo_++;
            switch (phase) {
            case 0:  if (postbyte_ & 0x80) return bus_setup_write(pins, --regs_[U], regs_[PC] & 0xFF);
                     break;
            case 1:  if (postbyte_ & 0x80) return bus_setup_write(pins, --regs_[U], regs_[PC] >> 8);
                     break;
            case 2:  if (postbyte_ & 0x40) return bus_setup_write(pins, --regs_[U], regs_[S] & 0xFF);
                     break;
            case 3:  if (postbyte_ & 0x40) return bus_setup_write(pins, --regs_[U], regs_[S] >> 8);
                     break;
            case 4:  if (postbyte_ & 0x20) return bus_setup_write(pins, --regs_[U], regs_[Y] & 0xFF);
                     break;
            case 5:  if (postbyte_ & 0x20) return bus_setup_write(pins, --regs_[U], regs_[Y] >> 8);
                     break;
            case 6:  if (postbyte_ & 0x10) return bus_setup_write(pins, --regs_[U], regs_[X] & 0xFF);
                     break;
            case 7:  if (postbyte_ & 0x10) return bus_setup_write(pins, --regs_[U], regs_[X] >> 8);
                     break;
            case 8:  if (postbyte_ & 0x08) return bus_setup_write(pins, --regs_[U], regs_[DP]);
                     break;
            case 9:  if (postbyte_ & 0x04) return bus_setup_write(pins, --regs_[U], regs_[B]);
                     break;
            case 10: if (postbyte_ & 0x02) return bus_setup_write(pins, --regs_[U], regs_[A]);
                     break;
            case 11: if (postbyte_ & 0x01) return bus_setup_write(pins, --regs_[U], regs_[CC]);
                     break;
            }
        }
        transition_to_fetch();
        return pins;
    }
    }
}

bus_state_t op_pulu(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);  // Fetch postbyte
    case 1:
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        data_lo_ = 0;
        return bus_internal(pins);
    default: {
        // Pull order: CC(0), A(1), B(2), DP(3), X(4), Y(5), S(6), PC(7)
        // Uses U stack; bit 6 = S (not U)
        while (data_lo_ < 12) {
            uint8_t phase = data_lo_++;
            switch (phase) {
            case 0:  if (postbyte_ & 0x01) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 1:  if (postbyte_ & 0x01) { regs_[CC] = bus_read_data(pins); }
                     if (postbyte_ & 0x02) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 2:  if (postbyte_ & 0x02) { regs_[A] = bus_read_data(pins); }
                     if (postbyte_ & 0x04) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 3:  if (postbyte_ & 0x04) { regs_[B] = bus_read_data(pins); }
                     if (postbyte_ & 0x08) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 4:  if (postbyte_ & 0x08) { regs_[DP] = bus_read_data(pins); }
                     if (postbyte_ & 0x10) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 5:  if (postbyte_ & 0x10) { regs_[X] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[U]++); }
                     break;
            case 6:  if (postbyte_ & 0x10) { regs_[X] |= bus_read_data(pins); }
                     if (postbyte_ & 0x20) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 7:  if (postbyte_ & 0x20) { regs_[Y] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[U]++); }
                     break;
            case 8:  if (postbyte_ & 0x20) { regs_[Y] |= bus_read_data(pins); }
                     if (postbyte_ & 0x40) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 9:  if (postbyte_ & 0x40) { regs_[S] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[U]++); }
                     break;
            case 10: if (postbyte_ & 0x40) { regs_[S] |= bus_read_data(pins); }
                     if (postbyte_ & 0x80) return bus_setup_read(pins, regs_[U]++);
                     break;
            case 11: if (postbyte_ & 0x80) { regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8; return bus_setup_read(pins, regs_[U]++); }
                     break;
            }
        }
        if (postbyte_ & 0x80) { regs_[PC] |= bus_read_data(pins); }
        transition_to_fetch();
        return pins;
    }
    }
}

// ========================================================================
// MISC INSTRUCTIONS
// ========================================================================

/// RTS — Return from subroutine
bus_state_t op_rts(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[S]++);
    case 1:
        regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, regs_[S]++);
    case 2:
        regs_[PC] |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

/// ABX — Add B to X (unsigned)
bus_state_t op_abx(bus_state_t pins) {
    regs_[X] = static_cast<uint16_t>(regs_[X] + regs_[B]);
    transition_to_fetch();
    return pins;
}

/// RTI — Return from interrupt
bus_state_t op_rti(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[S]++);
    case 1:
        regs_[CC] = bus_read_data(pins);
        if (!(regs_[CC] & Flags::E)) {
            // Fast interrupt: only CC and PC were saved
            return bus_setup_read(pins, regs_[S]++);
        }
        // Entire state: CC, A, B, DP, X, Y, U, PC
        return bus_setup_read(pins, regs_[S]++);
    case 2:
        if (!(regs_[CC] & Flags::E)) {
            regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8;
            return bus_setup_read(pins, regs_[S]++);
        }
        regs_[A] = bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 3:
        if (!(regs_[CC] & Flags::E)) {
            regs_[PC] |= bus_read_data(pins);
            transition_to_fetch();
            return pins;
        }
        regs_[B] = bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 4:
        regs_[DP] = bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 5:
        data_hi_ = bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 6:
        regs_[X] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 7:
        data_hi_ = bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 8:
        regs_[Y] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 9:
        data_hi_ = bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 10:
        regs_[U] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        return bus_setup_read(pins, regs_[S]++);
    case 11:
        regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, regs_[S]++);
    case 12:
        regs_[PC] |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

/// MUL — Multiply A × B → D
bus_state_t op_mul(bus_state_t pins) {
    alu_mul();
    transition_to_fetch();
    return pins;
}

/// SWI — Software interrupt
bus_state_t op_swi(bus_state_t pins) {
    switch (step_++) {
    case 0:
        regs_[CC] |= Flags::E;
        return bus_internal(pins);
    case 1:  return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    case 2:  return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 3:  return bus_setup_write(pins, --regs_[S], regs_[U] & 0xFF);
    case 4:  return bus_setup_write(pins, --regs_[S], regs_[U] >> 8);
    case 5:  return bus_setup_write(pins, --regs_[S], regs_[Y] & 0xFF);
    case 6:  return bus_setup_write(pins, --regs_[S], regs_[Y] >> 8);
    case 7:  return bus_setup_write(pins, --regs_[S], regs_[X] & 0xFF);
    case 8:  return bus_setup_write(pins, --regs_[S], regs_[X] >> 8);
    case 9:  return bus_setup_write(pins, --regs_[S], regs_[DP]);
    case 10: return bus_setup_write(pins, --regs_[S], regs_[B]);
    case 11: return bus_setup_write(pins, --regs_[S], regs_[A]);
    case 12: return bus_setup_write(pins, --regs_[S], regs_[CC]);
    case 13:
        regs_[CC] |= Flags::I | Flags::F;
        return bus_setup_read(pins, Vector::SWI);
    case 14:
        regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, Vector::SWI + 1);
    case 15:
        regs_[PC] |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

/// SYNC — Synchronize with interrupt
bus_state_t op_sync(bus_state_t pins) {
    sync_wait_ = true;
    // Check if any interrupt is pending
    if (!BUS_GET_BIT(pins, BUS_IRQ_BIT) ||
        !BUS_GET_BIT(pins, BUS_NMI_BIT) ||
        !BUS_GET_BIT(pins, MC6809_FIRQ_BIT)) {
        sync_wait_ = false;
        transition_to_fetch();
    }
    return pins;
}

/// CWAI — Clear flags and wait for interrupt
bus_state_t op_cwai(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        uint8_t mask = bus_read_data(pins);
        regs_[PC]++;
        regs_[CC] &= mask;
        // Push entire state
        regs_[CC] |= Flags::E;
        cwai_wait_ = true;
        return bus_internal(pins);
    }
    case 2:  return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    case 3:  return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 4:  return bus_setup_write(pins, --regs_[S], regs_[U] & 0xFF);
    case 5:  return bus_setup_write(pins, --regs_[S], regs_[U] >> 8);
    case 6:  return bus_setup_write(pins, --regs_[S], regs_[Y] & 0xFF);
    case 7:  return bus_setup_write(pins, --regs_[S], regs_[Y] >> 8);
    case 8:  return bus_setup_write(pins, --regs_[S], regs_[X] & 0xFF);
    case 9:  return bus_setup_write(pins, --regs_[S], regs_[X] >> 8);
    case 10: return bus_setup_write(pins, --regs_[S], regs_[DP]);
    case 11: return bus_setup_write(pins, --regs_[S], regs_[B]);
    case 12: return bus_setup_write(pins, --regs_[S], regs_[A]);
    case 13: return bus_setup_write(pins, --regs_[S], regs_[CC]);
    default:
        // Stay in CWAI until interrupt arrives
        if (!BUS_GET_BIT(pins, BUS_IRQ_BIT) ||
            !BUS_GET_BIT(pins, BUS_NMI_BIT) ||
            !BUS_GET_BIT(pins, MC6809_FIRQ_BIT)) {
            cwai_wait_ = false;
            transition_to_fetch();
        }
        return pins;
    }
}

/// DAA — Decimal Adjust Accumulator
bus_state_t op_daa(bus_state_t pins) {
    alu_daa();
    transition_to_fetch();
    return pins;
}

/// ORCC — OR Condition Codes with immediate
bus_state_t op_orcc(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        regs_[CC] |= bus_read_data(pins);
        regs_[PC]++;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

/// ANDCC — AND Condition Codes with immediate
bus_state_t op_andcc(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        regs_[CC] &= bus_read_data(pins);
        regs_[PC]++;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

/// SEX — Sign Extend B → D
bus_state_t op_sex(bus_state_t pins) {
    alu_sex();
    transition_to_fetch();
    return pins;
}

/// EXG — Exchange registers
bus_state_t op_exg(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        uint8_t pb = bus_read_data(pins);
        regs_[PC]++;
        uint8_t src = (pb >> 4) & 0x0F;
        uint8_t dst = pb & 0x0F;
        uint16_t tmp = get_tfr_reg(src);
        set_tfr_reg(src, get_tfr_reg(dst));
        set_tfr_reg(dst, tmp);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

/// TFR — Transfer register
bus_state_t op_tfr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        uint8_t pb = bus_read_data(pins);
        regs_[PC]++;
        uint8_t src = (pb >> 4) & 0x0F;
        uint8_t dst = pb & 0x0F;
        set_tfr_reg(dst, get_tfr_reg(src));
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// HD6309 STUBS (only compiled for HD6309 variant)
// ========================================================================

bus_state_t op_oim_direct(bus_state_t pins)  { transition_to_fetch(); return pins; }
bus_state_t op_aim_direct(bus_state_t pins)  { transition_to_fetch(); return pins; }
bus_state_t op_eim_direct(bus_state_t pins)  { transition_to_fetch(); return pins; }
bus_state_t op_tim_direct(bus_state_t pins)  { transition_to_fetch(); return pins; }
bus_state_t op_oim_indexed(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_aim_indexed(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_eim_indexed(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_tim_indexed(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_oim_extended(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_aim_extended(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_eim_extended(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_tim_extended(bus_state_t pins) { transition_to_fetch(); return pins; }
bus_state_t op_sexw(bus_state_t pins)        { transition_to_fetch(); return pins; }
bus_state_t op_ldq_imm(bus_state_t pins)     { transition_to_fetch(); return pins; }

// Cleanup RMW macros
#undef MC6809_DEFINE_RMW_DIRECT
#undef MC6809_DEFINE_RMW_INDEXED
#undef MC6809_DEFINE_RMW_EXTENDED
