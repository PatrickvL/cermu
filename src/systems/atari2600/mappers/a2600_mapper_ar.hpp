#pragma once
/*
 * a2600_mapper_ar.h — Atari 2600 AR (Starpath Supercharger)
 *
 * The Starpath Supercharger is a cartridge with 6KB RAM (3 × 2KB banks)
 * and a 2KB BIOS ROM.  Game data is loaded from tape; ROM dumps contain
 * one or more 8448-byte "loads" (8192 bytes of page data + 256-byte header).
 *
 * Memory layout (8KB working image):
 *   image_[0x0000–0x17FF]  3 × 2KB RAM banks (banks 0–2)
 *   image_[0x1800–0x1FFF]  2KB BIOS ROM      (bank 3, provided by emulator)
 *
 * Address space — two 2KB windows:
 *   $F000–$F7FF  Window 0 (maps any bank)
 *   $F800–$FFFF  Window 1 (maps any bank)
 *
 * Bank configuration via $FFF8 hotspot (offset $0FF8):
 *   D7–D5  Write pulse delay (ignored in emulation)
 *   D4–D2  Bank config index (see OFFSET tables)
 *   D1     Write enable (1 = RAM writes via data-hold mechanism)
 *   D0     ROM power  (0 = on, 1 = off — cosmetic in emulation)
 *
 *   Config Index:   0     1     2     3     4     5     6     7
 *   ──────────────────────────────────────────────────────────────
 *   Window 0:      RAM2  RAM0  RAM2  RAM0  RAM2  RAM1  RAM2  RAM1
 *   Window 1:      ROM   ROM   RAM0  RAM2  ROM   ROM   RAM1  RAM2
 *
 * Write mechanism:
 *   1. CPU reads $F000–$F0FF → data-hold register = address low byte,
 *      write pending armed (only when writes disabled OR no pending write).
 *   2. After exactly 5 distinct CPU accesses, the data-hold byte is
 *      written to the RAM byte at the current cart read/write address.
 *   3. Any access to $FFF8 cancels the pending write and applies the
 *      data-hold register as a bank configuration byte.
 *
 * Multiload:
 *   Each 8448-byte load in the ROM file has a 256-byte header at offset
 *   8192.  Header byte 5 is the load identifier.  Load 0 is auto-loaded
 *   on reset.  Subsequent loads are triggered by reading $F850 while ROM
 *   is mapped to the high bank; the load number is captured from writes
 *   to zero-page $80 via bus snoop.
 *
 * Games: Phaser Patrol, Communist Mutants from Space, Suicide Mission,
 *        Killer Satellites, Escape from the Mindmaster, Fireball,
 *        Party Mix, Sword of Saros, Dragonstomper, etc.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include <cstring>

struct A2600MapperAR : public A2600Mapper {
    static constexpr uint32_t BANK_SIZE  = 2048;
    static constexpr uint32_t RAM_SIZE   = 3 * BANK_SIZE;             // 6KB
    static constexpr uint32_t ROM_BASE   = RAM_SIZE;                  // offset 6144
    static constexpr uint32_t IMAGE_SIZE = RAM_SIZE + BANK_SIZE;      // 8KB
    static constexpr uint32_t LOAD_SIZE  = 8448;                      // per multiload

    uint8_t read(uint16_t offset) override {
        // Multiload trigger: $F850 (offset $0850) when ROM is mapped high
        if (offset == 0x0850 && bank_offset_[1] == ROM_BASE) {
            load_into_ram(pending_load_);
            return image_[(offset & 0x07FF) + bank_offset_[1]];
        }

        process_access(offset);
        return image_[(offset & 0x07FF) + bank_offset_[(offset & 0x0800) ? 1 : 0]];
    }

    void write(uint16_t offset, uint8_t /*data*/) override {
        process_access(offset);
    }

    void bus_snoop(uint16_t addr, uint8_t data, bool is_write) override {
        // Count distinct accesses for the 5-access write delay
        if (addr != last_addr_) {
            distinct_count_++;
            last_addr_ = addr;
        }

        // Capture writes to RIOT RAM location $80 (multiload number).
        // RIOT RAM select: A12=0, A9=0, A7=1; offset within RAM = addr & 0x7F.
        if (is_write && (addr & 0x12FF) == 0x0080)
            pending_load_ = data;
    }

    bool needs_bus_snoop() const override { return true; }

    const char* name() const override { return "AR"; }

    void reset() override {
        memset(image_, 0, IMAGE_SIZE);
        init_bios();
        bank_offset_[0] = 2 * BANK_SIZE;   // Window 0 → RAM bank 2
        bank_offset_[1] = ROM_BASE;         // Window 1 → BIOS ROM
        write_enabled_ = false;
        power_ = true;
        data_hold_ = 0;
        write_pending_ = false;
        write_access_mark_ = 0;
        distinct_count_ = 0;
        last_addr_ = 0xFFFF;
        pending_load_ = 0;
        current_bank_ = 0;

        // Auto-load initial game data (load 0)
        if (rom_ && rom_size_ >= LOAD_SIZE)
            load_into_ram(0);
    }

    uint8_t current_bank() const override { return current_bank_; }
    uint8_t bank_count() const override { return 32; }

