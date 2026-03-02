#pragma once
/*
 * mapper_nsf.h — Virtual mapper for NSF (NES Sound Format) playback
 *
 * Models the NSF address space through the standard Mapper interface so
 * that NSF files load through the same Cartridge → Mapper → bus pipeline
 * as regular iNES ROMs.  No NsfCartridge subclass required.
 *
 * Address map:
 *   $5FF8-$5FFF  Bank registers (read/write) — 8 × 4KB page select
 *   $6000-$7FFF  Work RAM (8KB, via PRG-RAM)
 *   $8000-$FFFF  NSF ROM (8 × 4KB banked pages, read+write for self-mod)
 *
 * Bank register writes at $5FF8-$5FFF select which 4KB slice of NSF ROM
 * appears at each CPU page $8000-$FFFF (same as FDS-style NSF banking).
 *
 * For non-bankswitched NSFs the registers are ignored and the 8 pages
 * point directly into the ROM at fixed offsets from load_addr.
 */

#include "../nes_mapper.h"
#include <cstdio>
#include <cstring>

namespace nes_system {

class MapperNsf : public Mapper {
private:
    bool bankswitched_;
    uint8_t bank_regs_[8] = {};
    uint8_t bank_init_[8] = {};   ///< Initial bank values (for reset)
    uint16_t load_addr_ = 0x8000;

    // Expansion page — a 4KB buffer backing the $5000-$5FFF read block.
    // Only $5FF8-$5FFF are meaningful (bank register readback); the rest
    // returns 0.  We keep it as a full page because that's the block
    // dispatch granularity.
    uint8_t expansion_page_[0x1000] = {};

    /// Recompute PRG page pointers from current bank_regs_.
    /// Called after any bank register write.
    void recompute_pages() {
        // Also keep expansion_page_ in sync for register readback.
        for (int i = 0; i < 8; i++) {
            expansion_page_[0xFF8 + i] = bank_regs_[i];
        }
    }

public:
    /// Construct the NSF mapper.
    /// @param bankswitched  True if the NSF uses bank switching
    /// @param bankswitch    8-byte initial bank register values
    /// @param load_addr     Base address for non-bankswitched loads
    MapperNsf(bool bankswitched, const uint8_t bankswitch[8], uint16_t load_addr)
        : bankswitched_(bankswitched), load_addr_(load_addr) {
        std::memcpy(bank_regs_, bankswitch, 8);
        std::memcpy(bank_init_, bankswitch, 8);
        recompute_pages();
    }

    void reset() override {
        std::memcpy(bank_regs_, bank_init_, 8);
        recompute_pages();
    }

    // ===================================================================
    // Bank configuration
    // ===================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        if (bankswitched_) {
            // Each bank register selects a 4KB slice of NSF ROM.
            for (int i = 0; i < 8; i++) {
                uint32_t offset = static_cast<uint32_t>(bank_regs_[i]) * 0x1000;
                if (offset < prg_rom_size_) {
                    config.prg_pages[i] = prg_rom_ + offset;
                } else {
                    config.prg_pages[i] = nullptr;  // → BLOCK_OPEN_BUS
                }
            }
        } else {
            // Non-bankswitched: ROM sits at load_addr within $8000-$FFFF.
            // Pages below load_addr return nullptr (open bus).
            uint16_t first_page = (load_addr_ - 0x8000) / 0x1000;
            for (int i = 0; i < 8; i++) {
                if (i >= first_page) {
                    uint32_t offset = static_cast<uint32_t>(i - first_page) * 0x1000;
                    if (offset < prg_rom_size_) {
                        config.prg_pages[i] = prg_rom_ + offset;
                    } else {
                        config.prg_pages[i] = nullptr;
                    }
                } else {
                    config.prg_pages[i] = nullptr;
                }
            }
        }

        // Writable PRG pages mirror the read pages (NSF self-modification).
        // prg_rom_ is const but the unified buffer copy is mutable — the
        // Cartridge layer casts the pointers for the write config since
        // NSF ROM lives in the unified buffer as writable memory.
        // We use the prg_ram_ trick: Cartridge sets prg_rom_ to the
        // mutable unified buffer copy, so const_cast is safe here.
        for (int i = 0; i < 8; i++) {
            config.prg_write_pages[i] = config.prg_pages[i]
                ? const_cast<uint8_t*>(config.prg_pages[i])
                : nullptr;
        }

        // PRG-RAM at $6000-$7FFF
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);

        // Expansion space: $5000-$5FFF readable for bank register readback
        config.expansion_read = expansion_page_;
        config.expansion_write = nullptr;  // writes go through register_write
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // CHR-RAM: 8 × 1KB writable pages
        for (int i = 0; i < 8; i++) {
            config.chr_pages[i] = chr_mem_ + (i * 0x0400);
            config.chr_writable[i] = true;
        }
        // Horizontal mirroring
        config.nt_page[0] = 0; config.nt_page[1] = 0;
        config.nt_page[2] = 1; config.nt_page[3] = 1;
    }

    /// Handle writes to bank registers ($5FF8-$5FFF) and $8000+ mapper space.
    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x5FF8 && addr <= 0x5FFF) {
            int reg = addr - 0x5FF8;
            if (bank_regs_[reg] != data) {
                bank_regs_[reg] = data;
                recompute_pages();
                return true;  // banking changed
            }
            return false;
        }
        // $8000-$FFFF writes are handled by prg_write_pages (block dispatch).
        // No mapper state change needed.
        return false;
    }

    /// Reinitialize bank registers (used by subtune switch).
    void set_bank_regs(const uint8_t bankswitch[8]) {
        std::memcpy(bank_regs_, bankswitch, 8);
        recompute_pages();
    }

    /// Public accessor for mapper ID logging.
    static constexpr uint8_t NSF_MAPPER_ID = 255;
};

} // namespace nes_system
