#pragma once
/*
 * mapper_184_sunsoft1.h — iNES Mapper 184 (Sunsoft-1)
 *
 * Very simple discrete CHR bank switching via the PRG-RAM port.
 * Fixed PRG (16KB mirrored or 32KB). Two 4KB CHR-ROM bank selects.
 *
 * Games: Wing of Madoola, Kanshakudama Nage Kantarou no Toukaidou Gojuusan Tsugi
 *
 * Register ($6000-$7FFF):
 *   D2-D0: 4KB CHR bank at $0000
 *   D6-D4: 4KB CHR bank at $1000
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper184 : public Mapper {
private:
    uint8_t chr_lo_ = 0;  // 4KB bank for $0000-$0FFF
    uint8_t chr_hi_ = 0;  // 4KB bank for $1000-$1FFF

public:
    Mapper184(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        chr_lo_ = 0;
        chr_hi_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_fixed(config, prg_rom_, prg_rom_size_);
        // $6000 is a register — prevent compatibility shim from wiring PRG-RAM
        config.prg_ram_enabled = true;
        config.prg_ram_base = nullptr;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t max_4k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x1000) : 1;

        // $0000-$0FFF: low nibble selects 4KB bank
        uint32_t lo_bank = (max_4k > 0) ? (chr_lo_ % max_4k) : 0;
        uint32_t lo_base = lo_bank * 0x1000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = lo_base + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }

        // $1000-$1FFF: high nibble selects 4KB bank
        uint32_t hi_bank = (max_4k > 0) ? (chr_hi_ % max_4k) : 0;
        uint32_t hi_base = hi_bank * 0x1000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = hi_base + i * 0x0400;
            config.chr_pages[4 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[4 + i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x6000 && addr < 0x8000) {
            chr_lo_ = data & 0x07;          // D2-D0 select CHR bank for $0000
            chr_hi_ = ((data >> 4) & 0x07); // D6-D4 select CHR bank for $1000
            return true;
        }
        return false;
    }
};

} // namespace nes_system
