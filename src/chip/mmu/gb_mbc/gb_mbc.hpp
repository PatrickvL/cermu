#pragma once
/*
 * gb_mbc.hpp — Game Boy Memory Bank Controllers
 *
 * The Game Boy cartridge header at $0147 specifies the cartridge type,
 * which determines the MBC variant used for ROM/RAM banking.
 *
 * Common MBC types:
 *   $00: ROM only (no MBC, max 32KB ROM)
 *   $01–$03: MBC1 (max 2MB ROM, 32KB RAM)
 *   $05–$06: MBC2 (max 256KB ROM, built-in 512×4 RAM)
 *   $0F–$13: MBC3 (max 2MB ROM, 32KB RAM, optional RTC)
 *   $19–$1E: MBC5 (max 8MB ROM, 128KB RAM)
 *
 * Cartridge header fields:
 *   $0147: Cartridge type (MBC variant)
 *   $0148: ROM size (0=32KB, 1=64KB, ... 8=8MB)
 *   $0149: RAM size (0=none, 2=8KB, 3=32KB, 4=128KB, 5=64KB)
 *
 * Architecture: NTTP-based template gb_mbc_t<GbMbcTraits> deriving from
 * MmuChipBase for chip-framework integration (debug, registry, layout).
 * Variant-specific logic is gated by if constexpr on the traits type field.
 */

#include "chip/mmu/mmu_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>
#include <memory>

// ── MBC variant type ────────────────────────────────────────────────
enum class GbMbcType : uint8_t { None, MBC1, MBC2, MBC3, MBC5 };

// ── NTTP traits ─────────────────────────────────────────────────────
struct GbMbcTraits {
    const char* name;
    const char* description;
    GbMbcType   type;
    uint8_t     rom_bank_bits;   // Width of ROM bank register
    uint8_t     ram_bank_bits;   // Width of RAM bank register
    bool        has_rtc;         // MBC3 real-time clock
    bool        has_builtin_ram; // MBC2 internal 512×4 RAM
    bool        has_mode_select; // MBC1 ROM/RAM banking mode
};

inline constexpr GbMbcTraits kMbcNoneTraits = {
    "ROM Only",  "No MBC — max 32KB ROM",
    GbMbcType::None, 0, 0, false, false, false
};
inline constexpr GbMbcTraits kMbc1Traits = {
    "MBC1", "Max 2MB ROM, 32KB RAM",
    GbMbcType::MBC1, 5, 2, false, false, true
};
inline constexpr GbMbcTraits kMbc2Traits = {
    "MBC2", "Max 256KB ROM, built-in 512x4b RAM",
    GbMbcType::MBC2, 4, 0, false, true, false
};
inline constexpr GbMbcTraits kMbc3Traits = {
    "MBC3", "Max 2MB ROM, 32KB RAM, RTC",
    GbMbcType::MBC3, 7, 2, true, false, false
};
inline constexpr GbMbcTraits kMbc5Traits = {
    "MBC5", "Max 8MB ROM, 128KB RAM",
    GbMbcType::MBC5, 9, 4, false, false, false
};

// ── DECL register map ───────────────────────────────────────────────
#define GB_MBC_DECL(REG, FLD, CMP) \
    REG(0x00, ROM_BANK_LO, "ROM Bank (low)") \
    REG(0x01, ROM_BANK_HI, "ROM Bank (high / MBC5 bit 8)") \
    REG(0x02, RAM_BANK,    "RAM Bank") \
    REG(0x03, RAM_ENABLE,  "RAM Enable") \
    REG(0x04, MODE_SELECT, "Banking Mode (MBC1)") \
    FLD(ROM_BANK_LO, BANK_LO, 7:0, "ROM bank low bits",  Value, 0, 0) \
    FLD(ROM_BANK_HI, BANK_HI, 0:0, "ROM bank bit 8",     Flag,  0, 0) \
    FLD(RAM_BANK,    BANK,    3:0, "RAM bank number",     Value, 0, 0) \
    FLD(RAM_ENABLE,  ENABLED, 0:0, "RAM enabled",         Flag,  0, 0) \
    FLD(MODE_SELECT, MODE,    0:0, "ROM/RAM mode select", Flag,  0, 0)

DECL_EXTRACT(GB_MBC, GB_MBC_DECL)

// ============================================================================
// GbMbcBase — abstract interface for all Game Boy MBC types
// ============================================================================
//
// Provides the runtime-polymorphic API (rom_read, rom_write, ram_read,
// ram_write) that the system tick loop calls.  gb_mbc_t<T> implements
// the variant-specific logic.

