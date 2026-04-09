#pragma once
/*
 * huc6270.hpp — Hudson HuC6270 VDC (Video Display Controller) — Stub
 *
 * The HuC6270 is the PC Engine / TurboGrafx-16 background and sprite
 * rendering chip.  It manages a 64KB VRAM, up to 64 sprites, two
 * tile-map layers (BAT), and generates raster interrupts.
 *
 * I/O interface (directly addressable by CPU via ST0/ST1/ST2):
 *   Port 0 ($0000): Register select (AR)
 *   Port 1 ($0002): Data low byte
 *   Port 2 ($0003): Data high byte
 *
 * 20 internal 16-bit registers (selected through AR):
 *   00 MAWR  — Memory Address Write Register
 *   01 MARR  — Memory Address Read Register
 *   02 VRR/VWR — VRAM Read/Write Register (data port)
 *   03 (unused)
 *   04 (unused)
 *   05 CR    — Control Register
 *   06 RCR   — Raster Counter Register
 *   07 BXR   — BG X Scroll
 *   08 BYR   — BG Y Scroll
 *   09 MWR   — Memory Width Register
 *   0A HSR   — Horizontal Sync Register
 *   0B HDR   — Horizontal Display Register
 *   0C VSR   — Vertical Sync Register
 *   0D VDR   — Vertical Display Register
 *   0E VCR   — Vertical Display End Position
 *   0F DCR   — DMA Control Register
 *   10 SOUR  — DMA Source Address
 *   11 DESR  — DMA Destination Address
 *   12 LENR  — DMA Transfer Length
 *   13 DVSSR — VRAM-SATB DMA Source Address
 *
 * Status register (read via port 0):
 *   Bit 0: Collision detect
 *   Bit 1: Overflow
 *   Bit 2: Raster compare (RCR)
 *   Bit 3: Reserved
 *   Bit 4: Reserved
 *   Bit 5: V-Blank
 *   Bit 6: Reserved
 */

#include "chip/video/video_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define HUC6270_DECL(REG, FLD, CMP) \
    REG(0x00, MAWR,  "Memory Address Write")                                       \
    REG(0x01, MARR,  "Memory Address Read")                                        \
    REG(0x02, VRR,   "VRAM Read/Write Register")                                   \
    REG(0x03, RSV03, "Reserved 03")                                                \
    REG(0x04, RSV04, "Reserved 04")                                                \
    REG(0x05, CR,    "Control Register")                                           \
      FLD(CR, INC_MODE,    11:11, "Increment mode",      Value, 0, 0)              \
      FLD(CR, DMASAT_IRQ,   3:3, "SATB DMA IRQ enable",  Flag,  0, 0)             \
      FLD(CR, DMA_IRQ,      2:2, "VRAM DMA IRQ enable",  Flag,  0, 0)             \
      FLD(CR, VBLANK_IRQ,   3:3, "V-Blank IRQ enable",   Flag,  0, 0)             \
      FLD(CR, RCR_IRQ,      2:2, "Raster compare IRQ",   Flag,  0, 0)             \
      FLD(CR, SPR_EN,       1:1, "Sprite enable",         Flag,  0, 0)             \
      FLD(CR, BG_EN,        0:0, "BG enable",             Flag,  0, 0)             \
    REG(0x06, RCR,   "Raster Counter Register")                                    \
    REG(0x07, BXR,   "BG X Scroll")                                                \
    REG(0x08, BYR,   "BG Y Scroll")                                                \
    REG(0x09, MWR,   "Memory Width Register")                                      \
      FLD(MWR, SCREEN_W,    5:4, "Screen width",         Value, 0, 0)              \
      FLD(MWR, SCREEN_H,    6:6, "Screen height",        Value, 0, 0)              \
    REG(0x0A, HSR,   "H-Sync Register")                                            \
    REG(0x0B, HDR,   "H-Display Register")                                         \
    REG(0x0C, VSR,   "V-Sync Register")                                            \
    REG(0x0D, VDR,   "V-Display Register")                                         \
    REG(0x0E, VCR,   "V-Display End")                                              \
    REG(0x0F, DCR,   "DMA Control")                                                \
      FLD(DCR, DST_INC,     3:3, "Dest auto-increment",   Flag, 0, 0)             \
      FLD(DCR, SRC_INC,     2:2, "Source auto-increment",  Flag, 0, 0)            \
      FLD(DCR, DMA_REPEAT,  1:1, "DMA repeat",             Flag, 0, 0)            \
      FLD(DCR, DMA_IRQ,     0:0, "DMA transfer IRQ enable",Flag, 0, 0)            \
    REG(0x10, SOUR,  "DMA Source Address")                                         \
    REG(0x11, DESR,  "DMA Destination Address")                                    \
    REG(0x12, LENR,  "DMA Transfer Length")                                        \
    REG(0x13, DVSSR, "VRAM→SATB DMA Source")

