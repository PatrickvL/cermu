#pragma once
/*
 * mapper_016_bandai_fcg.h — iNES Mappers 016/159/157 (Bandai FCG family)
 *
 * Template-based mapper covering the Bandai FCG board family:
 *   Mapper 016 (FCG-1/2): 16K+16K PRG, 8×1KB CHR, CPU-cycle IRQ, 24C02 EEPROM
 *   Mapper 159 (LZ93D50): Same as 016 but with smaller 24C01 EEPROM
 *   Mapper 157 (Datach):  Barcode reader variant, CHR fixed (8K CHR-RAM)
 *
 * Register map ($6000-$600F or $8000-$800F):
 *   $x000-$x007: CHR bank select R0-R7 (1KB each)
 *   $x008: PRG bank select (16KB at $8000)
 *   $x009: mirroring control
 *   $x00A: IRQ control (enable + reload)
 *   $x00B: IRQ counter low byte
 *   $x00C: IRQ counter high byte
 *   $x00D: EEPROM I/O — write: D6=SDA_OUT, D5=SCL; read: D4=SDA_IN
 *
 * Games: Akuma-kun, Crayon Shin-Chan, Dragon Ball Z series, SD Gundam.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>
#include <type_traits>

namespace nes_system {

// ============================================================================
// I2C EEPROM — 24C01 (128B) / 24C02 (256B) serial EEPROM
// ============================================================================
// Minimal I2C protocol: START/STOP detection, byte clocking on SCL edges,
// device address decode, sequential read/write with auto-increment.

template <uint16_t Size>  // 128 for 24C01, 256 for 24C02
struct I2C_EEPROM {
    static_assert(Size == 128 || Size == 256);
    static constexpr uint16_t ADDR_MASK = Size - 1;

    enum class State : uint8_t {
        IDLE, DEVICE_ADDR, WORD_ADDR, WRITE_DATA, READ_DATA
    };

    uint8_t data[Size] = {};         // EEPROM storage
    State   state      = State::IDLE;
    uint8_t shift_reg  = 0;          // 8-bit shift register for incoming bits
    uint8_t bit_count  = 0;          // bits shifted in (0-8)
    uint16_t word_addr = 0;          // current address pointer
    bool    sda_out    = true;       // SDA output (active-low ACK, data during read)
    bool    prev_scl   = false;      // previous SCL state for edge detection
    bool    prev_sda   = false;      // previous SDA state for START/STOP detection
    bool    rw_mode    = false;      // false=write, true=read
    uint8_t output_sr  = 0xFF;      // output shift register for reads

    void reset() {
        state = State::IDLE;
        shift_reg = 0;
        bit_count = 0;
        word_addr = 0;
        sda_out = true;
        prev_scl = false;
        prev_sda = false;
        rw_mode = false;
        output_sr = 0xFF;
    }

    // Drive SCL/SDA lines.  Called on every write to the EEPROM I/O register.
    void write(bool scl, bool sda) {
        // --- START condition: SDA falls while SCL is high ---
        if (prev_sda && !sda && scl) {
            state = State::DEVICE_ADDR;
            bit_count = 0;
            shift_reg = 0;
            sda_out = true;
            prev_scl = scl;
            prev_sda = sda;
            return;
        }

        // --- STOP condition: SDA rises while SCL is high ---
        if (!prev_sda && sda && scl) {
            state = State::IDLE;
            sda_out = true;
            prev_scl = scl;
            prev_sda = sda;
            return;
        }

        // --- SCL rising edge: clock data ---
        if (!prev_scl && scl) {
            switch (state) {
            case State::IDLE:
                break;

            case State::DEVICE_ADDR:
            case State::WORD_ADDR:
            case State::WRITE_DATA:
                if (bit_count < 8) {
                    // Clocks 1-8: shift in data bits (MSB first)
                    shift_reg = static_cast<uint8_t>((shift_reg << 1) | (sda ? 1 : 0));
                    bit_count++;
                    sda_out = true;  // release SDA during data phase
                } else if (bit_count == 8) {
                    // Clock 9: ACK cycle — pull SDA low and process byte
                    sda_out = false;  // ACK
                    process_byte();
                    // READ_DATA: process_byte() already set bit_count = 0
                    // for data output.  Write states: mark ACK sent so the
                    // falling edge resets for the next byte.
                    if (state != State::READ_DATA) {
                        bit_count = 9;
                    }
                }
                break;

            case State::READ_DATA:
                if (bit_count < 8) {
                    // Output one bit of read data on SDA
                    sda_out = (output_sr & 0x80) != 0;
                    output_sr <<= 1;
                    bit_count++;
                } else {
                    // Bit 8: master ACK/NAK
                    // If master sends ACK (SDA low), continue reading.
                    // If NAK (SDA high), stop.
                    if (!sda) {
                        // ACK — advance to next byte
                        word_addr = (word_addr + 1) & ADDR_MASK;
                        output_sr = data[word_addr];
                        bit_count = 0;
                    } else {
                        // NAK — done reading
                        state = State::IDLE;
                        sda_out = true;
                    }
                }
                break;
            }
        }

        // --- SCL falling edge: release SDA after ACK cycle ---
        if (prev_scl && !scl) {
            if (bit_count == 9 && state != State::READ_DATA) {
                bit_count = 0;
                shift_reg = 0;
                sda_out = true;  // release SDA after ACK cycle
            }
        }

        prev_scl = scl;
        prev_sda = sda;
    }

private:
    void process_byte() {
        switch (state) {
        case State::DEVICE_ADDR: {
            // 24C02: 1010_XXX_R/W.  We accept any device address (XXX ignored).
            // 24C01: bit 0 of device addr byte is A6 of word addr merged with R/W
            rw_mode = (shift_reg & 0x01) != 0;
            if (rw_mode) {
                // Read mode — start outputting data from current address
                state = State::READ_DATA;
                output_sr = data[word_addr];
                bit_count = 0;  // will be reset on SCL falling edge
            } else {
                state = State::WORD_ADDR;
            }
            break;
        }
        case State::WORD_ADDR:
            if constexpr (Size == 128) {
                word_addr = shift_reg & 0x7F;  // 7-bit address
            } else {
                word_addr = shift_reg;  // 8-bit address
            }
            state = State::WRITE_DATA;
            break;
        case State::WRITE_DATA:
            data[word_addr] = shift_reg;
            word_addr = (word_addr + 1) & ADDR_MASK;
            break;
        default:
            break;
        }
    }
};

// ============================================================================
// MAPPER — Bandai FCG family
// ============================================================================

enum class BandaiFCGVariant : uint8_t {
    FCG,      // 016: standard FCG-1/2 with 24C02 EEPROM
    LZ93D50,  // 159: 24C01 EEPROM variant
    Datach,   // 157: Datach barcode, fixed CHR-RAM
    SRAM      // 153: Famicom Jump II — standard 8KB SRAM, CHR-RAM,
              //       CHR reg bit 0 → outer PRG bank (256KB select)
};

template<BandaiFCGVariant V>
class MapperBandaiFCG : public Mapper {
private:
    // EEPROM only for non-SRAM variants
    static constexpr bool has_eeprom = (V != BandaiFCGVariant::SRAM);
    static constexpr uint16_t eeprom_size() {
        return (V == BandaiFCGVariant::LZ93D50) ? 128 : 256;
    }

    using EEPROM = I2C_EEPROM<(V == BandaiFCGVariant::LZ93D50) ? 128 : 256>;

    uint8_t chr_bank_[8] = {};
    uint8_t prg_bank_ = 0;
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // CPU-cycle countdown IRQ (fires on underflow: 0→0xFFFF)
    mapper_helpers::CPUCycleIRQ<> irq_;

    // I2C EEPROM (not used by SRAM variant)
    std::conditional_t<has_eeprom, EEPROM, uint8_t> eeprom_{};

    // For SRAM variant: outer PRG bank computed from CHR reg bit 0
    // OR of bit 0 across all 8 CHR regs → selects 256KB PRG bank
    uint8_t compute_prg_outer() const {
        if constexpr (V == BandaiFCGVariant::SRAM) {
            uint8_t outer = 0;
            for (int i = 0; i < 8; i++)
                outer |= chr_bank_[i] & 1;
            return outer;
        } else {
            return 0;
        }
    }

    // Update PRG-RAM area with EEPROM SDA output so CPU reads at
    // $6000-$7FFF see the correct data bit.  PRG-RAM lives in flat_mem
    // and is mapped via block dispatch.
    void update_eeprom_read_buf() {
        if constexpr (has_eeprom) {
            if (prg_ram_) {
                uint8_t val = eeprom_.sda_out ? 0x10 : 0x00;
                std::memset(prg_ram_, val, prg_ram_size_ < 8192 ? 8192 : prg_ram_size_);
            }
        }
    }

public:
    MapperBandaiFCG(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        std::memset(chr_bank_, 0, sizeof(chr_bank_));
        prg_bank_ = 0;
        mirror_mode_ = header_mirror_;
        irq_.reset();
        if constexpr (has_eeprom) {
            eeprom_.reset();
        }
        update_eeprom_read_buf();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    void notify_cpu_cycle() override { irq_.tick(); }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        if constexpr (V == BandaiFCGVariant::SRAM) {
            // SRAM variant: CHR reg bit 0 selects 256KB outer PRG bank,
            // prg_bank_ selects 16KB within that window.
            uint8_t outer = compute_prg_outer();
            uint8_t effective = (outer << 4) | (prg_bank_ & 0x0F);
            mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, effective);
        } else if constexpr (V == BandaiFCGVariant::Datach) {
            // Datach: PRG low selectable, PRG high fixed
            mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
        } else {
            mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
        }

        if constexpr (V == BandaiFCGVariant::SRAM) {
            // SRAM variant: standard writable 8KB SRAM at $6000-$7FFF
            config.prg_ram_base = prg_ram_;
            config.prg_ram_size = 8192;
            config.prg_ram_enabled = true;
            config.prg_ram_write_protected = false;
        } else {
            // EEPROM read-back via PRG-RAM bus dispatch.
            // PRG-RAM is in flat_mem, so ptr_to_block() resolves correctly.
            // The mapper fills this region with the EEPROM SDA output bit.
            // Write-protect so CPU writes to $6000-$7FFF reach register_write()
            // instead of being absorbed by the PRG-RAM block — FCG boards have
            // mapper registers at $6000-$600F, not writable SRAM.
            config.prg_ram_base = prg_ram_;
            config.prg_ram_size = 8192;
            config.prg_ram_enabled = true;
            config.prg_ram_write_protected = true;
        }
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        if constexpr (V == BandaiFCGVariant::SRAM) {
            // SRAM variant uses CHR-RAM fixed (CHR regs repurposed for PRG outer bank)
            mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
        } else if constexpr (V == BandaiFCGVariant::Datach) {
            // Datach uses CHR-RAM fixed
            mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
        } else {
            mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // SRAM variant: $6000-$7FFF is writable SRAM, not registers.
        // Registers only at $8000-$FFFF.
        // EEPROM variants: $6000-$FFFF — hardware decodes only A3-A0.
        uint8_t reg;
        if constexpr (V == BandaiFCGVariant::SRAM) {
            if (addr >= 0x8000) {
                reg = addr & 0x0F;
            } else {
                return false;
            }
        } else {
            if (addr >= 0x6000) {
                reg = addr & 0x0F;
            } else {
                return false;
            }
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
                if constexpr (has_eeprom) {
                    // EEPROM I/O: D6=SDA output, D5=SCL
                    bool scl = (data & 0x20) != 0;
                    bool sda = (data & 0x40) != 0;
                    eeprom_.write(scl, sda);
                    update_eeprom_read_buf();
                }
                return false;
        }
        return false;
    }
};

// Type aliases for the four variants
using Mapper016 = MapperBandaiFCG<BandaiFCGVariant::FCG>;
using Mapper153 = MapperBandaiFCG<BandaiFCGVariant::SRAM>;
using Mapper157 = MapperBandaiFCG<BandaiFCGVariant::Datach>;
using Mapper159 = MapperBandaiFCG<BandaiFCGVariant::LZ93D50>;

} // namespace nes_system
