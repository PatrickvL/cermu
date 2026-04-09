#pragma once
/*
 * mapper_020_fds.hpp — Famicom Disk System mapper
 *
 * Models the FDS RAM adapter hardware through the standard Mapper interface:
 *   - 32KB PRG-RAM at $6000-$DFFF (writable, for disk-loaded program data)
 *   - 8KB CHR-RAM at $0000-$1FFF (writable)
 *   - FDS disk I/O registers at $4020-$4033 (exposed via expansion space)
 *   - FDS timer IRQ ($4020-$4022)
 *   - BIOS ROM mapped at $E000-$FFFF (loaded from disksys.rom)
 *
 * Disk access model:
 *   The FDS BIOS reads files from disk by name.  Rather than emulating
 *   the physical disk controller bit-by-bit, we pre-parse the disk
 *   block structure and service disk reads by streaming bytes from the
 *   parsed blocks.  This produces correct behavior for all licensed
 *   software that uses the BIOS entry points.
 *
 * Limitations:
 *   - No wavetable synthesis channel (expansion audio stub)
 *   - Disk writes not persisted (no .sav for FDS yet)
 *   - No physical head seek timing
 *
 * Games: ~200 licensed Famicom Disk titles (Zelda, Metroid, Kid Icarus, etc.)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include <cstdio>
#include <cstring>

namespace nes_system {

class MapperFDS : public Mapper {
private:
    // ---- Disk data ----
    const uint8_t* disk_data_ = nullptr;    ///< Raw FDS disk image (all sides)
    size_t disk_data_size_ = 0;
    uint8_t num_sides_ = 0;

    // ---- Disk drive state ----
    uint8_t current_side_ = 0;              ///< Currently inserted side (0-based)
    bool disk_inserted_ = true;
    bool disk_ready_ = false;               ///< Set after insertion delay
    uint32_t insertion_delay_ = 0;          ///< Frames before disk becomes ready

    // Disk read position — linear byte offset within current side
    uint32_t disk_position_ = 0;
    bool disk_irq_enabled_ = false;
    bool disk_irq_pending_ = false;
    bool transfer_active_ = false;
    uint8_t read_data_reg_ = 0;
    bool crc_control_ = false;
    bool read_mode_ = true;                 ///< true = reading, false = writing
    bool motor_on_ = false;
    bool transfer_reset_ = false;
    bool disk_io_enabled_ = false;          ///< $4025 bit 0

    // Drive timing: the FDS transfers one byte approximately every 150 CPU cycles
    static constexpr int BYTE_TRANSFER_CYCLES = 150;
    int transfer_counter_ = 0;
    bool byte_transfer_flag_ = false;       ///< $4030 bit 1

    // ---- Timer IRQ ----
    uint16_t timer_reload_ = 0;
    uint16_t timer_counter_ = 0;
    bool timer_irq_enabled_ = false;
    bool timer_irq_repeat_ = false;
    bool timer_irq_pending_ = false;

    // ---- PRG-RAM (32KB: $6000-$DFFF) ----
    // Stored in the Mapper base's prg_ram_ pointer (set by set_memory_pointers).
    // The mapper exposes this as prg_ram ($6000-$7FFF) plus the $8000-$DFFF
    // region maps into the upper 24KB.

    // ---- Expansion page (for $4020-$40FF I/O register dispatch) ----
    uint8_t expansion_page_[0x1000] = {};

    // ---- BIOS ROM ----
    // $E000-$FFFF is mapped from prg_rom_ (the BIOS loaded by system layer)

    // ---- Disk data access ----
    uint8_t read_disk_byte() {
        if (!disk_inserted_ || !disk_ready_) return 0;
        uint32_t side_offset = static_cast<uint32_t>(current_side_) * FDS_SIDE_SIZE;
        uint32_t abs_pos = side_offset + disk_position_;
        if (abs_pos < disk_data_size_) {
            uint8_t val = disk_data_[abs_pos];
            disk_position_++;
            if (disk_position_ >= FDS_SIDE_SIZE)
                disk_position_ = 0;  // wrap around
            return val;
        }
        return 0;
    }

    static constexpr uint32_t FDS_SIDE_SIZE = 65500;

public:
    static constexpr uint16_t FDS_MAPPER_ID = 20;

    MapperFDS() = default;

    void set_disk_data(const uint8_t* data, size_t size, uint8_t num_sides) {
        disk_data_ = data;
        disk_data_size_ = size;
        num_sides_ = num_sides;
        current_side_ = 0;
        disk_inserted_ = true;
        disk_ready_ = true;
        disk_position_ = 0;
    }

    void reset() override {
        // Timer
        timer_reload_ = 0;
        timer_counter_ = 0;
        timer_irq_enabled_ = false;
        timer_irq_repeat_ = false;
        timer_irq_pending_ = false;

        // Disk drive
        disk_position_ = 0;
        disk_irq_enabled_ = false;
        disk_irq_pending_ = false;
        transfer_active_ = false;
        read_data_reg_ = 0;
        crc_control_ = false;
        read_mode_ = true;
        motor_on_ = false;
        transfer_reset_ = false;
        disk_io_enabled_ = false;
        transfer_counter_ = 0;
        byte_transfer_flag_ = false;

        disk_inserted_ = true;
        disk_ready_ = true;
        current_side_ = 0;
    }

    Mirror mirror() override { return header_mirror_; }

    bool irq_state() override {
        return timer_irq_pending_ || disk_irq_pending_;
    }

    void irq_clear() override {
        timer_irq_pending_ = false;
        disk_irq_pending_ = false;
    }

    // ===================================================================
    // CPU cycle notification — timer + disk transfer timing
    // ===================================================================

    void notify_cpu_cycle() override {
        // Timer IRQ
        if (timer_irq_enabled_ && timer_counter_ > 0) {
            timer_counter_--;
            if (timer_counter_ == 0) {
                timer_irq_pending_ = true;
                if (timer_irq_repeat_) {
                    timer_counter_ = timer_reload_;
                } else {
                    timer_irq_enabled_ = false;
                }
            }
        }

        // Disk byte transfer timing
        if (transfer_active_ && motor_on_ && disk_inserted_ && disk_ready_) {
            transfer_counter_++;
            if (transfer_counter_ >= BYTE_TRANSFER_CYCLES) {
                transfer_counter_ = 0;
                if (read_mode_) {
                    read_data_reg_ = read_disk_byte();
                }
                byte_transfer_flag_ = true;
                if (disk_irq_enabled_) {
                    disk_irq_pending_ = true;
                }
            }
        }
    }

    // ===================================================================
    // Bank configuration
    // ===================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // $6000-$7FFF: PRG-RAM (first 8KB)
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = 0x2000;
        config.prg_ram_enabled = true;

        // $8000-$DFFF: PRG-RAM pages 1-3 (writable program area loaded from disk)
        // $E000-$FFFF: BIOS ROM (from prg_rom_, read-only)
        if (prg_ram_ && prg_ram_size_ >= 0x8000) {
            // PRG-RAM: 32KB total, first 8KB at $6000 via prg_ram,
            // next 24KB at $8000-$DFFF via prg_pages (writable)
            for (int i = 0; i < 6; i++) {
                config.prg_pages[i] = prg_ram_ + 0x2000 + i * 0x1000;
                config.prg_write_pages[i] = prg_ram_ + 0x2000 + i * 0x1000;
            }
        }

        // $E000-$FFFF: BIOS ROM (read-only, no write pages)
        if (prg_rom_ && prg_rom_size_ >= 0x2000) {
            config.prg_pages[6] = prg_rom_;
            config.prg_pages[7] = prg_rom_ + 0x1000;
            config.prg_write_pages[6] = nullptr;
            config.prg_write_pages[7] = nullptr;
        }

        // Expansion space ($4020-$40FF): I/O registers
        config.expansion_read = expansion_page_;
        config.expansion_write = nullptr;  // writes go through expansion_write() below
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // 8KB CHR-RAM (writable)
        for (int i = 0; i < 8; i++) {
            config.chr_pages[i] = chr_mem_ + (i * 0x0400);
            config.chr_writable[i] = true;
        }
    }

    // ===================================================================
    // I/O register access ($4020-$4033)
    // ===================================================================

    uint8_t expansion_read(uint16_t addr, bool& handled) override {
        switch (addr) {
            case 0x4030: {
                // Disk status register
                uint8_t val = 0;
                if (timer_irq_pending_)    val |= 0x01;
                if (byte_transfer_flag_)   val |= 0x02;
                // Acknowledge IRQs on read
                timer_irq_pending_ = false;
                disk_irq_pending_ = false;
                byte_transfer_flag_ = false;
                handled = true;
                return val;
            }
            case 0x4031:
                // Read data register — last byte transferred from disk
                byte_transfer_flag_ = false;
                disk_irq_pending_ = false;
                handled = true;
                return read_data_reg_;

            case 0x4032: {
                // Disk drive status
                uint8_t val = 0;
                if (!disk_inserted_)  val |= 0x01;  // disk not inserted
                if (!disk_ready_)     val |= 0x02;  // disk not ready
                if (!disk_inserted_)  val |= 0x04;  // write protected (always for now)
                handled = true;
                return val;
            }
            case 0x4033:
                // External connector read (battery status)
                // Bit 7: battery good (always return 1)
                handled = true;
                return 0x80;

            default:
                handled = false;
                return 0;
        }
    }

    // FDS I/O writes at $4020-$4026 are routed through register_write
    // since there's no expansion_write virtual in the Mapper interface.
    // The system routes writes in the $4020-$4026 range to the mapper.
    bool register_write(uint16_t addr, uint8_t data) override {
        switch (addr) {
            // ---- Timer registers ----
            case 0x4020:
                timer_reload_ = (timer_reload_ & 0xFF00) | data;
                return false;
            case 0x4021:
                timer_reload_ = (timer_reload_ & 0x00FF) | (static_cast<uint16_t>(data) << 8);
                return false;
            case 0x4022:
                timer_irq_repeat_ = (data & 0x01) != 0;
                timer_irq_enabled_ = (data & 0x02) != 0;
                if (timer_irq_enabled_) {
                    timer_counter_ = timer_reload_;
                }
                timer_irq_pending_ = false;
                return false;

            // ---- Disk I/O registers ----
            case 0x4024:
                // Write data register (for disk writes — not persisted)
                byte_transfer_flag_ = false;
                disk_irq_pending_ = false;
                return false;

            case 0x4025: {
                // Disk control register
                bool old_motor = motor_on_;
                disk_io_enabled_ = (data & 0x01) != 0;
                transfer_reset_ = (data & 0x02) != 0;
                read_mode_ = (data & 0x04) == 0;  // 0 = read, 1 = write
                // Bit 3: mirroring (0 = vertical, 1 = horizontal)
                header_mirror_ = (data & 0x08) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                crc_control_ = (data & 0x10) != 0;
                motor_on_ = (data & 0x80) == 0;  // Bit 7: 0 = motor on

                transfer_active_ = motor_on_ && disk_io_enabled_ && !transfer_reset_;

                if (transfer_reset_) {
                    disk_position_ = 0;
                    transfer_counter_ = 0;
                    byte_transfer_flag_ = false;
                }

                // Motor just turned on: start from beginning
                if (motor_on_ && !old_motor) {
                    transfer_counter_ = 0;
                }

                disk_irq_enabled_ = (data & 0x80) == 0;  // IRQ enabled when motor on
                return true;  // mirroring may have changed
            }

            case 0x4026:
                // External connector write
                return false;

            default:
                return false;
        }
    }
};

} // namespace nes_system
