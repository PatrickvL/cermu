#pragma once
/*
 * huc6260.hpp — Hudson HuC6260 VCE (Video Color Encoder) — Stub
 *
 * The HuC6260 converts VDC output into composite video, managing a
 * 512-entry palette of 9-bit colors (3-3-3 GRB) and dot clock selection.
 *
 * I/O ports ($0400–$0405):
 *   $0400: Control register
 *          Bit 1–0: Dot clock select (00=5.37MHz, 01=7.16MHz, 10=10.7MHz)
 *          Bit 2:   Strip colorburst (B&W mode)
 *   $0402: Color table address (write only)
 *   $0403: Color table address (high byte)
 *   $0404: Color table data (R/W, low byte)
 *   $0405: Color table data (high byte, auto-increment on write)
 *
 * Palette entries: 9-bit (3-3-3 GRB)
 *   Bits 0–2: Blue
 *   Bits 3–5: Red
 *   Bits 6–8: Green
 */

#include "chip/video/video_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define HUC6260_DECL(REG, FLD, CMP) \
    REG(0x00, CTRL,      "Control Register")                                        \
      FLD(CTRL, DOT_CLK,    1:0, "Dot clock select",      Value, 0, 0)             \
      FLD(CTRL, STRIP_CB,   2:2, "Strip colorburst",      Flag,  0, 0)             \
    REG(0x01, CTA_LO,   "Color Table Address (low)")                                \
    REG(0x02, CTA_HI,   "Color Table Address (high)")                               \
    REG(0x03, CTW_LO,   "Color Table Data (low)")                                   \
    REG(0x04, CTW_HI,   "Color Table Data (high)")

namespace huc6260 {
    namespace reg {
        HUC6260_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 5;
    }
    using namespace reg;

    inline constexpr int PALETTE_SIZE = 512;  // 512 entries × 9-bit

    // Dot clock frequencies
    inline constexpr uint32_t DOT_CLOCK_5MHZ  = 5369318;   // 5.37 MHz (256 px)
    inline constexpr uint32_t DOT_CLOCK_7MHZ  = 7159090;   // 7.16 MHz (320 px)
    inline constexpr uint32_t DOT_CLOCK_10MHZ = 10738636;   // 10.74 MHz (512 px)
}

DECL_EXTRACT(HUC6260, HUC6260_DECL)

// ============================================================================
// HuC6260 VCE — Stub Implementation
// ============================================================================

struct huc6260_t : public VideoChipBase {

    huc6260_t()
        : VideoChipBase(ChipInfo{"HuC6260", "Hudson Soft", "Video Color Encoder"})
    {
        init_regs(huc6260::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(HUC6260_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t port = BUS_GET_ADDR(bus) & 0x07;
        switch (port) {
            case 0x04:  // Color data LSB
                BUS_SET_DATA(bus, palette_[color_addr_] & 0xFF);
                break;
            case 0x05:  // Color data MSB (bits 0-1 = green upper)
                BUS_SET_DATA(bus, (palette_[color_addr_] >> 8) & 0x01);
                color_addr_ = (color_addr_ + 1) & 0x1FF;
                break;
            default:
                BUS_SET_DATA(bus, 0xFF);
                break;
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t port = BUS_GET_ADDR(bus) & 0x07;
        uint8_t data = BUS_GET_DATA(bus);
        switch (port) {
            case 0x00:  // Control register
                regs_.data[huc6260::CTRL] = data;
                dot_clock_mode_ = data & 0x03;
                break;
            case 0x02:  // Color address LSB
                color_addr_ = (color_addr_ & 0x100) | data;
                break;
            case 0x03:  // Color address MSB
                color_addr_ = (color_addr_ & 0x0FF) | ((data & 0x01) << 8);
                break;
            case 0x04:  // Color data LSB
                color_latch_ = data;
                break;
            case 0x05: {  // Color data MSB — triggers write
                uint16_t val = color_latch_ | ((data & 0x01) << 8);
                palette_[color_addr_] = val;
                color_addr_ = (color_addr_ + 1) & 0x1FF;
                break;
            }
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, huc6260::reg::REG_COUNT);
        std::memset(palette_, 0, sizeof(palette_));
        color_addr_ = 0;
        color_latch_ = 0;
        dot_clock_mode_ = 0;
    }

    // Convert 9-bit GRB to 32-bit RGBA
    uint32_t palette_to_rgba(uint16_t idx) const noexcept {
        if (idx >= huc6260::PALETTE_SIZE) return 0xFF000000;
        uint16_t val = palette_[idx];
        uint8_t b = ((val >> 0) & 0x07) * 36;  // 3-bit → 8-bit
        uint8_t r = ((val >> 3) & 0x07) * 36;
        uint8_t g = ((val >> 6) & 0x07) * 36;
        return (0xFF << 24) | (b << 16) | (g << 8) | r;
    }

    // ── Palette RAM ──────────────────────────────────────────────
    uint16_t palette_[huc6260::PALETTE_SIZE] = {};  // 512 × 9-bit (in 16-bit words)
    uint16_t color_addr_ = 0;
    uint8_t  color_latch_ = 0;
    uint8_t  dot_clock_mode_ = 0;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("VCE — Control")
            .value("Dot Clock", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6260_t*>(c)->dot_clock_mode_; })
            .category("VCE — Palette")
            .value("Color Addr", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6260_t*>(c)->color_addr_; });
    }
#endif
};
