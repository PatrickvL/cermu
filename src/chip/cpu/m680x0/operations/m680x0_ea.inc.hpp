// m680x0_ea.inc.hpp — Effective Address evaluation helpers
//
// Provides calc_ea, read_ea, write_ea — shared infrastructure consumed
// by all instruction group decoders.
//
// Included inside m680x0_t class body with M680X0_TEMPLATE_CONTEXT defined.

#include "chip/cpu/m680x0/operations/inc_lint_prevention.hpp"

// ════════════════════════════════════════════════════════════════════
// Effective Address evaluation
// ════════════════════════════════════════════════════════════════════

// calc_ea: compute the effective address for a given mode/register/size.
// For register-direct modes, returns the register index.
// For memory modes, returns the memory address.
// NOTE: Extension words (displacement, index) consume prefetch — the
//       caller must arrange bus cycles for those reads.

inline uint32_t calc_ea(uint8_t mode, uint8_t reg, OpSize sz) {
    switch (static_cast<EAMode>(mode)) {
        case EAMode::DataRegDirect:
            return reg;  // Index into d[]
        case EAMode::AddrRegDirect:
            return reg;  // Index into a[]
        case EAMode::AddrRegIndirect:
            return get_a(reg);
        case EAMode::AddrRegPostInc: {
            uint32_t addr = get_a(reg);
            uint32_t inc = size_bytes(sz);
            // Stack pointer (A7) always stays word-aligned
            if (reg == 7 && sz == OpSize::Byte) inc = 2;
            set_a(reg, addr + inc);
            return addr;
        }
        case EAMode::AddrRegPreDec: {
            uint32_t dec = size_bytes(sz);
            if (reg == 7 && sz == OpSize::Byte) dec = 2;
            set_a(reg, get_a(reg) - dec);
            return get_a(reg);
        }
        case EAMode::AddrRegDisp: {
            int16_t disp = static_cast<int16_t>(consume_extension_word());
            return get_a(reg) + disp;
        }
        case EAMode::AddrRegIndex: {
            uint16_t ext = consume_extension_word();
            uint8_t  xn_reg  = (ext >> 12) & 7;
            bool     xn_is_a = (ext & 0x8000) != 0;
            bool     xn_long = (ext & 0x0800) != 0;
            int8_t   disp8   = static_cast<int8_t>(ext & 0xFF);
            int32_t  xn_val;
            if (xn_is_a) {
                xn_val = static_cast<int32_t>(get_a(xn_reg));
            } else {
                xn_val = xn_long ? static_cast<int32_t>(get_d(xn_reg))
                                 : static_cast<int32_t>(static_cast<int16_t>(get_d_w(xn_reg)));
            }
            clocks_remaining_ += 2;  // 2 idle clocks for index calculation
            return get_a(reg) + disp8 + xn_val;
        }
        case EAMode::Special:
            switch (reg) {
                case 0: {  // Abs.W
                    int16_t addr = static_cast<int16_t>(consume_extension_word());
                    return static_cast<uint32_t>(static_cast<int32_t>(addr));
                }
                case 1: {  // Abs.L
                    uint32_t addr = static_cast<uint32_t>(consume_extension_word()) << 16;
                    addr |= consume_extension_word();
                    return addr;
                }
                case 2: {  // (d16,PC)
                    uint32_t base = regs_.pc;
                    int16_t disp = static_cast<int16_t>(consume_extension_word());
                    return base + disp;
                }
                case 3: {  // (d8,PC,Xn)
                    uint32_t base = regs_.pc;
                    uint16_t ext = consume_extension_word();
                    uint8_t  xn_reg  = (ext >> 12) & 7;
                    bool     xn_is_a = (ext & 0x8000) != 0;
                    bool     xn_long = (ext & 0x0800) != 0;
                    int8_t   disp8   = static_cast<int8_t>(ext & 0xFF);
                    int32_t  xn_val;
                    if (xn_is_a) {
                        xn_val = static_cast<int32_t>(get_a(xn_reg));
                    } else {
                        xn_val = xn_long ? static_cast<int32_t>(get_d(xn_reg))
                                         : static_cast<int32_t>(static_cast<int16_t>(get_d_w(xn_reg)));
                    }
                    clocks_remaining_ += 2;  // 2 idle clocks for index calculation
                    return base + disp8 + xn_val;
                }
                case 4: {  // #imm
                    // Immediate: data from IRC
                    return 0;  // read_ea handles immediate differently
                }
                default:
                    return 0;
            }
    }
    return 0;
}

// read_ea / write_ea — synchronous via mem_read_ / mem_write_ callbacks.
// For memory modes: calculates EA, reads/writes via callback.
// For register/immediate modes: operates directly.

