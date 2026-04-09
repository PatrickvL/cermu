#pragma once
/*
 * genesis_315_5313.hpp — Sega 315-5313 / YM7101 Genesis VDP
 *
 * Member of the TMS9918 VDP family — 4th generation derivative
 *   TMS9918 → 315-5124 (SMS) → 315-5246 (SMS2) → 315-5313 (Genesis)
 *
 * The Genesis VDP retains backward compatibility with SMS Mode 4 and adds
 * Mode 5 with dual scrollable tile planes, 64KB VRAM, 64-entry CRAM,
 * hardware DMA, shadow/highlight, and H40/H32 display modes.
 *
 * Currently a standalone class pending integration into the tms9918_t<>
 * template once Mode 5 rendering is added to the shared pipeline.
 * The VDPTraits definition below places it in the family for
 * classification and feature queries.
 */

#include "chip/video/tms9918/tms9918_traits.hpp"
#include "chip/video/video_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// TMS9918 Family Traits — Genesis 315-5313
// ============================================================================

namespace tms9918 {

inline constexpr VDPTraits GENESIS_315_5313_NTSCTraits = {
    "Yamaha",                        // vendor
    "315-5313",                      // chip_id
    "Sega 315-5313 (YM7101)",       // display_name
    VDPRegion::NTSC,                 // region
    VDPOutput::COMPOSITE,            // output
    VDPSpriteModel::GENESIS,         // sprite_model
    VDPPaletteModel::GENESIS_CRAM,   // palette_model
    VDPScrollModel::GENESIS,         // scroll_model
    VDPFeatureFlags::SEGA_MODE_EXT         |  // SMS backward compat
    VDPFeatureFlags::SEGA_EXT_LINES        |  // 224/240 line modes
    VDPFeatureFlags::GENESIS_MODE5         |  // Mode 5 planes
    VDPFeatureFlags::GENESIS_DMA           |  // DMA engine
    VDPFeatureFlags::GENESIS_SHADOW_HL     |  // Shadow/highlight
    VDPFeatureFlags::GENESIS_H40,              // H40 mode
    24,                              // num_registers
    64,                              // vram_size_kb
    262,                             // total_lines (NTSC)
    224,                             // visible_lines
    13'423'294,                      // dot_clock_hz (53.69 MHz master / 4)
};

inline constexpr VDPTraits GENESIS_315_5313_PALTraits = {
    "Yamaha",                        // vendor
    "315-5313",                      // chip_id
    "Sega 315-5313 (YM7101, PAL)",  // display_name
    VDPRegion::PAL,                  // region
    VDPOutput::COMPOSITE,            // output
    VDPSpriteModel::GENESIS,         // sprite_model
    VDPPaletteModel::GENESIS_CRAM,   // palette_model
    VDPScrollModel::GENESIS,         // scroll_model
    VDPFeatureFlags::SEGA_MODE_EXT         |
    VDPFeatureFlags::SEGA_EXT_LINES        |
    VDPFeatureFlags::GENESIS_MODE5         |
    VDPFeatureFlags::GENESIS_DMA           |
    VDPFeatureFlags::GENESIS_SHADOW_HL     |
    VDPFeatureFlags::GENESIS_H40,
    24,                              // num_registers
    64,                              // vram_size_kb
    313,                             // total_lines (PAL)
    240,                             // visible_lines (PAL V30)
    13'300'856,                      // dot_clock_hz (PAL master / 4)
};

} // namespace tms9918

// ============================================================================
// GENESIS VDP REGISTER DECLARATIONS
// ============================================================================