class GbMbcBase : public MmuChipBase {
public:
    using MmuChipBase::MmuChipBase;

    /// Bind cartridge ROM/RAM buffers (owned by the system, not the chip).
    void bind_cartridge(uint8_t* rom, uint32_t rom_size,
                        uint8_t* ram, uint32_t ram_size) {
        rom_      = rom;
        rom_size_ = rom_size;
        ram_      = ram;
        ram_size_ = ram_size;
    }

    virtual uint8_t rom_read(uint16_t addr) const noexcept = 0;
    virtual void    rom_write(uint16_t addr, uint8_t data) noexcept = 0;
    virtual uint8_t ram_read(uint16_t addr) const noexcept = 0;
    virtual void    ram_write(uint16_t addr, uint8_t data) noexcept = 0;

protected:
    uint8_t*  rom_      = nullptr;
    uint32_t  rom_size_ = 0;
    uint8_t*  ram_      = nullptr;
    uint32_t  ram_size_ = 0;
    bool      ram_enabled_ = false;
};

// ============================================================================
// gb_mbc_t<T> — NTTP-parameterised MBC implementation
// ============================================================================
//
// MBC register map (active on write to ROM area $0000–$7FFF):
//
// MBC1:
//   $0000–$1FFF: RAM enable (0x0A = enable, else disable)
//   $2000–$3FFF: ROM bank number (lower 5 bits, 0 maps to 1)
//   $4000–$5FFF: RAM bank / upper ROM bank bits (2 bits)
//   $6000–$7FFF: Banking mode (0 = ROM mode, 1 = RAM mode)
//
// MBC2:
//   $0000–$3FFF: Bit 8 set → ROM bank (4 bits), clear → RAM enable
//
// MBC3:
//   $0000–$1FFF: RAM/RTC enable (0x0A = enable)
//   $2000–$3FFF: ROM bank (7 bits, 0 maps to 1)
//   $4000–$5FFF: RAM bank (0–3) or RTC register select ($08–$0C)
//   $6000–$7FFF: Latch clock data (write 0x00 then 0x01 to latch)
//
// MBC5:
//   $0000–$1FFF: RAM enable
//   $2000–$2FFF: ROM bank low 8 bits
//   $3000–$3FFF: ROM bank bit 8
//   $4000–$5FFF: RAM bank (4 bits, 0–15)