inline uint32_t read_ea(uint8_t mode, uint8_t reg, OpSize sz) {
    if (mode == static_cast<uint8_t>(EAMode::DataRegDirect)) {
        return read_dn(reg, sz);
    }
    if (mode == static_cast<uint8_t>(EAMode::AddrRegDirect)) {
        return get_a(reg);
    }
    if (mode == static_cast<uint8_t>(EAMode::Special) && reg == 4) {
        // Immediate: read from prefetch pipeline
        if (sz == OpSize::Long) {
            uint32_t val = static_cast<uint32_t>(consume_extension_word()) << 16;
            val |= consume_extension_word();
            return val;
        } else {
            return consume_extension_word() & size_mask(sz);
        }
    }
    // Memory modes — calculate address, read via callback
    ea_addr_ = calc_ea(mode, reg, sz);
    if (mem_read_) {
        uint32_t addr = ea_addr_ & address_mask();
        if (sz == OpSize::Long) {
            uint16_t hi = mem_read_(mem_ctx_, addr & ~1u);
            uint16_t lo = mem_read_(mem_ctx_, (addr + 2) & ~1u);
            clocks_remaining_ += 8;  // 2 × 4-clock word reads
            return (static_cast<uint32_t>(hi) << 16) | lo;
        } else if (sz == OpSize::Byte) {
            uint16_t word = mem_read_(mem_ctx_, addr & ~1u);
            clocks_remaining_ += 4;  // 1 × 4-clock bus cycle
            return (addr & 1) ? (word & 0xFF) : (word >> 8);
        } else {
            clocks_remaining_ += 4;  // 1 × 4-clock word read
            return mem_read_(mem_ctx_, addr);
        }
    }
    // Fallback: return data_latch_ (set by bus cycle handlers)
    return data_latch_ & size_mask(sz);
}

inline void write_ea(uint8_t mode, uint8_t reg, uint32_t value, OpSize sz) {
    if (mode == static_cast<uint8_t>(EAMode::DataRegDirect)) {
        write_dn(reg, value, sz);
        return;
    }
    if (mode == static_cast<uint8_t>(EAMode::AddrRegDirect)) {
        set_a(reg, value);
        return;
    }
    // Memory write via callback
    ea_addr_ = calc_ea(mode, reg, sz);
    if (mem_write_) {
        uint32_t addr = ea_addr_ & address_mask();
        if (sz == OpSize::Long) {
            mem_write_(mem_ctx_, addr,     static_cast<uint16_t>((value >> 16) & 0xFFFF));
            mem_write_(mem_ctx_, addr + 2, static_cast<uint16_t>(value & 0xFFFF));
            clocks_remaining_ += 8;
        } else if (sz == OpSize::Byte) {
            // Read-modify-write: only change the target byte in the word
            uint32_t even_addr = addr & ~1u;
            uint16_t existing = mem_read_(mem_ctx_, even_addr);
            uint16_t word;
            if (addr & 1)
                word = (existing & 0xFF00) | (value & 0xFF);          // odd → low byte
            else
                word = ((value & 0xFF) << 8) | (existing & 0x00FF);   // even → high byte
            mem_write_(mem_ctx_, even_addr, word);
            clocks_remaining_ += 4;
        } else {
            mem_write_(mem_ctx_, addr, static_cast<uint16_t>(value));
            clocks_remaining_ += 4;
        }
    }
    data_latch_ = value;
}

// write_back_ea: write result to the address already computed by a prior
// read_ea or calc_ea (stored in ea_addr_). Used by read-modify-write
// instructions to avoid a second calc_ea that would double side effects
// (e.g. PostInc / PreDec).
inline void write_back_ea(uint32_t value, OpSize sz) {
    if (mem_write_) {
        uint32_t addr = ea_addr_ & address_mask();
        if (sz == OpSize::Long) {
            mem_write_(mem_ctx_, addr,     static_cast<uint16_t>((value >> 16) & 0xFFFF));
            mem_write_(mem_ctx_, addr + 2, static_cast<uint16_t>(value & 0xFFFF));
            clocks_remaining_ += 8;
        } else if (sz == OpSize::Byte) {
            uint32_t even_addr = addr & ~1u;
            uint16_t existing = mem_read_(mem_ctx_, even_addr);
            uint16_t word;
            if (addr & 1)
                word = (existing & 0xFF00) | (value & 0xFF);
            else
                word = ((value & 0xFF) << 8) | (existing & 0x00FF);
            mem_write_(mem_ctx_, even_addr, word);
            clocks_remaining_ += 4;
        } else {
            mem_write_(mem_ctx_, addr, static_cast<uint16_t>(value));
            clocks_remaining_ += 4;
        }
    }
    data_latch_ = value;
}

#include "chip/cpu/m680x0/operations/inc_lint_prevention_footer.hpp"