#define GENESIS_VDP_DECL(REG, FLD, CMP) \
    REG(0x00, VDP_MODE1,    "Mode Register 1")                                  \
      FLD(VDP_MODE1, HINT_EN,  4:4, "H-Int enable",      Flag, 0, 0)           \
      FLD(VDP_MODE1, HV_LATCH, 1:1, "HV counter latch",  Flag, 0, 0)           \
      FLD(VDP_MODE1, DISP_EN,  0:0, "Display enable",    Flag, 0, 0)           \
    REG(0x01, VDP_MODE2,    "Mode Register 2")                                  \
      FLD(VDP_MODE2, VINT_EN,  5:5, "V-Int enable",      Flag, 0, 0)           \
      FLD(VDP_MODE2, DMA_EN,   4:4, "DMA enable",        Flag, 0, 0)           \
      FLD(VDP_MODE2, V30,      3:3, "V30 mode (PAL)",    Flag, 0, 0)           \
      FLD(VDP_MODE2, MODE5,    2:2, "Mode 5 (Genesis)",   Flag, 0, 0)          \
    REG(0x02, VDP_PLANE_A,  "Plane A name table address")                       \
    REG(0x03, VDP_WINDOW,   "Window name table address")                        \
    REG(0x04, VDP_PLANE_B,  "Plane B name table address")                       \
    REG(0x05, VDP_SPRITE,   "Sprite attribute table address")                   \
    REG(0x06, VDP_UNUSED06, "(unused)")                                         \
    REG(0x07, VDP_BGCOL,    "Background color (palette + index)")               \
    REG(0x08, VDP_UNUSED08, "(unused)")                                         \
    REG(0x09, VDP_UNUSED09, "(unused)")                                         \
    REG(0x0A, VDP_HINT_CTR, "H-Interrupt counter")                              \
    REG(0x0B, VDP_MODE3,    "Mode Register 3")                                  \
      FLD(VDP_MODE3, EINT_EN,  3:3, "Ext Int enable",    Flag, 0, 0)           \
      FLD(VDP_MODE3, VSCR_MODE,2:2, "V-Scroll mode",     Flag, 0, 0)           \
      FLD(VDP_MODE3, HSCR_MODE,1:0, "H-Scroll mode",     Value, 0, 0)          \
    REG(0x0C, VDP_MODE4,    "Mode Register 4")                                  \
      FLD(VDP_MODE4, H40,      7:7, "H40 mode",          Flag, 0, 0)           \
      FLD(VDP_MODE4, SH,       3:3, "Shadow/highlight",   Flag, 0, 0)          \
      FLD(VDP_MODE4, INTERLACE,1:0, "Interlace mode",     Value, 0, 0)         \
    REG(0x0D, VDP_HSCROLL,  "H-Scroll data table address")                     \
    REG(0x0E, VDP_UNUSED0E, "(unused)")                                         \
    REG(0x0F, VDP_AUTOINC,  "Auto-increment value")                             \
    REG(0x10, VDP_PLANE_SZ, "Plane size (H/V)")                                 \
      FLD(VDP_PLANE_SZ, VSZ,   5:4, "Vertical size",     Value, 0, 0)          \
      FLD(VDP_PLANE_SZ, HSZ,   1:0, "Horizontal size",   Value, 0, 0)          \
    REG(0x11, VDP_WIN_H,    "Window H position")                                \
    REG(0x12, VDP_WIN_V,    "Window V position")                                \
    REG(0x13, VDP_DMA_LEN_L,"DMA length low")                                  \
    REG(0x14, VDP_DMA_LEN_H,"DMA length high")                                 \
    REG(0x15, VDP_DMA_SRC_L,"DMA source low")                                  \
    REG(0x16, VDP_DMA_SRC_M,"DMA source mid")                                  \
    REG(0x17, VDP_DMA_SRC_H,"DMA source high / DMA type")

namespace genesis_vdp {
    namespace reg {
        GENESIS_VDP_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 24;
    }
    using namespace reg;

    inline constexpr int DISPLAY_WIDTH_H40  = 320;
    inline constexpr int DISPLAY_WIDTH_H32  = 256;
    inline constexpr int DISPLAY_HEIGHT     = 224;
    inline constexpr int DISPLAY_HEIGHT_PAL = 240;
    inline constexpr int VRAM_SIZE          = 65536;
    inline constexpr int CRAM_SIZE          = 128;    // 64 color entries × 2 bytes
    inline constexpr int VSRAM_SIZE         = 80;     // 40 entries × 2 bytes
}

DECL_EXTRACT(GENESIS_VDP, GENESIS_VDP_DECL)

// ============================================================================
// Genesis VDP — Stub Implementation
//
// Standalone class within the TMS9918 family directory.  Pending integration
// into tms9918_t<> when Mode 5 rendering is added to the shared pipeline.
// ============================================================================

struct genesis_vdp_t : public VideoChipBase {

