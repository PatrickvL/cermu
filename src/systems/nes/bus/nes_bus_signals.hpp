#pragma once
/*
 * nes_bus_signals.h — NES bus signal definitions
 *
 * Defines the PPU bus state type (ppu_bus_state_t) alongside the CPU
 * bus_state_t from system_lines.h.  Signals that bridge CPU ↔ PPU buses
 * through the cartridge connector (/IRQ, /NMI, /RES) are placed at
 * IDENTICAL bit positions in both bus words, enabling direct bitmixing:
 *
 *     cpu_bus = PPU_CPU_BITMIX(cpu_bus, ppu_bus);
 *
 * This transfers mapper /IRQ (MMC3 scanline counter) and PPU /NMI onto
 * the CPU bus with a single mask-OR — no bit shifting, no conditionals.
 *
 * See NES_MIGRATION_PLAN.md Part 3 for the full rationale.
 */

#include <cstdint>
#include "core/system_lines.hpp"

// ============================================================================
// PPU BUS STATE — 64-bit bus word for the PPU's 14-bit address bus
// ============================================================================
//
// The NES has two electrically independent buses:
//   CPU bus: 16-bit addr + 8-bit data @ ~1.79 MHz  (system_lines.h bus_state_t)
//   PPU bus: 14-bit addr + 8-bit data @ ~5.37 MHz  (this file, ppu_bus_state_t)
//
// Layout:
//   Bits 0-7:    PD0-PD7  (PPU data bus, bidirectional)
//   Bits 8-21:   PA0-PA13 (PPU address bus, 14-bit)
//   Bit 22:      /RD      (PPU read strobe, active low)       — PPU-only
//   Bit 23:      /WR      (PPU write strobe, active low)      — PPU-only
//   Bit 24:      ALE      (address latch enable)               — PPU-only
//   Bit 25:      /A13     (active-low complement of PA13)      — PPU-only
//   Bit 26:      CIRAM /CE (CIRAM chip enable)                 — PPU-only
//   Bit 27:      CIRAM A10 (nametable mirroring, cart output)  — PPU-only
//   Bit 28:      PA12     (exposed for mapper A12 edge detect) — PPU-only
//   Bits 29-31:  (reserved)
//
//   === SHARED BITS — same positions as CPU bus_state_t (system_lines.h) ===
//   Bit 32:      /RES     (system reset, active low)
//   Bit 33:      /IRQ     (cartridge → CPU, active low)
//   Bit 34:      /NMI     (PPU → CPU, active low)
//
// The shared region [32..34] enables zero-cost bitmixing between bus words.
// ============================================================================

using ppu_bus_state_t = uint64_t;

// ============================================================================
// PPU BUS FIELD LAYOUT
// ============================================================================

#define PPU_BUS_DATA_WIDTH        8
#define PPU_BUS_DATA_SHIFT        0
#define PPU_BUS_DATA_BITS         ((1ULL << PPU_BUS_DATA_WIDTH) - 1ULL)
#define PPU_BUS_DATA_MASK         (PPU_BUS_DATA_BITS << PPU_BUS_DATA_SHIFT)

#define PPU_BUS_ADDR_WIDTH        14
#define PPU_BUS_ADDR_SHIFT        (PPU_BUS_DATA_SHIFT + PPU_BUS_DATA_WIDTH)
#define PPU_BUS_ADDR_BITS         ((1ULL << PPU_BUS_ADDR_WIDTH) - 1ULL)
#define PPU_BUS_ADDR_MASK         (PPU_BUS_ADDR_BITS << PPU_BUS_ADDR_SHIFT)

// ============================================================================
// PPU BUS FIELD ACCESS MACROS
// ============================================================================

#define PPU_BUS_GET_DATA(b)       ((uint8_t)(((b) >> PPU_BUS_DATA_SHIFT) & PPU_BUS_DATA_BITS))
#define PPU_BUS_SET_DATA(b, d)    ((b) = (b) ^ (((b) ^ ((uint64_t)(d) << PPU_BUS_DATA_SHIFT)) & PPU_BUS_DATA_MASK))

#define PPU_BUS_GET_ADDR(b)       ((uint16_t)(((b) >> PPU_BUS_ADDR_SHIFT) & PPU_BUS_ADDR_BITS))
#define PPU_BUS_SET_ADDR(b, a)    ((b) = (b) ^ (((b) ^ ((uint64_t)(a) << PPU_BUS_ADDR_SHIFT)) & PPU_BUS_ADDR_MASK))

#define PPU_BUS_GET_BIT(b, bit)   (((b) & (1ULL << (bit))) != 0)
#define PPU_BUS_SET_BIT(b, bit)   ((b) |=  (1ULL << (bit)))
#define PPU_BUS_CLR_BIT(b, bit)   ((b) &= ~(1ULL << (bit)))

