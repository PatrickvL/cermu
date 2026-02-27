/*
 * nes_cartridge.cpp — NES/Famicom Cartridge Implementation
 *
 * iNES ROM loading, CPU/PPU bus dispatch, mapper delegation,
 * and battery-backed SRAM persistence.
 */

#include "nes_cartridge.h"
#include "nes_mapper_factory.h"

#include <fstream>
#include <iostream>
#include <cstring>

namespace nes_system {

// ============================================================================
// Cartridge IRQ / scanline / mirroring delegation
// ============================================================================

bool Cartridge::irq_state() const {
    return mapper ? mapper->irq_state() : false;
}

void Cartridge::irq_clear() {
    if (mapper) mapper->irq_clear();
}

void Cartridge::scanline() {
    if (mapper) mapper->scanline();
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
        chr_memory.resize(8192, 0);
    } else {
        // CHR ROM
        uint32_t chr_size = chr_banks * 8192;
        if (offset + chr_size > data_size) {
            return false;
        }
        chr_memory.resize(chr_size);
        memcpy(chr_memory.data(), data + offset, chr_size);
    }

    // Allocate PRG RAM (8KB, used by MMC1/MMC3 and others)
    uint32_t prg_ram_size = header.prg_ram_size ? header.prg_ram_size * 8192 : 8192;
    prg_ram.resize(prg_ram_size, 0);

    // Create appropriate mapper via factory
    mapper = create_mapper(mapper_id, prg_banks, chr_banks);

    // Store ROM path for SRAM persistence
    rom_filepath_ = filepath_for_sram;

    // Load battery-backed SRAM if present
    if (battery_backed) {
        load_sram(sram_path_for_rom(filepath_for_sram));
    }

    return true;
}

// ============================================================================
// CPU Bus Interface
// ============================================================================

bus_state_t Cartridge::cpu_bus_tick(bus_state_t bus, bool& handled) {
    const uint16_t addr   = BUS_GET_ADDR(bus);
    const bool     is_read = BUS_GET_BIT(bus, BUS_RW_BIT);
    handled = false;

    if (is_read) {
        // ---- READ ----
        uint32_t mapped_addr;
        if (mapper->cpu_map_read(addr, mapped_addr)) {
            if (mapped_addr == 0xFFFFFFFF) {
                // PRG RAM ($6000-$7FFF)
                uint16_t ram_offset = addr & 0x1FFF;
                if (ram_offset < prg_ram.size()) {
                    BUS_SET_DATA(bus, prg_ram[ram_offset]);
                }
                handled = true;
                return bus;
            }
            if (mapped_addr < prg_memory.size()) {
                BUS_SET_DATA(bus, prg_memory[mapped_addr]);
                handled = true;
                return bus;
            }
        }
    } else {
        // ---- WRITE ----
        uint8_t data = BUS_GET_DATA(bus);
        uint32_t mapped_addr;
        if (mapper->cpu_map_write(addr, mapped_addr, data)) {
            if (mapped_addr == 0xFFFFFFFF) {
                // PRG RAM ($6000-$7FFF)
                uint16_t ram_offset = addr & 0x1FFF;
                if (ram_offset < prg_ram.size()) {
                    prg_ram[ram_offset] = data;
                }
                handled = true;
                return bus;
            }
            if (mapped_addr < prg_memory.size()) {
                prg_memory[mapped_addr] = data;
                handled = true;
                return bus;
            }
        }
        // Even if mapper returned false, the write may have updated mapper state
        // (e.g. MMC1 shift register, UxROM bank select).  Update mirroring.
        mirror_mode = mapper->mirror();
    }

    return bus;
}

// ============================================================================
// PPU Bus Interface
// ============================================================================

bool Cartridge::ppu_read(uint16_t addr, uint8_t& data) {
    uint32_t mapped_addr;
    if (mapper->ppu_map_read(addr, mapped_addr)) {
        if (mapped_addr < chr_memory.size()) {
            data = chr_memory[mapped_addr];
            return true;
        }
    }
    return false;
}

bool Cartridge::ppu_write(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr;
    if (mapper->ppu_map_write(addr, mapped_addr)) {
        if (mapped_addr < chr_memory.size()) {
            chr_memory[mapped_addr] = data;
            return true;
        }
    }
    return false;
}

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
    if (!mapper) return 0;
    uint32_t mapped_addr;
    // cpu_map_read is logically const for all mappers (no state mutation)
    if (const_cast<Mapper*>(mapper.get())->cpu_map_read(addr, mapped_addr)) {
        if (mapped_addr == 0xFFFFFFFF) {
            uint16_t ram_offset = addr & 0x1FFF;
            if (ram_offset < prg_ram.size()) return prg_ram[ram_offset];
            return 0;
        }
        if (mapped_addr < prg_memory.size()) return prg_memory[mapped_addr];
    }
    return 0;
}

} // namespace nes_system
