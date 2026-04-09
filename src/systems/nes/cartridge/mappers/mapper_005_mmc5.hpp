#pragma once
/*
 * mapper_005_mmc5.h — iNES Mapper 005 (MMC5 / ExROM)
 *
 * Nintendo's most complex first-party mapper. Partial implementation covering
 * the features exercised by common test ROMs (mmc5test, mmc5exram):
 *
 *   - PRG banking modes 0-3 (four granularities from 32KB to 8KB)
 *   - CHR banking modes 0-3 (1KB, 2KB, 4KB, 8KB sprite/BG split)
 *   - 1KB ExRAM ($5C00-$5FFF) with mode selection
 *   - Nametable mapping ($5105) — any slot to CIRAM page 0/1 or ExRAM/fill
 *   - Fill-mode tile/attribute ($5106/$5107)
 *   - PRG-RAM banking at $6000-$7FFF
 *   - 8×8 hardware multiplier ($5205/$5206)
 *   - Scanline IRQ counter ($5203/$5204)
 *   - Vertical split mode ($5200-$5202) — PPU bus intercept for split region tiles
 *
 * Games: Castlevania III, Laser Invasion, Uncharted Waters, etc.
 *
 * Reference: https://www.nesdev.org/wiki/MMC5
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include <cstring>

namespace nes_system {

class Mapper005 : public Mapper {
private:

    // -----------------------------------------------------------------------
    // PRG banking
    // -----------------------------------------------------------------------
    uint8_t prg_mode_ = 3;             // $5100: PRG banking mode (0-3)
    uint8_t prg_bank_[5] = {};         // $5113-$5117: PRG bank registers
    bool prg_ram_protect_1_ = false;   // $5102 = $02
    bool prg_ram_protect_2_ = false;   // $5103 = $01

    // -----------------------------------------------------------------------
    // CHR banking
    // -----------------------------------------------------------------------
    uint8_t chr_mode_ = 0;             // $5101: CHR banking mode (0-3)
    uint16_t chr_bank_[12] = {};       // $5120-$512B: CHR bank registers
    uint8_t chr_upper_bits_ = 0;       // $5130: upper 2 bits for CHR banks (D1-D0 → bits 9-8)
    uint8_t ppuctrl_ = 0;              // Cached PPU $2000 for CHR split

    // -----------------------------------------------------------------------
    // Nametable mapping
    // -----------------------------------------------------------------------
    uint8_t nt_mapping_ = 0;           // $5105: nametable control
    uint8_t fill_tile_ = 0;            // $5106: fill-mode tile
    uint8_t fill_attr_ = 0;            // $5107: fill-mode attribute (bits 1-0)

    // -----------------------------------------------------------------------
    // ExRAM — stored at ciram_ + 0x800 (nametable page 2 in flat_mem)
    // -----------------------------------------------------------------------
    uint8_t exram_mode_ = 0;           // $5104: ExRAM mode (0-3)
    uint8_t* exram_ = nullptr;         // Points to ciram_ + 0x800 (1KB in flat_mem)

    // -----------------------------------------------------------------------
    // Fill nametable page — stored at ciram_ + 0xC00 (nametable page 3)
    // -----------------------------------------------------------------------
    uint8_t* fill_page_ = nullptr;     // Points to ciram_ + 0xC00 (1KB in flat_mem)

    // -----------------------------------------------------------------------
    // CIRAM pointer — set via set_ciram() after init_flat_mem
    // -----------------------------------------------------------------------
    uint8_t* ciram_ = nullptr;

    // -----------------------------------------------------------------------
    // IRQ
    // -----------------------------------------------------------------------
    uint8_t irq_scanline_ = 0;        // $5203: target scanline
    bool irq_enabled_ = false;         // $5204 bit 7
    bool irq_pending_ = false;         // IRQ pending flag
    bool in_frame_ = false;            // Whether PPU is rendering
    uint8_t scanline_counter_ = 0;     // Current scanline count

    // Scanline detector: MMC5 counts consecutive nametable-address reads
    // to detect scanline boundaries.  We track how many consecutive reads
    // targeted the $2xxx range without an intervening $0xxx/$1xxx read.
    // When the count reaches 3, we know rendering has started on a new
    // scanline (the PPU fetches two dummy NT bytes then the first real tile).
    uint8_t nt_read_count_ = 0;        // Consecutive $2xxx reads

    // -----------------------------------------------------------------------
    // Multiplier
    // -----------------------------------------------------------------------
    uint8_t multiplicand_ = 0xFF;      // $5205
    uint8_t multiplier_ = 0xFF;        // $5206
    uint16_t product_ = 0;             // multiplicand_ × multiplier_

    // -----------------------------------------------------------------------
    // Mirroring
    // -----------------------------------------------------------------------
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // -----------------------------------------------------------------------
    // Vertical split mode ($5200-$5202)
    // Overrides BG tile fetches for tiles in the split region: nametable
    // data from ExRAM, attributes from ExRAM upper bits, CHR from
    // split_bank_.  The split Y scroll is independent of the PPU's own
    // scroll registers, allowing a fixed sidebar (e.g. status panel).
    // -----------------------------------------------------------------------
    bool    split_enabled_ = false;    // $5200 bit 7
    bool    split_right_ = false;      // $5200 bit 6 (0=left, 1=right)
    uint8_t split_tile_ = 0;           // $5200 bits 4:0 (tile column 0-31)
    uint8_t split_scroll_ = 0;         // $5201 fine Y scroll for split region
    uint8_t split_bank_ = 0;           // $5202 CHR bank for split region

    // Per-fetch tracking for split intercept
    bool    split_active_fetch_ = false;  // Current tile is in split region
    uint8_t split_nt_byte_ = 0;           // Cached NT byte for pattern lookup
    uint8_t split_at_byte_ = 0;           // Cached attribute for AT fetch

    // ExRAM mode 1: per-tile extended attribute tracking
    uint8_t exattr_byte_ = 0;             // Cached ExRAM byte for current BG tile
    uint16_t last_nt_addr_ = 0;           // Last NT byte fetch address (for ExRAM lookup)

    // -----------------------------------------------------------------------
    // Expansion audio — two pulse channels (no sweep) + PCM DAC
    // -----------------------------------------------------------------------

    // Duty cycle waveforms (shared with APU)
    static constexpr uint8_t DUTY_TABLE[4][8] = {
        {0,1,0,0,0,0,0,0},  // 12.5%
        {0,1,1,0,0,0,0,0},  // 25%
        {0,1,1,1,1,0,0,0},  // 50%
        {1,0,0,1,1,1,1,1},  // 75% (inverted 25%)
    };

    // Length counter lookup table (identical to APU)
    static constexpr uint8_t LENGTH_TABLE[32] = {
        10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
    };

    struct MMC5Envelope {
        bool start = false;
        bool loop = false;
        bool constant_volume = false;
        uint8_t divider_period = 0;
        uint8_t constant_value = 0;
        uint8_t decay_counter = 0;
        uint8_t divider = 0;

        void reset() { start = true; }

        void clock() {
            if (start) {
                start = false;
                decay_counter = 15;
                divider = divider_period;
            } else if (divider == 0) {
                divider = divider_period;
                if (decay_counter > 0) decay_counter--;
                else if (loop) decay_counter = 15;
            } else {
                divider--;
            }
        }

        uint8_t volume() const {
            return constant_volume ? constant_value : decay_counter;
        }
    };

    struct MMC5Pulse {
        MMC5Envelope envelope;
        uint8_t duty = 0;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint8_t sequence_pos = 0;
        uint8_t length_counter = 0;
        bool length_halt = false;
        bool enabled = false;

        void write_control(uint8_t value) {
            duty = (value >> 6) & 0x03;
            length_halt = (value >> 5) & 1;
            envelope.loop = (value >> 5) & 1;
            envelope.constant_volume = (value >> 4) & 1;
            envelope.divider_period = value & 0x0F;
            envelope.constant_value = value & 0x0F;
        }

        void write_timer_low(uint8_t value) {
            timer_period = (timer_period & 0x700) | value;
        }

        void write_timer_high(uint8_t value) {
            timer_period = (timer_period & 0xFF) | ((value & 0x07) << 8);
            if (enabled)
                length_counter = LENGTH_TABLE[value >> 3];
            sequence_pos = 0;
            envelope.reset();
        }

        void clock_timer() {
            if (timer == 0) {
                timer = timer_period;
                sequence_pos = (sequence_pos + 1) & 0x07;
            } else {
                timer--;
            }
        }

        void clock_length() {
            if (!length_halt && length_counter > 0)
                length_counter--;
        }

        uint8_t output() const {
            if (!enabled || length_counter == 0)
                return 0;
            if (timer_period < 8)
                return 0;
            if (DUTY_TABLE[duty][sequence_pos] == 0)
                return 0;
            return envelope.volume();
        }
    };

    MMC5Pulse pulse1_;
    MMC5Pulse pulse2_;
    uint8_t pcm_value_ = 0;       // $5011 raw DAC value
    uint8_t audio_enable_ = 0;    // $5015 (bits 0-1 = pulse 1/2 enable)

    // Simple frame counter — counts CPU cycles for quarter/half-frame events.
    // Uses 4-step mode timing (NTSC: 7457 CPU cycles per half-frame).
    uint32_t audio_cycle_ = 0;
    static constexpr uint32_t QUARTER_FRAME = 3729;
    static constexpr uint32_t HALF_FRAME    = 7457;
    static constexpr uint32_t THREE_QUARTER = 11186;
    static constexpr uint32_t FULL_FRAME    = 14915;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    bool prg_ram_writable() const {
        return prg_ram_protect_1_ && prg_ram_protect_2_;
    }

    /// Rebuild the 1KB fill-mode nametable page at ciram_ + 0xC00.
    /// Tile bytes (offsets 0-959) = fill_tile_.
    /// Attribute bytes (offsets 960-1023) = fill_attr_ replicated to all quadrants.
    void rebuild_fill_page() {
        if (!fill_page_) return;
        std::memset(fill_page_, fill_tile_, 960);
        uint8_t attr_byte = fill_attr_ | (fill_attr_ << 2)
                          | (fill_attr_ << 4) | (fill_attr_ << 6);
        std::memset(fill_page_ + 960, attr_byte, 64);
    }

    /// Resolve a PRG bank register value to a ROM/RAM pointer.
    /// bit 7 of the bank value: 0 = RAM, 1 = ROM.
    const uint8_t* resolve_prg_page(uint8_t bank_val, uint32_t page_size,
                                     bool& is_ram_out) const {
        is_ram_out = !(bank_val & 0x80);
        if (is_ram_out) {
            // PRG-RAM bank (ignore bit 7)
            if (!prg_ram_ || prg_ram_size_ == 0) return nullptr;
            uint32_t offset = (bank_val & 0x07) * page_size;
            return (offset < prg_ram_size_) ? prg_ram_ + offset : prg_ram_;
        } else {
            // PRG-ROM bank
            uint32_t total_pages = static_cast<uint32_t>(prg_rom_size_ / page_size);
            if (total_pages == 0) return nullptr;
            uint32_t page = (bank_val & 0x7F) % total_pages;
            return prg_rom_ + page * page_size;
        }
    }

    uint8_t* resolve_prg_page_write(uint8_t bank_val, uint32_t page_size) const {
        if (bank_val & 0x80) return nullptr;  // ROM — not writable
        if (!prg_ram_ || prg_ram_size_ == 0) return nullptr;
        if (!prg_ram_writable()) return nullptr;
        uint32_t offset = (bank_val & 0x07) * page_size;
        return (offset < prg_ram_size_) ? prg_ram_ + offset : prg_ram_;
    }

public:
    Mapper005(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void set_ciram(uint8_t* ciram) override {
        ciram_ = ciram;
        exram_ = ciram + 0x800;   // NT page 2 slot in flat_mem
        fill_page_ = ciram + 0xC00; // NT page 3 slot in flat_mem
        // Initialize fill page
        rebuild_fill_page();
    }

    void reset() override {
        prg_mode_ = 3;
        chr_mode_ = 0;
        std::memset(prg_bank_, 0, sizeof(prg_bank_));
        prg_bank_[4] = 0xFF;  // Last 8KB bank defaults to last page (ROM)
        std::memset(chr_bank_, 0, sizeof(chr_bank_));
        ppuctrl_ = 0;
        nt_mapping_ = 0;
        fill_tile_ = 0;
        fill_attr_ = 0;
        exram_mode_ = 0;
        if (exram_) std::memset(exram_, 0, 1024);
        prg_ram_protect_1_ = false;
        prg_ram_protect_2_ = false;
        irq_scanline_ = 0;
        irq_enabled_ = false;
        irq_pending_ = false;
        in_frame_ = false;
        scanline_counter_ = 0;
        nt_read_count_ = 0;
        multiplicand_ = 0xFF;
        multiplier_ = 0xFF;
        product_ = 0;
        mirror_mode_ = Mirror::VERTICAL;
        rebuild_fill_page();

        // Audio reset
        pulse1_ = {};
        pulse2_ = {};
        pcm_value_ = 0;
        audio_enable_ = 0;
        audio_cycle_ = 0;

        // Vertical split reset
        split_enabled_ = false;
        split_right_ = false;
        split_tile_ = 0;
        split_scroll_ = 0;
        split_bank_ = 0;
        exattr_byte_ = 0;
        last_nt_addr_ = 0;
    }

    Mirror mirror() override { return mirror_mode_; }

    bool irq_state() override { return irq_pending_ && irq_enabled_; }

    void irq_clear() override { irq_pending_ = false; }

    // =======================================================================
    // Expansion audio
    // =======================================================================

    void audio_tick() override {
        // Clock pulse timers every other CPU cycle (APU half-rate)
        if (audio_cycle_ & 1) {
            pulse1_.clock_timer();
            pulse2_.clock_timer();
        }

        // Frame counter — quarter and half frame events
        uint32_t fc = audio_cycle_ % FULL_FRAME;
        if (fc == QUARTER_FRAME || fc == HALF_FRAME ||
            fc == THREE_QUARTER || fc == 0) {
            // Quarter-frame: clock envelopes
            pulse1_.envelope.clock();
            pulse2_.envelope.clock();
        }
        if (fc == HALF_FRAME || fc == 0) {
            // Half-frame: clock length counters
            pulse1_.clock_length();
            pulse2_.clock_length();
        }

        audio_cycle_++;
    }

    float audio_output() const override {
        // Pulse output (0-15 each) — NES mixer formula (NESdev wiki)
        uint8_t p = pulse1_.output() + pulse2_.output();
        float pulse_out = (p > 0) ? 95.52f / (8128.0f / p + 100.0f) : 0.0f;

        // PCM DAC (0-255) — scale to match APU output range (~0 to ~0.5)
        float pcm_out = static_cast<float>(pcm_value_) / 255.0f * 0.4f;

        return pulse_out + pcm_out;
    }

    bool notify_ppuctrl(uint8_t value) override {
        if (ppuctrl_ == value) return false;
        ppuctrl_ = value;
        return true;  // CHR split depends on pattern table bits
    }

    // =======================================================================
    // Scanline detection — consecutive nametable-read counting
    // =======================================================================
    //
    // Real MMC5 detects scanline boundaries by watching PPU address lines.
    // When A13 is high (nametable range $2xxx), it counts consecutive reads.
    // After three $2xxx reads without an intervening pattern-table fetch
    // ($0xxx/$1xxx), the MMC5 considers rendering active and increments the
    // scanline counter.  During VBlank ($3Fxx palette reads or no reads),
    // the consecutive-read counter resets and in_frame_ clears.
    //
    // We use notify_a12() to detect when the PPU switches between the
    // pattern table (A12=0→1 transition = entering $1xxx) and nametable
    // (A12 stays low during $2xxx fetches but A13 is high).  We track
    // the A12 transitions to count scanlines: each A12 falling edge after
    // a period of being high (pattern fetches done → NT fetches start)
    // signals we've completed a set of fetches for one scanline.

    void notify_a12(bool a12_high, uint64_t /*ppu_cycle*/) override {
        if (a12_high) {
            // Pattern table fetch ($1xxx) — interrupts consecutive NT reads
            if (nt_read_count_ >= 3) {
                // We had 3+ consecutive NT reads — a scanline just completed
                if (!in_frame_) {
                    in_frame_ = true;
                    scanline_counter_ = 0;
                }
                scanline_counter_++;

                if (scanline_counter_ == irq_scanline_) {
                    irq_pending_ = true;
                }
                if (scanline_counter_ >= 241) {
                    in_frame_ = false;
                }
            }
            nt_read_count_ = 0;
        } else {
            // A12 low — could be NT fetch ($2xxx where A13=1, A12=0)
            nt_read_count_++;
        }
    }

    // =======================================================================
    // Vertical split — PPU bus intercept
    // =======================================================================
    //
    // The MMC5 sits between the PPU and VRAM/CHR, intercepting every PPU
    // read during rendering.  When vertical split is enabled, tile fetches
    // in the split region are redirected:
    //
    //   NT fetch  → ExRAM[split_y_coarse * 32 + coarse_x]
    //   AT fetch  → ExRAM upper 2 bits, replicated to fill the byte
    //   Pattern   → CHR at split_bank_ * 4KB + tile_index * 16 + fine_y
    //
    // The split uses its own independent Y scroll (split_scroll_), adding
    // the current scanline number to compute the actual tile row and fine Y.

    bool ppu_bus_intercept(uint16_t addr, uint8_t& data) override {
        if (!exram_) return false;

        // Nametable fetch region ($2000-$2FFF, excluding pattern table $0-$1FFF)
        if ((addr & 0x2000) && !(addr & 0x1000)) {
            bool is_attr = (addr & 0x03C0) == 0x03C0;

            // --- Vertical split mode (takes priority over ExRAM mode 1) ---
            if (split_enabled_) {
                if (!is_attr) {
                    uint8_t coarse_x = addr & 0x1F;
                    bool in_split = split_right_ ? (coarse_x >= split_tile_)
                                                 : (coarse_x < split_tile_);
                    split_active_fetch_ = in_split;

                    if (!in_split) goto exram_mode1_nt;

                    uint16_t split_y = static_cast<uint16_t>(split_scroll_)
                                     + scanline_counter_;
                    uint8_t coarse_y = (split_y >> 3) % 30;
                    uint16_t exram_addr = coarse_y * 32 + coarse_x;

                    uint8_t exram_byte = exram_[exram_addr & 0x3FF];
                    split_nt_byte_ = exram_byte;

                    uint8_t attr2 = (exram_byte >> 6) & 0x03;
                    split_at_byte_ = attr2 | (attr2 << 2) | (attr2 << 4) | (attr2 << 6);

                    data = split_nt_byte_;
                    return true;
                }

                if (split_active_fetch_) {
                    data = split_at_byte_;
                    return true;
                }
            } else {
                split_active_fetch_ = false;
            }

        exram_mode1_nt:
            // --- ExRAM mode 1: extended attributes ---
            // In mode 1, ExRAM provides per-tile attributes (D7-D6) and
            // upper CHR bank bits (D5-D0) for BG tiles (not sprites).
            if (exram_mode_ == 1) {
                if (!is_attr) {
                    // NT byte fetch — cache tile address for ExRAM lookup
                    last_nt_addr_ = addr & 0x03FF;
                    // Look up ExRAM at the same offset
                    exattr_byte_ = exram_[last_nt_addr_ & 0x3FF];
                    // Don't override the NT byte itself — let normal CIRAM provide it
                    return false;
                }

                // Attribute fetch — override with per-tile palette from ExRAM
                // ExRAM D7-D6 = 2-bit palette select, replicated across all quadrants
                uint8_t attr2 = (exattr_byte_ >> 6) & 0x03;
                data = attr2 | (attr2 << 2) | (attr2 << 4) | (attr2 << 6);
                return true;
            }

            return false;
        }

        // Pattern table fetch ($0000-$1FFF)
        if (!(addr & 0x2000)) {
            // Split region pattern fetch
            if (split_active_fetch_ && split_enabled_) {
                uint32_t chr_1k = (chr_mem_size_ > 0)
                                ? static_cast<uint32_t>(chr_mem_size_ >> 10) : 1;
                uint32_t bank_4k = split_bank_ % ((chr_1k + 3) / 4);
                uint32_t base_offset = bank_4k * 0x1000;

                uint16_t split_y = static_cast<uint16_t>(split_scroll_)
                                 + scanline_counter_;
                uint8_t fine_y = split_y & 0x07;

                uint16_t pattern_offset = split_nt_byte_ * 16 + fine_y;
                if (addr & 0x08) pattern_offset += 8;

                uint32_t chr_addr = base_offset + (pattern_offset & 0x0FFF);
                data = (chr_addr < chr_mem_size_) ? chr_mem_[chr_addr] : 0;
                return true;
            }

            // ExRAM mode 1: use ExRAM D5-D0 as upper CHR bank bits for BG tiles
            if (exram_mode_ == 1) {
                // ExRAM D5-D0 provide the upper bits of the CHR address.
                // Combined with the tile number from the NT byte, this allows
                // up to 16384 unique BG tiles (256 tiles × 64 banks).
                uint8_t exram_chr = exattr_byte_ & 0x3F;
                uint32_t chr_1k = (chr_mem_size_ > 0)
                                ? static_cast<uint32_t>(chr_mem_size_ >> 10) : 1;
                // Each ExRAM bank value selects a 4KB CHR page
                uint32_t bank_4k = exram_chr % ((chr_1k + 3) / 4);
                uint32_t base_4k = bank_4k * 0x1000;

                // Replace the CHR address with: base_4k + (tile_pattern_offset within 4KB)
                uint32_t chr_addr = base_4k + (addr & 0x0FFF);
                data = (chr_addr < chr_mem_size_) ? chr_mem_[chr_addr] : 0;
                return true;
            }
        }

        return false;
    }

    // =======================================================================
    // Bank configuration — PRG
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        bool dummy_ram;

        switch (prg_mode_) {
            case 0: {
                // Mode 0: One 32KB bank at $8000-$FFFF
                // $5117 selects 32KB page (bits 6-2 used, bit 7 = ROM always)
                uint8_t bank_val = prg_bank_[4] | 0x80;  // Force ROM
                uint32_t total_32k = static_cast<uint32_t>(prg_rom_size_ / 0x8000);
                if (total_32k == 0) total_32k = 1;
                uint32_t page = ((bank_val & 0x7F) >> 2) % total_32k;
                uint32_t base = page * 0x8000;
                const uint32_t prg_mask = static_cast<uint32_t>(prg_rom_size_) - 1;
                for (int i = 0; i < 8; i++) {
                    uint32_t offset = (base + i * 0x1000) & prg_mask;
                    config.prg_pages[i] = prg_rom_ + offset;
                }
                break;
            }
            case 1: {
                // Mode 1: Two 16KB banks — $5115 at $8000, $5117 at $C000
                // $5115: ROM or RAM
                const uint8_t* p0 = resolve_prg_page(prg_bank_[2], 0x4000, dummy_ram);
                const uint8_t* p1 = resolve_prg_page(prg_bank_[4] | 0x80, 0x4000, dummy_ram);
                for (int i = 0; i < 4; i++) {
                    config.prg_pages[i] = p0 ? p0 + i * 0x1000 : nullptr;
                    config.prg_pages[4 + i] = p1 ? p1 + i * 0x1000 : nullptr;
                }
                // Write pages for RAM banks
                uint8_t* w0 = resolve_prg_page_write(prg_bank_[2], 0x4000);
                for (int i = 0; i < 4; i++) {
                    config.prg_write_pages[i] = w0 ? w0 + i * 0x1000 : nullptr;
                    config.prg_write_pages[4 + i] = nullptr;  // $C000 is ROM
                }
                break;
            }
            case 2: {
                // Mode 2: 16KB + 8KB + 8KB
                // $5115 at $8000-$BFFF (16KB), $5116 at $C000, $5117 at $E000
                const uint8_t* p0 = resolve_prg_page(prg_bank_[2], 0x4000, dummy_ram);
                const uint8_t* p1 = resolve_prg_page(prg_bank_[3], 0x2000, dummy_ram);
                const uint8_t* p2 = resolve_prg_page(prg_bank_[4] | 0x80, 0x2000, dummy_ram);
                for (int i = 0; i < 4; i++)
                    config.prg_pages[i] = p0 ? p0 + i * 0x1000 : nullptr;
                config.prg_pages[4] = p1;
                config.prg_pages[5] = p1 ? p1 + 0x1000 : nullptr;
                config.prg_pages[6] = p2;
                config.prg_pages[7] = p2 ? p2 + 0x1000 : nullptr;

                uint8_t* w0 = resolve_prg_page_write(prg_bank_[2], 0x4000);
                uint8_t* w1 = resolve_prg_page_write(prg_bank_[3], 0x2000);
                for (int i = 0; i < 4; i++)
                    config.prg_write_pages[i] = w0 ? w0 + i * 0x1000 : nullptr;
                config.prg_write_pages[4] = w1;
                config.prg_write_pages[5] = w1 ? w1 + 0x1000 : nullptr;
                config.prg_write_pages[6] = nullptr;
                config.prg_write_pages[7] = nullptr;
                break;
            }
            case 3: {
                // Mode 3: Four 8KB banks
                // $5114 at $8000, $5115 at $A000, $5116 at $C000, $5117 at $E000
                for (int slot = 0; slot < 4; slot++) {
                    uint8_t bv = (slot == 3) ? (prg_bank_[slot + 1] | 0x80) : prg_bank_[slot + 1];
                    const uint8_t* p = resolve_prg_page(bv, 0x2000, dummy_ram);
                    config.prg_pages[slot * 2]     = p;
                    config.prg_pages[slot * 2 + 1] = p ? p + 0x1000 : nullptr;

                    uint8_t* w = (slot < 3) ? resolve_prg_page_write(prg_bank_[slot + 1], 0x2000) : nullptr;
                    config.prg_write_pages[slot * 2]     = w;
                    config.prg_write_pages[slot * 2 + 1] = w ? w + 0x1000 : nullptr;
                }
                break;
            }
        }

        // PRG-RAM at $6000-$7FFF from $5113
        if (prg_ram_ && prg_ram_size_ > 0) {
            uint32_t ram_bank = (prg_bank_[0] & 0x07);
            uint32_t offset = ram_bank * 0x2000;
            if (offset + 0x2000 <= prg_ram_size_) {
                config.prg_ram_base = prg_ram_ + offset;
            } else {
                config.prg_ram_base = prg_ram_;
            }
            config.prg_ram_size = 0x2000;
            config.prg_ram_enabled = true;
            config.prg_ram_write_protected = !prg_ram_writable();
        }

        // Expansion area at $5000-$5FFF is handled by register_write/read
        // No direct page mapping for expansion space
    }

    // =======================================================================
    // Bank configuration — CHR
    // =======================================================================

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        if (chr_1k == 0) chr_1k = 1;

        // MMC5 has two CHR bank register sets:
        //   - Sprite set ($5120-$5127): 8 registers for sprite tile fetches
        //   - BG set ($5128-$512B): 4 registers for BG tile fetches
        //
        // Real hardware switches per-PPU-fetch.  We approximate by mapping
        // each 4KB pattern table half from the appropriate set based on
        // PPUCTRL ($2000):
        //   bit 4: BG pattern table base (0=$0000, 1=$1000)
        //   bit 3: sprite pattern table base (0=$0000, 1=$1000) [8×8 mode]
        //
        // The 4KB half that $2000.4 selects → BG set.
        // The other 4KB half → sprite set.
        // When both select the same half → BG set for that half, sprite
        // set for the other (sprites can still work via 8×16 tile index).
        //
        // The BG set has only 4 registers; they replicate across 8 × 1KB:
        //   slot 0,4 ← $5128    slot 1,5 ← $5129
        //   slot 2,6 ← $512A    slot 3,7 ← $512B

        // bg_half: which 4KB half (0=low $0000-$0FFF, 1=high $1000-$1FFF)
        // uses the BG set.  The other half uses the sprite set.
        const int bg_half = (ppuctrl_ & 0x10) ? 1 : 0;

        // Helper: fill 4 slots from the BG set (registers 8-11, replicated)
        auto fill_bg_set = [&](int base_slot) {
            switch (chr_mode_) {
                case 0: {
                    // 8KB — $512B selects 8KB; take the relevant 4 slots
                    uint32_t base = (chr_bank_[11] * 8) % chr_1k;
                    int offset = base_slot;  // 0 or 4
                    for (int i = 0; i < 4; i++) {
                        config.chr_pages[base_slot + i] = chr_mem_ + ((base + offset + i) % chr_1k) * 0x0400;
                        config.chr_writable[base_slot + i] = chr_is_ram_;
                    }
                    break;
                }
                case 1: {
                    // 4KB — $512B selects 4KB bank
                    uint32_t lo = (chr_bank_[11] * 4) % chr_1k;
                    for (int i = 0; i < 4; i++) {
                        config.chr_pages[base_slot + i] = chr_mem_ + ((lo + i) % chr_1k) * 0x0400;
                        config.chr_writable[base_slot + i] = chr_is_ram_;
                    }
                    break;
                }
                case 2: {
                    // 2KB — $5129/$512B
                    for (int half = 0; half < 2; half++) {
                        int reg = 9 + half * 2;  // registers 9, 11
                        uint32_t base = (chr_bank_[reg] * 2) % chr_1k;
                        for (int i = 0; i < 2; i++) {
                            config.chr_pages[base_slot + half * 2 + i] = chr_mem_ + ((base + i) % chr_1k) * 0x0400;
                            config.chr_writable[base_slot + half * 2 + i] = chr_is_ram_;
                        }
                    }
                    break;
                }
                case 3: {
                    // 1KB — 4 registers replicated
                    for (int i = 0; i < 4; i++) {
                        uint32_t b = chr_bank_[8 + i] % chr_1k;
                        config.chr_pages[base_slot + i] = chr_mem_ + b * 0x0400;
                        config.chr_writable[base_slot + i] = chr_is_ram_;
                    }
                    break;
                }
            }
        };

        // Helper: fill 4 slots from the sprite set (registers 0-7)
        auto fill_spr_set = [&](int base_slot) {
            switch (chr_mode_) {
                case 0: {
                    // 8KB — $5127 selects 8KB; take the relevant 4 slots
                    uint32_t base = (chr_bank_[7] * 8) % chr_1k;
                    int offset = base_slot;  // 0 or 4
                    for (int i = 0; i < 4; i++) {
                        config.chr_pages[base_slot + i] = chr_mem_ + ((base + offset + i) % chr_1k) * 0x0400;
                        config.chr_writable[base_slot + i] = chr_is_ram_;
                    }
                    break;
                }
                case 1: {
                    // 4KB — $5123 at low half, $5127 at high half
                    int reg = (base_slot == 0) ? 3 : 7;
                    uint32_t lo = (chr_bank_[reg] * 4) % chr_1k;
                    for (int i = 0; i < 4; i++) {
                        config.chr_pages[base_slot + i] = chr_mem_ + ((lo + i) % chr_1k) * 0x0400;
                        config.chr_writable[base_slot + i] = chr_is_ram_;
                    }
                    break;
                }
                case 2: {
                    // 2KB — base_slot=0: $5121/$5123, base_slot=4: $5125/$5127
                    int start_pair = (base_slot == 0) ? 0 : 2;
                    for (int half = 0; half < 2; half++) {
                        int reg = (start_pair + half) * 2 + 1;
                        uint32_t base = (chr_bank_[reg] * 2) % chr_1k;
                        for (int i = 0; i < 2; i++) {
                            config.chr_pages[base_slot + half * 2 + i] = chr_mem_ + ((base + i) % chr_1k) * 0x0400;
                            config.chr_writable[base_slot + half * 2 + i] = chr_is_ram_;
                        }
                    }
                    break;
                }
                case 3: {
                    // 1KB — 4 registers per half
                    int start_reg = base_slot;  // 0 or 4
                    for (int i = 0; i < 4; i++) {
                        uint32_t b = chr_bank_[start_reg + i] % chr_1k;
                        config.chr_pages[base_slot + i] = chr_mem_ + b * 0x0400;
                        config.chr_writable[base_slot + i] = chr_is_ram_;
                    }
                    break;
                }
            }
        };

        // Map each 4KB half from the appropriate set
        if (bg_half == 0) {
            // BG uses $0000-$0FFF, sprites use $1000-$1FFF
            fill_bg_set(0);
            fill_spr_set(4);
        } else {
            // Sprites use $0000-$0FFF, BG uses $1000-$1FFF
            fill_spr_set(0);
            fill_bg_set(4);
        }

        // Nametable mapping — decode $5105
        for (int slot = 0; slot < 4; slot++) {
            uint8_t src = (nt_mapping_ >> (slot * 2)) & 0x03;
            config.nt_page[slot] = src;  // 0=CIRAM-0, 1=CIRAM-1, 2=ExRAM, 3=fill
        }
    }

    // =======================================================================
    // Register read — $5000-$5FFF expansion area
    // =======================================================================

    /// CPU read in $5000-$5FFF range. Must be called by the bus for
    /// expansion area reads. Returns the byte and sets `handled` to true
    /// if the address was serviced.
    uint8_t expansion_read(uint16_t addr, bool& handled) override {
        handled = true;

        if (addr == 0x5015) {
            // Audio status — bit 0/1 = pulse 1/2 length counter > 0
            uint8_t val = 0;
            if (pulse1_.length_counter > 0) val |= 0x01;
            if (pulse2_.length_counter > 0) val |= 0x02;
            return val;
        }

        if (addr == 0x5204) {
            // IRQ status — bit 7 = pending, bit 6 = in-frame
            uint8_t val = 0;
            if (irq_pending_) val |= 0x80;
            if (in_frame_)    val |= 0x40;
            // Reading $5204 acknowledges (clears) the IRQ
            irq_pending_ = false;
            return val;
        }

        if (addr == 0x5205) {
            return static_cast<uint8_t>(product_ & 0xFF);
        }
        if (addr == 0x5206) {
            return static_cast<uint8_t>((product_ >> 8) & 0xFF);
        }

        // ExRAM read ($5C00-$5FFF)
        if (addr >= 0x5C00 && addr <= 0x5FFF) {
            if (exram_mode_ >= 2 && exram_) {  // Modes 2 & 3: readable
                return exram_[addr - 0x5C00];
            }
            return 0;  // Modes 0 & 1: returns open bus (0 as fallback)
        }

        handled = false;
        return 0;
    }

    // =======================================================================
    // Register write — combines $5000-$5FFF and $8000-$FFFF
    // =======================================================================

    bool register_write(uint16_t addr, uint8_t data) override {
        // MMC5 internal registers $5000-$5FFF
        if (addr >= 0x5000 && addr < 0x6000) {
            return write_expansion(addr, data);
        }

        // No mapper register writes at $8000-$FFFF for MMC5
        // (PRG banking is controlled entirely via $5100-$5117)
        return false;
    }

