#pragma once
/*
 * gtia.hpp — Atari GTIA (Graphic Television Interface Adapter)
 *
 * GTIA handles color generation, player/missile graphics (PMG),
 * collision detection, and joystick/trigger input for Atari 8-bit computers.
 *
 * Works in tandem with ANTIC: ANTIC provides the playfield data stream,
 * GTIA colorizes it and composites player/missile overlays.
 *
 * Features:
 *   - 128 hues × 16 luminances = 256-color palette
 *   - 4 players + 4 missiles (hardware sprites, each 8 pixels wide)
 *   - Programmable player/missile width (1×, 2×, 4× clock)
 *   - 16 collision registers (player–playfield, player–player, missile–*)
 *   - Priority control (players vs playfield layering)
 *   - GTIA enhanced modes: GTIA9 (16 luma), GTIA10 (9 color), GTIA11 (16 hue)
 *   - Console key registers (Start, Select, Option)
 *   - Trigger input (4 trigger buttons, active-low)
 *
 * Register map: 32 read registers ($D000–$D01F) + 32 write registers
 *   Write: HPOSP0–3, HPOSM0–3, SIZEP0–3, SIZEM, GRAFP0–3, GRAFM,
 *          COLPM0–3, COLPF0–3, COLBK, PRIOR, VDELAY, GRACTL, HITCLR, CONSPK
 *   Read:  M0PF–M3PF, P0PF–P3PF, M0PL–M3PL, P0PL–P3PL, TRIG0–3, PAL, CONSOL
 *
 * 40-pin DIP package (CO14805 / CO14889).
 */

#include "chip/video/video_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// GTIA UNIFIED DECLARATION TABLE
// ============================================================================

#define GTIA_DECL(REG, FLD, CMP) \
    /* Player/missile horizontal positions (write) */ \
    REG(0x00, HPOSP0, "Player 0 horizontal position")      \
    REG(0x01, HPOSP1, "Player 1 horizontal position")      \
    REG(0x02, HPOSP2, "Player 2 horizontal position")      \
    REG(0x03, HPOSP3, "Player 3 horizontal position")      \
    REG(0x04, HPOSM0, "Missile 0 horizontal position")     \
    REG(0x05, HPOSM1, "Missile 1 horizontal position")     \
    REG(0x06, HPOSM2, "Missile 2 horizontal position")     \
    REG(0x07, HPOSM3, "Missile 3 horizontal position")     \
    /* Size registers */ \
    REG(0x08, SIZEP0, "Player 0 size")                     \
      FLD(SIZEP0, P0WIDTH, 1:0, "Width", Value, 0, 0)     \
    REG(0x09, SIZEP1, "Player 1 size")                     \
    REG(0x0A, SIZEP2, "Player 2 size")                     \
    REG(0x0B, SIZEP3, "Player 3 size")                     \
    REG(0x0C, SIZEM,  "All missiles size")                 \
    /* Graphic shape data */ \
    REG(0x0D, GRAFP0, "Player 0 graphics")                 \
    REG(0x0E, GRAFP1, "Player 1 graphics")                 \
    REG(0x0F, GRAFP2, "Player 2 graphics")                 \
    REG(0x10, GRAFP3, "Player 3 graphics")                 \
    REG(0x11, GRAFM,  "Missile graphics (2 bits each)")    \
    /* Color registers */ \
    REG(0x12, COLPM0, "Player/Missile 0 color")            \
    REG(0x13, COLPM1, "Player/Missile 1 color")            \
    REG(0x14, COLPM2, "Player/Missile 2 color")            \
    REG(0x15, COLPM3, "Player/Missile 3 color")            \
    REG(0x16, COLPF0, "Playfield 0 color")                 \
    REG(0x17, COLPF1, "Playfield 1 color")                 \
    REG(0x18, COLPF2, "Playfield 2 color")                 \
    REG(0x19, COLPF3, "Playfield 3 color")                 \
    REG(0x1A, COLBK,  "Background color")                  \
    /* Control / Priority */ \
    REG(0x1B, PRIOR,  "Priority / GTIA mode select")       \
      FLD(PRIOR, MODE, 7:6, "GTIA mode (0=normal)", Value, 0, 0)  \
      FLD(PRIOR, MULTI, 5:5, "5th player enable",    Flag, 0, 0)  \
      FLD(PRIOR, PRI,   3:0, "Priority bits",        Value, 0, 0) \
    REG(0x1C, VDELAY, "Vertical delay")                    \
    REG(0x1D, GRACTL, "Graphics control")                  \
      FLD(GRACTL, TRIG_LATCH, 2:2, "Trigger latch",   Flag, 0, 0) \
      FLD(GRACTL, MISSILE_EN, 1:1, "Missile enable",   Flag, 0, 0) \
      FLD(GRACTL, PLAYER_EN,  0:0, "Player enable",    Flag, 0, 0) \
    REG(0x1E, HITCLR, "Collision clear (write)")           \
    REG(0x1F, CONSOL, "Console keys (read=status, write=speaker)")