// ============================================================================
// PPU BUS CONSTRUCTION HELPERS
// ============================================================================
// Build a ppu_bus_state_t from address (and optionally data) in one expression.
// Useful at call sites that construct a bus transaction inline.

#define PPU_BUS_WITH_ADDR(a)        ((ppu_bus_state_t)(((uint64_t)((a) & PPU_BUS_ADDR_BITS)) << PPU_BUS_ADDR_SHIFT))
#define PPU_BUS_WITH_ADDR_DATA(a,d) ((ppu_bus_state_t)((((uint64_t)((a) & PPU_BUS_ADDR_BITS)) << PPU_BUS_ADDR_SHIFT) | ((uint64_t)(d) & PPU_BUS_DATA_BITS)))

// ============================================================================
// PPU-ONLY PIN BIT POSITIONS (22-28)
// ============================================================================

#define PPU_BUS_RD_BIT      22  // /RD — VRAM read strobe (active low)
#define PPU_BUS_WR_BIT      23  // /WR — VRAM write strobe (active low)
#define PPU_BUS_ALE_BIT     24  // ALE — address latch enable
#define PPU_BUS_A13N_BIT    25  // /A13 — active-low complement of PA13
#define PPU_BUS_CIRAM_CE    26  // CIRAM /CE — CIRAM chip enable
#define PPU_BUS_CIRAM_A10   27  // CIRAM A10 — nametable mirroring selection (cart output)
#define PPU_BUS_PA12_BIT    28  // PA12 — exposed for mapper A12 edge detection (MMC3)

// ============================================================================
// SHARED PIN BIT POSITIONS — identical to CPU bus (system_lines.h)
// ============================================================================
// These are intentionally NOT redefined — we use the originals from system_lines.h
// to guarantee bit-position alignment.  Listed here for documentation:
//
//   PPU_BUS_RES_BIT = BUS_RES_BIT = 32   (/RES — system reset)
//   PPU_BUS_IRQ_BIT = BUS_IRQ_BIT = 33   (/IRQ — cartridge interrupt)
//   PPU_BUS_NMI_BIT = BUS_NMI_BIT = 34   (/NMI — PPU vertical blank)

// ============================================================================
// BITMIX — transfer shared signals between CPU and PPU bus words
// ============================================================================

// Mask covering all signals that bridge CPU ↔ PPU buses through the
// PPU → CPU shared signals — NMI and RES only.
// /IRQ is NOT included: the PPU never drives /IRQ (it's always HIGH on
// ppu_bus_), so mixing it in would overwrite the cartridge's active-low
// IRQ assertion every cycle, preventing the CPU from ever seeing mapper
// IRQs (e.g. MMC3 scanline counter).
#define PPU_CPU_TRANSFER_MASK ( \
    BUS_BIT(BUS_RES_BIT) |   \
    BUS_BIT(BUS_NMI_BIT)     \
)

// Transfer shared signals from PPU bus onto CPU bus:
//   cpu_bus = PPU_CPU_BITMIX(cpu_bus, ppu_bus);
//
// XOR-AND-XOR form (3 ops) — same pattern as bitmix() in cermu.h.
// Zero bit shifting, zero conditionals.
#define PPU_CPU_BITMIX(cpu, ppu) \
    ((cpu) ^ (((cpu) ^ (ppu)) & PPU_CPU_TRANSFER_MASK))

// ============================================================================
// PPU BUS DEFAULT STATE
// ============================================================================
// Active-low signals start HIGH (inactive): /RD, /WR, /RES, /IRQ, /NMI
#define PPU_BUS_DEFAULT_STATE ( \
    (1ULL << PPU_BUS_RD_BIT)  | \
    (1ULL << PPU_BUS_WR_BIT)  | \
    BUS_BIT(BUS_RES_BIT)      | \
    BUS_BIT(BUS_IRQ_BIT)      | \
    BUS_BIT(BUS_NMI_BIT)        \
)

// ============================================================================
// COMPILE-TIME VALIDATION
// ============================================================================
// Ensure shared bits are at identical positions (this is the whole point of
// the aligned layout).  A mismatch here would silently corrupt IRQ/NMI/RES.
static_assert(BUS_RES_BIT == 32, "BUS_RES_BIT must be at position 32 for PPU bitmix");
static_assert(BUS_IRQ_BIT == 33, "BUS_IRQ_BIT must be at position 33 for PPU bitmix");
static_assert(BUS_NMI_BIT == 34, "BUS_NMI_BIT must be at position 34 for PPU bitmix");
