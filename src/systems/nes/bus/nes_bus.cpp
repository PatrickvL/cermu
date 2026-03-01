/*
 * nes_bus.cpp — NES bus implementation
 *
 * Initialisation, reset, and bank map update functions for nes_bus_t.
 * The hot-path read/write helpers are inline in nes_bus.h.
 */

#include "nes_bus.h"
#include "nes_bus_chips.h"

#include <cstring>

namespace nes_bus {

// ============================================================================
// Initialization
// ============================================================================

void nes_bus_t::init() {
    std::memset(cpu_ram, 0, sizeof(cpu_ram));
    std::memset(cpu_read_page, 0, sizeof(cpu_read_page));
    std::memset(cpu_write_page, 0, sizeof(cpu_write_page));
    std::memset(ppu_read_page, 0, sizeof(ppu_read_page));
    std::memset(ppu_write_page, 0, sizeof(ppu_write_page));

    dma_page = 0;
    dma_addr = 0;
    dma_data = 0;
    dma_transfer = false;
    dma_dummy = true;
    system_clock_counter = 0;
    cpu_div_ = 0;
    dma_odd_cycle_ = false;
}

// ============================================================================
// Reset — clears RAM and DMA state, preserves page pointer config
// ============================================================================

void nes_bus_t::reset() {
    std::memset(cpu_ram, 0, sizeof(cpu_ram));
    dma_page = 0;
    dma_addr = 0;
    dma_data = 0;
    dma_transfer = false;
    dma_dummy = true;
    system_clock_counter = 0;
    cpu_div_ = 0;
    dma_odd_cycle_ = false;
}

// ============================================================================
// CPU Bank Map Update
// ============================================================================

void nes_bus_t::update_cpu_banks(const nes_system::MapperBankConfig& config) {
    // Pages 0-1 ($0000-$1FFF): left as nullptr — WRAM fast path
    cpu_read_page[0]  = nullptr;
    cpu_read_page[1]  = nullptr;
    cpu_write_page[0] = nullptr;
    cpu_write_page[1] = nullptr;

    // Pages 2-3 ($2000-$3FFF): PPU registers → nullptr (I/O dispatch)
    cpu_read_page[2]  = nullptr;
    cpu_read_page[3]  = nullptr;
    cpu_write_page[2] = nullptr;
    cpu_write_page[3] = nullptr;

    // Page 4 ($4000-$4FFF): APU/IO registers → nullptr (I/O dispatch)
    cpu_read_page[4]  = nullptr;
    cpu_write_page[4] = nullptr;

    // Page 5 ($5000-$5FFF): Expansion (mapper-dependent)
    cpu_read_page[5]  = config.expansion_read;
    cpu_write_page[5] = config.expansion_write;

    // Pages 6-7 ($6000-$7FFF): PRG-RAM (if enabled)
    if (config.prg_ram_enabled && config.prg_ram_base != nullptr) {
        cpu_read_page[6] = config.prg_ram_base;
        cpu_read_page[7] = (config.prg_ram_size > 0x1000)
                           ? config.prg_ram_base + 0x1000
                           : config.prg_ram_base;  // mirror if <8KB

        if (!config.prg_ram_write_protected) {
            cpu_write_page[6] = config.prg_ram_base;
            cpu_write_page[7] = (config.prg_ram_size > 0x1000)
                                ? config.prg_ram_base + 0x1000
                                : config.prg_ram_base;
        } else {
            cpu_write_page[6] = nullptr;
            cpu_write_page[7] = nullptr;
        }
    } else {
        cpu_read_page[6]  = nullptr;  // open bus
        cpu_read_page[7]  = nullptr;
        cpu_write_page[6] = nullptr;
        cpu_write_page[7] = nullptr;
    }

    // Pages 8-15 ($8000-$FFFF): PRG-ROM banks from mapper config
    // cpu_write_page stays nullptr — ROM writes go to mapper register dispatch
    for (int i = 0; i < 8; i++) {
        cpu_read_page[8 + i] = config.prg_pages[i];
        cpu_write_page[8 + i] = nullptr;
    }
}

// ============================================================================
// PPU Bank Map Update
// ============================================================================

void nes_bus_t::update_ppu_banks(const nes_system::MapperChrConfig& config,
                                  uint8_t* ciram) {
    // Pages 0-7 ($0000-$1FFF): CHR-ROM/RAM banks
    for (int i = 0; i < 8; i++) {
        ppu_read_page[i] = config.chr_pages[i];
        ppu_write_page[i] = config.chr_writable[i]
                            ? const_cast<uint8_t*>(config.chr_pages[i])
                            : nullptr;
    }

    // Pages 8-11 ($2000-$2FFF): Nametable (CIRAM) with mirroring
    for (int i = 0; i < 4; i++) {
        uint8_t* nt = ciram + config.nt_page[i] * 0x0400;
        ppu_read_page[8 + i]  = nt;
        ppu_write_page[8 + i] = nt;
    }

    // Pages 12-15 ($3000-$3FFF): Mirror of $2000-$2FFF nametables
    // (Palette at $3F00-$3F1F is intercepted by PPU before page lookup)
    for (int i = 0; i < 4; i++) {
        ppu_read_page[12 + i]  = ppu_read_page[8 + i];
        ppu_write_page[12 + i] = ppu_write_page[8 + i];
    }
}

} // namespace nes_bus
