/*
 * nes_cartridge.cpp — NES/Famicom Cartridge Implementation
 *
 * iNES ROM loading, CPU/PPU bus dispatch, mapper delegation,
 * and battery-backed SRAM persistence.
 */

#include "core/cermu.hpp"
#include "systems/nes/cartridge/nes_cartridge.hpp"
#include "systems/nes/cartridge/nes_mapper_factory.hpp"
#include "systems/nes/bus/nes_bus.hpp"
#include "systems/nes/nes_system.hpp"

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
    //
    // Use the bus's flat-mem copy of PRG-RAM (not the Cartridge vector),
    // because ptr_to_block() can only resolve pointers inside flat_mem.
    if (!prg_config.prg_ram_enabled && bus->prg_ram_size > 0) {
        prg_config.prg_ram_base = bus->prg_ram;
        prg_config.prg_ram_size = bus->prg_ram_size;
        prg_config.prg_ram_enabled = true;
    }

    // Apply mirroring from header or mapper for nametable config
    MapperChrConfig chr_config;
    mapper->get_chr_bank_config(chr_config);

    // Override nametable mirroring from the authoritative mirror mode.
    // FOUR_SCREEN keeps the mapper's config (needs 4KB on-cart VRAM).
    Mirror m = mapper->mirror();
    mirror_mode = m;
    if (m != Mirror::FOUR_SCREEN) {
        const auto& nt = MIRROR_NT_PAGES[static_cast<int>(m)];
        std::memcpy(chr_config.nt_page, nt, sizeof(chr_config.nt_page));
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
    log_info("NES: Loaded SRAM from %s (%zu bytes)\n", sav_path.c_str(), prg_ram.size());
    return true;
}

