#pragma once
// mos8722.hpp — MOS 8722 Memory Management Unit (C128)
//
// The 8722 is the programmable MMU in the Commodore 128.  It arbitrates
// the entire 64 KB address space for two CPUs (8502 + Z80), controlling:
//   - ROM/RAM/I/O overlay per 16 KB region (configuration register)
//   - RAM bank selection (128 KB = 2 × 64 KB banks)
//   - Common RAM areas (bottom/top of address space always visible)
//   - Zero page and stack page relocation
//   - CPU mode (8502 vs Z80) and C64 compatibility mode
//   - Fast serial and 40/80 column key sensing
//
// Register interface:
//   $D500-$D50B — direct register access (12 registers)
//   $FF00-$FF04 — configuration/preconfiguration registers
//                  (always visible regardless of bank config)
//
// 48-pin DIP package.
//
// Used in: Commodore 128, Commodore 128D

#include "chip/mmu/mmu_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>

// ============================================================================
// 8722 MMU register map
// ============================================================================

namespace mos8722 {

// Register offsets within the $D500 I/O window
namespace reg {
    inline constexpr uint8_t CR      = 0x00;  // Configuration register
    inline constexpr uint8_t PCR_A   = 0x01;  // Preconfiguration register A
    inline constexpr uint8_t PCR_B   = 0x02;  // Preconfiguration register B
    inline constexpr uint8_t PCR_C   = 0x03;  // Preconfiguration register C
    inline constexpr uint8_t PCR_D   = 0x04;  // Preconfiguration register D
    inline constexpr uint8_t MCR     = 0x05;  // Mode configuration register
    inline constexpr uint8_t RCR     = 0x06;  // RAM configuration register
    inline constexpr uint8_t P0L     = 0x07;  // Page 0 pointer low
    inline constexpr uint8_t P0H     = 0x08;  // Page 0 pointer high
    inline constexpr uint8_t P1L     = 0x09;  // Page 1 pointer low
    inline constexpr uint8_t P1H     = 0x0A;  // Page 1 pointer high
    inline constexpr uint8_t VERSION = 0x0B;  // Version register (read-only)
    inline constexpr uint8_t NUM_REGS = 0x0C; // 12 registers total
} // namespace reg

// Configuration Register (CR, $D500 / $FF00) bit fields
//
//   Bit 7    : CPU speed — 0 = fast (2 MHz), 1 = slow (1 MHz)
//              (directly controls the 8502 clock divider)
//   Bit 6    : RAM bank for $0000-$3FFF — 0 = bank 0, 1 = bank 1
//   Bits 5-4 : $C000-$FFFF high ROM select
//              00 = Kernal ROM + Editor ROM
//              01 = Internal function ROM
//              10 = External function ROM
//              11 = RAM
//   Bits 3-2 : $8000-$BFFF mid-high ROM select
//              00 = BASIC hi ROM
//              01 = Internal function ROM
//              10 = External function ROM
//              11 = RAM
//   Bit 1    : $4000-$7FFF BASIC LO ROM select
//              0 = BASIC lo ROM visible
//              1 = RAM
//   Bit 0    : $D000-$DFFF I/O / character ROM select
//              0 = I/O devices visible
//              1 = Character ROM (or ROM from bits 5-4)
namespace cr {
    inline constexpr uint8_t CPU_SPEED      = 0x80;  // Bit 7
    inline constexpr uint8_t RAM_BANK       = 0x40;  // Bit 6
    inline constexpr uint8_t HIGH_ROM_MASK  = 0x30;  // Bits 5-4
    inline constexpr uint8_t HIGH_ROM_SHIFT = 4;
    inline constexpr uint8_t MID_HI_MASK   = 0x0C;  // Bits 3-2
    inline constexpr uint8_t MID_HI_SHIFT  = 2;
    inline constexpr uint8_t BASIC_LO       = 0x02;  // Bit 1: 0 = BASIC LO ROM, 1 = RAM
    inline constexpr uint8_t IO_SELECT      = 0x01;  // Bit 0: 0 = I/O visible, 1 = char ROM
    // ROM select values (for 2-bit HIGH_ROM and MID_HI fields)
    inline constexpr uint8_t ROM_DEFAULT   = 0;  // Default ROM (BASIC/Kernal/Editor)
    inline constexpr uint8_t ROM_INT_FUNC  = 1;  // Internal function ROM
    inline constexpr uint8_t ROM_EXT_FUNC  = 2;  // External function ROM
    inline constexpr uint8_t ROM_RAM       = 3;  // RAM (no ROM overlay)
} // namespace cr

// Mode Configuration Register (MCR, $D505) bit fields
//
//   Bit 7    : Reserved (normally 0)
//   Bit 6    : C64 mode — 1 = enter C64 compatibility mode
//   Bit 5    : 40/80 key sense — 0 = pressed (40-col), 1 = not pressed (80-col)
//              (directly reads the physical key, active-low wired-AND)
//   Bit 4    : Fast serial output — directly controls serial clock line
//   Bit 3    : Fast serial direction — 0 = input, 1 = output
//   Bit 2    : Fast serial input (active-low, directly reads serial clock)
//   Bit 1    : OS ROM select bit 1 (operating system version)
//   Bit 0    : OS ROM select bit 0
namespace mcr {
    inline constexpr uint8_t C64_MODE     = 0x40;  // Bit 6 — enter C64 mode
    inline constexpr uint8_t COL_KEY      = 0x20;  // Bit 5 — 40/80 column key
    inline constexpr uint8_t FSDIR        = 0x08;  // Bit 3 — fast serial direction
    inline constexpr uint8_t FSIN         = 0x04;  // Bit 2 — fast serial input
    inline constexpr uint8_t CPU_SELECT   = 0x01;  // Bit 0 — 0 = Z80 active, 1 = 8502 active
} // namespace mcr

// RAM Configuration Register (RCR, $D506) bit fields
//
//   Bits 7-6 : RAM bank for VIC-IIe (00=bank0, 01=bank1, 10/11=bank0)
//   Bits 5-4 : Common RAM top-of-memory size
//              00 = 1 KB  ($FC00-$FFFF)
//              01 = 4 KB  ($F000-$FFFF)
//              10 = 8 KB  ($E000-$FFFF)
//              11 = 16 KB ($C000-$FFFF)
//   Bits 3-2 : Common RAM bottom-of-memory size
//              00 = 1 KB  ($0000-$03FF)
//              01 = 4 KB  ($0000-$0FFF)
//              10 = 8 KB  ($0000-$1FFF)
//              11 = 16 KB ($0000-$3FFF)
//   Bit 1    : Bottom common RAM enable (1 = enabled)
//   Bit 0    : Top common RAM enable (1 = enabled)
namespace rcr {
    inline constexpr uint8_t VIC_BANK_MASK  = 0xC0;
    inline constexpr uint8_t VIC_BANK_SHIFT = 6;
    inline constexpr uint8_t TOP_SIZE_MASK  = 0x30;
    inline constexpr uint8_t TOP_SIZE_SHIFT = 4;
    inline constexpr uint8_t BOT_SIZE_MASK  = 0x0C;
    inline constexpr uint8_t BOT_SIZE_SHIFT = 2;
    inline constexpr uint8_t BOT_COMMON_EN  = 0x02;
    inline constexpr uint8_t TOP_COMMON_EN  = 0x01;
} // namespace rcr

// Version register ($D50B) — read-only chip revision
// The 8722 returns 0x00; the earlier 8721 returns a different value.
inline constexpr uint8_t VERSION_8722 = 0x00;

} // namespace mos8722