template<const GbMbcTraits& T>
class gb_mbc_t : public GbMbcBase {
public:
    gb_mbc_t()
        : GbMbcBase(ChipInfo{T.name, "Nintendo", T.description})
    {
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void reset() override {
        ram_enabled_ = false;
        rom_bank_ = (T.type == GbMbcType::None) ? static_cast<uint16_t>(0) : static_cast<uint16_t>(1);
        ram_bank_ = 0;
        mode_ = false;

        if constexpr (T.has_rtc) {
            std::memset(rtc_regs_, 0, sizeof(rtc_regs_));
            std::memset(rtc_latched_, 0, sizeof(rtc_latched_));
            latch_state_ = 0xFF;
            rtc_select_ = false;
        }
        if constexpr (T.has_builtin_ram) {
            std::memset(internal_ram_, 0, sizeof(internal_ram_));
        }
    }

    // ── ROM read ($0000–$7FFF) ──────────────────────────────────────
    uint8_t rom_read(uint16_t addr) const noexcept override {
        if constexpr (T.type == GbMbcType::None) {
            return (addr < rom_size_) ? rom_[addr] : 0xFF;
        } else if constexpr (T.type == GbMbcType::MBC1) {
            if (addr < 0x4000) {
                // Bank 0 area — in RAM mode, upper bits can select bank
                uint32_t bank0 = mode_ ? (static_cast<uint32_t>(ram_bank_) << 5) : 0;
                uint32_t offset = (bank0 * 0x4000 + addr) % rom_size_;
                return rom_[offset];
            }
            // Bank N area
            uint32_t bank = rom_bank_;
            if (!mode_)
                bank |= (static_cast<uint32_t>(ram_bank_) << 5);
            uint32_t offset = (bank * 0x4000 + (addr - 0x4000)) % rom_size_;
            return rom_[offset];
        } else {
            // MBC2, MBC3, MBC5: bank 0 always at $0000–$3FFF
            if (addr < 0x4000) return rom_[addr % rom_size_];
            uint32_t offset = (static_cast<uint32_t>(rom_bank_) * 0x4000 + (addr - 0x4000)) % rom_size_;
            return rom_[offset];
        }
    }

    // ── ROM write ($0000–$7FFF) — bank register updates ─────────────
    void rom_write(uint16_t addr, uint8_t data) noexcept override {
        if constexpr (T.type == GbMbcType::None) {
            // No MBC — writes to ROM area are ignored
        } else if constexpr (T.type == GbMbcType::MBC1) {
            if (addr < 0x2000) {
                ram_enabled_ = ((data & 0x0F) == 0x0A);
            } else if (addr < 0x4000) {
                rom_bank_ = data & 0x1F;
                if (rom_bank_ == 0) rom_bank_ = 1;
            } else if (addr < 0x6000) {
                ram_bank_ = data & 0x03;
            } else {
                mode_ = (data & 0x01) != 0;
            }
        } else if constexpr (T.type == GbMbcType::MBC2) {
            if (addr < 0x4000) {
                if (addr & 0x0100) {
                    // Bit 8 set: ROM bank select
                    rom_bank_ = data & 0x0F;
                    if (rom_bank_ == 0) rom_bank_ = 1;
                } else {
                    // Bit 8 clear: RAM enable
                    ram_enabled_ = ((data & 0x0F) == 0x0A);
                }
            }
        } else if constexpr (T.type == GbMbcType::MBC3) {
            if (addr < 0x2000) {
                ram_enabled_ = ((data & 0x0F) == 0x0A);
            } else if (addr < 0x4000) {
                rom_bank_ = data & 0x7F;
                if (rom_bank_ == 0) rom_bank_ = 1;
            } else if (addr < 0x6000) {
                if (data <= 0x03) {
                    ram_bank_ = data;
                    rtc_select_ = false;
                } else if (data >= 0x08 && data <= 0x0C) {
                    ram_bank_ = data - 0x08;
                    rtc_select_ = true;
                }
            } else {
                // Latch clock: write 0x00 then 0x01
                if (latch_state_ == 0x00 && data == 0x01)
                    std::memcpy(rtc_latched_, rtc_regs_, sizeof(rtc_regs_));
                latch_state_ = data;
            }
        } else if constexpr (T.type == GbMbcType::MBC5) {
            if (addr < 0x2000) {
                ram_enabled_ = ((data & 0x0F) == 0x0A);
            } else if (addr < 0x3000) {
                rom_bank_ = (rom_bank_ & 0x100) | data;
            } else if (addr < 0x4000) {
                rom_bank_ = (rom_bank_ & 0xFF) | ((data & 0x01) << 8);
            } else if (addr < 0x6000) {
                ram_bank_ = data & 0x0F;
            }
        }
    }

    // ── RAM read ($A000–$BFFF) ──────────────────────────────────────
    uint8_t ram_read(uint16_t addr) const noexcept override {
        if constexpr (T.has_builtin_ram) {
            // MBC2: built-in 512×4 bit RAM (upper 4 bits undefined)
            if (!ram_enabled_) return 0xFF;
            return internal_ram_[(addr - 0xA000) & 0x01FF] | 0xF0;
        } else if constexpr (T.has_rtc) {
            // MBC3: RAM or RTC register
            if (!ram_enabled_) return 0xFF;
            if (rtc_select_)
                return (ram_bank_ < 5) ? rtc_latched_[ram_bank_] : 0xFF;
            return default_ram_read(addr);
        } else {
            return default_ram_read(addr);
        }
    }

    // ── RAM write ($A000–$BFFF) ─────────────────────────────────────
    void ram_write(uint16_t addr, uint8_t data) noexcept override {
        if constexpr (T.has_builtin_ram) {
            if (!ram_enabled_) return;
            internal_ram_[(addr - 0xA000) & 0x01FF] = data & 0x0F;
        } else if constexpr (T.has_rtc) {
            if (!ram_enabled_) return;
            if (rtc_select_) {
                if (ram_bank_ < 5) rtc_regs_[ram_bank_] = data;
                return;
            }
            default_ram_write(addr, data);
        } else {
            default_ram_write(addr, data);
        }
    }

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint16_t rom_bank_ = 1;     // Current ROM bank register
    uint8_t  ram_bank_ = 0;     // Current RAM bank register
    bool     mode_     = false; // MBC1 ROM/RAM mode select

    // MBC3-specific: RTC registers
    bool     rtc_select_      = false;
    uint8_t  rtc_regs_[5]     = {};     // S, M, H, DL, DH
    uint8_t  rtc_latched_[5]  = {};
    uint8_t  latch_state_     = 0xFF;

    // MBC2-specific: built-in 512×4 bit RAM
    uint8_t  internal_ram_[512] = {};

    // ── Banked RAM offset calculation ───────────────────────────────
    uint32_t ram_offset(uint16_t addr) const noexcept {
        if constexpr (T.type == GbMbcType::MBC1) {
            uint32_t bank = mode_ ? ram_bank_ : 0;
            return bank * 0x2000 + (addr - 0xA000);
        } else {
            return static_cast<uint32_t>(ram_bank_) * 0x2000 + (addr - 0xA000);
        }
    }

    uint8_t default_ram_read(uint16_t addr) const noexcept {
        if (!ram_enabled_ || !ram_ || ram_size_ == 0) return 0xFF;
        uint32_t offset = ram_offset(addr);
        return (offset < ram_size_) ? ram_[offset] : 0xFF;
    }

    void default_ram_write(uint16_t addr, uint8_t data) noexcept {
        if (!ram_enabled_ || !ram_ || ram_size_ == 0) return;
        uint32_t offset = ram_offset(addr);
        if (offset < ram_size_) ram_[offset] = data;
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category(T.name)
            .value("ROM Bank", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_mbc_t*>(c)->rom_bank_;
            })
            .value("RAM Bank", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_mbc_t*>(c)->ram_bank_;
            })
            .flag("RAM Enabled", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_mbc_t*>(c)->ram_enabled_ ? 1u : 0u;
            });

        if constexpr (T.has_mode_select) {
            debug_registry_
                .flag("Mode (0=ROM, 1=RAM)", +[](const ChipBase* c) -> uint32_t {
                    return static_cast<const gb_mbc_t*>(c)->mode_ ? 1u : 0u;
                });
        }
        if constexpr (T.has_rtc) {
            debug_registry_
                .flag("RTC Selected", +[](const ChipBase* c) -> uint32_t {
                    return static_cast<const gb_mbc_t*>(c)->rtc_select_ ? 1u : 0u;
                })
                .value("RTC Seconds", +[](const ChipBase* c) -> uint32_t {
                    return static_cast<const gb_mbc_t*>(c)->rtc_regs_[0];
                })
                .value("RTC Minutes", +[](const ChipBase* c) -> uint32_t {
                    return static_cast<const gb_mbc_t*>(c)->rtc_regs_[1];
                })
                .value("RTC Hours", +[](const ChipBase* c) -> uint32_t {
                    return static_cast<const gb_mbc_t*>(c)->rtc_regs_[2];
                });
        }
    }
