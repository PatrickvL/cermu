#pragma once
/**
 * NSF Cartridge — Virtual Cartridge for NSF Music Playback
 *
 * A special cartridge implementation that provides the memory layout
 * required by NSF (NES Sound Format) files.  Instead of loading a
 * real iNES ROM, this cartridge:
 *
 *   1. Maps NSF program data into $8000-$FFFF as writable RAM
 *   2. Provides CHR-RAM (8KB) for font tiles used by the info display
 *   3. Handles optional bank-switching via $5FF8-$5FFF registers
 *   4. Maps $6000-$7FFF as 8KB work RAM
 *   5. Uses horizontal mirroring by default
 *
 * The NsfCartridge plugs into the existing NES MemoryBus and PPU
 * through the same shared_ptr<Cartridge> interface.
 */

#include "../nes_system.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>

namespace nes_system {

/**
 * NsfCartridge — inherits from Cartridge to plug into MemoryBus/PPU.
 *
 * Since Cartridge's default constructor is protected, NsfCartridge
 * can construct via the protected path without loading a file.
 */
class NsfCartridge : public Cartridge {
private:
    // NSF ROM data (up to 1MB for bankswitched NSFs)
    std::vector<uint8_t> nsf_rom_;

    // Work RAM at $6000-$7FFF (8KB)
    uint8_t work_ram_[nes_constants::INES_PRG_RAM_DEFAULT] = {};

    // CHR-RAM for font tiles (8KB = pattern tables 0+1)
    uint8_t chr_ram_[nes_constants::INES_CHR_BANK_SIZE] = {};

    // Banking state
    bool bankswitched_;
    uint8_t bank_regs_[8] = {};   ///< Current bank register values ($5FF8-$5FFF)
    size_t nsf_data_size_;         ///< Size of original NSF data

    // Base address for non-bankswitched loads
    uint16_t load_addr_;

    /**
     * Map a CPU address through bank registers to an offset in nsf_rom_.
     *
     * For bankswitched NSFs, $8000-$FFFF is divided into 8 × 4KB pages.
     * Bank register N maps page N, where page N covers addresses
     * $8000 + N*$1000 .. $8000 + N*$1000 + $0FFF.
     * $5FF8 → page at $8000, $5FF9 → page at $9000, etc.
     *
     * For non-bankswitched NSFs, the data is directly accessible at
     * its load address within $8000-$FFFF.
     */
    bool map_cpu_addr(uint16_t addr, uint32_t& mapped) const {
        if (addr < 0x8000) return false;

        if (bankswitched_) {
            int page = (addr - 0x8000) / 0x1000;  // 0-7
            uint32_t bank = bank_regs_[page];
            uint32_t offset = bank * 0x1000 + (addr & 0x0FFF);
            if (offset < nsf_rom_.size()) {
                mapped = offset;
                return true;
            }
            return false;
        } else {
            // Non-bankswitched: data loaded at load_addr
            uint32_t offset = addr - load_addr_;
            if (offset < nsf_rom_.size()) {
                mapped = offset;
                return true;
            }
            return false;
        }
    }

public:
    /**
     * Create an NSF cartridge from parsed NSF data.
     *
     * @param nsf_data       Raw NSF payload (after the 128-byte header)
     * @param data_size      Size of the payload
     * @param load_addr      Address where NSF code expects to be loaded
     * @param bankswitch     8-byte bankswitch init values (all zero = no banking)
     */
    NsfCartridge(const uint8_t* nsf_data, size_t data_size,
                 uint16_t load_addr, const uint8_t bankswitch[8])
        : Cartridge()  // Protected default constructor
    {
        load_addr_ = load_addr;
        nsf_data_size_ = data_size;

        // Check for bankswitching
        bankswitched_ = false;
        for (int i = 0; i < 8; i++) {
            if (bankswitch[i] != 0) {
                bankswitched_ = true;
                break;
            }
        }

        // Copy bankswitch init values
        memcpy(bank_regs_, bankswitch, 8);

        if (bankswitched_) {
            // For bankswitched NSFs, the load address is aligned to 4KB
            // and data is organized in 4KB banks.
            // Allocate enough ROM for all banks (round up to 4KB pages)
            size_t num_banks = (data_size + 0x0FFF) / 0x1000;
            nsf_rom_.resize(num_banks * 0x1000, 0);
            memcpy(nsf_rom_.data(), nsf_data, data_size);

            printf("NES NSF: Bankswitched — %zu × 4KB banks, init:",
                   num_banks);
            for (int i = 0; i < 8; i++) printf(" %02X", bank_regs_[i]);
            printf("\n");
        } else {
            // Non-bankswitched: pad to fill from load_addr to $FFFF
            size_t rom_size = 0x10000 - load_addr;
            nsf_rom_.resize(rom_size, 0);
            memcpy(nsf_rom_.data(), nsf_data, data_size);

            printf("NES NSF: Non-bankswitched — load=$%04X, %zu bytes\n",
                   load_addr, data_size);
        }

        // Horizontal mirroring (vertical arrangement)
        mirror_mode = Mirror::HORIZONTAL;

        // Zero work RAM and CHR-RAM
        memset(work_ram_, 0, sizeof(work_ram_));
        memset(chr_ram_, 0, sizeof(chr_ram_));
    }