// ============================================================================
// DECL register map — for debug field generation
// ============================================================================

#define MOS8722_DECL(REG, FLD, CMP)                                                 \
    REG(0x00, CR,      "Configuration register")                                   \
      FLD(CR,  CR_SPEED,    7:7, "CPU speed (0=fast)",      Flag, 0, 0)            \
      FLD(CR,  CR_BANK,     6:6, "RAM bank select",         Value, 0, 0)           \
      FLD(CR,  CR_HIGH_ROM, 5:4, "High ROM ($C000+)",       Value, 0, 0)           \
      FLD(CR,  CR_MID_HI,   3:2, "Mid-hi ROM ($8000+)",     Value, 0, 0)           \
      FLD(CR,  CR_BASIC_LO, 1:1, "BASIC LO (0=ROM)",        Flag, 0, 0)            \
      FLD(CR,  CR_IO_SEL,   0:0, "I/O sel (0=I/O)",         Flag, 0, 0)            \
    REG(0x01, PCR_A,   "Preconfiguration A")                                       \
    REG(0x02, PCR_B,   "Preconfiguration B")                                       \
    REG(0x03, PCR_C,   "Preconfiguration C")                                       \
    REG(0x04, PCR_D,   "Preconfiguration D")                                       \
    REG(0x05, MCR,     "Mode configuration")                                       \
      FLD(MCR, MCR_C64MODE, 6:6, "C64 mode",               Flag, 0, 0)            \
      FLD(MCR, MCR_COLKEY,  5:5, "40/80 key (0=40col)",    Flag, 0, 0)            \
      FLD(MCR, MCR_FSDIR,   3:3, "Fast serial dir",        Flag, 0, 0)            \
    REG(0x06, RCR,     "RAM configuration")                                        \
      FLD(RCR, RCR_VICBANK, 7:6, "VIC-IIe bank",           Value, 0, 0)           \
      FLD(RCR, RCR_TOP_SZ,  5:4, "Top common size",        Value, 0, 0)           \
      FLD(RCR, RCR_BOT_SZ,  3:2, "Bottom common size",     Value, 0, 0)           \
      FLD(RCR, RCR_BOT_EN,  1:1, "Bottom common enable",   Flag, 0, 0)            \
      FLD(RCR, RCR_TOP_EN,  0:0, "Top common enable",      Flag, 0, 0)            \
    REG(0x07, P0L,     "Page 0 pointer low")                                       \
    REG(0x08, P0H,     "Page 0 pointer high")                                      \
    REG(0x09, P1L,     "Page 1 pointer low")                                       \
    REG(0x0A, P1H,     "Page 1 pointer high")                                      \
    REG(0x0B, VERSION, "Version (read-only)")

