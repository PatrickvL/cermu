/**
 * NES NSF Player — Implementation
 *
 * Builds 6502 player stubs and info pages for NSF music playback.
 *
 * NSF playback on the NES uses NMI (VBlank) for timing:
 *   - RESET vector → init stub (calls NSF init routine)
 *   - NMI vector   → play handler (calls NSF play routine each frame)
 *   - IRQ vector   → RTI (unused)
 *
 * The init stub runs once at startup, then enters an infinite wait loop.
 * Every VBlank, the PPU triggers NMI which calls the play routine.
 *
 * Memory layout in NES RAM ($0000-$07FF):
 *   $0700-$074F  — Init/playback stub
 *   $0750-$076F  — NMI handler (play routine wrapper)
 *   $0770-$077F  — IRQ handler
 */

#include "systems/nes/nsf/nes_nsf_player.hpp"
#include "systems/nes/cartridge/mappers/mapper_nsf.hpp"
#include "systems/nes/nes_screen_utils.hpp"
#include "systems/nes/nes_system.hpp"
#include "chip/cpu/fam65xx/asm6510.hpp"
#include <cstdio>
#include <cstring>

namespace nes_system {

// ============================================================================
// INFO PAGE — NES-native display of NSF metadata
// ============================================================================

void NsfPlayer::write_info_page(PPU* ppu,
                              const nsf_header_t* nsf,
                              uint16_t subtune) {
    if (!ppu || !nsf) return;

    // Initialize font and palette
    NesScreenUtils::init_font(ppu);
    NesScreenUtils::set_palette(ppu,
                           0x0F,   // Black background
                           0x30,   // White text
                           0x12,   // Blue accent
                           0x10);  // Light grey dim

    NesScreenUtils::clear(ppu);

    // ---- Header bar (row 1) ----
    NesScreenUtils::fill_row(ppu, 1, 0x01);  // Solid block tile for bar
    NesScreenUtils::write_text(ppu, 1, 2, "  NES  NSF  PLAYER  ");

    // ---- Title / Artist / Copyright (rows 4-8, raw bytes for NES display) ----
    NesScreenUtils::write_text(ppu, 4, 1, "TITLE:");
    NesScreenUtils::write_text_n(ppu, 5, 2, nsf->name_raw, 28);

    NesScreenUtils::write_text(ppu, 7, 1, "ARTIST:");
    NesScreenUtils::write_text_n(ppu, 8, 2, nsf->artist_raw, 28);

    NesScreenUtils::write_text(ppu, 10, 1, "COPYRIGHT:");
    NesScreenUtils::write_text_n(ppu, 11, 2, nsf->copyright_raw, 28);

    // ---- Separator ----
    NesScreenUtils::fill_row(ppu, 13, '-');

    // ---- Technical details (rows 14-18) ----

    // Row 14: Songs + Load address
    NesScreenUtils::write_text(ppu, 14, 1, "SONGS:");
    NesScreenUtils::write_dec(ppu, 14, 8, nsf->num_songs);
    NesScreenUtils::write_text(ppu, 14, 18, "LOAD:");
    NesScreenUtils::write_hex16(ppu, 14, 24, nsf->load_addr);

    // Row 15: Default + Init address
    NesScreenUtils::write_text(ppu, 15, 1, "DEFAULT:");
    NesScreenUtils::write_dec(ppu, 15, 10, nsf->start_song);
    NesScreenUtils::write_text(ppu, 15, 18, "INIT:");
    NesScreenUtils::write_hex16(ppu, 15, 24, nsf->init_addr);

    // Row 16: Region + Play address
    NesScreenUtils::write_text(ppu, 16, 1, "REGION:");
    const char* region_str = "NTSC";
    if (nsf->region_flags == NSF_REGION_PAL) region_str = "PAL";
    else if (nsf->region_flags == NSF_REGION_DUAL) region_str = "DUAL";
    NesScreenUtils::write_text(ppu, 16, 9, region_str);
    NesScreenUtils::write_text(ppu, 16, 18, "PLAY:");
    NesScreenUtils::write_hex16(ppu, 16, 24, nsf->play_addr);

    // Dim the address values on the right (palette 2 = light grey)
    for (int col = 18; col < 32; col += 2) {
        NesScreenUtils::set_attribute(ppu, 14, col, 2);
        NesScreenUtils::set_attribute(ppu, 16, col, 2);
    }

    // Row 17: Extra chips
    if (nsf->chip_flags) {
        NesScreenUtils::write_text(ppu, 17, 1, "CHIPS:");
        char chip_str[24] = "";
        if (nsf->chip_flags & NSF_CHIP_VRC6)      strcat(chip_str, "VRC6 ");
        if (nsf->chip_flags & NSF_CHIP_VRC7)       strcat(chip_str, "VRC7 ");
        if (nsf->chip_flags & NSF_CHIP_FDS)        strcat(chip_str, "FDS ");
        if (nsf->chip_flags & NSF_CHIP_MMC5)       strcat(chip_str, "MMC5 ");
        if (nsf->chip_flags & NSF_CHIP_NAMCO163)   strcat(chip_str, "N163 ");
        if (nsf->chip_flags & NSF_CHIP_SUNSOFT5B)  strcat(chip_str, "5B ");
        NesScreenUtils::write_text(ppu, 17, 8, chip_str);
    } else {
        NesScreenUtils::write_text(ppu, 17, 1, "CHIPS: 2A03 (STANDARD)");
    }

    // Row 18: Bankswitching
    if (nsf->uses_bankswitching) {
        NesScreenUtils::write_text(ppu, 18, 1, "BANKS: YES");
    }

    // ---- Separator ----
    NesScreenUtils::fill_row(ppu, 19, '-');

    // ---- Now playing (row 20-21) ----
    NesScreenUtils::write_text(ppu, 21, 1, "NOW PLAYING:");

    // Song number display: "SONG XX / YY"
    char song_buf[24];
    snprintf(song_buf, sizeof(song_buf), "SONG %d / %d",
             subtune + 1, nsf->num_songs);
    NesScreenUtils::write_text(ppu, 21, 14, song_buf);

    // ---- Key help (rows 25-27) ----
    NesScreenUtils::write_text(ppu, 25, 1, "1-9,0: SELECT SONG");
    NesScreenUtils::write_text(ppu, 26, 1, "LEFT/RIGHT: PREV/NEXT");
    NesScreenUtils::write_text(ppu, 27, 1, "ESC: EXIT");

    // ---- Bottom bar (row 29) ----
    NesScreenUtils::fill_row(ppu, 29, 0x01);

    // Enable PPU display
    NesScreenUtils::enable_display(ppu);
}

// ============================================================================
// NMI HANDLER — Calls NSF play routine on each VBlank
// ============================================================================
//
// PHA / TXA / PHA / TYA / PHA    — save registers
// JSR play_addr                   — call play routine
// PLA / TAY / PLA / TAX / PLA    — restore registers
// RTI                             — return from NMI
// ============================================================================

void NsfPlayer::build_nmi_handler(uint8_t* ram, uint16_t play_addr) {
    asm6510 a(ram + NMI_HANDLER, 0x20, NMI_HANDLER);

    a.pha();
    a.txa();
    a.pha();
    a.tya();
    a.pha();

    a.jsr(play_addr);             // JSR play_addr

    a.pla();
    a.tay();
    a.pla();
    a.tax();
    a.pla();
    a.rti();
}

// ============================================================================
// IRQ HANDLER — Simple RTI (NSF doesn't use IRQ by default)
// ============================================================================

void NsfPlayer::build_irq_handler(uint8_t* ram) {
    asm6510 a(ram + IRQ_HANDLER, 1, IRQ_HANDLER);
    a.rti();
}

// ============================================================================
// INIT STUB — Called once at startup via RESET vector
// ============================================================================
//
// SEI                              — disable interrupts during init
// CLD                              — clear decimal mode
// LDX #$FF / TXS                   — reset stack pointer
// LDA #$00 / STA $2000             — disable NMI during init
// STA $2001                        — disable rendering
// LDA #subtune / LDX #pal_flag     — set up init parameters
// JSR init_addr                    — call NSF init routine
// LDA #$80 / STA $2000             — enable NMI (VBlank → play routine)
// LDA #$00 / STA $2001             — keep rendering disabled (no sprites)
// idle: JMP idle                   — wait for NMI
// ============================================================================

void NsfPlayer::build_init_stub(uint8_t* ram,
                             const nsf_header_t* nsf,
                             uint16_t subtune,
                             bool is_pal) {
    asm6510 a(ram + STUB_BASE, 0x50, STUB_BASE);

    // --- CPU initialisation ---
    a.sei();
    a.cld();
    a.ldx_imm(0xFF);
    a.txs();

    // --- Disable PPU during init ---
    a.lda_imm(0x00);
    a.sta_abs(0x2000);            // STA $2000 (PPUCTRL)
    a.sta_abs(0x2001);            // STA $2001 (PPUMASK)

    // --- Silence APU channels ---
    a.lda_imm(0x00);
    a.sta_abs(0x4015);            // STA $4015 (APU status = all off)

    // --- Wait for PPU warm-up (two VBlanks) ---
    auto vbl1 = a.here();
    a.bit_abs(0x2002);            // BIT $2002
    a.bpl(vbl1);

    auto vbl2 = a.here();
    a.bit_abs(0x2002);            // BIT $2002
    a.bpl(vbl2);

    // --- Call NSF init routine ---
    // A = subtune number (0-based), X = PAL flag (0=NTSC, 1=PAL)
    a.lda_imm(static_cast<uint8_t>(subtune));
    a.ldx_imm(is_pal ? 0x01 : 0x00);
    a.jsr(nsf->init_addr);

    // --- Enable NMI for playback ---
    // PPU CTRL: bit 7 = NMI enable
    a.store_imm(0x2000, 0x80);

    // Enable background rendering so NMI fires
    a.store_imm(0x2001, 0x0A);

    // Enable APU channels (all standard channels on)
    a.store_imm(0x4015, 0x0F);

    // --- Idle loop (NMI will call play) ---
    a.jmp_self();
}

// ============================================================================
// VECTOR TABLE — Written to PRG-ROM buffer at $FFFA-$FFFF
// ============================================================================

int32_t NsfPlayer::map_vector_offset(
        uint16_t addr,
        bool bankswitched,
        const uint8_t bank_regs[8],
        uint16_t load_addr,
        size_t prg_rom_size) {
    if (addr < 0x8000) return -1;

    if (bankswitched) {
        int page = (addr - 0x8000) / 0x1000;  // 0-7
        uint32_t offset = static_cast<uint32_t>(bank_regs[page]) * 0x1000
                        + (addr & 0x0FFF);
        return (offset < prg_rom_size) ? static_cast<int32_t>(offset) : -1;
    } else {
        if (addr < load_addr) return -1;
        uint32_t offset = addr - load_addr;
        return (offset < prg_rom_size) ? static_cast<int32_t>(offset) : -1;
    }
}

void NsfPlayer::write_vectors(
        uint8_t* prg_rom,
        size_t prg_rom_size,
        bool bankswitched,
        const uint8_t bank_regs[8],
        uint16_t load_addr) {
    auto write_vec = [&](uint16_t vec_addr, uint16_t target) {
        int32_t lo = map_vector_offset(vec_addr,     bankswitched, bank_regs, load_addr, prg_rom_size);
        int32_t hi = map_vector_offset(vec_addr + 1, bankswitched, bank_regs, load_addr, prg_rom_size);
        if (lo >= 0) prg_rom[lo] = static_cast<uint8_t>(target & 0xFF);
        if (hi >= 0) prg_rom[hi] = static_cast<uint8_t>(target >> 8);
    };

    write_vec(VECTOR_NMI,   NMI_HANDLER);
    write_vec(VECTOR_RESET, STUB_BASE);
    write_vec(VECTOR_IRQ,   IRQ_HANDLER);
}

// ============================================================================
// CPU SETUP — Build stubs, write vectors, reset CPU
// ============================================================================

void NsfPlayer::setup_cpu(
        RICOH_2A03* cpu,
        uint8_t* cpu_ram,
        uint8_t* prg_rom,
        size_t prg_rom_size,
        bool bankswitched,
        const uint8_t bank_regs[8],
        uint16_t load_addr,
        const nsf_header_t* nsf,
        uint16_t subtune,
        bool is_pal) {
    if (!cpu || !cpu_ram || !nsf) return;

    // Build 6502 stubs in CPU RAM
    build_nmi_handler(cpu_ram, nsf->play_addr);
    build_irq_handler(cpu_ram);
    build_init_stub(cpu_ram, nsf, subtune, is_pal);

    // Write CPU vectors to ROM buffer
    write_vectors(prg_rom, prg_rom_size, bankswitched, bank_regs, load_addr);

    printf("NES NSF: Stub at $%04X, NMI handler at $%04X\n",
           STUB_BASE, NMI_HANDLER);

    // Reset CPU to RESET vector
    bus_state_t pins = NES_BUS_DEFAULT_STATE;
    cpu->reset(pins);
}

// ============================================================================
// SUBTUNE SWITCH — Lightweight re-init
// ============================================================================

void NsfPlayer::switch_subtune(
        RICOH_2A03* cpu,
        PPU* ppu,
        uint8_t* cpu_ram,
        uint8_t* prg_rom,
        size_t prg_rom_size,
        Mapper* mapper,
        const nsf_header_t* nsf,
        const uint8_t* payload,
        size_t payload_size,
        uint16_t subtune,
        bool is_pal) {
    if (!cpu || !ppu || !cpu_ram || !prg_rom || !nsf) return;

    // Reload NSF data into unified buffer (in case tune self-modified)
    if (payload && payload_size > 0) {
        size_t copy_size = (payload_size < prg_rom_size) ? payload_size : prg_rom_size;
        std::memset(prg_rom, 0, prg_rom_size);
        std::memcpy(prg_rom, payload, copy_size);
    }

    // Reset bank registers on mapper
    if (mapper) {
        auto* nsf_mapper = static_cast<MapperNsf*>(mapper);
        nsf_mapper->set_bank_regs(nsf->bankswitch);
    }

    // Determine bankswitching state for vector mapping
    bool bankswitched = false;
    for (int i = 0; i < 8; i++) {
        if (nsf->bankswitch[i] != 0) { bankswitched = true; break; }
    }

    // Re-write vectors (reload may have cleared them)
    write_vectors(prg_rom, prg_rom_size, bankswitched,
                  nsf->bankswitch, nsf->load_addr);

    // Clear CPU RAM (except stack) and rebuild stubs
    std::memset(cpu_ram, 0, 0x0100);
    std::memset(cpu_ram + 0x0200, 0, 0x0600);

    build_nmi_handler(cpu_ram, nsf->play_addr);
    build_irq_handler(cpu_ram);
    build_init_stub(cpu_ram, nsf, subtune, is_pal);

    // Update info page
    write_info_page(ppu, nsf, subtune);

    // Reset CPU
    bus_state_t pins = NES_BUS_DEFAULT_STATE;
    cpu->reset(pins);

    printf("NES NSF: Switched to subtune %d/%d\n",
           subtune + 1, nsf->num_songs);
}

} // namespace nes_system
