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
 *   $0770-$077F  — Vectors (mapped by NsfCartridge)
 */

#include "nes_nsf_player.h"
#include "nes_nsf_cartridge.h"
#include "nes_screen_utils.h"
#include "nes_system.h"
#include "../../chip/cpu/fam65xx/nes6502.h"
#include "asm6510.h"
#include <cstdio>
#include <cstring>

namespace nes_system {

// ============================================================================
// STUB ADDRESSES IN NES CPU RAM
// ============================================================================

static constexpr uint16_t STUB_BASE    = 0x0700;  // Init stub
static constexpr uint16_t NMI_HANDLER  = 0x0750;  // NMI play handler
static constexpr uint16_t IRQ_HANDLER  = 0x0770;  // IRQ handler (just RTI)

// NES CPU vectors (in ROM space — written to NsfCartridge)
static constexpr uint16_t VECTOR_NMI   = 0xFFFA;
static constexpr uint16_t VECTOR_RESET = 0xFFFC;
static constexpr uint16_t VECTOR_IRQ   = 0xFFFE;

// ============================================================================
// INFO PAGE — NES-native display of NSF metadata
// ============================================================================

void nes_write_nsf_info_page(PPU* ppu,
                              const nsf_header_t* nsf,
                              uint16_t subtune) {
    if (!ppu || !nsf) return;

    // Initialize font and palette
    nes_screen_init_font(ppu);
    nes_screen_set_palette(ppu,
                           0x0F,   // Black background
                           0x30,   // White text
                           0x12,   // Blue accent
                           0x10);  // Light grey dim

    nes_screen_clear(ppu);

    // ---- Header bar (row 1) ----
    nes_screen_fill_row(ppu, 1, 0x01);  // Solid block tile for bar
    nes_screen_write_text(ppu, 1, 2, "  NES  NSF  PLAYER  ");

    // ---- Title / Artist / Copyright (rows 4-8, raw bytes for NES display) ----
    nes_screen_write_text(ppu, 4, 1, "TITLE:");
    nes_screen_write_text_n(ppu, 5, 2, nsf->name_raw, 28);

    nes_screen_write_text(ppu, 7, 1, "ARTIST:");
    nes_screen_write_text_n(ppu, 8, 2, nsf->artist_raw, 28);

    nes_screen_write_text(ppu, 10, 1, "COPYRIGHT:");
    nes_screen_write_text_n(ppu, 11, 2, nsf->copyright_raw, 28);

    // ---- Separator ----
    nes_screen_fill_row(ppu, 13, '-');

    // ---- Technical details (rows 14-18) ----

    // Row 14: Songs + Load address
    nes_screen_write_text(ppu, 14, 1, "SONGS:");
    nes_screen_write_dec(ppu, 14, 8, nsf->num_songs);
    nes_screen_write_text(ppu, 14, 18, "LOAD:");
    nes_screen_write_hex16(ppu, 14, 24, nsf->load_addr);

    // Row 15: Default + Init address
    nes_screen_write_text(ppu, 15, 1, "DEFAULT:");
    nes_screen_write_dec(ppu, 15, 10, nsf->start_song);
    nes_screen_write_text(ppu, 15, 18, "INIT:");
    nes_screen_write_hex16(ppu, 15, 24, nsf->init_addr);

    // Row 16: Region + Play address
    nes_screen_write_text(ppu, 16, 1, "REGION:");
    const char* region_str = "NTSC";
    if (nsf->region_flags == NSF_REGION_PAL) region_str = "PAL";
    else if (nsf->region_flags == NSF_REGION_DUAL) region_str = "DUAL";
    nes_screen_write_text(ppu, 16, 9, region_str);
    nes_screen_write_text(ppu, 16, 18, "PLAY:");
    nes_screen_write_hex16(ppu, 16, 24, nsf->play_addr);

    // Dim the address values on the right (palette 2 = light grey)
    for (int col = 18; col < 32; col += 2) {
        nes_screen_set_attribute(ppu, 14, col, 2);
        nes_screen_set_attribute(ppu, 16, col, 2);
    }

    // Row 17: Extra chips
    if (nsf->chip_flags) {
        nes_screen_write_text(ppu, 17, 1, "CHIPS:");
        char chip_str[24] = "";
        if (nsf->chip_flags & NSF_CHIP_VRC6)      strcat(chip_str, "VRC6 ");
        if (nsf->chip_flags & NSF_CHIP_VRC7)       strcat(chip_str, "VRC7 ");
        if (nsf->chip_flags & NSF_CHIP_FDS)        strcat(chip_str, "FDS ");
        if (nsf->chip_flags & NSF_CHIP_MMC5)       strcat(chip_str, "MMC5 ");
        if (nsf->chip_flags & NSF_CHIP_NAMCO163)   strcat(chip_str, "N163 ");
        if (nsf->chip_flags & NSF_CHIP_SUNSOFT5B)  strcat(chip_str, "5B ");
        nes_screen_write_text(ppu, 17, 8, chip_str);
    } else {
        nes_screen_write_text(ppu, 17, 1, "CHIPS: 2A03 (STANDARD)");
    }

    // Row 18: Bankswitching
    if (nsf->uses_bankswitching) {
        nes_screen_write_text(ppu, 18, 1, "BANKS: YES");
    }

    // ---- Separator ----
    nes_screen_fill_row(ppu, 19, '-');

    // ---- Now playing (row 20-21) ----
    nes_screen_write_text(ppu, 21, 1, "NOW PLAYING:");

    // Song number display: "SONG XX / YY"
    char song_buf[24];
    snprintf(song_buf, sizeof(song_buf), "SONG %d / %d",
             subtune + 1, nsf->num_songs);
    nes_screen_write_text(ppu, 21, 14, song_buf);

    // ---- Key help (rows 25-27) ----
    nes_screen_write_text(ppu, 25, 1, "1-9,0: SELECT SONG");
    nes_screen_write_text(ppu, 26, 1, "LEFT/RIGHT: PREV/NEXT");
    nes_screen_write_text(ppu, 27, 1, "ESC: EXIT");

    // ---- Bottom bar (row 29) ----
    nes_screen_fill_row(ppu, 29, 0x01);

    // Enable PPU display
    nes_screen_enable_display(ppu);
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

static void build_nmi_handler(uint8_t* ram, uint16_t play_addr) {
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

static void build_irq_handler(uint8_t* ram) {
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

static void build_init_stub(uint8_t* ram,
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
// VECTOR TABLE — Written to NsfCartridge ROM at $FFFA-$FFFF
// ============================================================================

static void write_vectors(NsfCartridge* cart) {
    // NMI vector ($FFFA) → NMI handler in RAM
    cart->write_rom_direct(VECTOR_NMI,     static_cast<uint8_t>(NMI_HANDLER & 0xFF));
    cart->write_rom_direct(VECTOR_NMI + 1, static_cast<uint8_t>(NMI_HANDLER >> 8));

    // RESET vector ($FFFC) → init stub in RAM
    cart->write_rom_direct(VECTOR_RESET,     static_cast<uint8_t>(STUB_BASE & 0xFF));
    cart->write_rom_direct(VECTOR_RESET + 1, static_cast<uint8_t>(STUB_BASE >> 8));

    // IRQ vector ($FFFE) → IRQ handler in RAM
    cart->write_rom_direct(VECTOR_IRQ,     static_cast<uint8_t>(IRQ_HANDLER & 0xFF));
    cart->write_rom_direct(VECTOR_IRQ + 1, static_cast<uint8_t>(IRQ_HANDLER >> 8));
}

// ============================================================================
// NSF LOAD — Full setup
// ============================================================================

std::shared_ptr<NsfCartridge> nes_apply_nsf_load(
    nes6502_t* cpu,
    PPU* ppu,
    MemoryBus* bus,
    const nsf_header_t* nsf,
    const program_data_t* prog,
    uint16_t subtune,
    bool is_pal) {

    if (!cpu || !ppu || !bus || !nsf) return nullptr;

    printf("NES NSF: Loading \"%s\" by %s\n", nsf->name, nsf->artist);
    printf("NES NSF: load=$%04X init=$%04X play=$%04X songs=%d start=%d\n",
           nsf->load_addr, nsf->init_addr, nsf->play_addr,
           nsf->num_songs, nsf->start_song);

    // ---- Step 1: Create NsfCartridge ----
    auto nsf_cart = std::make_shared<NsfCartridge>(
        prog->data, prog->data_size, nsf->load_addr, nsf->bankswitch);

    // Write CPU vectors to the cartridge ROM space
    write_vectors(nsf_cart.get());

    // Connect cartridge to bus and PPU
    bus->connect_cartridge(nsf_cart);
    ppu->connect_cartridge(nsf_cart);

    // ---- Step 2: Reset bus (clears CPU RAM) ----
    bus->reset();

    // ---- Step 3: Build 6502 stubs in CPU RAM ----
    uint8_t* ram = bus->cpu_ram.data();

    build_nmi_handler(ram, nsf->play_addr);
    build_irq_handler(ram);
    build_init_stub(ram, nsf, subtune, is_pal);

    printf("NES NSF: Stub at $%04X, NMI handler at $%04X\n",
           STUB_BASE, NMI_HANDLER);

    // ---- Step 4: Write info page to PPU nametable ----
    nes_write_nsf_info_page(ppu, nsf, subtune);

    // ---- Step 5: Reset CPU to RESET vector ----
    bus_state_t pins = NES_BUS_DEFAULT_STATE;
    nes6502_reset(cpu, pins);

    // After reset, CPU reads RESET vector ($FFFC/$FFFD)
    // which points to STUB_BASE — our init stub.
    // The NMI handler will be called on each VBlank frame.

    printf("NES NSF: CPU reset — subtune %d/%d starting\n",
           subtune + 1, nsf->num_songs);

    return nsf_cart;
}

// ============================================================================
// SUBTUNE SWITCH — Lightweight re-init
// ============================================================================

void nes_nsf_switch_subtune(
    nes6502_t* cpu,
    PPU* ppu,
    MemoryBus* bus,
    NsfCartridge* nsf_cart,
    const nsf_header_t* nsf,
    const uint8_t* payload,
    size_t payload_size,
    uint16_t subtune,
    bool is_pal) {

    if (!cpu || !ppu || !bus || !nsf_cart || !nsf) return;

    // ---- Silence APU: write $00 to $4015 to disable all channels ----
    {
        bus_state_t s = 0;
        BUS_SET_ADDR(s, 0x4015);
        BUS_SET_DATA(s, 0x00);
        // RW=0 (write) — bit 48 already clear since s started as 0
        bus->mem_tick(s);
    }

    // ---- Reload NSF data (in case tune self-modified) ----
    nsf_cart->reload_nsf_data(payload, payload_size);
    nsf_cart->set_bank_regs(nsf->bankswitch);

    // Re-write vectors (reload may have cleared them)
    write_vectors(nsf_cart);

    // ---- Clear CPU RAM (except stack) and rebuild stubs ----
    uint8_t* ram = bus->cpu_ram.data();
    // Clear $0000-$00FF and $0200-$07FF, preserve stack $0100-$01FF
    memset(ram, 0, 0x0100);
    memset(ram + 0x0200, 0, 0x0600);

    build_nmi_handler(ram, nsf->play_addr);
    build_irq_handler(ram);
    build_init_stub(ram, nsf, subtune, is_pal);

    // ---- Update info page ----
    nes_write_nsf_info_page(ppu, nsf, subtune);

    // ---- Reset CPU ----
    bus_state_t pins = NES_BUS_DEFAULT_STATE;
    nes6502_reset(cpu, pins);

    printf("NES NSF: Switched to subtune %d/%d\n",
           subtune + 1, nsf->num_songs);
}

} // namespace nes_system
