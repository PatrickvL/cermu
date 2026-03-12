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

#include "core/chip.hpp"           // ChipBase, ChipInfo
#include "systems/nes/bus/nes_bus.hpp"              // nes_bus_t, block dispatch, PPU_PAGE_SHIFT
#include "systems/nes/bus/nes_bus_signals.hpp"      // ppu_bus_state_t, PPU_BUS_* macros
#include "systems/nes/cartridge/nes_mapper.hpp"                  // Mirror, Mapper, MapperBankConfig, MapperChrConfig

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
        : ChipBase(ChipInfo{"Cartridge", "iNES ROM Cartridge", "Various"}) {
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

public:
    virtual ~Cartridge() = default;

    // -- ChipBase GUI overrides (implemented in nes_cartridge_gui.cpp) --

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
    // Called by the system tick after PPU::clock() returns.  Services the
    // PPU bus transaction for each dot:
    //
    // READ (default):  Reads the 14-bit address the PPU placed on the bus,
    //     performs A12 edge detection (mapper IRQ) and block dispatch
    //     (CHR/nametable read), then places the result on the bus data
    //     lines.  The PPU captures the data at the start of the next
    //     clock() call.  After default dispatch, the mapper's
    //     ppu_bus_read() hook is called — mappers like MMC2/MMC4 use
    //     this to detect pattern-table address ranges and switch CHR
    //     bank latches.
    //
    // WRITE (/WR low):  When the PPU wrote to $2007 targeting CHR or
    //     nametable space, the address + data + /WR flag are on the bus.
    //     Default dispatch writes to the block.  The mapper's
    //     ppu_bus_write() hook is called for bus-conflict detection or
    //     special behavior.
    //
    // ppu_dot_count is passed for the mapper's A12 timing filter
    // (e.g. MMC3's requirement that A12 was low for ≥16 dots).
    inline ppu_bus_state_t ppu_memory_tick(
            ppu_bus_state_t ppu_bus,
            nes_bus::nes_bus_t* bus,
            uint64_t ppu_dot_count) {
        const uint16_t addr = PPU_BUS_GET_ADDR(ppu_bus);

        // A12 edge detection — compare current bit 12 against PA12 on bus
        const bool new_a12 = (addr & 0x1000) != 0;
        const bool old_a12 = PPU_BUS_GET_BIT(ppu_bus, PPU_BUS_PA12_BIT);
        if (new_a12) PPU_BUS_SET_BIT(ppu_bus, PPU_BUS_PA12_BIT);
        else         PPU_BUS_CLR_BIT(ppu_bus, PPU_BUS_PA12_BIT);
        if (unlikely(new_a12 != old_a12)) {
            if (mapper) mapper->notify_a12(new_a12, ppu_dot_count);
        }

        const bool is_write = !PPU_BUS_GET_BIT(ppu_bus, PPU_BUS_WR_BIT);

        if (unlikely(is_write)) {
            // ---- WRITE transaction (from CPU $2007 write) ----
            if (likely(bus != nullptr)) {
                const uint16_t block = bus->ppu_write_block[addr >> nes_bus::PPU_PAGE_SHIFT];
                if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
                    bus->ppu_block_write(block, addr, PPU_BUS_GET_DATA(ppu_bus));
                }
            }
            // Mapper write hook (bus-conflict, special behavior)
            bool banking_changed = false;
            if (mapper) banking_changed = mapper->ppu_bus_write(addr, PPU_BUS_GET_DATA(ppu_bus));
            if (unlikely(banking_changed) && bus) {
                update_bank_map(bus, bus->ciram);
            }
            // Clear /WR — transaction complete, return to idle (read) state
            PPU_BUS_SET_BIT(ppu_bus, PPU_BUS_WR_BIT);
        } else {
            // ---- READ transaction (default: rendering fetch) ----
            if (likely(bus != nullptr)) {
                const uint16_t block = bus->ppu_read_block[addr >> nes_bus::PPU_PAGE_SHIFT];
                if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
                    PPU_BUS_SET_DATA(ppu_bus, bus->ppu_block_read(block, addr));
                }
            }
            // Mapper read hook (MMC2/MMC4 CHR latch switching)
            bool banking_changed = false;
            if (unlikely(mapper != nullptr)) banking_changed = mapper->ppu_bus_read(addr);
            if (unlikely(banking_changed) && bus) {
                update_bank_map(bus, bus->ciram);
            }
        }

        return ppu_bus;
    }

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

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};

} // namespace nes_system
