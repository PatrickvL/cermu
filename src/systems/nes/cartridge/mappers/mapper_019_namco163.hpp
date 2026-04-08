#pragma once
/*
 * mapper_019_namco163.h — iNES Mapper 019 (Namco 163 / Namco 129)
 *
 * Complex banking + wavetable expansion audio.
 *
 * PRG: 3×8KB switchable ($8000,$A000,$C000) + 8KB fixed ($E000).
 *      PRG-RAM at $6000-$7FFF.
 * CHR: 8×1KB banks. Banks $E0-$FF map to internal CIRAM (nametable RAM)
 *      when CHR value >= $E0, enabling on-cart nametable mapping.
 * IRQ: 15-bit CPU-cycle up-counter, fires when bit 15 set.
 *
 * Expansion audio: 8-channel wavetable synthesizer with shared 128-byte
 * internal RAM.  Audio is complex — stubbed for initial mapper support.
 *
 * Register map:
 *   $4800: data port (read/write internal RAM, auto-increment)
 *   $5000-$5FFF: IRQ counter low / high
 *   $8000-$BFFF: CHR bank select (R0-R7)
 *   $C000-$C7FF: CHR bank for NT slot 0
 *   $C800-$CFFF: CHR bank for NT slot 1
 *   $D000-$D7FF: CHR bank for NT slot 2
 *   $D800-$DFFF: CHR bank for NT slot 3
 *   $E000: PRG bank 0 + sound enable
 *   $E800: PRG bank 1 + CHR-ROM/RAM mode
 *   $F000: PRG bank 2
 *   $F800: write protect + auto-increment
 *
 * Games: Rolling Thunder, King of Kings, Youkai Douchuki, Megami Tensei II,
 *        Sangokushi, Final Lap, Famista '90, Splatter House, etc.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper019 : public Mapper {
private:

    // PRG bank registers
    uint8_t prg_bank_[3] = {};

    // CHR bank registers (8 pattern + 4 nametable)
    uint8_t chr_bank_[12] = {};  // [0-7] = pattern, [8-11] = nametable

    // Internal 128-byte RAM (shared between audio wavetable + scratch)
    uint8_t internal_ram_[128] = {};
    uint8_t ram_addr_ = 0;
    bool auto_increment_ = false;

    // CHR-ROM/RAM mode flags (from $E800 write)
    bool chr_lo_ram_ = false;  // true = banks 0-3 use CHR-RAM
    bool chr_hi_ram_ = false;  // true = banks 4-7 use CHR-RAM

    // IRQ: 15-bit up-counter
    uint16_t irq_counter_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;

    // Sound enable
    bool sound_enabled_ = false;

    // N163 wavetable audio state (time-division multiplexed, 1 channel per 15 CPU cycles)
    uint16_t n163_tick_ = 0;
    uint8_t n163_ch_index_ = 0;
    float n163_ch_out_[8] = {};

    // CHR-ROM size — when CHR-ROM exists, extra CHR-RAM appended after it.
    // Up to 8KB CHR-RAM for writable pattern table banks.
    static constexpr uint32_t CHR_RAM_SIZE = 8 * 1024;
    uint32_t chr_rom_size_ = 0;

    // Nametable CIRAM pointer (set by set_ciram)
    uint8_t* ciram_ = nullptr;

    bool is_ciram_bank(uint8_t bank) const {
        return bank >= 0xE0;
    }

public:
    Mapper019(uint8_t /*prgBanks*/, uint8_t chrBanks)
        : chr_rom_size_(chrBanks * 8192u) {}

    void reset() override {
        std::memset(prg_bank_, 0, sizeof(prg_bank_));
        std::memset(chr_bank_, 0, sizeof(chr_bank_));
        std::memset(internal_ram_, 0, sizeof(internal_ram_));
        ram_addr_ = 0;
        auto_increment_ = false;
        chr_lo_ram_ = false;
        chr_hi_ram_ = false;
        irq_counter_ = 0;
        irq_enabled_ = false;
        irq_active_ = false;
        sound_enabled_ = false;
        n163_tick_ = 0;
        n163_ch_index_ = 0;
        std::memset(n163_ch_out_, 0, sizeof(n163_ch_out_));
    }

    void set_ciram(uint8_t* ciram) override { ciram_ = ciram; }

    Mirror mirror() override {
        // Mirroring controlled by NT bank registers — handled in get_chr_bank_config
        return header_mirror_;
    }

    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    // Request extra CHR-RAM when board has CHR-ROM (for writable bank modes)
    uint32_t extra_chr_ram_size() const override {
        return (chr_rom_size_ > 0) ? CHR_RAM_SIZE : 0;
    }

    void notify_cpu_cycle() override {
        if (!irq_enabled_) return;
        if (irq_counter_ < 0x7FFF) {
            irq_counter_++;
            if (irq_counter_ >= 0x7FFF) {
                irq_active_ = true;
            }
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_bank_[0] & 0x3F) % n,
            static_cast<uint32_t>(prg_bank_[1] & 0x3F) % n,
            static_cast<uint32_t>(prg_bank_[2] & 0x3F) % n,
            n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);

        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t num_1k = mapper_helpers::chr_1k_count(chr_mem_size_);

        // CHR-RAM region (appended after CHR-ROM by Cartridge)
        const uint8_t* extra_ram = (chr_rom_size_ > 0) ? chr_mem_ + chr_rom_size_ : nullptr;

        // Pattern table: $0000-$1FFF (8 × 1KB)
        for (int i = 0; i < 8; i++) {
            uint32_t bank = chr_bank_[i];
            if (is_ciram_bank(static_cast<uint8_t>(bank)) && ciram_) {
                config.chr_pages[i] = ciram_ + ((bank & 0x01) * 0x0400);
                config.chr_writable[i] = true;
            } else {
                bool want_ram = (i < 4) ? chr_lo_ram_ : chr_hi_ram_;
                if (want_ram && extra_ram) {
                    // Map to extra CHR-RAM (8KB, writable)
                    uint32_t ram_1k = CHR_RAM_SIZE / 0x0400;
                    uint32_t offset = (bank % ram_1k) * 0x0400;
                    config.chr_pages[i] = extra_ram + offset;
                    config.chr_writable[i] = true;
                } else {
                    uint32_t offset = (bank % num_1k) * 0x0400;
                    config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                    config.chr_writable[i] = false;
                }
            }
        }

        // Nametable mappings from chr_bank_[8-11]
        for (int i = 0; i < 4; i++) {
            uint8_t bank = chr_bank_[8 + i];
            if (is_ciram_bank(bank)) {
                config.nt_page[i] = bank & 0x01;
                config.nt_ptr[i] = nullptr;
            } else if (chr_mem_size_ > 0) {
                // Map CHR-ROM into nametable slot
                uint32_t offset = (bank % num_1k) * 0x0400;
                config.nt_ptr[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.nt_page[i] = 0;
            }
        }
        config.custom_nt = true;  // Namco 163: per-slot NT from CHR bank regs
    }

    uint8_t expansion_read(uint16_t addr, bool& handled) override {
        if (addr == 0x4800) {
            handled = true;
            uint8_t val = internal_ram_[ram_addr_ & 0x7F];
            if (auto_increment_) ram_addr_ = (ram_addr_ + 1) & 0x7F;
            return val;
        }
        if ((addr & 0xF800) == 0x5000) {
            handled = true;
            return static_cast<uint8_t>(irq_counter_ & 0xFF);
        }
        if ((addr & 0xF800) == 0x5800) {
            handled = true;
            return static_cast<uint8_t>((irq_counter_ >> 8) & 0x7F) |
                   (irq_enabled_ ? 0x80 : 0x00);
        }
        handled = false;
        return 0;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // Internal RAM data port
        if (addr == 0x4800) {
            internal_ram_[ram_addr_ & 0x7F] = data;
            if (auto_increment_) ram_addr_ = (ram_addr_ + 1) & 0x7F;
            return false;
        }

        // IRQ registers
        if ((addr & 0xF800) == 0x5000) {
            irq_counter_ = (irq_counter_ & 0xFF00) | data;
            irq_active_ = false;
            return false;
        }
        if ((addr & 0xF800) == 0x5800) {
            irq_counter_ = (irq_counter_ & 0x00FF) | (static_cast<uint16_t>(data & 0x7F) << 8);
            irq_enabled_ = (data & 0x80) != 0;
            irq_active_ = false;
            return false;
        }

        if (addr < 0x8000) return false;

        // CHR bank registers
        uint16_t reg = addr & 0xF800;
        switch (reg) {
            case 0x8000: chr_bank_[0]  = data; return true;
            case 0x8800: chr_bank_[1]  = data; return true;
            case 0x9000: chr_bank_[2]  = data; return true;
            case 0x9800: chr_bank_[3]  = data; return true;
            case 0xA000: chr_bank_[4]  = data; return true;
            case 0xA800: chr_bank_[5]  = data; return true;
            case 0xB000: chr_bank_[6]  = data; return true;
            case 0xB800: chr_bank_[7]  = data; return true;
            case 0xC000: chr_bank_[8]  = data; return true;
            case 0xC800: chr_bank_[9]  = data; return true;
            case 0xD000: chr_bank_[10] = data; return true;
            case 0xD800: chr_bank_[11] = data; return true;

            case 0xE000:
                prg_bank_[0] = data & 0x3F;
                sound_enabled_ = !(data & 0x40);
                return true;
            case 0xE800:
                prg_bank_[1] = data & 0x3F;
                chr_lo_ram_ = (data & 0x40) != 0;
                chr_hi_ram_ = (data & 0x80) != 0;
                return true;
            case 0xF000:
                prg_bank_[2] = data & 0x3F;
                return true;
            case 0xF800:
                ram_addr_ = data & 0x7F;
                auto_increment_ = (data & 0x80) != 0;
                return false;  // no banking change
        }
        return false;
    }

    // --- N163 wavetable expansion audio ---
    // 8-channel time-division multiplexed synthesizer sharing internal_ram_.
    // Hardware processes one channel every 15 CPU cycles, cycling through active
    // channels from 7 downward.  Channel registers live at $40+ch*8 in the
    // 128-byte internal RAM (same memory games also use for waveform data).

    void audio_tick() override {
        if (!sound_enabled_) return;

        if (++n163_tick_ < 15) return;
        n163_tick_ = 0;

        uint8_t active = ((internal_ram_[0x7F] >> 4) & 0x07) + 1;
        uint8_t ch = 7 - n163_ch_index_;
        uint8_t r  = 0x40 + (ch << 3);

        // 18-bit frequency
        uint32_t freq = internal_ram_[r]
                      | (static_cast<uint32_t>(internal_ram_[r + 2]) << 8)
                      | (static_cast<uint32_t>(internal_ram_[r + 4] & 0x03) << 16);

        // 24-bit phase accumulator
        uint32_t phase = internal_ram_[r + 1]
                       | (static_cast<uint32_t>(internal_ram_[r + 3]) << 8)
                       | (static_cast<uint32_t>(internal_ram_[r + 5]) << 16);

        // Advance phase
        phase = (phase + freq) & 0x00FFFFFF;

        // Wave length (in 4-bit samples): 256 - (reg & 0xFC)
        uint32_t wave_len = 256 - (internal_ram_[r + 4] & 0xFC);

        // Wrap phase to wavelength boundary
        uint32_t hi_len = wave_len << 16;
        while (phase >= hi_len) phase -= hi_len;

        // Write phase back to internal RAM (hardware does this)
        internal_ram_[r + 1] = phase & 0xFF;
        internal_ram_[r + 3] = (phase >> 8) & 0xFF;
        internal_ram_[r + 5] = (phase >> 16) & 0xFF;

        // Fetch 4-bit waveform sample from internal RAM
        uint8_t wave_off   = internal_ram_[r + 6];
        uint8_t sample_idx = ((phase >> 16) + wave_off) & 0xFF;
        uint8_t sample_byte = internal_ram_[sample_idx >> 1];
        uint8_t sample = (sample_idx & 1) ? (sample_byte >> 4) : (sample_byte & 0x0F);

        // Volume (4-bit)
        uint8_t vol = internal_ram_[r + 7] & 0x0F;

        // Cache normalized output: sample(0-15) * vol(0-15) / 225
        n163_ch_out_[ch] = static_cast<float>(sample * vol) / 225.0f;

        // Advance to next active channel
        if (++n163_ch_index_ >= active) n163_ch_index_ = 0;
    }

    float audio_output() const override {
        if (!sound_enabled_) return 0.0f;
        uint8_t active = ((internal_ram_[0x7F] >> 4) & 0x07) + 1;
        float sum = 0.0f;
        for (uint8_t i = 0; i < active; i++)
            sum += n163_ch_out_[7 - i];
        return sum / static_cast<float>(active);
    }
};

} // namespace nes_system
