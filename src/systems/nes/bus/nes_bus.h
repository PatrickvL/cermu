#pragma once
/*
 * nes_bus.h — NES unified bus structure with page-pointer dispatch
 *
 * Replaces the old cascading if-else MemoryBus::mem_tick() with
 * precomputed page pointer tables.  Memory reads and writes resolve
 * to a single pointer dereference for the common case (RAM, ROM).
 *
 * CPU address space: 16 × 4KB pages ($0000-$FFFF)
 *   Pages 0-1 ($0000-$1FFF): WRAM handled by fast path (not page pointers)
 *   Pages 2-3 ($2000-$3FFF): nullptr → PPU register I/O dispatch
 *   Page 4   ($4000-$4FFF): nullptr → APU/IO register dispatch
 *   Page 5   ($5000-$5FFF): expansion (mapper-dependent, usually nullptr)
 *   Pages 6-7 ($6000-$7FFF): PRG-RAM (mapper-dependent)
 *   Pages 8-15 ($8000-$FFFF): PRG-ROM banks (mapper-configured)
 *
 * PPU address space: 16 × 1KB pages ($0000-$3FFF)
 *   Pages 0-7  ($0000-$1FFF): CHR-ROM/RAM banks
 *   Pages 8-11 ($2000-$2FFF): Nametable (CIRAM) with mirroring
 *   Pages 12-15 ($3000-$3FFF): Mirror of $2000-$2FFF
 *   Palette ($3F00-$3F1F): special-cased in PPU, not through page pointers
 *
 * See NES_MIGRATION_PLAN.md Part 2 for the full rationale.
 */

#include <cstdint>
#include <cstring>

#include "../../../core/cermu.h"         // likely/unlikely
#include "../../../core/system_lines.h"  // bus_state_t, BUS_* macros
#include "nes_bus_signals.h"             // ppu_bus_state_t, PPU_BUS_* macros
#include "../cartridge/nes_mapper.h"     // MapperBankConfig, MapperChrConfig

namespace nes_bus {

// ============================================================================
// BUS SIZING CONSTANTS
// ============================================================================

inline constexpr uint32_t CPU_PAGE_COUNT = 16;     // 16 × 4KB CPU pages
inline constexpr uint32_t PPU_PAGE_COUNT = 16;     // 16 × 1KB PPU pages

// ============================================================================
// NES BUS STRUCTURE
// ============================================================================

struct nes_bus_t {
    // ====================================================================
    // CPU internal RAM — 2KB, accessed directly for $0000-$1FFF fast path
    // ====================================================================
    alignas(64) uint8_t cpu_ram[2048];

    // ====================================================================
    // CPU page pointer tables — 16 × 4KB pages covering $0000-$FFFF
    // ====================================================================
    //
    // Pages 0-1: unused (WRAM handled by fast path before page lookup)
    // Pages 2-4: nullptr (I/O registers — PPU, APU, controllers)
    // Pages 5-7: expansion / PRG-RAM (mapper-dependent)
    // Pages 8-15: PRG-ROM banks
    //
    // Read:  if (ptr) return ptr[addr & 0x0FFF];
    // Write: if (ptr) ptr[addr & 0x0FFF] = data;
    //        if (!ptr && page >= 8) → mapper register write
    //        if (!ptr && page < 8) → I/O dispatch
    //
    alignas(64) const uint8_t* cpu_read_page[CPU_PAGE_COUNT];
    alignas(64) uint8_t* cpu_write_page[CPU_PAGE_COUNT];

    // ====================================================================
    // PPU page pointer tables — 16 × 1KB pages covering $0000-$3FFF
    // ====================================================================
    //
    // Pages 0-7:   CHR-ROM/RAM (mapper-configured banking)
    // Pages 8-11:  Nametable (CIRAM, mirroring via page pointers)
    // Pages 12-15: Mirror of pages 8-11
    // Palette at $3F00-$3F1F is intercepted by PPU before page lookup.
    //
    alignas(64) const uint8_t* ppu_read_page[PPU_PAGE_COUNT];
    alignas(64) uint8_t* ppu_write_page[PPU_PAGE_COUNT];

    // ====================================================================
    // OAM DMA controller state
    // ====================================================================
    uint8_t dma_page = 0;
    uint8_t dma_addr = 0;
    uint8_t dma_data = 0;
    bool dma_transfer = false;
    bool dma_dummy = true;

    // ====================================================================
    // System clock counter (incremented every PPU tick)
    // ====================================================================
    uint32_t system_clock_counter = 0;

    // ====================================================================
    // Initialization / reset
    // ====================================================================

    /// Zero all buffers and page pointers.
    void init();

    /// Reset RAM and DMA state (preserves page pointer configuration).
    void reset();

    // ====================================================================
    // Bank map update — called after mapper register writes
    // ====================================================================

    /// Set CPU page pointers from mapper's PRG bank configuration.
    /// Pages 0-1 are left as nullptr (WRAM fast path handles them).
    /// Pages 2-4 are left as nullptr (I/O dispatch).
    /// Pages 5-7: expansion / PRG-RAM from config.
    /// Pages 8-15: PRG-ROM banks from config.
    void update_cpu_banks(const nes_system::MapperBankConfig& config);

    /// Set PPU page pointers from mapper's CHR bank configuration.
    /// ciram points to the PPU's 2KB nametable VRAM.
    /// Pages 0-7: CHR banks from config.
    /// Pages 8-15: nametable mirroring from config.nt_page[].
    void update_ppu_banks(const nes_system::MapperChrConfig& config, uint8_t* ciram);

    // ====================================================================
    // Inline CPU read helper — for DMA and debug peek
    // ====================================================================
    //
    // Does NOT handle I/O dispatch (PPU regs, APU, controllers).
    // Returns open bus (0xFF) for nullptr pages.

    inline uint8_t cpu_read(uint16_t addr) const {
        if (addr < 0x2000) {
            return cpu_ram[addr & 0x07FF];
        }
        const uint8_t* rp = cpu_read_page[addr >> 12];
        if (likely(rp != nullptr)) {
            return rp[addr & 0x0FFF];
        }
        return 0xFF;  // open bus
    }

    // ====================================================================
    // Inline PPU read/write — bus_state_t receiving/returning pattern
    // ====================================================================
    //
    // Palette ($3F00-$3F1F) must be intercepted BEFORE calling these.

    inline ppu_bus_state_t ppu_read(ppu_bus_state_t bus) const {
        uint16_t mapped = PPU_BUS_GET_ADDR(bus) & 0x3FFF;
        const uint8_t* rp = ppu_read_page[mapped >> 10];
        if (likely(rp != nullptr)) {
            PPU_BUS_SET_DATA(bus, rp[mapped & 0x03FF]);
        }
        return bus;
    }

    inline ppu_bus_state_t ppu_write(ppu_bus_state_t bus) {
        uint16_t mapped = PPU_BUS_GET_ADDR(bus) & 0x3FFF;
        uint8_t* wp = ppu_write_page[mapped >> 10];
        if (likely(wp != nullptr)) {
            wp[mapped & 0x03FF] = PPU_BUS_GET_DATA(bus);
        }
        return bus;
    }
};

} // namespace nes_bus
