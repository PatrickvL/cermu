// =============================================================================
// C64 SID Player — Implementation
// =============================================================================

#include "c64_sid_player.h"
#include "c64_screen_utils.h"
#include "../../chip/cpu/fam65xx/mos6510.h"
#include "../../chip/sound/mos6581.h"
#include <cstdio>
#include <cstring>

// =============================================================================
// SID Info Page — Full-screen display of SID header information
// =============================================================================

void c64_write_sid_info_page(uint8_t* screen, uint8_t* color,
                             const sid_header_t* sid,
                             uint16_t subtune, bool use_cia) {
    // Colour palette for the info page
    const uint8_t COL_BORDER = 0x0E;  // Light blue
    const uint8_t COL_LABEL  = 0x01;  // White
    const uint8_t COL_VALUE  = 0x03;  // Cyan
    const uint8_t COL_TITLE  = 0x07;  // Yellow
    const uint8_t COL_STATUS = 0x0D;  // Light green
    const uint8_t COL_DETAIL = 0x0F;  // Light grey

    // Clear screen to spaces with dark grey background colour
    for (int i = 0; i < 1000; i++) {
        screen[i] = 0x20;  // space
        color[i]  = 0x0B;  // dark grey
    }

    const uint8_t SC_BAR = 0x40;  // Horizontal bar (graphics character)

    // ---- Header ----
    c64_fill_screen_row(screen, color, 0, SC_BAR, COL_BORDER);
    c64_write_screen_text(screen, color, 1, 11, "SID MUSIC PLAYER", COL_TITLE);

    // Version badge (e.g. "PSID V2")
    char ver_str[16];
    const char* type_str = (sid->type == SID_TYPE_RSID) ? "RSID" : "PSID";
    snprintf(ver_str, sizeof(ver_str), "%s V%u", type_str, sid->version);
    c64_write_screen_text(screen, color, 1, 30, ver_str, COL_DETAIL);
    c64_fill_screen_row(screen, color, 2, SC_BAR, COL_BORDER);

    // ---- Metadata fields ----
    c64_write_screen_text(screen, color, 4, 1, "TITLE:", COL_LABEL);
    c64_write_screen_text(screen, color, 5, 2, sid->name, COL_VALUE);

    c64_write_screen_text(screen, color, 7, 1, "AUTHOR:", COL_LABEL);
    c64_write_screen_text(screen, color, 8, 2, sid->author, COL_VALUE);

    c64_write_screen_text(screen, color, 10, 1, "RELEASED:", COL_LABEL);
    c64_write_screen_text(screen, color, 11, 2, sid->released, COL_VALUE);

    // ---- Technical details ----
    c64_fill_screen_row(screen, color, 13, SC_BAR, COL_BORDER);

    // Row 14: SID model + Video standard
    c64_write_screen_text(screen, color, 14, 1, "SID:", COL_LABEL);
    const char* sid_names[] = {"UNKNOWN", "MOS 6581", "MOS 8580", "6581/8580"};
    c64_write_screen_text(screen, color, 14, 6,
        sid_names[sid->sid_model & 3], COL_VALUE);

    c64_write_screen_text(screen, color, 14, 21, "VIDEO:", COL_LABEL);
    const char* vid_names[] = {"UNKNOWN", "PAL", "NTSC", "PAL/NTSC"};
    c64_write_screen_text(screen, color, 14, 28,
        vid_names[sid->video & 3], COL_VALUE);

    // Row 15: Songs + Default subtune
    c64_write_screen_text(screen, color, 15, 1, "SONGS:", COL_LABEL);
    char num_buf[8];
    snprintf(num_buf, sizeof(num_buf), "%u", sid->num_songs);
    c64_write_screen_text(screen, color, 15, 8, num_buf, COL_VALUE);

    c64_write_screen_text(screen, color, 15, 21, "DEFAULT:", COL_LABEL);
    snprintf(num_buf, sizeof(num_buf), "%u", sid->start_song);
    c64_write_screen_text(screen, color, 15, 30, num_buf, COL_VALUE);

    // Row 16: Load + Init addresses
    c64_write_screen_text(screen, color, 16, 1, "LOAD:", COL_LABEL);
    c64_write_hex16(screen, color, 16, 7, sid->load_addr, COL_VALUE);

    c64_write_screen_text(screen, color, 16, 21, "INIT:", COL_LABEL);
    c64_write_hex16(screen, color, 16, 27, sid->init_addr, COL_VALUE);

    // Row 17: Play address + Speed
    c64_write_screen_text(screen, color, 17, 1, "PLAY:", COL_LABEL);
    if (sid->play_addr != 0) {
        c64_write_hex16(screen, color, 17, 7, sid->play_addr, COL_VALUE);
    } else {
        c64_write_screen_text(screen, color, 17, 7, "IRQ", COL_VALUE);
    }

    c64_write_screen_text(screen, color, 17, 21, "SPEED:", COL_LABEL);
    const char* speed_str = use_cia ? "CIA (60HZ)" : "VBI (50HZ)";
    c64_write_screen_text(screen, color, 17, 28, speed_str, COL_VALUE);

    // ---- Bottom section ----
    c64_fill_screen_row(screen, color, 19, SC_BAR, COL_BORDER);

    // "Now playing" status — centred
    char playing_str[41];
    snprintf(playing_str, sizeof(playing_str), "NOW PLAYING SUBTUNE %u/%u",
             subtune + 1, sid->num_songs);
    int playing_col = (40 - (int)strlen(playing_str)) / 2;
    if (playing_col < 0) playing_col = 0;
    c64_write_screen_text(screen, color, 21, playing_col, playing_str, COL_STATUS);
}

