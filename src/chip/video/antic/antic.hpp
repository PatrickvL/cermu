#pragma once
/*
 * antic.hpp — Atari ANTIC (Alpha-Numeric Television Interface Controller)
 *
 * ANTIC is the display list coprocessor in Atari 400/800/XL/XE computers.
 * It reads a programmable display list from RAM that describes screen layout
 * and content, performs DMA to fetch character/bitmap data, and generates
 * video output via GTIA.
 *
 * Features:
 *   - 14 display modes (6 text, 8 graphics)
 *   - Display List (DL) program: repeating line instructions
 *   - DMA cycle steal from 6502 (scanline-based)
 *   - Player/Missile DMA (feeds GTIA)
 *   - Horizontal/Vertical scroll
 *   - Fine/Coarse scroll registers
 *   - Display List Interrupt (DLI) per scanline instruction
 *   - NMI generation: DLI, VBI, Reset
 *   - WSYNC (Wait for Horizontal Sync) halts CPU
 *
 * Register map: 16 read registers + 16 write registers
 *   Read  ($D400–$D40F): VCOUNT, PENH, PENV, NMIST, NMIRES
 *   Write ($D400–$D40F): DMACTL, CHACTL, DLISTL/H, HSCROL, VSCROL,
 *                          PMBASE, CHBASE, WSYNC, VCOUNT, NMIEN, NMIRES
 *
 * 40-pin DIP package (CO12296).
 */

#include "chip/video/video_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>

// ============================================================================
// ANTIC UNIFIED DECLARATION TABLE
// ============================================================================

#define ANTIC_DECL(REG, FLD, CMP) \
    REG(0x00, DMACTL,  "DMA control")                                          \
      FLD(DMACTL, NARROW,     0:0, "Narrow playfield",       Flag, 0, 0)       \
      FLD(DMACTL, NORMAL,     1:1, "Normal playfield",       Flag, 0, 0)       \
      FLD(DMACTL, MISSILE_DMA,2:2, "Missile DMA enable",     Flag, 0, 0)       \
      FLD(DMACTL, PLAYER_DMA, 3:3, "Player DMA enable",      Flag, 0, 0)       \
      FLD(DMACTL, PM_1LINE,   4:4, "PM single-line resolution",Flag, 0, 0)     \
      FLD(DMACTL, DL_DMA,     5:5, "Display list DMA enable", Flag, 0, 0)      \
    REG(0x01, CHACTL,  "Character control")                                     \
      FLD(CHACTL, REFLECT,    0:0, "Vertical reflect",        Flag, 0, 0)       \
      FLD(CHACTL, INVERSE,    1:1, "Inverse video",           Flag, 0, 0)       \
      FLD(CHACTL, BLANK,      2:2, "Blank characters",        Flag, 0, 0)       \
    REG(0x02, DLISTL,  "Display list pointer low")                              \
    REG(0x03, DLISTH,  "Display list pointer high")                             \
    REG(0x04, HSCROL,  "Horizontal fine scroll")                                \
      FLD(HSCROL, HSCR,       3:0, "Horizontal scroll",       Value, 0, 0)     \
    REG(0x05, VSCROL,  "Vertical fine scroll")                                  \
      FLD(VSCROL, VSCR,       3:0, "Vertical scroll",         Value, 0, 0)     \
    REG(0x06, UNUSED06,"(unused)")                                              \
    REG(0x07, PMBASE,  "Player/Missile base address high")                      \
    REG(0x08, UNUSED08,"(unused)")                                              \
    REG(0x09, CHBASE,  "Character set base address high")                       \
    REG(0x0A, WSYNC,   "Wait for horizontal sync (write halts CPU)")            \
    REG(0x0B, VCOUNT,  "Vertical line counter (read)")                          \
    REG(0x0C, PENH,    "Light pen horizontal (read)")                           \
    REG(0x0D, PENV,    "Light pen vertical (read)")                             \
    REG(0x0E, NMIEN,   "NMI enable")                                            \
      FLD(NMIEN, DLI_EN,      7:7, "DLI enable",              Flag, 0, 0)      \
      FLD(NMIEN, VBI_EN,      6:6, "VBI enable",              Flag, 0, 0)      \
      FLD(NMIEN, RESET_EN,    5:5, "Reset key enable",        Flag, 0, 0)      \
    REG(0x0F, NMIRES,  "NMI reset / status (read=NMIST)")

namespace antic {
    namespace reg {
        ANTIC_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 16;
    }
    using namespace reg;
}

DECL_EXTRACT(ANTIC, ANTIC_DECL)

// ============================================================================
// ANTIC — Stub Implementation
// ============================================================================

struct antic_t : public VideoChipBase {

    antic_t()
        : VideoChipBase(ChipInfo{"ANTIC", "Atari", "Alpha-Numeric Television Interface Controller"})
    {
        init_regs(antic::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(ANTIC_REG_INFO);
        register_debug_fields();
#endif
    }

    // ── ChipBase bus interface ───────────────────────────────────────
    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x0F;
        switch (addr) {
            case antic::VCOUNT: BUS_SET_DATA(bus, vcount_);   break;
            case antic::PENH:   BUS_SET_DATA(bus, pen_h_);    break;
            case antic::PENV:   BUS_SET_DATA(bus, pen_v_);    break;
            case antic::NMIRES: BUS_SET_DATA(bus, nmi_status_); break;
            default:            BUS_SET_DATA(bus, 0xFF);       break;
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x0F;
        uint8_t data = BUS_GET_DATA(bus);
        regs_.data[addr] = data;

        switch (addr) {
            case antic::WSYNC:  wsync_pending_ = true;            break;
            case antic::NMIRES: nmi_status_ = 0;                  break;
            default: break;
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        // TODO: Display list DMA, scanline rendering, NMI generation, WSYNC
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, antic::reg::REG_COUNT);
        vcount_       = 0;
        pen_h_        = 0;
        pen_v_        = 0;
        nmi_status_   = 0;
        wsync_pending_= false;
    }

    // ── Display list pointer ─────────────────────────────────────────
    uint16_t display_list_addr() const {
        return regs_.data[antic::DLISTL] | (regs_.data[antic::DLISTH] << 8);
    }

    // ── State ────────────────────────────────────────────────────────
    uint8_t vcount_        = 0;   // Current scanline counter
    uint8_t pen_h_         = 0;   // Light pen horizontal
    uint8_t pen_v_         = 0;   // Light pen vertical
    uint8_t nmi_status_    = 0;   // NMI status register (NMIST)
    bool    wsync_pending_ = false;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("ANTIC — DMA Control")
            .value("DMACTL", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const antic_t*>(c)->regs_.data[antic::DMACTL];
            })
            .category("ANTIC — Display List")
            .value("DL Address", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const antic_t*>(c)->display_list_addr();
            })
            .value("CHBASE", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const antic_t*>(c)->regs_.data[antic::CHBASE];
            })
            .category("ANTIC — Scroll")
            .value("HSCROL", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const antic_t*>(c)->regs_.data[antic::HSCROL] & 0x0F;
            })
            .value("VSCROL", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const antic_t*>(c)->regs_.data[antic::VSCROL] & 0x0F;
            })
            .category("ANTIC — Status")
            .value("VCOUNT", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const antic_t*>(c)->vcount_;
            })
            .flag("WSYNC pending", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const antic_t*>(c)->wsync_pending_;
            });
    }
#endif
};
