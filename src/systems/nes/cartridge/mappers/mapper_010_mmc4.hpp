#pragma once
/*
 * mapper_010_mmc4.h — iNES Mapper 010 (MMC4 / FxROM)
 *
 * Similar to MMC2 but with 16KB PRG switching instead of 8KB.
 * Same CHR latch mechanism as MMC2.
 * 16KB switchable PRG at $8000, fixed last 16KB at $C000.
 * Games: Fire Emblem, Fire Emblem Gaiden.
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper010 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;

    uint8_t chr_bank_0_fd_ = 0;
    uint8_t chr_bank_0_fe_ = 0;
    uint8_t chr_bank_1_fd_ = 0;
    uint8_t chr_bank_1_fe_ = 0;

    bool latch_0_ = true;
    bool latch_1_ = true;

    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper010(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_0_fd_ = 0;
        chr_bank_0_fe_ = 0;
        chr_bank_1_fd_ = 0;
        chr_bank_1_fe_ = 0;
        latch_0_ = true;
        latch_1_ = true;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_4k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x1000) : 1;
        if (chr_4k == 0) chr_4k = 1;

        uint32_t bank_lo = (latch_0_ ? chr_bank_0_fe_ : chr_bank_0_fd_) % chr_4k;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_lo * 0x1000 + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }

        uint32_t bank_hi = (latch_1_ ? chr_bank_1_fe_ : chr_bank_1_fd_) % chr_4k;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_hi * 0x1000 + i * 0x0400;
            config.chr_pages[4 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[4 + i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF000) {
            case 0xA000:
                prg_bank_select_ = data & 0x0F;
                return true;
            case 0xB000:
                chr_bank_0_fd_ = data & 0x1F;
                return true;
            case 0xC000:
                chr_bank_0_fe_ = data & 0x1F;
                return true;
            case 0xD000:
                chr_bank_1_fd_ = data & 0x1F;
                return true;
            case 0xE000:
                chr_bank_1_fe_ = data & 0x1F;
                return true;
            case 0xF000:
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                return true;
        }
        return false;
    }

    // PPU bus read hook — CHR latch switching (same mechanism as MMC2).
    // Returns true if a latch switched → triggers bank map rebuild.
    bool ppu_bus_read(uint16_t addr) override {
        const bool old_l0 = latch_0_;
        const bool old_l1 = latch_1_;
        chr_read_hook(addr);
        return (latch_0_ != old_l0) || (latch_1_ != old_l1);
    }

private:
    void chr_read_hook(uint16_t addr) {
        if (addr >= 0x0FD8 && addr <= 0x0FDF) latch_0_ = false;
        else if (addr >= 0x0FE8 && addr <= 0x0FEF) latch_0_ = true;
        else if (addr >= 0x1FD8 && addr <= 0x1FDF) latch_1_ = false;
        else if (addr >= 0x1FE8 && addr <= 0x1FEF) latch_1_ = true;
    }
};

} // namespace nes_system
