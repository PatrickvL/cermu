#pragma once
/*
 * mapper_016_bandai_fcg.h — iNES Mappers 016/159/157 (Bandai FCG family)
 *
 * Template-based mapper covering the Bandai FCG board family:
 *   Mapper 016 (FCG-1/2): 16K+16K PRG, 8×1KB CHR, CPU-cycle IRQ, EEPROM
 *   Mapper 159 (LZ93D50): Same as 016 but with smaller EEPROM (24C01)
 *   Mapper 157 (Datach):  Barcode reader variant, CHR fixed (8K CHR-RAM)
 *
 * Register map ($6000-$600F or $8000-$800F):
 *   $x000-$x007: CHR bank select R0-R7 (1KB each)
 *   $x008: PRG bank select (16KB at $8000)
 *   $x009: mirroring control
 *   $x00A: IRQ control (enable + reload)
 *   $x00B: IRQ counter low byte
 *   $x00C: IRQ counter high byte
 *   $x00D: EEPROM I/O (not emulated — games work without it for basic play)
 *
 * Games: Akuma-kun, Crayon Shin-Chan, Dragon Ball Z series, SD Gundam.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

enum class BandaiFCGVariant : uint8_t {
    FCG,      // 016: standard FCG-1/2 with 24C02 EEPROM
    LZ93D50,  // 159: 24C01 EEPROM variant
    Datach    // 157: Datach barcode, fixed CHR-RAM
};

template<BandaiFCGVariant V>
class MapperBandaiFCG : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t chr_bank_[8] = {};
    uint8_t prg_bank_ = 0;
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // CPU-cycle countdown IRQ
    mapper_helpers::CPUCycleIRQ irq_;

public:
    MapperBandaiFCG(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        std::memset(chr_bank_, 0, sizeof(chr_bank_));
        prg_bank_ = 0;
        mirror_mode_ = header_mirror_;
        irq_.reset();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    void notify_cpu_cycle() override { irq_.tick(); }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        if constexpr (V == BandaiFCGVariant::Datach) {
            // Datach: PRG low selectable, PRG high fixed
            mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
        } else {
            mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
        }

        // PRG-RAM at $6000 for mapper 153 variant (Famicom Jump II)
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        if constexpr (V == BandaiFCGVariant::Datach) {
            // Datach uses CHR-RAM fixed
            mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
        } else {
            mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // FCG registers are at $6000-$600F or $8000-$800F depending on board
        uint8_t reg;
        if (addr >= 0x8000 && addr <= 0x800F) {
            reg = addr & 0x0F;
        } else if (addr >= 0x6000 && addr <= 0x600F) {
            reg = addr & 0x0F;
        } else {
            return false;
        }

        switch (reg) {
            case 0x00: case 0x01: case 0x02: case 0x03:
            case 0x04: case 0x05: case 0x06: case 0x07:
                chr_bank_[reg] = data;
                return true;

            case 0x08:
                prg_bank_ = data & 0x0F;
                return true;

            case 0x09:
                mirror_mode_ = mapper_helpers::mirror_from_2bit(data);
                return true;

            case 0x0A:
                irq_.active = false;
                irq_.enabled = (data & 0x01) != 0;
                irq_.counter = irq_.reload;
                return false;

            case 0x0B:
                irq_.reload = (irq_.reload & 0xFF00) | data;
                return false;

            case 0x0C:
                irq_.reload = (irq_.reload & 0x00FF) | (static_cast<uint16_t>(data) << 8);
                return false;

            case 0x0D:
                // TODO: Implement 24C01/24C02 EEPROM for save support (Dragon Ball Z, etc.)
                return false;
        }
        return false;
    }
};

// Type aliases for the three variants
using Mapper016 = MapperBandaiFCG<BandaiFCGVariant::FCG>;
using Mapper159 = MapperBandaiFCG<BandaiFCGVariant::LZ93D50>;
using Mapper157 = MapperBandaiFCG<BandaiFCGVariant::Datach>;

} // namespace nes_system
