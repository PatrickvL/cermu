#pragma once
/*
 * mc6883_sam.hpp — MC6883 Synchronous Address Multiplexer (SAM)
 *
 * The MC6883 (also SN74LS783) is the address multiplexer and DRAM refresh
 * controller used in the TRS-80 Color Computer and Dragon 32/64.
 *
 * Features:
 *   - VDG display mode selection (V0–V2 → MC6847 mode pins)
 *   - Display start address offset (F0–F6 → address × 512)
 *   - RAM size/type selection (R0–R1 → 4K/16K/64K DRAM page mode)
 *   - MPU rate control (M0 → 0.89 MHz / 1.78 MHz)
 *   - Memory page select (P1 → 32K–64K upper page in 64K mode)
 *   - Full TY (map type) support for ROM overlay
 *
 * Register interface:
 *   16 bit-addressable registers at $FFC0–$FFDF.
 *   Each register occupies 2 bytes: even address clears the bit,
 *   odd address sets the bit. Data bus value is ignored.
 *
 *   $FFC0/$FFC1 — V0   VDG mode bit 0
 *   $FFC2/$FFC3 — V1   VDG mode bit 1
 *   $FFC4/$FFC5 — V2   VDG mode bit 2
 *   $FFC6/$FFC7 — F0   Display offset bit 0  (× 512)
 *   $FFC8/$FFC9 — F1   Display offset bit 1
 *   $FFCA/$FFCB — F2   Display offset bit 2
 *   $FFCC/$FFCD — F3   Display offset bit 3
 *   $FFCE/$FFCF — F4   Display offset bit 4
 *   $FFD0/$FFD1 — F5   Display offset bit 5
 *   $FFD2/$FFD3 — F6   Display offset bit 6
 *   $FFD4/$FFD5 — P1   Page select (64K mode: 0=lower, 1=upper)
 *   $FFD6/$FFD7 — R0   Memory size bit 0
 *   $FFD8/$FFD9 — R1   Memory size bit 1
 *   $FFDA/$FFDB — TY   Map type (0=RAM at $0000–$7FFF, 1=ROM at $8000–$FEFF)
 *   $FFDC/$FFDD — (reserved)
 *   $FFDE/$FFDF — M0   MPU rate (0=normal, 1=double speed)
 *
 * 40-pin DIP package.
 */

#include "chip/io/io_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>

// ============================================================================
// MC6883 SAM UNIFIED DECLARATION TABLE
// ============================================================================

#define MC6883_DECL(REG, FLD, CMP) \
    REG(0x00, SAM_VDG_MODE,    "VDG Video Mode (V0-V2)")               \
      FLD(SAM_VDG_MODE, V0,    0:0, "VDG mode bit 0",     Flag, 0, 0) \
      FLD(SAM_VDG_MODE, V1,    1:1, "VDG mode bit 1",     Flag, 0, 0) \
      FLD(SAM_VDG_MODE, V2,    2:2, "VDG mode bit 2",     Flag, 0, 0) \
    REG(0x01, SAM_DISPLAY_OFS, "Display Offset (F0-F6)")               \
      FLD(SAM_DISPLAY_OFS, F0, 0:0, "Offset bit 0",       Flag, 0, 0) \
      FLD(SAM_DISPLAY_OFS, F1, 1:1, "Offset bit 1",       Flag, 0, 0) \
      FLD(SAM_DISPLAY_OFS, F2, 2:2, "Offset bit 2",       Flag, 0, 0) \
      FLD(SAM_DISPLAY_OFS, F3, 3:3, "Offset bit 3",       Flag, 0, 0) \
      FLD(SAM_DISPLAY_OFS, F4, 4:4, "Offset bit 4",       Flag, 0, 0) \
      FLD(SAM_DISPLAY_OFS, F5, 5:5, "Offset bit 5",       Flag, 0, 0) \
      FLD(SAM_DISPLAY_OFS, F6, 6:6, "Offset bit 6",       Flag, 0, 0) \
    REG(0x02, SAM_CONFIG,      "Config (P1, R0-R1, TY, M0)")           \
      FLD(SAM_CONFIG, P1,      0:0, "Page select",        Flag, 0, 0) \
      FLD(SAM_CONFIG, R0,      1:1, "RAM size bit 0",     Flag, 0, 0) \
      FLD(SAM_CONFIG, R1,      2:2, "RAM size bit 1",     Flag, 0, 0) \
      FLD(SAM_CONFIG, TY,      3:3, "Map type",           Flag, 0, 0) \
      FLD(SAM_CONFIG, M0,      4:4, "MPU rate",           Flag, 0, 0)