    // ---- PPU page-pointer setup (CHR-RAM for font tiles) ----

    void update_bank_map(nes_bus::nes_bus_t* bus, uint8_t* ciram) override {
        if (!bus) return;

        // CHR-RAM: 8 × 1KB pages covering $0000-$1FFF (writable)
        MapperChrConfig chr_cfg;
        for (int i = 0; i < 8; i++) {
            chr_cfg.chr_pages[i] = chr_ram_ + (i * 0x400);
            chr_cfg.chr_writable[i] = true;
        }
        // Horizontal mirroring: $2000/$2400 → page 0, $2800/$2C00 → page 1
        chr_cfg.nt_page[0] = 0;
        chr_cfg.nt_page[1] = 0;
        chr_cfg.nt_page[2] = 1;
        chr_cfg.nt_page[3] = 1;
        bus->update_ppu_banks(chr_cfg, ciram);

        // CPU page pointers are NOT set — NsfCartridge uses cpu_bus_tick
        // fallback for all CPU address space access (banked ROM, work RAM,
        // bank registers).
    }

    // ---- CPU bus interface (overrides Cartridge::cpu_bus_tick) ----

    bus_state_t cpu_bus_tick(bus_state_t bus, bool& handled) override {
        const uint16_t addr   = BUS_GET_ADDR(bus);
        const bool     is_read = BUS_GET_BIT(bus, BUS_RW_BIT);
        handled = false;

        if (is_read) {
            // Work RAM: $6000-$7FFF
            if (addr >= 0x6000 && addr <= 0x7FFF) {
                BUS_SET_DATA(bus, work_ram_[addr - 0x6000]);
                handled = true;
                return bus;
            }

            // Bank-switch registers: $5FF8-$5FFF (readable)
            if (addr >= 0x5FF8 && addr <= 0x5FFF) {
                BUS_SET_DATA(bus, bank_regs_[addr - 0x5FF8]);
                handled = true;
                return bus;
            }

            // NSF ROM: $8000-$FFFF
            if (addr >= 0x8000) {
                uint32_t mapped;
                if (map_cpu_addr(addr, mapped)) {
                    BUS_SET_DATA(bus, nsf_rom_[mapped]);
                } else {
                    BUS_SET_DATA(bus, 0x00);  // unmapped reads return 0
                }
                handled = true;
                return bus;
            }
        } else {
            uint8_t data = BUS_GET_DATA(bus);

            // Work RAM: $6000-$7FFF
            if (addr >= 0x6000 && addr <= 0x7FFF) {
                work_ram_[addr - 0x6000] = data;
                handled = true;
                return bus;
            }

            // Bank-switch registers: $5FF8-$5FFF
            if (addr >= 0x5FF8 && addr <= 0x5FFF) {
                bank_regs_[addr - 0x5FF8] = data;
                handled = true;
                return bus;
            }

            // NSF ROM writes: $8000-$FFFF (self-modifying NSFs)
            if (addr >= 0x8000) {
                uint32_t mapped;
                if (map_cpu_addr(addr, mapped)) {
                    nsf_rom_[mapped] = data;
                }
                handled = true;
                return bus;
            }
        }

        return bus;
    }

    // ---- PPU memory access (CHR-RAM) ----

    bool ppu_read(uint16_t addr, uint8_t& data) {
        if (addr <= 0x1FFF) {
            data = chr_ram_[addr];
            return true;
        }
        return false;
    }

    bool ppu_write(uint16_t addr, uint8_t data) {
        if (addr <= 0x1FFF) {
            chr_ram_[addr] = data;
            return true;
        }
        return false;
    }

    void reset() {
        // Restore initial bank values
        // (The caller should re-apply bankswitch init from the NSF header)
        memset(work_ram_, 0, sizeof(work_ram_));
    }

    /**
     * Write a byte directly to NSF ROM space (for stub injection).
     * This bypasses bank mapping and writes relative to load_addr.
     */
    void write_rom_direct(uint16_t addr, uint8_t data) {
        if (addr >= load_addr_) {
            uint32_t offset = addr - load_addr_;
            if (!bankswitched_ && offset < nsf_rom_.size()) {
                nsf_rom_[offset] = data;
            }
        }
    }

    /**
     * Get a writable pointer to the NSF ROM data for bulk injection.
     */
    uint8_t* get_rom_data() { return nsf_rom_.data(); }
    size_t get_rom_size() const { return nsf_rom_.size(); }

    /**
     * Re-initialize bank registers from NSF header values.
     */
    void set_bank_regs(const uint8_t bankswitch[8]) {
        memcpy(bank_regs_, bankswitch, 8);
    }

    /**
     * Reload NSF payload data (for subtune switching — reset self-modified code).
     */
    void reload_nsf_data(const uint8_t* data, size_t size) {
        if (data && size > 0) {
            size_t copy_size = (size < nsf_rom_.size()) ? size : nsf_rom_.size();
            // Clear and re-copy
            memset(nsf_rom_.data(), 0, nsf_rom_.size());
            memcpy(nsf_rom_.data(), data, copy_size);
        }
    }
};

} // namespace nes_system