// =============================================================================
// SID Loader — Payload injection + 6502 player stub
// =============================================================================

void c64_apply_sid_load(c64_t* c64, const sid_header_t* sid,
                        const program_data_t* prog) {
    if (!sid || !c64 || !c64->ram) return;

    const char* type_str = (sid->type == SID_TYPE_RSID) ? "RSID" : "PSID";
    printf("C64: %s loader — \"%s\" by %s\n", type_str, sid->name, sid->author);
    printf("C64: load=$%04X init=$%04X play=$%04X songs=%u default=%u\n",
           sid->load_addr, sid->init_addr, sid->play_addr,
           sid->num_songs, sid->start_song);

    uint8_t* ram = c64->ram->memory;
    mos6510_t* cpu = (mos6510_t*)c64->mos6510;

    // ---- Step 1: Write tune payload to C64 RAM ----
    if (prog->data && prog->data_size > 0) {
        memcpy(&ram[sid->load_addr], prog->data, prog->data_size);
        printf("C64: SID payload written: $%04X–$%04X (%zu bytes)\n",
               sid->load_addr,
               (unsigned)(sid->load_addr + prog->data_size - 1),
               prog->data_size);
    }

    // ---- Step 2: Set SID revision from metadata (v2+ flags) ----
    if (sid->version >= 2 && sid->sid_model != SID_MODEL_UNKNOWN && c64->sid) {
        sid_revision_t rev = (sid->sid_model == SID_MODEL_8580)
                             ? SID_REVISION_8580_R5
                             : SID_REVISION_6581_R4AR;
        mos6581_set_revision(c64->sid, rev);
        printf("C64: SID revision set to %s (from SID file flags)\n",
               rev == SID_REVISION_8580_R5 ? "MOS 8580" : "MOS 6581");
    }

    // ---- Step 3: Determine playback timer period ----
    uint16_t subtune = sid->start_song;
    if (subtune > 0) subtune--;  // Convert 1-based to 0-based index

    bool use_cia_rate = false;
    if (subtune < 32) {
        use_cia_rate = (sid->speed_flags >> subtune) & 1;
    }

    // PAL 50Hz: 985248/50 = 19705 cycles
    // CIA 60Hz: 985248/60 = 16421 cycles
    uint16_t timer_period = use_cia_rate ? 16421 : 19705;
    printf("C64: Speed flag for subtune %u: %s (timer=%u cycles)\n",
           subtune + 1, use_cia_rate ? "CIA" : "VBI", timer_period);

    // ---- Step 4: Write SID info page to screen RAM ----
    uint8_t* screen_ram = &ram[0x0400];
    uint8_t* color_ram = c64->colorram ? c64->colorram->memory : nullptr;
    if (color_ram) {
        c64_write_sid_info_page(screen_ram, color_ram, sid, subtune, use_cia_rate);
    }

    // ---- Step 5: Inject 6502 player stub at $0340 (cassette buffer) ----
    const uint16_t STUB_BASE   = 0x0340;
    const uint16_t IRQ_HANDLER = 0x0380;

    bool needs_timer_irq = (sid->type == SID_TYPE_PSID && sid->play_addr != 0);

    if (needs_timer_irq) {
        // Build the IRQ handler first (at $0380)
        uint16_t p = IRQ_HANDLER;
        ram[p++] = 0x20;  // JSR play_addr
        ram[p++] = (uint8_t)(sid->play_addr & 0xFF);
        ram[p++] = (uint8_t)(sid->play_addr >> 8);
        ram[p++] = 0xAD;  // LDA $DC0D (acknowledge CIA1 interrupt)
        ram[p++] = 0x0D;
        ram[p++] = 0xDC;
        ram[p++] = 0x4C;  // JMP $EA81 (KERNAL IRQ exit)
        ram[p++] = 0x81;
        ram[p++] = 0xEA;

        // Build the init stub (at $0340)
        p = STUB_BASE;
        ram[p++] = 0x78;  // SEI

        // Set VIC-II border ($D020) = dark blue ($06), background ($D021) = black ($00)
        ram[p++] = 0xA9; ram[p++] = 0x06;  // LDA #$06
        ram[p++] = 0x8D; ram[p++] = 0x20; ram[p++] = 0xD0;  // STA $D020
        ram[p++] = 0xA9; ram[p++] = 0x00;  // LDA #$00
        ram[p++] = 0x8D; ram[p++] = 0x21; ram[p++] = 0xD0;  // STA $D021

        // Hook IRQ vector $0314/$0315 → our handler
        ram[p++] = 0xA9; ram[p++] = (uint8_t)(IRQ_HANDLER & 0xFF);  // LDA #<IRQ_HANDLER
        ram[p++] = 0x8D; ram[p++] = 0x14; ram[p++] = 0x03;          // STA $0314
        ram[p++] = 0xA9; ram[p++] = (uint8_t)(IRQ_HANDLER >> 8);    // LDA #>IRQ_HANDLER
        ram[p++] = 0x8D; ram[p++] = 0x15; ram[p++] = 0x03;          // STA $0315

        // Set CIA1 Timer A period
        ram[p++] = 0xA9; ram[p++] = (uint8_t)(timer_period & 0xFF);  // LDA #<timer
        ram[p++] = 0x8D; ram[p++] = 0x04; ram[p++] = 0xDC;           // STA $DC04
        ram[p++] = 0xA9; ram[p++] = (uint8_t)(timer_period >> 8);    // LDA #>timer
        ram[p++] = 0x8D; ram[p++] = 0x05; ram[p++] = 0xDC;           // STA $DC05

        // Enable CIA1 Timer A interrupt
        ram[p++] = 0xA9; ram[p++] = 0x81;  // LDA #$81
        ram[p++] = 0x8D; ram[p++] = 0x0D; ram[p++] = 0xDC;  // STA $DC0D

        // Start Timer A in continuous mode
        ram[p++] = 0xA9; ram[p++] = 0x11;  // LDA #$11
        ram[p++] = 0x8D; ram[p++] = 0x0E; ram[p++] = 0xDC;  // STA $DC0E

        // Call init subroutine: LDA #subtune, JSR init_addr
        ram[p++] = 0xA9; ram[p++] = (uint8_t)subtune;  // LDA #subtune
        ram[p++] = 0x20;  // JSR init_addr
        ram[p++] = (uint8_t)(sid->init_addr & 0xFF);
        ram[p++] = (uint8_t)(sid->init_addr >> 8);

        // CLI + infinite loop
        ram[p++] = 0x58;  // CLI
        uint16_t loop_addr = p;
        ram[p++] = 0x4C;  // JMP loop_addr
        ram[p++] = (uint8_t)(loop_addr & 0xFF);
        ram[p++] = (uint8_t)(loop_addr >> 8);

        printf("C64: Player stub at $%04X, IRQ handler at $%04X\n",
               STUB_BASE, IRQ_HANDLER);
    } else {
        // PSID with play_addr==0, or RSID:
        // Just call init with subtune — the tune sets up its own IRQ handler.
        uint16_t p = STUB_BASE;

        // Set VIC-II border and background colours
        ram[p++] = 0xA9; ram[p++] = 0x06;  // LDA #$06
        ram[p++] = 0x8D; ram[p++] = 0x20; ram[p++] = 0xD0;  // STA $D020
        ram[p++] = 0xA9; ram[p++] = 0x00;  // LDA #$00
        ram[p++] = 0x8D; ram[p++] = 0x21; ram[p++] = 0xD0;  // STA $D021

        ram[p++] = 0xA9; ram[p++] = (uint8_t)subtune;  // LDA #subtune
        ram[p++] = 0x20;  // JSR init_addr
        ram[p++] = (uint8_t)(sid->init_addr & 0xFF);
        ram[p++] = (uint8_t)(sid->init_addr >> 8);
        uint16_t loop_addr = p;
        ram[p++] = 0x4C;  // JMP loop_addr
        ram[p++] = (uint8_t)(loop_addr & 0xFF);
        ram[p++] = (uint8_t)(loop_addr >> 8);

        printf("C64: Init-only stub at $%04X (tune manages own IRQ)\n", STUB_BASE);
    }

    // ---- Step 6: Set CPU to execute the stub ----
    mos6510_set_a(cpu, (uint8_t)subtune);
    mos6510_set_x(cpu, 0);
    mos6510_set_y(cpu, 0);
    mos6510_set_s(cpu, 0xFF);    // Reset stack
    mos6510_set_pc(cpu, STUB_BASE);
    mos6510_transition_to_fetch(cpu);  // Reset pipeline for clean fetch

    printf("C64: PC set to $%04X — SID playback starting\n", STUB_BASE);
}
