#pragma once
/*
 * nes_cartridge.h — NES/Famicom Cartridge
 *
 * Manages PRG-ROM, CHR-ROM/RAM, PRG-RAM, mapper dispatch, battery-backed
 * SRAM persistence, and nametable mirroring.  The cartridge sits on both
 * the CPU bus ($4020–$FFFF) and the PPU bus ($0000–$1FFF CHR space).
 *
 * Mapper implementations live in cartridge/mappers/ as header-only types
 * instantiated by the factory in nes_mapper_factory.h.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../../core/chip.h"           // ChipBase, ChipInfo
#include "../bus/nes_bus.h"              // nes_bus_t, block dispatch, PPU_PAGE_SHIFT
#include "../bus/nes_bus_signals.h"      // ppu_bus_state_t, PPU_BUS_* macros
#include "nes_mapper.h"                  // Mirror, Mapper, MapperBankConfig, MapperChrConfig

namespace nes_system {

class Cartridge : public ChipBase {
public:
    // iNES header structure
    struct Header {
        char name[4];           // "NES" + 0x1A
        uint8_t prg_rom_chunks; // Size of PRG ROM in 16KB chunks
        uint8_t chr_rom_chunks; // Size of CHR ROM in 8KB chunks
        uint8_t mapper1;        // Mapper, mirroring, battery, trainer
        uint8_t mapper2;        // Mapper, VS/Playchoice, NES 2.0
        uint8_t prg_ram_size;   // Size of PRG RAM in 8KB chunks
        uint8_t tv_system1;     // TV system (0=NTSC, 1=PAL)
        uint8_t tv_system2;     // TV system, PRG-RAM presence
        char unused[5];         // Unused padding
    };

    // Memory banks
    std::vector<uint8_t> prg_memory;  // Program ROM
    std::vector<uint8_t> chr_memory;  // Character ROM/RAM
    std::vector<uint8_t> prg_ram;     // Program RAM (battery backed)

    // Cartridge info
    uint8_t mapper_id = 0;
    uint8_t prg_banks = 0;
    uint8_t chr_banks = 0;
    bool battery_backed = false;

    /** Protected default constructor — allows direct construction for
     *  programmatic cartridge setup (e.g. NSF player). */
    Cartridge()
        : ChipBase(ChipInfo{"Cartridge", "iNES ROM Cartridge", "Various"}) {}

public:
    virtual ~Cartridge() = default;

    /** Parse iNES ROM from an already-loaded buffer.
     *  filepath_for_sram is stored for battery-backed SRAM persistence. */
    bool load_from_buffer(const uint8_t* data, size_t data_size,
                          const std::string& filepath_for_sram);

    // ====================================================================
    // Phase 2: Page-pointer bank map interface
    // ====================================================================

    /// Update nes_bus_t page pointers from current mapper state.
    /// ciram points to the PPU's 2KB nametable VRAM for nametable mirroring.
    /// Called after load, reset, and every mapper register write that changes banking.
    virtual void update_bank_map(nes_bus::nes_bus_t* bus, uint8_t* ciram);

    /// Handle a mapper register write ($8000-$FFFF ROM write).
    /// Returns true if banking changed (caller should call update_bank_map).
    bool handle_mapper_write(uint16_t addr, uint8_t data);

    /// Get the mapper instance (for IRQ state, scanline, etc.)
    Mapper* get_mapper() const { return mapper.get(); }

    /// Install a pre-configured mapper instance (used by NSF load path).
    /// The caller must have already called set_memory_pointers() and
    /// set_header_mirror() on the mapper before installing.
    void install_mapper(std::unique_ptr<Mapper> m) {
        mapper = std::move(m);
        mirror_mode = mapper->mirror();
    }

    // Nametable mirroring
    using Mirror = nes_system::Mirror;

    // Mirroring — the active mode may be changed by the mapper at runtime
    Mirror mirror_mode = Mirror::HORIZONTAL;
    bool get_mirror_horizontal() const { return mirror_mode == Mirror::HORIZONTAL; }
    bool get_mirror_vertical() const { return mirror_mode == Mirror::VERTICAL; }
    Mirror get_mirror_mode() const { return mirror_mode; }

    // Mapper IRQ (e.g. MMC3 scanline counter) — inlined for hot-path performance
    inline bool irq_state() const { return mapper && mapper->irq_state(); }
    void irq_clear();

    // ====================================================================
    // Bus-mediated PPU memory access
    // ====================================================================
    //
    // Called by the system tick after PPU::clock() returns.  Reads the
    // 14-bit address the PPU placed on the bus, performs A12 edge
    // detection (mapper IRQ) and block dispatch (CHR/nametable read),
    // then places the result on the bus data lines.
    //
    // This replaces the old fast_vram_read() path where the PPU did
    // block dispatch + A12 detection internally.  Now the cartridge
    // — which physically sits on the PPU bus — owns both operations.
    //
    // ppu_dot_count is passed for the mapper's A12 timing filter
    // (e.g. MMC3's requirement that A12 was low for ≥16 dots).
    inline ppu_bus_state_t ppu_memory_tick(
            ppu_bus_state_t ppu_bus,
            const nes_bus::nes_bus_t* bus,
            uint64_t ppu_dot_count) {
        const uint16_t addr = PPU_BUS_GET_ADDR(ppu_bus);

        // A12 edge detection — compare current bit 12 against PA12 on bus
        const bool new_a12 = (addr & 0x1000) != 0;
        const bool old_a12 = PPU_BUS_GET_BIT(ppu_bus, PPU_BUS_PA12_BIT);
        if (new_a12) PPU_BUS_SET_BIT(ppu_bus, PPU_BUS_PA12_BIT);
        else         PPU_BUS_CLR_BIT(ppu_bus, PPU_BUS_PA12_BIT);
        if (unlikely(new_a12 != old_a12)) {
            notify_a12(new_a12, ppu_dot_count);
        }

        // Block dispatch — CHR + nametable read
        if (likely(bus != nullptr)) {
            const uint16_t block = bus->ppu_read_block[addr >> nes_bus::PPU_PAGE_SHIFT];
            if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
                PPU_BUS_SET_DATA(ppu_bus, bus->ppu_block_read(block, addr));
            }
        }

        return ppu_bus;
    }

    // A12 transition notification — delegates to mapper
    void notify_a12(bool a12_high, uint64_t ppu_cycle);

    // Mapper interface
    virtual void reset();

    // Battery-backed SRAM persistence
    bool load_sram(const std::string& sav_path);
    bool save_sram(const std::string& sav_path) const;
    std::string sram_path_for_rom(const std::string& rom_path) const;
    const std::string& get_rom_filepath() const { return rom_filepath_; }

private:
    std::string rom_filepath_;  // stored for SRAM path derivation

    std::unique_ptr<Mapper> mapper;
};

} // namespace nes_system