namespace mc6883 {
    namespace reg {
        MC6883_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 3;
    }
    using namespace reg;
}

DECL_EXTRACT(MC6883, MC6883_DECL)

// ============================================================================
// MC6883 SAM Implementation
// ============================================================================

struct mc6883_sam_t : public IoChipBase {

    mc6883_sam_t()
        : IoChipBase(ChipInfo{"MC6883", "Motorola", "Synchronous Address Multiplexer"})
    {
        init_regs(mc6883::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(MC6883_REG_INFO);
        register_debug_fields();
#endif
    }

    // ── Register state (derived from 16 bit-addressable latches) ─────
    //    These are computed from the raw bit-address writes
    uint8_t  vdg_mode()       const { return regs_.data[mc6883::SAM_VDG_MODE] & 0x07; }
    uint16_t display_offset() const { return static_cast<uint16_t>(regs_.data[mc6883::SAM_DISPLAY_OFS] & 0x7F) << 9; }
    bool     page_select()    const { return regs_.data[mc6883::SAM_CONFIG] & 0x01; }
    uint8_t  ram_size()       const { return (regs_.data[mc6883::SAM_CONFIG] >> 1) & 0x03; }
    bool     map_type()       const { return (regs_.data[mc6883::SAM_CONFIG] >> 3) & 0x01; }
    bool     mpu_rate()       const { return (regs_.data[mc6883::SAM_CONFIG] >> 4) & 0x01; }

    // ── ChipBase bus interface ───────────────────────────────────────
    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        // SAM registers are write-only; reads return open bus
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint16_t addr = BUS_GET_ADDR(bus);
        write_bit_address(addr);
        return bus;
    }

    // ── CS-tick: MMIO self-dispatch ──────────────────────────────────
    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            if (!BUS_GET_BIT(bus, BUS_RW_BIT))  // Write only
                on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    void reset() override {
        // All bits clear on power-on
        regs_.data[mc6883::SAM_VDG_MODE]    = 0;
        regs_.data[mc6883::SAM_DISPLAY_OFS] = 0;
        regs_.data[mc6883::SAM_CONFIG]      = 0;
        raw_bits_ = 0;
    }

    // ── GUI ─────────────────────────────────────────────────────────
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ── Callbacks for system-level response to config changes ────────
    void* user_data = nullptr;
    void (*on_config_change)(void* user_data) = nullptr;

private:
    uint16_t raw_bits_ = 0;  // All 16 addressable bits packed

    // Write via bit-address mechanism:
    // Address $FFC0 + (bit_index * 2) clears bit, +1 sets it
    void write_bit_address(uint16_t addr) {
        uint16_t offset = addr - 0xFFC0;
        if (offset > 0x1F) return;

        uint8_t bit_index = offset >> 1;
        bool set_bit = offset & 1;

        uint16_t old_bits = raw_bits_;
        if (set_bit)
            raw_bits_ |= (1u << bit_index);
        else
            raw_bits_ &= ~(1u << bit_index);

        // Unpack into register file
        regs_.data[mc6883::SAM_VDG_MODE]    = raw_bits_ & 0x07;         // V0–V2 (bits 0–2)
        regs_.data[mc6883::SAM_DISPLAY_OFS] = (raw_bits_ >> 3) & 0x7F;  // F0–F6 (bits 3–9)
        regs_.data[mc6883::SAM_CONFIG]      = (raw_bits_ >> 10) & 0x1F; // P1,R0,R1,TY,M0 (bits 10–14)
        // Bit 15 (M0) is bit index 15: $FFDE/$FFDF

        if (raw_bits_ != old_bits && on_config_change)
            on_config_change(user_data);
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("MC6883 — VDG Mode")
            .value("V (mode)",  +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mc6883_sam_t*>(c)->vdg_mode();
            })
            .category("MC6883 — Display")
            .value("Display Offset", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mc6883_sam_t*>(c)->display_offset();
            })
            .category("MC6883 — Config")
            .flag("Page Select (P1)", +[](const ChipBase* c) -> bool {
                return static_cast<const mc6883_sam_t*>(c)->page_select();
            })
            .value("RAM Size (R)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mc6883_sam_t*>(c)->ram_size();
            })
            .flag("Map Type (TY)", +[](const ChipBase* c) -> bool {
                return static_cast<const mc6883_sam_t*>(c)->map_type();
            })
            .flag("MPU Rate (M0)", +[](const ChipBase* c) -> bool {
                return static_cast<const mc6883_sam_t*>(c)->mpu_rate();
            });
    }
#endif
};
