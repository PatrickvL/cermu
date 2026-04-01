/*
 * nes_ppu.cpp — PPU (Picture Processing Unit) implementation
 *
 * Cycle-accurate PPU rendering including:
 *   - Background tile fetching and shifter logic
 *   - Sprite evaluation and rendering (8×8 and 8×16)
 *   - VRAM address management (coarse/fine scroll, nametable mirroring)
 *   - VBlank / NMI generation
 *
 * Extracted from the monolithic nes_system.cpp — behavior unchanged.
 */

// Include nes_system.h for full type definitions (Cartridge, etc.)
#include "core/cermu.hpp"
#include "systems/nes/nes_system.hpp"

// nes_ppu.h is transitively included via nes_system.h but be explicit
#include "chip/video/nes_ppu/nes_ppu.hpp"
#include "chip/video/nes_ppu/nes_palette.hpp"
#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

REGISTER_CHIP_TYPE("PPU", nes_system::PPU)

// ============================================================================
// Optional PPU sub-component profiling — enabled with -DNES_PROFILING
// ============================================================================
#ifdef NES_PROFILING
#include <x86intrin.h>

struct PpuSubProfile {
    uint64_t bg_fetch_cycles = 0;      // BG tile fetch switch statement
    uint64_t pixel_render_cycles = 0;  // Pixel compositing + screen write
    uint64_t sprite_eval_cycles = 0;   // evaluate_sprites() + load_sprite_shifters()
    uint64_t scroll_cycles = 0;        // increment_scroll + transfer_address
    uint64_t vbl_misc_cycles = 0;      // VBL handling, cycle advance, other
    uint64_t ppu_read_cycles = 0;      // ppu_read() calls (VRAM access)
    uint64_t shifter_cycles = 0;       // update_shifters + load_background_shifters
    uint64_t total_dots = 0;