#endif
};

// ── Type aliases ────────────────────────────────────────────────────
using GbMbcNone = gb_mbc_t<kMbcNoneTraits>;
using GbMbc1    = gb_mbc_t<kMbc1Traits>;
using GbMbc2    = gb_mbc_t<kMbc2Traits>;
using GbMbc3    = gb_mbc_t<kMbc3Traits>;
using GbMbc5    = gb_mbc_t<kMbc5Traits>;

// ============================================================================
// Factory: create MBC from cartridge type byte ($0147)
// ============================================================================

inline std::unique_ptr<GbMbcBase> create_mbc(uint8_t cart_type) {
    switch (cart_type) {
        case 0x00:                                         // ROM only
            return std::make_unique<GbMbcNone>();
        case 0x01: case 0x02: case 0x03:                   // MBC1 (+RAM, +RAM+BATTERY)
            return std::make_unique<GbMbc1>();
        case 0x05: case 0x06:                              // MBC2 (+BATTERY)
            return std::make_unique<GbMbc2>();
        case 0x0F: case 0x10: case 0x11:                   // MBC3 (+TIMER+BATTERY, +TIMER+RAM+BATTERY, +RAM)
        case 0x12: case 0x13:                              // MBC3 (+RAM+BATTERY, +RAM+BATTERY)
            return std::make_unique<GbMbc3>();
        case 0x19: case 0x1A: case 0x1B:                   // MBC5 (+RAM, +RAM+BATTERY)
        case 0x1C: case 0x1D: case 0x1E:                   // MBC5+RUMBLE (+RAM, +RAM+BATTERY)
            return std::make_unique<GbMbc5>();
        default:
            // Unknown type — fall back to MBC5 (most permissive)
            return std::make_unique<GbMbc5>();
    }
}

// ── ROM size from header byte $0148 ─────────────────────────────────
inline uint32_t rom_size_from_header(uint8_t code) {
    // 32KB << code (0=32KB, 1=64KB, ..., 8=8MB)
    if (code <= 8) return 32768u << code;
    return 32768u;  // Fallback
}

// ── RAM size from header byte $0149 ─────────────────────────────────
inline uint32_t ram_size_from_header(uint8_t code) {
    static constexpr uint32_t sizes[] = { 0, 0, 8192, 32768, 131072, 65536 };
    return (code < 6) ? sizes[code] : 0;
}