private:
    bool write_expansion(uint16_t addr, uint8_t data) {
        switch (addr) {
            // --- Pulse 1 ($5000-$5003) ---
            case 0x5000: pulse1_.write_control(data); return false;
            case 0x5001: return false;  // No sweep on MMC5
            case 0x5002: pulse1_.write_timer_low(data); return false;
            case 0x5003: pulse1_.write_timer_high(data); return false;

            // --- Pulse 2 ($5004-$5007) ---
            case 0x5004: pulse2_.write_control(data); return false;
            case 0x5005: return false;  // No sweep on MMC5
            case 0x5006: pulse2_.write_timer_low(data); return false;
            case 0x5007: pulse2_.write_timer_high(data); return false;

            // --- PCM ($5010-$5011) ---
            case 0x5010: return false;  // PCM mode/IRQ (not implemented)
            case 0x5011: pcm_value_ = data; return false;

            // --- Channel enable ---
            case 0x5015:
                audio_enable_ = data & 0x03;
                pulse1_.enabled = (data & 0x01) != 0;
                pulse2_.enabled = (data & 0x02) != 0;
                if (!pulse1_.enabled) pulse1_.length_counter = 0;
                if (!pulse2_.enabled) pulse2_.length_counter = 0;
                return false;

            // --- PRG mode ---
            case 0x5100:
                prg_mode_ = data & 0x03;
                return true;

            // --- CHR mode ---
            case 0x5101:
                chr_mode_ = data & 0x03;
                return true;

            // --- PRG-RAM protect ---
            case 0x5102:
                prg_ram_protect_1_ = (data & 0x03) == 0x02;
                return false;  // Doesn't change banking
            case 0x5103:
                prg_ram_protect_2_ = (data & 0x03) == 0x01;
                return false;

            // --- ExRAM mode ---
            case 0x5104:
                exram_mode_ = data & 0x03;
                return false;

            // --- Nametable mapping ---
            case 0x5105:
                nt_mapping_ = data;
                // Derive a Mirror mode for the cartridge layer.
                // The cartridge overrides nt_page from MIRROR_NT_PAGES for
                // standard patterns; FOUR_SCREEN preserves the mapper's raw
                // nt_page decode (required for ExRAM/fill nametable pages).
                // 0x50 = {0,0,1,1} = HORIZONTAL, 0x44 = {0,1,0,1} = VERTICAL
                if (data == 0x50) mirror_mode_ = Mirror::HORIZONTAL;
                else if (data == 0x44) mirror_mode_ = Mirror::VERTICAL;
                else if (data == 0x00) mirror_mode_ = Mirror::ONESCREEN_LO;
                else if (data == 0x55) mirror_mode_ = Mirror::ONESCREEN_HI;
                else mirror_mode_ = Mirror::FOUR_SCREEN;  // Custom mapping
                return true;

            // --- Fill-mode tile & attribute ---
            case 0x5106:
                fill_tile_ = data;
                rebuild_fill_page();
                return false;
            case 0x5107:
                fill_attr_ = data & 0x03;
                rebuild_fill_page();
                return false;

            // --- PRG bank registers ---
            case 0x5113: prg_bank_[0] = data & 0x07; return true;   // $6000-$7FFF RAM
            case 0x5114: prg_bank_[1] = data;         return true;   // $8000-$9FFF (mode 3)
            case 0x5115: prg_bank_[2] = data;         return true;   // $A000-$BFFF / $8000-$BFFF
            case 0x5116: prg_bank_[3] = data;         return true;   // $C000-$DFFF
            case 0x5117: prg_bank_[4] = data;         return true;   // $E000-$FFFF (always ROM)

            // --- CHR bank registers (sprite set $5120-$5127) ---
            // $5130 upper bits are OR'd in as bits 9-8
            case 0x5120: chr_bank_[0]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5121: chr_bank_[1]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5122: chr_bank_[2]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5123: chr_bank_[3]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5124: chr_bank_[4]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5125: chr_bank_[5]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5126: chr_bank_[6]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5127: chr_bank_[7]  = data | (chr_upper_bits_ << 8); return true;

            // --- CHR bank registers (BG set $5128-$512B) ---
            case 0x5128: chr_bank_[8]  = data | (chr_upper_bits_ << 8); return true;
            case 0x5129: chr_bank_[9]  = data | (chr_upper_bits_ << 8); return true;
            case 0x512A: chr_bank_[10] = data | (chr_upper_bits_ << 8); return true;
            case 0x512B: chr_bank_[11] = data | (chr_upper_bits_ << 8); return true;

            // --- CHR upper bank bits ---
            case 0x5130: chr_upper_bits_ = data & 0x03; return true;

            // --- Vertical split ---
            case 0x5200:
                split_enabled_ = (data & 0x80) != 0;
                split_right_   = (data & 0x40) != 0;
                split_tile_    = data & 0x1F;
                return false;
            case 0x5201: split_scroll_ = data; return false;
            case 0x5202: split_bank_   = data; return false;

            // --- IRQ ---
            case 0x5203:
                irq_scanline_ = data;
                return false;
            case 0x5204:
                irq_enabled_ = (data & 0x80) != 0;
                return false;

            // --- Multiplier ---
            case 0x5205:
                multiplicand_ = data;
                product_ = static_cast<uint16_t>(multiplicand_) * multiplier_;
                return false;
            case 0x5206:
                multiplier_ = data;
                product_ = static_cast<uint16_t>(multiplicand_) * multiplier_;
                return false;

            default:
                break;
        }

        // ExRAM write ($5C00-$5FFF) — stored at ciram_ + 0x800
        if (addr >= 0x5C00 && addr <= 0x5FFF && exram_) {
            if (exram_mode_ <= 1) {
                // Modes 0 & 1: writable during rendering (simplified: always writable)
                exram_[addr - 0x5C00] = data;
            } else if (exram_mode_ == 2) {
                // Mode 2: writable anytime
                exram_[addr - 0x5C00] = data;
            }
            // Mode 3: read-only
            return false;
        }

        return false;
    }
};

} // namespace nes_system