    genesis_vdp_t()
        : VideoChipBase(ChipInfo{"315-5313", "Yamaha", "Sega Genesis VDP (YM7101)"})
    {
        init_regs(genesis_vdp::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(GENESIS_VDP_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    // Data port read/write ($C00000–$C00003)
    // Control port read/write ($C00004–$C00007)
    // HV counter read ($C00008–$C0000B)
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint32_t addr = BUS_GET_ADDR(bus) & 0x1F;
        if (addr < 4) {
            BUS_SET_DATA(bus, read_data_port());
        } else if (addr < 8) {
            BUS_SET_DATA(bus, status_);
        } else if (addr < 12) {
            BUS_SET_DATA(bus, (vcounter_ << 8) | hcounter_);
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint32_t addr = BUS_GET_ADDR(bus) & 0x1F;
        uint16_t data = BUS_GET_DATA(bus);
        if (addr < 4) {
            write_data_port(data);
        } else if (addr < 8) {
            write_control_port(data);
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        // TODO: Scanline rendering, DMA processing, H/V counter, interrupt generation
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, genesis_vdp::reg::REG_COUNT);
        std::memset(vram_, 0, sizeof(vram_));
        std::memset(cram_, 0, sizeof(cram_));
        std::memset(vsram_, 0, sizeof(vsram_));
        status_ = 0x3400;  // FIFO empty + VBlank
        vcounter_ = 0;
        hcounter_ = 0;
        command_pending_ = false;
        command_word_ = 0;
        address_ = 0;
        code_ = 0;
    }

    // ── VRAM / CRAM / VSRAM ─────────────────────────────────────────
    uint8_t  vram_[genesis_vdp::VRAM_SIZE] = {};
    uint8_t  cram_[genesis_vdp::CRAM_SIZE] = {};
    uint8_t  vsram_[genesis_vdp::VSRAM_SIZE] = {};

    uint16_t status_       = 0x3400;
    uint16_t vcounter_     = 0;
    uint16_t hcounter_     = 0;

    // Command port state machine
    bool     command_pending_ = false;
    uint16_t command_word_    = 0;
    uint16_t address_         = 0;
    uint8_t  code_            = 0;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint8_t read_data_port() {
        // TODO: read from VRAM/CRAM/VSRAM based on code_
        return 0;
    }

    void write_data_port(uint16_t data) {
        if (code_ < 2) {
            // VRAM write
            if (address_ < genesis_vdp::VRAM_SIZE) {
                vram_[address_] = data >> 8;
                if (address_ + 1 < genesis_vdp::VRAM_SIZE)
                    vram_[address_ + 1] = data & 0xFF;
            }
        }
        address_ += regs_.data[genesis_vdp::VDP_AUTOINC];
    }

    void write_control_port(uint16_t data) {
        if (command_pending_) {
            // Second word: complete the command
            command_word_ = data;
            code_    = ((command_word_ >> 2) & 0x3C) | (code_ & 0x03);
            address_ = (address_ & 0x3FFF) | ((command_word_ & 0x03) << 14);
            command_pending_ = false;
        } else if ((data & 0xC000) == 0x8000) {
            // Register write: 100R RRRR DDDD DDDD
            uint8_t reg = (data >> 8) & 0x1F;
            if (reg < genesis_vdp::reg::REG_COUNT)
                regs_.data[reg] = data & 0xFF;
        } else {
            // First word of command
            code_    = (data >> 14) & 0x03;
            address_ = data & 0x3FFF;
            command_pending_ = true;
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("VDP — Mode")
            .value("Mode1", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const genesis_vdp_t*>(c)->regs_.data[genesis_vdp::VDP_MODE1]; })
            .value("Mode2", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const genesis_vdp_t*>(c)->regs_.data[genesis_vdp::VDP_MODE2]; })
            .category("VDP — Status")
            .value("Status", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const genesis_vdp_t*>(c)->status_; })
            .value("V Counter", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const genesis_vdp_t*>(c)->vcounter_; })
            .value("H Counter", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const genesis_vdp_t*>(c)->hcounter_; })
            .category("VDP — Tables")
            .value("Plane A", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const genesis_vdp_t*>(c)->regs_.data[genesis_vdp::VDP_PLANE_A]; })
            .value("Sprite", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const genesis_vdp_t*>(c)->regs_.data[genesis_vdp::VDP_SPRITE]; });
    }
#endif
};