namespace huc6270 {
    namespace reg {
        HUC6270_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 20;
    }
    using namespace reg;

    inline constexpr int VRAM_SIZE       = 65536;  // 64KB (32K words)
    inline constexpr int SATB_SIZE       = 256;    // 64 sprites × 4 words
    inline constexpr int SCREEN_WIDTH    = 256;
    inline constexpr int SCREEN_HEIGHT   = 240;
    inline constexpr int TOTAL_LINES     = 263;    // NTSC

    // Status register bits
    inline constexpr uint8_t STATUS_COLLISION = 0x01;
    inline constexpr uint8_t STATUS_OVERFLOW  = 0x02;
    inline constexpr uint8_t STATUS_RCR       = 0x04;
    inline constexpr uint8_t STATUS_VBLANK    = 0x20;
}

DECL_EXTRACT(HUC6270, HUC6270_DECL)

// ============================================================================
// HuC6270 VDC — Stub Implementation
// ============================================================================

struct huc6270_t : public VideoChipBase {

    huc6270_t()
        : VideoChipBase(ChipInfo{"HuC6270", "Hudson Soft", "Video Display Controller"})
    {
        init_regs(huc6270::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(HUC6270_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    // External port access (ST0/ST1/ST2 or memory-mapped):
    //   Port 0: register select (write) / status (read)
    //   Port 2-3: data low/high
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t port = BUS_GET_ADDR(bus) & 0x03;
        switch (port) {
            case 0:  // Status register
                BUS_SET_DATA(bus, status_);
                status_ &= ~(huc6270::STATUS_COLLISION |
                              huc6270::STATUS_OVERFLOW |
                              huc6270::STATUS_RCR |
                              huc6270::STATUS_VBLANK);
                break;
            case 2:  // Data LSB — read from VRR
                if (address_reg_ == huc6270::VRR) {
                    uint16_t word = vram_read(read_addr_);
                    read_latch_ = word;
                    BUS_SET_DATA(bus, word & 0xFF);
                } else {
                    BUS_SET_DATA(bus, regs16_[address_reg_] & 0xFF);
                }
                break;
            case 3:  // Data MSB
                if (address_reg_ == huc6270::VRR) {
                    BUS_SET_DATA(bus, (read_latch_ >> 8) & 0xFF);
                    read_addr_ += increment();
                } else {
                    BUS_SET_DATA(bus, (regs16_[address_reg_] >> 8) & 0xFF);
                }
                break;
            default:
                BUS_SET_DATA(bus, 0xFF);
                break;
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t port = BUS_GET_ADDR(bus) & 0x03;
        uint8_t data = BUS_GET_DATA(bus);
        switch (port) {
            case 0:  // Register select
                address_reg_ = data & 0x1F;
                break;
            case 2:  // Data LSB
                write_latch_ = (write_latch_ & 0xFF00) | data;
                break;
            case 3: {  // Data MSB — triggers 16-bit register write
                uint16_t val = write_latch_ | (static_cast<uint16_t>(data) << 8);
                if (address_reg_ < huc6270::reg::REG_COUNT) {
                    regs16_[address_reg_] = val;

                    if (address_reg_ == huc6270::MAWR)
                        write_addr_ = val;
                    else if (address_reg_ == huc6270::MARR) {
                        read_addr_ = val;
                        read_latch_ = vram_read(read_addr_);
                    }
                    else if (address_reg_ == huc6270::VRR) {
                        // VWR — write to VRAM at MAWR
                        vram_write(write_addr_, val);
                        write_addr_ += increment();
                    }
                }
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

        // TODO: Scanline rendering, sprite evaluation, BAT/CG fetches, DMA

        dot_counter_++;
        if (dot_counter_ >= 1365) {  // dots per line (NTSC)
            dot_counter_ = 0;
            scanline_++;
            if (scanline_ >= huc6270::TOTAL_LINES) {
                scanline_ = 0;
            }
            // V-Blank flag at visible end
            if (scanline_ == huc6270::SCREEN_HEIGHT) {
                status_ |= huc6270::STATUS_VBLANK;
            }
            // Raster compare
            if (scanline_ == (regs16_[huc6270::RCR] & 0x3FF)) {
                status_ |= huc6270::STATUS_RCR;
            }
        }
        return bus;
    }

    void reset() override {
        std::memset(regs16_, 0, sizeof(regs16_));
        std::memset(vram_, 0, sizeof(vram_));
        address_reg_ = 0;
        write_addr_ = 0;
        read_addr_ = 0;
        write_latch_ = 0;
        read_latch_ = 0;
        status_ = 0;
        scanline_ = 0;
        dot_counter_ = 0;
    }

    // ── Accessors ────────────────────────────────────────────────
    bool irq_pending() const noexcept {
        uint16_t cr = regs16_[huc6270::CR];
        return (status_ & huc6270::STATUS_VBLANK && (cr & (1 << 3))) ||
               (status_ & huc6270::STATUS_RCR    && (cr & (1 << 2)));
    }

    // ── VRAM ─────────────────────────────────────────────────────
    uint8_t vram_[huc6270::VRAM_SIZE] = {};          // 64KB byte-addressed
    uint16_t regs16_[huc6270::reg::REG_COUNT] = {};  // 20 × 16-bit registers

    uint8_t  address_reg_ = 0;     // AR — register select
    uint16_t write_addr_ = 0;      // MAWR latched
    uint16_t read_addr_ = 0;       // MARR latched
    uint16_t write_latch_ = 0;     // low-byte latch for 16-bit writes
    uint16_t read_latch_ = 0;      // read data latch
    uint8_t  status_ = 0;          // Status register (cleared on read)
    uint16_t scanline_ = 0;
    uint16_t dot_counter_ = 0;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint16_t vram_read(uint16_t addr) const noexcept {
        uint32_t byte_addr = static_cast<uint32_t>(addr) << 1;
        return vram_[byte_addr] | (static_cast<uint16_t>(vram_[byte_addr + 1]) << 8);
    }
    void vram_write(uint16_t addr, uint16_t data) noexcept {
        uint32_t byte_addr = static_cast<uint32_t>(addr) << 1;
        if (byte_addr + 1 < huc6270::VRAM_SIZE) {
            vram_[byte_addr]     = data & 0xFF;
            vram_[byte_addr + 1] = (data >> 8) & 0xFF;
        }
    }
    uint16_t increment() const noexcept {
        static constexpr uint16_t inc_table[] = {1, 32, 64, 128};
        return inc_table[(regs16_[huc6270::CR] >> 11) & 0x03];
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("VDC — Control")
            .value("AR", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->address_reg_; })
            .value("Status", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->status_; })
            .value("CR", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->regs16_[huc6270::CR]; })
            .category("VDC — Scroll")
            .value("BXR", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->regs16_[huc6270::BXR]; })
            .value("BYR", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->regs16_[huc6270::BYR]; })
            .category("VDC — Raster")
            .value("Scanline", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->scanline_; })
            .value("RCR", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->regs16_[huc6270::RCR]; })
            .category("VDC — VRAM")
            .value("MAWR", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->regs16_[huc6270::MAWR]; })
            .value("MARR", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const huc6270_t*>(c)->regs16_[huc6270::MARR]; });
    }
#endif
};