bool Cartridge::save_sram(const std::string& sav_path) const {
    if (prg_ram.empty() || !battery_backed) return false;
    std::ofstream f(sav_path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(prg_ram.data()),
            static_cast<std::streamsize>(prg_ram.size()));
    log_info("NES: Saved SRAM to %s (%zu bytes)\n", sav_path.c_str(), prg_ram.size());
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

    // ====================================================================
    // Archaic iNES header sanitization
    // ====================================================================
    // Many older ROM dumps have garbage in bytes 7-15 (DiskDude!, various
    // copier tool signatures).  The standard heuristic used by FCEUX,
    // Mesen, and Nestopia: if ANY of bytes 12-15 are non-zero, the upper
    // nibble of the mapper (from byte 7) cannot be trusted.  Zero out
    // bytes 7-15 so only byte 6 contributes to the mapper ID.
    //
    // NES 2.0 is identified by (flags7 & 0x0C) == 0x08 and is exempt.
    bool archaic_ines = false;
    if ((header.mapper2 & 0x0C) != 0x08) {
        if (header.unused[1] != 0 || header.unused[2] != 0 ||
            header.unused[3] != 0 || header.unused[4] != 0) {
            archaic_ines = true;
            log_info("NES: Archaic iNES header detected (garbage in bytes 12-15) — "
                   "sanitizing mapper from %d", ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4));
            header.mapper2 = 0;
            header.prg_ram_size = 0;
            header.tv_system1 = 0;
            header.tv_system2 = 0;
            memset(header.unused, 0, sizeof(header.unused));
            log_info(" to %d\n", (header.mapper1 >> 4));
        }
    }

    // Extract mapper ID and sizes
    bool is_nes20 = (header.mapper2 & 0x0C) == 0x08;
    mirror_mode = (header.mapper1 & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
    battery_backed = (header.mapper1 & 0x02) != 0;

    if (is_nes20) {
        // NES 2.0: 12-bit mapper ID + 4-bit submapper
        mapper_id = ((header.prg_ram_size & 0x0F) << 8)
                     | ((header.mapper2 >> 4) << 4)
                     | (header.mapper1 >> 4);
        submapper = (header.prg_ram_size >> 4) & 0x0F;

        // Extended PRG/CHR sizes (byte 9: D3-D0 = PRG MSB, D7-D4 = CHR MSB)
        uint16_t prg_msb = header.tv_system1 & 0x0F;
        uint16_t chr_msb = (header.tv_system1 >> 4) & 0x0F;
        prg_banks = header.prg_rom_chunks | (prg_msb << 8);
        chr_banks = header.chr_rom_chunks | (chr_msb << 8);

        log_info("NES 2.0: mapper %d submapper %d, %d×16KB PRG, %d×8KB CHR\n",
                 mapper_id, submapper, prg_banks, chr_banks);
    } else {
        mapper_id = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);
        submapper = 0;
        prg_banks = header.prg_rom_chunks;
        chr_banks = header.chr_rom_chunks;
    }

    // ====================================================================
    // Mapper inference heuristics for sanitized headers
    // ====================================================================
    // After archaic header cleanup, some ROMs end up as mapper 0 (NROM) but
    // have CHR sizes incompatible with NROM (max 8KB).  Infer the correct
    // mapper from ROM geometry:
    //   - Mapper 0 + CHR > 8KB → Mapper 3 (CNROM, simple CHR bank switch)
    if (archaic_ines && mapper_id == 0 && chr_banks > 1) {
        log_info("NES: Mapper 0 with %dKB CHR — inferring mapper 3 (CNROM)\n",
               chr_banks * 8);
        mapper_id = 3;
    }

    // Skip trainer if present (save pointer for later loading into PRG-RAM)
    const uint8_t* trainer_data = nullptr;
    if (header.mapper1 & 0x04) {
        if (offset + 512 <= data_size) {
            trainer_data = data + offset;
        }
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

    // Load trainer data into PRG-RAM at $7000-$71FF (offset $1000 in the
    // 8KB RAM mapped at $6000).  Some multicart and pirate ROMs have their
    // reset vector pointing into the trainer.
    if (trainer_data && prg_ram_size >= 0x1200) {
        memcpy(prg_ram.data() + 0x1000, trainer_data, 512);
    }

    // Create appropriate mapper via factory
    mapper = MapperFactory::create(mapper_id, prg_banks, chr_banks);

    // Some mappers need extra CHR-RAM alongside CHR-ROM (e.g. mapper 077
    // has 6KB on-board CHR-RAM for pattern table $0800-$1FFF).
    // Extend chr_memory so the extra RAM lives in flat_mem and
    // ptr_to_block() can resolve pointers into it.
    uint32_t extra_chr = mapper->extra_chr_ram_size();
    if (extra_chr > 0) {
        chr_memory.resize(chr_memory.size() + extra_chr, 0);
    }

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
    mapper->set_submapper(submapper);

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

// ============================================================================
// ChipDebugRegistry
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void Cartridge::register_debug_fields() {
    static const char* const mirror_names[] = {
        "Horizontal", "Vertical", "One-Screen (Lo)",
        "One-Screen (Hi)", "Four-Screen"
    };

    using C = const Cartridge;
    debug_registry_
        .category("Cartridge Info")
        .value("Mapper", +[](const ChipBase* c) -> uint32_t { return static_cast<C*>(c)->mapper_id; })
        .value("PRG Banks", +[](const ChipBase* c) -> uint32_t { return static_cast<C*>(c)->prg_banks; })
        .value("CHR Banks", +[](const ChipBase* c) -> uint32_t { return static_cast<C*>(c)->chr_banks; })
        .flag("Battery Backed", +[](const ChipBase* c) -> uint32_t { return static_cast<C*>(c)->battery_backed; })

        .category("Memory")
        .value("PRG ROM (bytes)", +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<C*>(c)->prg_memory.size());
        })
        .value("CHR Size (bytes)", +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<C*>(c)->chr_memory.size());
        })
        .value("PRG RAM (bytes)", +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<C*>(c)->prg_ram.size());
        })

        .category("Mirroring")
        .state("Mode", +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<C*>(c)->mirror_mode);
        }, mirror_names, 5)

        .category("Mapper State")
        .flag("IRQ Active", +[](const ChipBase* c) -> uint32_t {
            auto* self = static_cast<C*>(c);
            return self->mapper && self->mapper->irq_state();
        })
        .flag("CHR is RAM", +[](const ChipBase* c) -> uint32_t {
            auto* self = static_cast<C*>(c);
            return self->mapper && self->mapper->chr_is_ram();
        });
}
#endif // CERMU_HAS_CHIP_DEBUG

} // namespace nes_system
