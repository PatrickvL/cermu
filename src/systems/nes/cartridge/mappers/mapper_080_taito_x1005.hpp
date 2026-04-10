#pragma once
/*
 * mapper_080_taito_x1005.h — iNES Mappers 080/207 (Taito X1-005)
 *
 * 3×8KB switchable PRG + 8KB fixed + 6×CHR banks (2×2KB + 4×1KB).
 * 128 bytes on-chip RAM at $7F00-$7FFF, gated by security latch.
 *
 * Mapper 207 variant: CHR D7 bit used as a one-screen nametable selector
 * per 2KB/1KB bank (similar to mapper 095).
 *
 * Register map ($7EF0-$7EFF):
 *   $7EF0: CHR bank 0 (2KB at $0000)
 *   $7EF1: CHR bank 1 (2KB at $0800)
 *   $7EF2: CHR bank 2 (1KB at $1000)
 *   $7EF3: CHR bank 3 (1KB at $1400)
 *   $7EF4: CHR bank 4 (1KB at $1800)
 *   $7EF5: CHR bank 5 (1KB at $1C00)
 *   $7EF6: mirroring (D0: 0=vertical, 1=horizontal) [mapper 080]
 *   $7EF7: same as $7EF6
 *   $7EF8-$7EF9: security latch — write $A3 to enable RAM at $7F00
 *   $7EFA-$7EFB: PRG bank 0 (8KB at $8000)
 *   $7EFC-$7EFD: PRG bank 1 (8KB at $A000)
 *   $7EFE-$7EFF: PRG bank 2 (8KB at $C000)
 *
 * Games: Fudou Myouou Den, Kyonshiizu 2, Minelvaton Saga, Taito Grand Prix.
 * Mapper 207: Fudou Myouou Den (alternate board).
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

template<bool NtFromChr>  // false = mapper 080, true = mapper 207
class MapperTaitoX1005 : public Mapper {
private:

    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[6] = {};
    Mirror mirror_mode_ = Mirror::VERTICAL;
    bool ram_enabled_ = false;  // security latch: $A3 written to $7EF8/$7EF9

    // Mapper 207 nametable map
    uint8_t nt_map_[4] = {};

    void update_nametables() {
        if constexpr (NtFromChr) {
            // 2KB banks control NT for their slots
            nt_map_[0] = nt_map_[1] = (chr_bank_[0] >> 7) & 1;
            nt_map_[2] = nt_map_[3] = (chr_bank_[1] >> 7) & 1;
        }
    }

public:
    MapperTaitoX1005(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        for (int i = 0; i < 3; i++) prg_bank_[i] = 0;
        for (int i = 0; i < 6; i++) chr_bank_[i] = 0;
        mirror_mode_ = header_mirror_;
        ram_enabled_ = false;
        for (int i = 0; i < 4; i++) nt_map_[i] = 0;
    }

    Mirror mirror() override {
        if constexpr (NtFromChr) return header_mirror_;  // NT handled in chr config
        else return mirror_mode_;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_bank_[0]) % n,
            static_cast<uint32_t>(prg_bank_[1]) % n,
            static_cast<uint32_t>(prg_bank_[2]) % n,
            n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);

        // PRG-RAM at $6000-$7FFF — reads work normally but writes must
        // go through register_write() so register writes at $7EF0-$7EFF
        // are intercepted instead of silently absorbed by RAM.
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
        config.prg_ram_write_protected = true;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t num_1k = mapper_helpers::chr_1k_count(chr_mem_size_);

        // 2×2KB banks at $0000-$0FFF
        for (int i = 0; i < 2; i++) {
            uint8_t bank_val = chr_bank_[i];
            if constexpr (NtFromChr) bank_val &= 0x7F;  // strip NT bit
            uint32_t b = (static_cast<uint32_t>(bank_val) & 0xFE) % num_1k;
            config.chr_pages[i * 2]     = chr_mem_ + b * 0x0400;
            config.chr_pages[i * 2 + 1] = chr_mem_ + ((b + 1) % num_1k) * 0x0400;
        }
        // 4×1KB banks at $1000-$1FFF
        for (int i = 0; i < 4; i++) {
            uint8_t bank_val = chr_bank_[2 + i];
            if constexpr (NtFromChr) bank_val &= 0x7F;
            uint32_t b = static_cast<uint32_t>(bank_val) % num_1k;
            config.chr_pages[4 + i] = chr_mem_ + b * 0x0400;
        }
        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;

        if constexpr (NtFromChr) {
            for (int i = 0; i < 4; i++)
                config.nt_page[i] = nt_map_[i];
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x7EF0 && addr <= 0x7EFF) {
            uint8_t reg = addr & 0x0F;

            switch (reg) {
                // $7EF0-$7EF5: CHR bank select
                case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
                    chr_bank_[reg] = data;
                    if constexpr (NtFromChr) {
                        if (reg <= 1) update_nametables();
                    }
                    return true;

                // $7EF6-$7EF7: mirroring (mapper 080) or NT from CHR D7 (mapper 207)
                case 0x06: case 0x07:
                    if constexpr (!NtFromChr) {
                        mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                    }
                    return true;

                // $7EF8-$7EF9: security latch — $A3 enables RAM at $7F00
                case 0x08: case 0x09:
                    ram_enabled_ = (data == 0xA3);
                    return false;

                // $7EFA-$7EFB: PRG bank 0 (8KB at $8000)
                case 0x0A: case 0x0B:
                    prg_bank_[0] = data;
                    return true;

                // $7EFC-$7EFD: PRG bank 1 (8KB at $A000)
                case 0x0C: case 0x0D:
                    prg_bank_[1] = data;
                    return true;

                // $7EFE-$7EFF: PRG bank 2 (8KB at $C000)
                case 0x0E: case 0x0F:
                    prg_bank_[2] = data;
                    return true;
            }
            return false;
        }

        // Write-through to PRG-RAM for non-register addresses
        // Only $7F00-$7FFF (128 bytes, mirrored) when security latch is enabled
        if (addr >= 0x7F00 && addr <= 0x7FFF && prg_ram_ != nullptr && ram_enabled_) {
            uint16_t offset = (addr - 0x6000);
            if (offset < prg_ram_size_)
                prg_ram_[offset] = data;
            return false;
        }

        return false;
    }
};

// Mapper 080: standard Taito X1-005
using Mapper080 = MapperTaitoX1005<false>;
// Mapper 207: nametable-from-CHR variant
using Mapper207 = MapperTaitoX1005<true>;

} // namespace nes_system
