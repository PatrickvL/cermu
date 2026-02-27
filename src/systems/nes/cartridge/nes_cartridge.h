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

#include "../../../core/system_lines.h"  // bus_state_t, BUS_* macros
#include "nes_mapper.h"                  // Mirror, Mapper, MapperBankConfig, MapperChrConfig

// Forward declaration for nes_bus_t
namespace nes_bus { struct nes_bus_t; }

namespace nes_system {

class Cartridge {
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

    /** Protected default constructor for subclasses (e.g. NsfCartridge). */
    Cartridge() = default;

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

    // ====================================================================
    // Legacy bus interfaces (used by NsfCartridge and fallback dispatch)
    // ====================================================================

    // CPU bus interface — cartridge sits on the shared bus.
    // Returns the bus with data lines driven (for reads) or absorbed (for writes).
    // The bool return indicates whether the cartridge claimed the address.
    virtual bus_state_t cpu_bus_tick(bus_state_t bus, bool& handled);

    // PPU bus interface (CHR ROM/RAM) — separate internal bus, not CPU data bus.
    virtual bool ppu_read(uint16_t addr, uint8_t& data);
    virtual bool ppu_write(uint16_t addr, uint8_t data);

    // Nametable mirroring
    using Mirror = nes_system::Mirror;

    // Mirroring — the active mode may be changed by the mapper at runtime
    Mirror mirror_mode = Mirror::HORIZONTAL;
    bool get_mirror_horizontal() const { return mirror_mode == Mirror::HORIZONTAL; }
    bool get_mirror_vertical() const { return mirror_mode == Mirror::VERTICAL; }
    Mirror get_mirror_mode() const { return mirror_mode; }

    // Mapper IRQ (e.g. MMC3 scanline counter)
    bool irq_state() const;
    void irq_clear();

    // Scanline callback — the PPU calls this once per visible scanline
    void scanline();

    // Mapper interface
    virtual void reset();

    // Battery-backed SRAM persistence
    bool load_sram(const std::string& sav_path);
    bool save_sram(const std::string& sav_path) const;
    std::string sram_path_for_rom(const std::string& rom_path) const;
    const std::string& get_rom_filepath() const { return rom_filepath_; }

    // Debug read-only peek — no mapper side-effects, no bus modification
    uint8_t peek(uint16_t addr) const;

private:
    std::string rom_filepath_;  // stored for SRAM path derivation

    std::unique_ptr<Mapper> mapper;
};

} // namespace nes_system
