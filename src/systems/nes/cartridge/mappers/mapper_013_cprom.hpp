#pragma once
/*
 * mapper_013_cprom.h — iNES Mapper 013 (CPROM)
 *
 * 32KB fixed PRG-ROM + 16KB CHR-RAM with 4KB bank switching.
 * $0000-$0FFF: fixed CHR-RAM bank 0
 * $1000-$1FFF: switchable CHR-RAM bank (D1-D0)
 *
 * Games: Videomation
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper013 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper013(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        chr_bank_select_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // Fixed 32KB PRG-ROM
        for (int i = 0; i < 8; i++) {
            uint32_t offset = i * 0x1000;
            config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // $0000-$0FFF: fixed bank 0 of CHR-RAM
        for (int i = 0; i < 4; i++) {
            uint32_t offset = i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = true;  // Always CHR-RAM
        }
        // $1000-$1FFF: switchable 4KB CHR-RAM bank
        uint32_t bank_base = (chr_bank_select_ & 0x03) * 0x1000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_base + i * 0x0400;
            config.chr_pages[4 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[4 + i] = true;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            chr_bank_select_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