DECL_EXTRACT(MOS8722, MOS8722_DECL)

// ============================================================================
// mos8722_t — MOS 8722 Memory Management Unit
// ============================================================================

struct mos8722_t : public MmuChipBase {

    mos8722_t()
        : MmuChipBase(ChipInfo{"MOS8722", "MOS Technology", "MOS 8722 MMU"})
    {
        init_regs(mos8722::reg::NUM_REGS);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    // ── ChipBase bus interface ───────────────────────────────────────

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        const uint8_t reg = BUS_GET_ADDR(bus) & 0x0F;
        BUS_SET_DATA(bus, read_register(reg));
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        const uint8_t reg = BUS_GET_ADDR(bus) & 0x0F;
        write_register(reg, BUS_GET_DATA(bus));
        return bus;
    }

    /// CS-tick: self-dispatch register access when chip-selected.
    bus_state_t tick_mmio(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    // ── Reset ────────────────────────────────────────────────────────

    void reset() override {
        regs_.clear();
        // Power-on defaults
        regs_[mos8722::reg::CR]      = 0x00;  // Default bank config: all ROMs visible, bank 0
        regs_[mos8722::reg::PCR_A]   = 0x00;
        regs_[mos8722::reg::PCR_B]   = 0x00;
        regs_[mos8722::reg::PCR_C]   = 0x00;
        regs_[mos8722::reg::PCR_D]   = 0x00;
        regs_[mos8722::reg::MCR]     = 0x00;  // Z80 active (bit 0=0), C128 mode (bit 6=0)
        regs_[mos8722::reg::RCR]     = 0x00;
        regs_[mos8722::reg::P0L]     = 0x00;  // Page 0 = $0000
        regs_[mos8722::reg::P0H]     = 0x00;
        regs_[mos8722::reg::P1L]     = 0x00;  // Page 1 = $0100
        regs_[mos8722::reg::P1H]     = 0x01;
        regs_[mos8722::reg::VERSION] = mos8722::VERSION_8722;

        bank_config_dirty_    = true;
        cpu_switch_requested_ = false;
    }

    // ── Register access (called by on_bus_read/write and by system for $FF00) ──

    uint8_t read_register(uint8_t reg) const {
        using namespace mos8722::reg;
        if (reg >= NUM_REGS) return 0xFF;

        switch (reg) {
            case MCR:
                // Bit 5 reads the 40/80 column key (active-low)
                // Default: not pressed → bit 5 = 1 (80-col mode)
                return (regs_[MCR] & ~mos8722::mcr::COL_KEY) | mos8722::mcr::COL_KEY;

            case VERSION:
                return mos8722::VERSION_8722;

            default:
                return regs_[reg];
        }
    }

    void write_register(uint8_t reg, uint8_t data) {
        using namespace mos8722::reg;
        if (reg >= NUM_REGS) return;

        // Version register is read-only
        if (reg == VERSION) return;

        const uint8_t old = regs_[reg];
        regs_[reg] = data;

        switch (reg) {
            case CR:
                // Configuration changed → bank config needs update
                if (data != old) bank_config_dirty_ = true;
                break;

            case MCR:
                // Bit 0: CPU select — 0=Z80, 1=8502.
                // A change in bit 0 triggers a CPU switch request.
                if ((data ^ old) & mos8722::mcr::CPU_SELECT) {
                    cpu_switch_requested_ = true;
                }
                // Bit 6: C64 mode trigger — sticky: once set, only cleared by
                // reset or explicit clear_c64_mode_request().
                if (old & mos8722::mcr::C64_MODE) {
                    regs_[MCR] |= mos8722::mcr::C64_MODE;
                }
                if (regs_[MCR] != old) bank_config_dirty_ = true;
                break;

            case RCR:
                // RAM config changed → bank config needs update
                if (data != old) bank_config_dirty_ = true;
                break;

            case P0L: case P0H:
            case P1L: case P1H:
                // Page relocation changed — flag for system to handle
                if (data != old) bank_config_dirty_ = true;
                break;

            default:
                break;
        }
    }

    // ── $FF00-$FF04 access (handled by system tick, not CS-tick) ────

    /// Write to $FF00: load CR from the written data.
    /// On real hardware, any write to $FF00 loads the CR register.
    void write_ff00(uint8_t data) {
        write_register(mos8722::reg::CR, data);
    }

    /// Read from $FF00: returns CR.
    uint8_t read_ff00() const {
        return regs_[mos8722::reg::CR];
    }

    /// Write to $FF01-$FF04: load CR from preconfiguration register N-1.
    /// The written data is ignored — it's the address that matters.
    void write_ff01_ff04(uint8_t index) {
        // index: 0=PCR_A, 1=PCR_B, 2=PCR_C, 3=PCR_D
        if (index < 4) {
            write_register(mos8722::reg::CR, regs_[mos8722::reg::PCR_A + index]);
        }
    }

    /// Read from $FF01-$FF04: returns the preconfiguration register.
    uint8_t read_ff01_ff04(uint8_t index) const {
        if (index < 4) return regs_[mos8722::reg::PCR_A + index];
        return 0xFF;
    }

    // ── Derived state queries ────────────────────────────────────────

    /// Current CR register value.
    uint8_t cr() const { return regs_[mos8722::reg::CR]; }

    /// RAM bank for low memory ($0000-$3FFF): 0 or 1.
    uint8_t ram_bank() const {
        return (regs_[mos8722::reg::CR] & mos8722::cr::RAM_BANK) ? 1 : 0;
    }

    /// True if BASIC LO ROM is visible at $4000-$7FFF (CR bit 1 = 0).
    bool basic_lo_rom_enabled() const {
        return (regs_[mos8722::reg::CR] & mos8722::cr::BASIC_LO) == 0;
    }

    /// True if I/O devices are visible at $D000-$DFFF (CR bit 0 = 0).
    /// When false, character ROM (or ROM from bits 5-4) is mapped there.
    bool io_visible() const {
        return (regs_[mos8722::reg::CR] & mos8722::cr::IO_SELECT) == 0;
    }

    /// ROM selection for $8000-$BFFF.
    uint8_t mid_hi_select() const {
        return (regs_[mos8722::reg::CR] & mos8722::cr::MID_HI_MASK) >> mos8722::cr::MID_HI_SHIFT;
    }

    /// ROM selection for $C000-$FFFF high.
    uint8_t high_rom_select() const {
        return (regs_[mos8722::reg::CR] & mos8722::cr::HIGH_ROM_MASK) >> mos8722::cr::HIGH_ROM_SHIFT;
    }

    /// True if C64 mode has been requested (MCR bit 6, sticky until reset/clear).
    bool c64_mode_requested() const { return (regs_[mos8722::reg::MCR] & mos8722::mcr::C64_MODE) != 0; }

    /// Clear C64 mode request (called by system after entering C64 mode).
    void clear_c64_mode_request() { regs_[mos8722::reg::MCR] &= ~mos8722::mcr::C64_MODE; }

    /// CPU select from MCR bit 0: true = 8502 active, false = Z80 active.
    bool cpu_is_8502() const { return (regs_[mos8722::reg::MCR] & mos8722::mcr::CPU_SELECT) != 0; }

    /// True if a CPU switch was requested by MCR write (consumed by system).
    bool cpu_switch_requested() const { return cpu_switch_requested_; }
    void acknowledge_cpu_switch() { cpu_switch_requested_ = false; }

    /// True if bank config has changed since last acknowledgement.
    bool bank_config_dirty() const { return bank_config_dirty_; }
    void acknowledge_bank_config() { bank_config_dirty_ = false; }

    /// Common RAM configuration
    bool bottom_common_enabled() const { return (regs_[mos8722::reg::RCR] & mos8722::rcr::BOT_COMMON_EN) != 0; }
    bool top_common_enabled()    const { return (regs_[mos8722::reg::RCR] & mos8722::rcr::TOP_COMMON_EN) != 0; }

    /// Bottom common RAM size in bytes (1K, 4K, 8K, or 16K).
    uint16_t bottom_common_size() const {
        static constexpr uint16_t sizes[] = { 0x0400, 0x1000, 0x2000, 0x4000 };
        return sizes[(regs_[mos8722::reg::RCR] & mos8722::rcr::BOT_SIZE_MASK) >> mos8722::rcr::BOT_SIZE_SHIFT];
    }

    /// Top common RAM start address (e.g. 0xFC00 for 1K, 0xC000 for 16K).
    uint16_t top_common_start() const {
        static constexpr uint16_t starts[] = { 0xFC00, 0xF000, 0xE000, 0xC000 };
        return starts[(regs_[mos8722::reg::RCR] & mos8722::rcr::TOP_SIZE_MASK) >> mos8722::rcr::TOP_SIZE_SHIFT];
    }

    /// Page 0 physical address (relocated zero page).
    uint16_t page0_address() const {
        return (static_cast<uint16_t>(regs_[mos8722::reg::P0H]) << 8)
             | (regs_[mos8722::reg::P0L] & 0xFE);  // Bit 0 unused (page-aligned)
    }

    /// Page 1 physical address (relocated stack page).
    uint16_t page1_address() const {
        return (static_cast<uint16_t>(regs_[mos8722::reg::P1H]) << 8)
             | (regs_[mos8722::reg::P1L] & 0xFE);
    }

    /// VIC-IIe RAM bank from RCR bits 7-6.
    uint8_t vic_ram_bank() const {
        return (regs_[mos8722::reg::RCR] & mos8722::rcr::VIC_BANK_MASK) >> mos8722::rcr::VIC_BANK_SHIFT;
    }

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ── Static methods for legacy handler registration ───────────────

    static bus_state_t registers_read(void* context, bus_state_t bus) {
        auto* mmu = static_cast<mos8722_t*>(context);
        return mmu->on_bus_read(bus);
    }

    static bus_state_t registers_write(void* context, bus_state_t bus) {
        auto* mmu = static_cast<mos8722_t*>(context);
        return mmu->on_bus_write(bus);
    }

private:
    bool bank_config_dirty_   = true;
    bool cpu_switch_requested_ = false;

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using M = const mos8722_t;

        wire_debug_registers(MOS8722_REG_INFO, 0xD500);
        debug_registry_.set_decl_entries(MOS8722_DECL_ENTRIES.data(), MOS8722_DECL_ENTRIES.size());

        // CR bitfields (RAM Bank, BASIC LO, I/O Select, Mid-Hi, High ROM)
        // RCR bitfields (Bottom/Top Enable) and MCR C64 Mode flag are in the
        // DECL register walk.  Only internal latches and derived values remain.
        debug_registry_.category("Internal State")
            .flag("Config Dirty", +[](const ChipBase* c) -> uint32_t {
                return static_cast<M*>(c)->bank_config_dirty_ ? 1 : 0;
            });

        debug_registry_.category("Common RAM (derived)", false)
            .value("Bottom Size", +[](const ChipBase* c) -> uint32_t {
                return static_cast<M*>(c)->bottom_common_size();
            })
            .value("Top Start", +[](const ChipBase* c) -> uint32_t {
                return static_cast<M*>(c)->top_common_start();
            });

        debug_registry_.category("Page Relocation", false)
            .value("Page 0 Addr", +[](const ChipBase* c) -> uint32_t {
                return static_cast<M*>(c)->page0_address();
            })
            .value("Page 1 Addr", +[](const ChipBase* c) -> uint32_t {
                return static_cast<M*>(c)->page1_address();
            });
    }
#endif
};