private:
    uint8_t image_[IMAGE_SIZE] = {};

    // Bank mapping: offsets into image_ for each 2KB window
    uint32_t bank_offset_[2] = {};

    // Write mechanism state
    uint8_t data_hold_ = 0;
    bool write_pending_ = false;
    bool write_enabled_ = false;
    uint32_t write_access_mark_ = 0;
    uint32_t distinct_count_ = 0;
    uint16_t last_addr_ = 0xFFFF;

    // Multiload
    uint8_t pending_load_ = 0;

    // Display state
    bool power_ = true;
    uint8_t current_bank_ = 0;

    // Bank configuration offset tables.
    //   Index:           0      1      2      3      4      5      6      7
    //   Window 0:       RAM2   RAM0   RAM2   RAM0   RAM2   RAM1   RAM2   RAM1
    //   Window 1:       ROM    ROM    RAM0   RAM2   ROM    ROM    RAM1   RAM2
    static constexpr uint32_t OFFSET_0[8] = {
        2 * BANK_SIZE, 0 * BANK_SIZE, 2 * BANK_SIZE, 0 * BANK_SIZE,
        2 * BANK_SIZE, 1 * BANK_SIZE, 2 * BANK_SIZE, 1 * BANK_SIZE
    };
    static constexpr uint32_t OFFSET_1[8] = {
        3 * BANK_SIZE, 3 * BANK_SIZE, 0 * BANK_SIZE, 2 * BANK_SIZE,
        3 * BANK_SIZE, 3 * BANK_SIZE, 1 * BANK_SIZE, 2 * BANK_SIZE
    };

    inline void set_bank_config(uint8_t config) {
        uint8_t idx = (config >> 2) & 0x07;
        bank_offset_[0] = OFFSET_0[idx];
        bank_offset_[1] = OFFSET_1[idx];
        write_enabled_ = (config & 0x02) != 0;
        power_ = !(config & 0x01);
        current_bank_ = config & 0x1F;
    }

    inline void process_access(uint16_t offset) {
        // Cancel overdue pending writes (> 5 distinct accesses)
        if (write_pending_ && (distinct_count_ > write_access_mark_ + 5))
            write_pending_ = false;

        // Data hold register: CPU accesses offset $000–$0FF
        if (!(offset & 0x0F00) && (!write_enabled_ || !write_pending_)) {
            data_hold_ = static_cast<uint8_t>(offset);
            write_access_mark_ = distinct_count_;
            write_pending_ = true;
        }
        // Bank configuration hotspot: offset $0FF8
        else if (offset == 0x0FF8) {
            write_pending_ = false;
            set_bank_config(data_hold_);
        }
        // Commit pending write on exactly the 5th distinct access
        else if (write_enabled_ && write_pending_ &&
                 (distinct_count_ == write_access_mark_ + 5)) {
            if (!(offset & 0x0800))
                image_[(offset & 0x07FF) + bank_offset_[0]] = data_hold_;
            else if (bank_offset_[1] != ROM_BASE)   // can't write to ROM
                image_[(offset & 0x07FF) + bank_offset_[1]] = data_hold_;
            write_pending_ = false;
        }
    }

    // Load a multiload image into RAM.  Scans all loads in the ROM file for
    // one whose header byte 5 matches load_num, copies pages into the RAM
    // banks, and stores the start address + bank config in the BIOS data area.
    void load_into_ram(uint8_t load_num) {
        if (!rom_ || rom_size_ < LOAD_SIZE) return;

        uint32_t num_loads = rom_size_ / LOAD_SIZE;

        for (uint32_t img = 0; img < num_loads; ++img) {
            uint32_t img_off = img * LOAD_SIZE;
            const uint8_t* header = rom_ + img_off + IMAGE_SIZE;

            if (header[5] != load_num) continue;

            // Copy pages into RAM banks
            uint8_t num_pages = header[3];
            for (uint8_t p = 0; p < num_pages && p < 32; ++p) {
                uint8_t ctrl = header[16 + p];
                uint8_t bank = ctrl & 0x03;
                uint8_t page = (ctrl >> 2) & 0x07;

                if (bank < 3) {
                    const uint8_t* src = rom_ + img_off + p * 256;
                    uint8_t* dst = image_ + bank * BANK_SIZE + page * 256;
                    memcpy(dst, src, 256);
                }
            }

            // Publish start address and bank config in BIOS data area
            // ($FFF0–$FFF2 = ROM offset $7F0–$7F2)
            image_[ROM_BASE + 0x7F0] = header[0];   // start address low
            image_[ROM_BASE + 0x7F1] = header[1];   // start address high
            image_[ROM_BASE + 0x7F2] = header[2];   // bank configuration
            return;
        }
    }

    // Populate the 2KB BIOS ROM with a minimal clean-room boot program.
    //
    // Reset entry ($F800):
    //   Initializes CPU, reads start address and bank config from the BIOS
    //   data area ($FFF0–$FFF2), builds a small trampoline in zero-page RAM,
    //   then executes it.  The trampoline sets the data-hold register (by
    //   accessing $F000+config), triggers the bank switch ($FFF8), and does
    //   an indirect jump through the start address at $FA/$FB.
    //
    // Multiload entry ($F850):
    //   NOP (the mapper intercepts this read and copies the new load into
    //   RAM), then falls into the same bank-switch-and-jump sequence.
    void init_bios() {
        uint8_t* bios = image_ + ROM_BASE;

        // Fill with JAM opcode ($02) to catch stray execution
        memset(bios, 0x02, BANK_SIZE);

        // ── Reset entry at $F800 (ROM offset 0x000) ───────────────────────
        //
        // Trampoline (written to zero page $F0–$F8):
        //   $F0: BD 00 F0   LDA $F000,X  — set data-hold register to X
        //   $F3: AD F8 FF   LDA $FFF8    — trigger bank switch
        //   $F6: 6C FA 00   JMP ($00FA)  — indirect jump to game start
        static constexpr uint8_t reset_code[] = {
            0x78,                           // SEI
            0xD8,                           // CLD
            0xA2, 0xFF,                     // LDX #$FF
            0x9A,                           // TXS
            // Read start address and bank config from BIOS data area
            0xAD, 0xF0, 0xFF,               // LDA $FFF0  (start addr low)
            0x85, 0xFA,                     // STA $FA
            0xAD, 0xF1, 0xFF,               // LDA $FFF1  (start addr high)
            0x85, 0xFB,                     // STA $FB
            0xAE, 0xF2, 0xFF,               // LDX $FFF2  (bank config)
            // Build trampoline at $F0–$F8
            0xA9, 0xBD,                     // LDA #$BD   (LDA abs,X)
            0x85, 0xF0,                     // STA $F0
            0xA9, 0x00,                     // LDA #$00
            0x85, 0xF1,                     // STA $F1
            0xA9, 0xF0,                     // LDA #$F0
            0x85, 0xF2,                     // STA $F2    → BD 00 F0
            0xA9, 0xAD,                     // LDA #$AD   (LDA abs)
            0x85, 0xF3,                     // STA $F3
            0xA9, 0xF8,                     // LDA #$F8
            0x85, 0xF4,                     // STA $F4
            0xA9, 0xFF,                     // LDA #$FF
            0x85, 0xF5,                     // STA $F5    → AD F8 FF
            0xA9, 0x6C,                     // LDA #$6C   (JMP indirect)
            0x85, 0xF6,                     // STA $F6
            0xA9, 0xFA,                     // LDA #$FA
            0x85, 0xF7,                     // STA $F7
            0xA9, 0x00,                     // LDA #$00
            0x85, 0xF8,                     // STA $F8    → 6C FA 00
            0x4C, 0xF0, 0x00,               // JMP $00F0  (execute trampoline)
        };
        memcpy(bios, reset_code, sizeof(reset_code));

        // ── Multiload trigger at $F850 (ROM offset 0x050) ─────────────────
        bios[0x050] = 0xEA;                 // NOP  (mapper loads data here)
        bios[0x051] = 0x4C;                 // JMP  $F805
        bios[0x052] = 0x05;
        bios[0x053] = 0xF8;

        // ── BIOS data area at $FFF0 (ROM offset 0x7F0) ───────────────────
        bios[0x7F0] = 0x00;                 // start address low
        bios[0x7F1] = 0xF8;                 // start address high (default)
        bios[0x7F2] = 0x00;                 // bank config (default)

        // ── 6502 vectors ──────────────────────────────────────────────────
        bios[0x7FA] = 0x00; bios[0x7FB] = 0xF8;  // NMI   → $F800
        bios[0x7FC] = 0x00; bios[0x7FD] = 0xF8;  // RESET → $F800
        bios[0x7FE] = 0x00; bios[0x7FF] = 0xF8;  // IRQ   → $F800
    }
};
