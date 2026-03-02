/*
 * nes_bus.cpp -- NES bus implementation
 *
 * Unified buffer allocation, bank map update, and reset logic.
 * The hot-path read/write helpers are inline in nes_bus.h.
 */

#include "nes_bus.h"
#include "nes_bus_chips.h"

#include <algorithm>
#include <cstring>

namespace nes_bus {

// ============================================================================
// Destructor -- free unified buffer
// ============================================================================

nes_bus_t::~nes_bus_t() {
    delete[] unified_buf;
    unified_buf = nullptr;
    cpu_ram = nullptr;
    ciram = nullptr;
    prg_ram = nullptr;
    chr_data_ptr = nullptr;
    prg_rom_ptr = nullptr;
}

// ============================================================================
// Initialization -- allocate fixed-region buffer (WRAM + CIRAM + PRG-RAM)
// ============================================================================

void nes_bus_t::init() {
    // Free any existing buffer (e.g. from a previous cartridge load)
    delete[] unified_buf;

    // Allocate fixed region: WRAM + CIRAM + reserved + PRG-RAM
    unified_buf_size = FIXED_SIZE;
    unified_buf = new uint8_t[unified_buf_size]();  // zero-init
    total_blocks = FIXED_BLOCKS;

    // Set convenience pointers into fixed region
    cpu_ram = unified_buf + BLOCK_WRAM * BLOCK_SIZE;
    ciram = unified_buf + BLOCK_CIRAM * BLOCK_SIZE;
    prg_ram = unified_buf + BLOCK_PRG_RAM * BLOCK_SIZE;
    prg_ram_size = 0;
    chr_data_ptr = nullptr;
    prg_rom_ptr = nullptr;
    chr_data_size = 0;
    chr_is_ram = false;
    prg_rom_size = 0;
    chr_base_block = 0;
    prg_rom_base_block = 0;

    // Block arrays -- mapper-dependent pages default to open bus
    std::fill(cpu_read_block,  cpu_read_block  + CPU_PAGE_COUNT, BLOCK_OPEN_BUS);
    std::fill(cpu_write_block, cpu_write_block + CPU_PAGE_COUNT, BLOCK_OPEN_BUS);
    std::fill(ppu_read_block,  ppu_read_block  + PPU_PAGE_COUNT, BLOCK_OPEN_BUS);
    std::fill(ppu_write_block, ppu_write_block + PPU_PAGE_COUNT, BLOCK_OPEN_BUS);

    // Hardware-invariant CPU pages (never change after init)
    // Pages 0-1 ($0000-$1FFF): WRAM
    cpu_read_block[0]  = BLOCK_WRAM;  cpu_read_block[1]  = BLOCK_WRAM;
    cpu_write_block[0] = BLOCK_WRAM;  cpu_write_block[1] = BLOCK_WRAM;
    // Pages 2-3 ($2000-$3FFF): PPU registers -> I/O dispatch
    cpu_read_block[2]  = BLOCK_PPU_REGS;  cpu_read_block[3]  = BLOCK_PPU_REGS;
    cpu_write_block[2] = BLOCK_PPU_REGS;  cpu_write_block[3] = BLOCK_PPU_REGS;
    // Page 4 ($4000-$4FFF): APU/IO registers -> I/O dispatch
    cpu_read_block[4]  = BLOCK_APU_IO;
    cpu_write_block[4] = BLOCK_APU_IO;

    // (DMA + clock state now lives in NintendoSystem.)
}

// ============================================================================
// Unified buffer extension -- allocate space for cartridge ROM/RAM
// ============================================================================

void nes_bus_t::init_unified_buffer(const uint8_t* prg_rom_data, size_t prg_rom_sz,
                                     const uint8_t* chr_data_in, size_t chr_sz,
                                     bool chr_is_ram_in,
                                     const uint8_t* prg_ram_data, size_t prg_ram_sz) {
    // Round up to whole blocks
    uint32_t chr_blocks = static_cast<uint32_t>((chr_sz + BLOCK_SIZE - 1) / BLOCK_SIZE);
    uint32_t prg_blocks = static_cast<uint32_t>((prg_rom_sz + BLOCK_SIZE - 1) / BLOCK_SIZE);

    total_blocks = BLOCK_DYNAMIC + chr_blocks + prg_blocks;
    uint32_t new_size = total_blocks * BLOCK_SIZE;

    // Preserve fixed-region data (WRAM may have been written to by NSF stubs
    // or test code between init() and this call)
    uint8_t saved_fixed[FIXED_SIZE];
    if (unified_buf) {
        std::memcpy(saved_fixed, unified_buf, FIXED_SIZE);
    } else {
        std::memset(saved_fixed, 0, FIXED_SIZE);
    }

    // Reallocate
    delete[] unified_buf;
    unified_buf = new uint8_t[new_size]();  // zero-init
    unified_buf_size = new_size;

    // Restore fixed region
    std::memcpy(unified_buf, saved_fixed, FIXED_SIZE);

    // Convenience pointers -- fixed region
    cpu_ram = unified_buf + BLOCK_WRAM * BLOCK_SIZE;
    ciram = unified_buf + BLOCK_CIRAM * BLOCK_SIZE;
    prg_ram = unified_buf + BLOCK_PRG_RAM * BLOCK_SIZE;
    prg_ram_size = static_cast<uint32_t>(std::min(prg_ram_sz, static_cast<size_t>(PRG_RAM_MAX)));

    // Copy PRG-RAM initial data (from cartridge SRAM load)
    if (prg_ram_data && prg_ram_sz > 0) {
        std::memcpy(prg_ram, prg_ram_data, std::min(prg_ram_sz, static_cast<size_t>(PRG_RAM_MAX)));
    }

    // Dynamic region: CHR data
    chr_base_block = BLOCK_DYNAMIC;
    chr_data_ptr = unified_buf + chr_base_block * BLOCK_SIZE;
    if (chr_data_in && chr_sz > 0) {
        std::memcpy(chr_data_ptr, chr_data_in, chr_sz);
    }
    chr_data_size = static_cast<uint32_t>(chr_sz);
    chr_is_ram = chr_is_ram_in;

    // Dynamic region: PRG-ROM
    prg_rom_base_block = chr_base_block + chr_blocks;
    uint8_t* prg_buf = unified_buf + prg_rom_base_block * BLOCK_SIZE;
    if (prg_rom_data && prg_rom_sz > 0) {
        std::memcpy(prg_buf, prg_rom_data, prg_rom_sz);
    }
    prg_rom_ptr = prg_buf;
    prg_rom_size = static_cast<uint32_t>(prg_rom_sz);
}

// ============================================================================
// Reset -- DMA state only; preserves page pointer / block configuration.
// Does NOT clear cpu_ram -- real NES preserves RAM across soft reset.
// ============================================================================

void nes_bus_t::reset() {
    // DMA + clock state now lives in NintendoSystem.
    // Bus reset is a no-op; kept for future bank-map reset if needed.
}

// ============================================================================
// CPU Bank Map Update
// ============================================================================

void nes_bus_t::update_cpu_banks(const nes_system::MapperBankConfig& config) {
    // Pages 0-4 are hardware-invariant (set once in init()).

    // Page 5 ($5000-$5FFF): Expansion (mapper-dependent)
    cpu_read_block[5]  = ptr_to_block(config.expansion_read);
    cpu_write_block[5] = ptr_to_block(config.expansion_write);

    // Pages 6-7 ($6000-$7FFF): PRG-RAM (if enabled)
    if (config.prg_ram_enabled && config.prg_ram_base != nullptr) {
        const uint8_t* base = config.prg_ram_base;
        const uint8_t* hi_page = (config.prg_ram_size > 0x1000)
                                 ? base + 0x1000 : base;

        cpu_read_block[6] = ptr_to_block(base);
        cpu_read_block[7] = ptr_to_block(hi_page);

        if (!config.prg_ram_write_protected) {
            cpu_write_block[6] = ptr_to_block(base);
            cpu_write_block[7] = ptr_to_block(hi_page);
        } else {
            cpu_write_block[6] = BLOCK_OPEN_BUS;
            cpu_write_block[7] = BLOCK_OPEN_BUS;
        }
    } else {
        cpu_read_block[6]  = BLOCK_OPEN_BUS;
        cpu_read_block[7]  = BLOCK_OPEN_BUS;
        cpu_write_block[6] = BLOCK_OPEN_BUS;
        cpu_write_block[7] = BLOCK_OPEN_BUS;
    }

    // Pages 8-15 ($8000-$FFFF): PRG-ROM banks from mapper config
    // Write blocks default to BLOCK_OPEN_BUS (ROM writes go to mapper dispatch)
    // unless prg_write_pages provides writable pointers (e.g. NSF self-modify).
    for (int i = 0; i < 8; i++) {
        cpu_read_block[8 + i]  = ptr_to_block(config.prg_pages[i]);
        cpu_write_block[8 + i] = config.prg_write_pages[i]
                                ? ptr_to_block(config.prg_write_pages[i])
                                : BLOCK_OPEN_BUS;
    }
}

// ============================================================================
// PPU Bank Map Update
// ============================================================================

void nes_bus_t::update_ppu_banks(const nes_system::MapperChrConfig& config,
                                  uint8_t* ciram_ptr) {
    // Pages 0-7 ($0000-$1FFF): CHR-ROM/RAM banks
    for (int i = 0; i < 8; i++) {
        ppu_read_block[i]  = ptr_to_block(config.chr_pages[i]);
        ppu_write_block[i] = config.chr_writable[i]
                            ? ptr_to_block(config.chr_pages[i])
                            : BLOCK_OPEN_BUS;
    }

    // Pages 8-11 ($2000-$2FFF): Nametable (CIRAM) with mirroring
    for (int i = 0; i < 4; i++) {
        uint8_t* nt = ciram_ptr + config.nt_page[i] * 0x0400;
        uint16_t block = ptr_to_block(nt);
        ppu_read_block[8 + i]  = block;
        ppu_write_block[8 + i] = block;
    }

    // Pages 12-15 ($3000-$3FFF): Mirror of $2000-$2FFF nametables
    for (int i = 0; i < 4; i++) {
        ppu_read_block[12 + i]  = ppu_read_block[8 + i];
        ppu_write_block[12 + i] = ppu_write_block[8 + i];
    }
}

} // namespace nes_bus