    void report() const {
        if (total_dots == 0) return;
        uint64_t total = bg_fetch_cycles + pixel_render_cycles + sprite_eval_cycles +
                         scroll_cycles + vbl_misc_cycles + shifter_cycles;
        auto pct = [total](uint64_t c) { return total ? 100.0 * c / total : 0.0; };
        log_info("\n  === PPU Sub-Component Breakdown (rdtsc) ===\n");
        log_info("  BG fetch:        %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               bg_fetch_cycles, pct(bg_fetch_cycles), (double)bg_fetch_cycles / total_dots);
        log_info("  Pixel render:    %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               pixel_render_cycles, pct(pixel_render_cycles), (double)pixel_render_cycles / total_dots);
        log_info("  Sprite eval:     %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               sprite_eval_cycles, pct(sprite_eval_cycles), (double)sprite_eval_cycles / total_dots);
        log_info("  Shifter update:  %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               shifter_cycles, pct(shifter_cycles), (double)shifter_cycles / total_dots);
        log_info("  Scroll:          %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               scroll_cycles, pct(scroll_cycles), (double)scroll_cycles / total_dots);
        log_info("  VBL/misc:        %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               vbl_misc_cycles, pct(vbl_misc_cycles), (double)vbl_misc_cycles / total_dots);
        log_info("  -----------------------------------------\n");
        log_info("  Total measured:  %12lu rdtsc cycles\n", total);
    }

    void reset() { *this = {}; }
};

PpuSubProfile g_ppu_subprofile;
#define PPU_PROF_START(var)       uint64_t ppu_##var##_t0 = __rdtsc()
#define PPU_PROF_END(counter, var) g_ppu_subprofile.counter += __rdtsc() - ppu_##var##_t0
#define PPU_PROF_DOT()            g_ppu_subprofile.total_dots++
#else
#define PPU_PROF_START(var)       ((void)0)
#define PPU_PROF_END(counter, var) ((void)0)
#define PPU_PROF_DOT()            ((void)0)
#endif

namespace nes_system {

// ============================================================================
// PPU — CPU BUS INTERFACE
// ============================================================================

std::pair<bus_state_t, ppu_bus_state_t> PPU::service_cpu_bus(
        bus_state_t bus, ppu_bus_state_t ppu_bus) {
    const uint16_t addr = BUS_GET_ADDR(bus) & 0x2007;   // PPU mirrors every 8 bytes
    const bool is_read  = BUS_GET_BIT(bus, BUS_RW_BIT);

    if (is_read) {
        // ---- READ ----
        // CPU is not driving D0-D7.  Load the decayed PPU data latch
        // onto the bus, preserving addr/control.
        bus = apply_open_bus_decay(bus);

        switch (addr) {
            case 0x2000: // Control — write only (decayed data carries through)
            case 0x2001: // Mask — write only
            case 0x2003: // OAM Address — write only
            case 0x2005: // Scroll — write only
            case 0x2006: // PPU Address — write only
                break;
            case 0x2002: // Status
                // PPU drives bits 7-5 from status; bits 4-0 carry through.
                BUS_SET_DATA(bus, (regs_[PPUSTATUS] & 0xE0) | (BUS_GET_DATA(bus) & 0x1F));
                refresh_open_bus_timestamps(bus, 0xE0);

                // Flag that $2002 was read this dot, for VBL suppression
                // race-condition detection at the next clock() commit point.
                status_read_last_dot_ = true;

                // Clear VBL on read — both internal (NMI) and external ($2002).
                // Also cancel any pending propagation since the read overtakes it.
                regs_[PPUSTATUS] &= ~0x80;
                vbl_flag_internal_ = false;
                pending_vbl_set_ = false;
                internal.w = false;   // Reset write toggle
                update_nmi_output(ppu_bus);  // NMI level changes (VBL cleared)
                break;
            case 0x2004: { // OAM Data
                uint8_t data = oam.bytes[regs_[OAMADDR]];
                // Attribute byte (offset 2 in each 4-byte entry): bits 2-4
                // are unimplemented in hardware and always read back as 0.
                if ((regs_[OAMADDR] & 3) == 2) data &= 0xE3;
                BUS_SET_DATA(bus, data);
                refresh_open_bus_timestamps(bus);
                break;
            }
            case 0x2007: { // PPU Data
                uint8_t data = regs_[PPUDATA];
                regs_[PPUDATA] = ppu_read_byte(internal.v);

                // Palette reads are immediate (no buffering delay).
                // However, the read buffer must be filled with the
                // underlying nametable VRAM at the mirrored address
                // (addr & 0x2FFF), not the palette value itself.
                // Test: blargg vram_access test $06.
                if (internal.v >= 0x3F00) {
                    data = regs_[PPUDATA] & (regs_[PPUMASK] & 0x01 ? 0x30 : 0x3F);
                    // Backfill read buffer with nametable data behind palette
                    regs_[PPUDATA] = ppu_read_byte(internal.v & 0x2FFF);
                    // Palette read: PPU drives bits 5-0; bits 7-6 carry through.
                    BUS_SET_DATA(bus, (BUS_GET_DATA(bus) & 0xC0) | (data & 0x3F));
                    refresh_open_bus_timestamps(bus, 0x3F);
                } else {
                    BUS_SET_DATA(bus, data);
                    refresh_open_bus_timestamps(bus);
                }

                // Increment VRAM address
                internal.v += (regs_[PPUCTRL] & 0x04) ? 32 : 1;
                internal.v &= 0x7FFF;

                // Post-increment address drives the PPU bus.  A12 edge
                // detection is handled by ppu_memory_tick (called by
                // the system tick after service_cpu_bus returns).
                PPU_BUS_SET_ADDR(ppu_bus, internal.v & 0x3FFF);
                break;
            }
        }
    } else {
        // ---- WRITE ----
        // CPU drives all 8 data lines — refresh latch + timestamps.
        refresh_open_bus_timestamps(bus);
        const uint8_t data = BUS_GET_DATA(bus);

        switch (addr) {
            case 0x2000: // Control
                regs_[PPUCTRL] = data;
                internal.t = (internal.t & 0xF3FF) | ((data & 0x03) << 10);
                update_nmi_output(ppu_bus);  // NMI enable may have changed
                break;
            case 0x2001: // Mask
                flush_scanline_segment();   // Flush pixels rendered with old mask
                regs_[PPUMASK] = data;
                active_palette_ = nullptr;  // Invalidate pixel LUT
                break;
            case 0x2002: // Status — read only (write is ignored, bus latch updated above)
                break;
            case 0x2003: // OAM Address
                regs_[OAMADDR] = data;
                break;
            case 0x2004: // OAM Data
                oam_write(regs_[OAMADDR], data);
                regs_[OAMADDR]++;
                break;
            case 0x2005: // Scroll
                if (!internal.w) {
                    internal.t = (internal.t & 0x7FE0) | ((data & 0xF8) >> 3);
                    internal.x = data & 0x07;
                    internal.w = true;
                } else {
                    internal.t = (internal.t & 0x0FFF) | ((data & 0x07) << 12);
                    internal.t = (internal.t & 0x7C1F) | ((data & 0xF8) << 2);
                    internal.w = false;
                }
                break;
            case 0x2006: // PPU Address
                if (!internal.w) {
                    internal.t = (internal.t & 0x00FF) | ((data & 0x3F) << 8);
                    internal.w = true;
                } else {
                    internal.t = (internal.t & 0xFF00) | data;
                    internal.v = internal.t;
                    internal.w = false;
                    // v now drives the PPU address bus.  Games (and
                    // test ROMs) can clock the MMC3 counter by toggling
                    // A12 via $2006 writes.  A12 edge detection is
                    // handled by ppu_memory_tick after this returns.
                    PPU_BUS_SET_ADDR(ppu_bus, internal.v & 0x3FFF);
                }
                break;
            case 0x2007: // PPU Data
                ppu_write_byte(internal.v, data);
                internal.v += (regs_[PPUCTRL] & 0x04) ? 32 : 1;
                internal.v &= 0x7FFF;  // v is 15 bits
                // Post-increment address drives the PPU bus.  A12 edge
                // detection handled by ppu_memory_tick after this.
                PPU_BUS_SET_ADDR(ppu_bus, internal.v & 0x3FFF);
                break;
        }
    }

    // Return both bus states — the caller stores them as snapshots.
    return {bus, ppu_bus};
}

// ============================================================================
// PPU — Debug peek (no side-effects)
// ============================================================================

uint8_t PPU::cpu_peek(uint16_t addr) const {
    const uint8_t decayed = decayed_latch_data();
    switch (addr & 0x2007) {
        case 0x2002: return (regs_[PPUSTATUS] & 0xE0) | (decayed & 0x1F);
        case 0x2004: return oam.bytes[regs_[OAMADDR]];
        case 0x2007: return regs_[PPUDATA];
        default:     return decayed;
    }
}

// ============================================================================
// PPU — VRAM read / write
// ============================================================================

uint8_t PPU::ppu_read_byte(uint16_t addr) const {
    // ---- Palette RAM ($3F00-$3FFF) — internal to PPU, no bus access ----
    // Returns the raw palette byte.  Greyscale masking (PPUMASK bit 0) is
    // handled by the precalculated palette cache for rendering, and applied
    // explicitly in the $2007 CPU-read handler for CPU-visible reads.
    if (unlikely(addr >= 0x3F00)) {
        return palette[pal_mirror_[addr & 0x1F]];
    }

    // ---- CHR + nametable via block dispatch ----
    if (bus_ptr_) {
        uint16_t block = bus_ptr_->ppu_read_block[addr >> nes_bus::PPU_PAGE_SHIFT];
        if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
            return bus_ptr_->ppu_block_read(block, addr);
        }
    }

    return 0;
}

void PPU::ppu_write_byte(uint16_t addr, uint8_t data) {
    // ---- Palette RAM ($3F00-$3FFF) — internal to PPU ----
    if (unlikely(addr >= 0x3F00)) {
        flush_scanline_segment();   // Flush pixels rendered with old palette
        palette[pal_mirror_[addr & 0x1F]] = data;
        active_palette_ = nullptr;  // Invalidate pixel LUT
        return;
    }

    // ---- CHR + nametable via block dispatch ----
    if (bus_ptr_) {
        uint16_t block = bus_ptr_->ppu_write_block[addr >> nes_bus::PPU_PAGE_SHIFT];
        if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
            bus_ptr_->ppu_block_write(block, addr, data);
        }
    }
}

// clock(), increment_scroll_x/y(), transfer_address_x/y(),
// load_background_shifters(), update_shifters(), evaluate_sprites(),
// sprite_eval_step() — moved to nes_ppu_clock.inl (header-inline)
// for hot-path inlining into the system tick loop.

// ============================================================================
// PPU — Cartridge connection
// ============================================================================

void PPU::connect_cartridge(Cartridge* cartridge) {
    cart_ = cartridge;
}

// ============================================================================
// PPU — Debug field registration (ChipDebugRegistry)
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void PPU::register_debug_fields() {
    using P = const PPU;
    auto& r = debug_registry_;
    wire_debug_registers(NES_PPU_REG_INFO, 0x2000);
    r.set_decl_entries(NES_PPU_DECL_ENTRIES.data(), NES_PPU_DECL_ENTRIES.size());

    // Control/mask/status register values and bitfields are in the DECL walk.
    // Timing, internal state, and palette RAM remain as categories.

    // ---- Timing ----
    r.category("Timing");
    r.raster_position("Raster",
        +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<P*>(c)->scanline + 1);
        },
        +[](const ChipBase* c) -> uint32_t {
            return static_cast<P*>(c)->cycle;
        },
        +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<P*>(c)->total_scanlines_minus_one_ + 1);
        },
        uint32_t(nes_constants::DOTS_PER_SCANLINE));
    r.value("Frame", +[](const ChipBase* c) -> uint32_t {
        return static_cast<uint32_t>(static_cast<P*>(c)->frame_count);
    }, 32);
    r.state("Region", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->is_pal ? 1u : 0u; },
        (const char* const[]){"NTSC", "PAL"}, 2);
    r.flag("Frame Complete", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->frame_complete; });
    r.flag("NMI Internal", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->vbl_flag_internal_; });

    // ---- Internal State ----
    r.category("Internal State", false);
    r.address("VRAM Addr (v)", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->internal.v; }, 15);
    r.address("Temp Addr (t)", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->internal.t; }, 15);
    r.value("Fine X Scroll", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->internal.x; }, 3);
    r.flag("Write Toggle (w)", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->internal.w ? 1u : 0u; });
    r.value("Fine Y", +[](const ChipBase* c) -> uint32_t { return static_cast<P*>(c)->internal.fine_y; }, 3);

    // ---- Palette RAM ----
    r.category("Palette RAM", false);
    r.memory("Palette", +[](const ChipBase* c) -> std::pair<const uint8_t*, size_t> {
        return {static_cast<P*>(c)->palette.data(), static_cast<P*>(c)->palette.size()};
    }, 0x3F00, 32);
}
#endif // CERMU_HAS_CHIP_DEBUG

} // namespace nes_system
