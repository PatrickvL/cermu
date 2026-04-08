#pragma once
/*
 * mapper_077_irem_early.h — iNES Mapper 077 (Irem early mapper)
 *
 * Used exclusively by Napoleon Senki.  Combines CHR-ROM and CHR-RAM
 * with four-screen nametable mirroring.
 *
 *   PRG: 32KB switchable at $8000-$FFFF
 *   CHR: $0000-$07FF (2KB) — switchable CHR-ROM bank
 *        $0800-$1FFF (6KB) — fixed CHR-RAM (writable)
 *   NT:  FOUR_SCREEN (extra 2KB VRAM on cartridge PCB)
 *
 * Register ($8000-$FFFF):
 *   D3-D0: PRG 32KB bank select
 *   D7-D4: CHR 2KB bank select (mapped to $0000-$07FF from ROM)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper077 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

    // CHR-ROM size in bytes — CHR-RAM starts at chr_mem_ + chr_rom_size_.
    // The cartridge extends chr_memory with extra_chr_ram_size() bytes so
    // the CHR-RAM lives in flat_mem and ptr_to_block() can resolve it.
    static constexpr uint32_t CHR_RAM_SIZE = 6 * 1024;
    uint32_t chr_rom_size_ = 0;

public:
    Mapper077(uint8_t /*prgBanks*/, uint8_t chrBanks)
        : chr_rom_size_(chrBanks * 8192u) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
    }

    Mirror mirror() override { return Mirror::FOUR_SCREEN; }

    uint32_t extra_chr_ram_size() const override { return CHR_RAM_SIZE; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // $0000-$07FF: 2KB switchable CHR-ROM bank (pages 0-1, read-only)
        uint32_t num_2k = (chr_rom_size_ > 0) ? (chr_rom_size_ / 0x0800) : 1;
        if (num_2k == 0) num_2k = 1;
        uint32_t chr_base = (chr_bank_select_ % num_2k) * 0x0800;

        config.chr_pages[0] = chr_mem_ + chr_base;
        config.chr_pages[1] = chr_mem_ + chr_base + 0x0400;
        config.chr_writable[0] = false;
        config.chr_writable[1] = false;

        // $0800-$1FFF: 6KB fixed CHR-RAM (pages 2-7, writable)
        // CHR-RAM lives at chr_mem_ + chr_rom_size_ (appended by Cartridge
        // via extra_chr_ram_size()).
        const uint8_t* chr_ram = chr_mem_ + chr_rom_size_;
        for (int i = 0; i < 6; i++) {
            config.chr_pages[2 + i] = chr_ram + i * 0x0400;
            config.chr_writable[2 + i] = true;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x0F;
            chr_bank_select_ = (data >> 4) & 0x0F;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