namespace gtia {
    namespace reg {
        GTIA_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 32;
    }
    using namespace reg;
}

DECL_EXTRACT(GTIA, GTIA_DECL)

// ============================================================================
// GTIA Palette — 128 hues × 16 luminances
// ============================================================================

namespace gtia_palette {
    // NTSC palette: 256 entries (hue<<4 | luma)
    // Placeholder — accurate palette requires per-hue calibration
    inline constexpr uint32_t PALETTE_SIZE = 256;
    uint32_t generate_ntsc_color(uint8_t hue_luma);
}

// ============================================================================
// GTIA — Stub Implementation
// ============================================================================

struct gtia_t : public VideoChipBase {

    gtia_t()
        : VideoChipBase(ChipInfo{"GTIA", "Atari", "Graphic Television Interface Adapter"})
    {
        init_regs(gtia::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(GTIA_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x1F;
        // Read registers (collision, trigger, console)
        switch (addr) {
            case 0x00: case 0x01: case 0x02: case 0x03:  // M0PF–M3PF
            case 0x04: case 0x05: case 0x06: case 0x07:  // P0PF–P3PF
            case 0x08: case 0x09: case 0x0A: case 0x0B:  // M0PL–M3PL
            case 0x0C: case 0x0D: case 0x0E: case 0x0F:  // P0PL–P3PL
                BUS_SET_DATA(bus, collision_regs_[addr]);
                break;
            case 0x10: case 0x11: case 0x12: case 0x13:  // TRIG0–3
                BUS_SET_DATA(bus, trigger_[addr - 0x10] ? 0 : 1);
                break;
            case 0x14:  // PAL flag
                BUS_SET_DATA(bus, is_pal_ ? 0x01 : 0x0F);
                break;
            case 0x1F:  // CONSOL
                BUS_SET_DATA(bus, console_keys_);
                break;
            default:
                BUS_SET_DATA(bus, 0xFF);
                break;
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x1F;
        uint8_t data = BUS_GET_DATA(bus);
        regs_.data[addr] = data;

        if (addr == gtia::HITCLR) {
            std::memset(collision_regs_, 0, sizeof(collision_regs_));
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        // TODO: Player/missile rendering, collision detection, color generation
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, gtia::reg::REG_COUNT);
        std::memset(collision_regs_, 0, sizeof(collision_regs_));
        std::memset(trigger_, 0, sizeof(trigger_));
        console_keys_ = 0x07;  // All keys released (active-low)
        is_pal_ = false;
    }

    // Collision detection array (16 read registers)
    uint8_t collision_regs_[16] = {};

    // Trigger buttons (active-low)
    bool trigger_[4] = {};

    // Console keys: bit 0=Start, 1=Select, 2=Option (active-low)
    uint8_t console_keys_ = 0x07;

    // PAL/NTSC flag
    bool is_pal_ = false;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("GTIA — Colors")
            .value("COLPM0", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gtia_t*>(c)->regs_.data[gtia::COLPM0]; })
            .value("COLPF0", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gtia_t*>(c)->regs_.data[gtia::COLPF0]; })
            .value("COLBK",  +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gtia_t*>(c)->regs_.data[gtia::COLBK]; })
            .category("GTIA — Players/Missiles")
            .value("HPOSP0", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gtia_t*>(c)->regs_.data[gtia::HPOSP0]; })
            .value("GRAFP0", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gtia_t*>(c)->regs_.data[gtia::GRAFP0]; })
            .category("GTIA — Control")
            .value("PRIOR",  +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gtia_t*>(c)->regs_.data[gtia::PRIOR]; })
            .value("GRACTL", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gtia_t*>(c)->regs_.data[gtia::GRACTL]; });
    }
#endif
};
