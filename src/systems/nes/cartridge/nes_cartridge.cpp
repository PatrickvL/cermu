/*
 * nes_cartridge.cpp — NES/Famicom Cartridge Implementation
 *
 * iNES ROM loading, CPU/PPU bus dispatch, mapper delegation,
 * and battery-backed SRAM persistence.
 */

#include "nes_cartridge.h"
#include "nes_mapper_factory.h"
#include "../bus/nes_bus.h"
#include "../nes_system.h"

#include <fstream>
#include <iostream>
#include <cstring>

namespace nes_system {

// ============================================================================
// Phase 2: Page-pointer bank map interface
// ============================================================================

void Cartridge::update_bank_map(nes_bus::nes_bus_t* bus, uint8_t* ciram) {
    if (!mapper || !bus) return;

    MapperBankConfig prg_config;
    mapper->get_prg_bank_config(prg_config);

    // Compatibility: if the mapper reports no PRG-RAM but we have an
    // allocated buffer, wire it in anyway.  Many test ROMs and homebrews
    // use the $6000 write-back protocol on cartridge types that lack
    // battery-backed SRAM (NROM, UxROM, CNROM).  Real hardware returns
    // open bus, but every major emulator (Mesen, FCEUX, Nestopia)
    // provides RAM here unconditionally.  The mapper stays hardware-
    // accurate; this shim lives at the cartridge/system layer.
    if (!prg_config.prg_ram_enabled && !prg_ram.empty()) {
        prg_config.prg_ram_base = prg_ram.data();
        prg_config.prg_ram_size = static_cast<uint32_t>(prg_ram.size());
        prg_config.prg_ram_enabled = true;
    }

    // Apply mirroring from header or mapper for nametable config
    MapperChrConfig chr_config;
    mapper->get_chr_bank_config(chr_config);

    // Override nametable mirroring based on current mirror mode
    Mirror m = mapper->mirror();
    mirror_mode = m;
    switch (m) {
        case Mirror::HORIZONTAL:   chr_config.nt_page[0] = 0; chr_config.nt_page[1] = 0; chr_config.nt_page[2] = 1; chr_config.nt_page[3] = 1; break;
        case Mirror::VERTICAL:     chr_config.nt_page[0] = 0; chr_config.nt_page[1] = 1; chr_config.nt_page[2] = 0; chr_config.nt_page[3] = 1; break;
        case Mirror::ONESCREEN_LO: chr_config.nt_page[0] = 0; chr_config.nt_page[1] = 0; chr_config.nt_page[2] = 0; chr_config.nt_page[3] = 0; break;
        case Mirror::ONESCREEN_HI: chr_config.nt_page[0] = 1; chr_config.nt_page[1] = 1; chr_config.nt_page[2] = 1; chr_config.nt_page[3] = 1; break;
        default: break;  // FOUR_SCREEN: use mapper's config
    }

    bus->update_cpu_banks(prg_config);
    bus->update_ppu_banks(chr_config, ciram);
}

bool Cartridge::handle_mapper_write(uint16_t addr, uint8_t data) {
    if (!mapper) return false;

    bool changed = mapper->register_write(addr, data);
    if (changed) {
        mirror_mode = mapper->mirror();
    }
    return changed;
}

// ============================================================================
// Cartridge IRQ / scanline / mirroring delegation
// ============================================================================

void Cartridge::irq_clear() {
    if (mapper) mapper->irq_clear();
}

void Cartridge::notify_a12(bool a12_high, uint64_t ppu_cycle) {
    if (mapper) mapper->notify_a12(a12_high, ppu_cycle);
}

// ============================================================================
// Battery-backed SRAM persistence
// ============================================================================

std::string Cartridge::sram_path_for_rom(const std::string& rom_path) const {
    // Replace .nes extension with .sav
    std::string sav = rom_path;
    auto dot = sav.rfind('.');
    if (dot != std::string::npos) {
        sav = sav.substr(0, dot);
    }
    sav += ".sav";
    return sav;
}

bool Cartridge::load_sram(const std::string& sav_path) {
    if (prg_ram.empty()) return false;
    std::ifstream f(sav_path, std::ios::binary);
    if (!f.is_open()) return false;
    f.read(reinterpret_cast<char*>(prg_ram.data()),
           static_cast<std::streamsize>(prg_ram.size()));
    printf("NES: Loaded SRAM from %s (%zu bytes)\n", sav_path.c_str(), prg_ram.size());
    return true;
}

bool Cartridge::save_sram(const std::string& sav_path) const {
    if (prg_ram.empty() || !battery_backed) return false;
    std::ofstream f(sav_path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(prg_ram.data()),
            static_cast<std::streamsize>(prg_ram.size()));
    printf("NES: Saved SRAM to %s (%zu bytes)\n", sav_path.c_str(), prg_ram.size());
    return true;
}

// ============================================================================
// iNES ROM Loading
// ============================================================================

bool Cartridge::load_from_buffer(const uint8_t* data, size_t data_size,
                                  const std::string& filepath_for_sram) {
    if (!data || data_size < sizeof(Header)) {
        return false;
    }

    Header header;
    memcpy(&header, data, sizeof(Header));
    size_t offset = sizeof(Header);

    // Verify iNES header
    if (header.name[0] != 'N' || header.name[1] != 'E' ||
        header.name[2] != 'S' || header.name[3] != 0x1A) {
        return false;
    }

    // Extract mapper ID
    mapper_id = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);
    mirror_mode = (header.mapper1 & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
    battery_backed = (header.mapper1 & 0x02) != 0;

    prg_banks = header.prg_rom_chunks;
    chr_banks = header.chr_rom_chunks;

    // Skip trainer if present
    if (header.mapper1 & 0x04) {
        offset += 512;
    }

    // Load PRG ROM
    uint32_t prg_size = prg_banks * 16384;
    if (offset + prg_size > data_size) {
        return false;
    }
    prg_memory.resize(prg_size);
    memcpy(prg_memory.data(), data + offset, prg_size);
    offset += prg_size;

    // Load CHR ROM/RAM
    if (chr_banks == 0) {
        // CHR RAM
        chr_memory.resize(nes_constants::INES_CHR_BANK_SIZE, 0);
    } else {
        // CHR ROM
        uint32_t chr_size = chr_banks * nes_constants::INES_CHR_BANK_SIZE;
        if (offset + chr_size > data_size) {
            return false;
        }
        chr_memory.resize(chr_size);
        memcpy(chr_memory.data(), data + offset, chr_size);
    }

    // Allocate PRG RAM (8KB, used by MMC1/MMC3 and others)
    uint32_t prg_ram_size = header.prg_ram_size ? header.prg_ram_size * nes_constants::INES_PRG_RAM_DEFAULT : nes_constants::INES_PRG_RAM_DEFAULT;
    prg_ram.resize(prg_ram_size, 0);

    // Create appropriate mapper via factory
    mapper = MapperFactory::create(mapper_id, prg_banks, chr_banks);

    // Phase 2: Give mapper direct pointers into our ROM/RAM vectors
    mapper->set_memory_pointers(
        prg_memory.data(), prg_memory.size(),
        chr_memory.data(), chr_memory.size(),
        chr_banks == 0,  // chr_is_ram: true when no CHR-ROM (CHR-RAM mode)
        prg_ram.data(), prg_ram.size()
    );

    // Propagate iNES header mirroring to mapper (used by the default
    // mirror() for mappers that don't dynamically change mirroring).
    mapper->set_header_mirror(mirror_mode);

    // Store ROM path for SRAM persistence
    rom_filepath_ = filepath_for_sram;

    // Load battery-backed SRAM if present
    if (battery_backed) {
        load_sram(sram_path_for_rom(filepath_for_sram));
    }

    return true;
}

// ============================================================================
// PPU Bus Interface
// ============================================================================

// ============================================================================
// Reset / Debug
// ============================================================================

void Cartridge::reset() {
    if (mapper) {
        mapper->reset();
        mirror_mode = mapper->mirror();
    }
}

uint8_t Cartridge::peek(uint16_t addr) const {
    // Block dispatch handles all standard reads via unified buffer.
    // This method is only called as a fallback when block dispatch
    // returns a sentinel.  In normal operation, PRG-ROM/RAM are block-mapped.
    (void)addr;
    return 0;
}

} // namespace nes_system
