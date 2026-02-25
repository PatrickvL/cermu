// =============================================================================
// C64 SID Player — Implementation
// =============================================================================

#include "c64_sid_player.h"
#include "c64_screen_utils.h"
#include "../../chip/cpu/fam65xx/fam65xx.hpp"
#include "../../chip/sound/mos6581.h"
#include "asm6510.h"
#include <cstdio>
#include <cstring>

using mos6510_cpu_t = fam65xx::mos6510_cpu_impl_t;
#define CPU(ptr) reinterpret_cast<mos6510_cpu_t*>(ptr)

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

    // ---- Metadata fields (raw Latin-1 bytes → accent-stripped screen codes) ----
    c64_write_screen_text(screen, color, 4, 1, "TITLE:", COL_LABEL);
    c64_write_screen_latin1(screen, color, 5, 2, sid->name_raw, COL_VALUE);

    c64_write_screen_text(screen, color, 7, 1, "AUTHOR:", COL_LABEL);
    c64_write_screen_latin1(screen, color, 8, 2, sid->author_raw, COL_VALUE);

    c64_write_screen_text(screen, color, 10, 1, "RELEASED:", COL_LABEL);
    c64_write_screen_latin1(screen, color, 11, 2, sid->released_raw, COL_VALUE);

    // ---- Technical details ----
    c64_fill_screen_row(screen, color, 13, SC_BAR, COL_BORDER);

    // Row 14: SID model + Load address
    c64_write_screen_text(screen, color, 14, 1, "SID:", COL_LABEL);
    const char* sid_names[] = {"UNKNOWN", "MOS 6581", "MOS 8580", "6581/8580"};
    c64_write_screen_text(screen, color, 14, 6,
        sid_names[sid->sid_model & 3], COL_VALUE);

    c64_write_screen_text(screen, color, 14, 21, "LOAD:", COL_LABEL);
    c64_write_hex16(screen, color, 14, 27, sid->load_addr, COL_DETAIL);

    // Row 15: Songs + Init address
    c64_write_screen_text(screen, color, 15, 1, "SONGS:", COL_LABEL);
    char num_buf[8];
    snprintf(num_buf, sizeof(num_buf), "%u", sid->num_songs);
    c64_write_screen_text(screen, color, 15, 8, num_buf, COL_VALUE);

    c64_write_screen_text(screen, color, 15, 21, "INIT:", COL_LABEL);
    c64_write_hex16(screen, color, 15, 27, sid->init_addr, COL_DETAIL);

    // Row 16: Default subtune + Play address
    c64_write_screen_text(screen, color, 16, 1, "DEFAULT:", COL_LABEL);
    snprintf(num_buf, sizeof(num_buf), "%u", sid->start_song);
    c64_write_screen_text(screen, color, 16, 10, num_buf, COL_VALUE);

    c64_write_screen_text(screen, color, 16, 21, "PLAY:", COL_LABEL);
    if (sid->play_addr != 0) {
        c64_write_hex16(screen, color, 16, 27, sid->play_addr, COL_DETAIL);
    } else {
        c64_write_screen_text(screen, color, 16, 27, "IRQ", COL_DETAIL);
    }

    // Row 17: Speed + Video standard
    c64_write_screen_text(screen, color, 17, 1, "SPEED:", COL_LABEL);
    const char* speed_str = use_cia ? "CIA (60HZ)" : "VBI (50HZ)";
    c64_write_screen_text(screen, color, 17, 8, speed_str, COL_VALUE);

    c64_write_screen_text(screen, color, 17, 21, "VIDEO:", COL_LABEL);
    const char* vid_names[] = {"UNKNOWN", "PAL", "NTSC", "PAL/NTSC"};
    c64_write_screen_text(screen, color, 17, 28,
        vid_names[sid->video & 3], COL_DETAIL);

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
// 6502 Stub Builder — Helper functions
// =============================================================================

// Memory layout in the cassette buffer ($0340-$03FF):
//   $0340-$038F  — Init/playback stub (executed once at startup)
//   $0390-$03AF  — IRQ handler (called by hardware IRQ vector)
static constexpr uint16_t STUB_BASE   = 0x0340;
static constexpr uint16_t IRQ_HANDLER = 0x0390;

/**
 * Build a self-contained IRQ handler at IRQ_HANDLER ($0390).
 *
 * This handler is fully independent of KERNAL ROM — it saves/restores
 * all CPU registers, ensures correct RAM banking, calls the SID play
 * routine, acknowledges the CIA1 timer interrupt, and returns via RTI.
 *
 * Banking strategy:
 *   idle_banking ($35) — used during the idle loop so the CPU reads our
 *     custom IRQ vector from RAM at $FFFE (HIRAM=0 → RAM at $E000+).
 *   play_banking ($36) — used when calling the play routine.  HIRAM=1
 *     keeps I/O visible at $D000 even when tunes do `AND #$FE` on $01
 *     (a common pattern to access RAM under BASIC ROM).  Without HIRAM,
 *     AND #$FE on $35 gives $34 which disables I/O entirely.
 */
static void build_irq_handler(uint8_t* ram, uint16_t play_addr,
                               uint8_t idle_banking) {
    // Play banking: set HIRAM (bit 1) so AND #$FE doesn't disable I/O.
    // Clear LORAM (bit 0) — play routines that toggle it won't change state.
    // $35 → $36: I/O visible, KERNAL ROM at $E000, no BASIC ROM.
    uint8_t play_banking = (idle_banking | 0x02) & ~0x01;

    asm6510 a(ram + IRQ_HANDLER, 0x20, IRQ_HANDLER);

    a.pha();
    a.txa();
    a.pha();
    a.tya();
    a.pha();

    a.lda_imm(play_banking);      // LDA #$36 — I/O stays visible after AND #$FE
    a.sta_zp(0x01);               // STA $01

    a.jsr(play_addr);             // JSR play_addr

    a.lda_imm(idle_banking);      // LDA #$35 — RAM at $E000+ for IRQ vector
    a.sta_zp(0x01);               // STA $01

    a.lda_abs(0xDC0D);            // LDA $DC0D (acknowledge CIA1)

    a.pla();
    a.tay();
    a.pla();
    a.tax();
    a.pla();
    a.rti();
}

/**
 * Build the init/playback stub at STUB_BASE ($0340).
 *
 * For PSID with play_addr != 0 (we manage the playback IRQ):
 *   SEI
 *   Set border/background colours
 *   Disable ALL CIA interrupt sources + acknowledge pending
 *   LDA #$35 / STA $01          — bank out BASIC+KERNAL, keep I/O
 *   Write IRQ handler addr → $FFFE/$FFFF in RAM
 *   LDA #subtune / JSR init     — init BEFORE starting timer
 *   Set CIA1 Timer A + enable + start
 *   CLI / JMP self
 *
 * For PSID play_addr==0 or RSID (tune manages its own IRQ):
 *   SEI
 *   Set border/background colours
 *   Disable CIA interrupts + acknowledge
 *   LDA #$35 / STA $01          — RAM visible for init (PSID)
 *     or LDA #$37 / STA $01     — ROMs visible (RSID)
 *   LDA #subtune / JSR init
 *   LDA #$36 / STA $01          — KERNAL back for IRQ dispatch (PSID only)
 *   CLI / JMP self
 */
static void build_init_stub(uint8_t* ram,
                             const sid_header_t* sid,
                             uint16_t subtune,
                             uint16_t timer_period,
                             bool needs_timer_irq) {
    asm6510 a(ram + STUB_BASE, 0xC0, STUB_BASE);

    a.sei();

    // Set VIC-II border ($D020) = dark blue, background ($D021) = black
    a.store_imm(0xD020, 0x06);
    a.store_imm(0xD021, 0x00);

    // Disable ALL CIA interrupt sources and acknowledge any pending.
    // This prevents leftover KERNAL timer/keyboard IRQs from firing
    // before our handler is installed.
    a.store_imm(0xDC0D, 0x7F);
    a.store_imm(0xDD0D, 0x7F);
    a.lda_abs(0xDC0D);            // ack CIA1
    a.lda_abs(0xDD0D);            // ack CIA2

    if (needs_timer_irq) {
        // ---- PSID with play_addr != 0: we manage the playback IRQ ----

        // Bank out KERNAL, keep I/O visible ($01=$35) so we can write
        // our IRQ vector to RAM at $FFFE/$FFFF and the CPU will read it.
        a.lda_imm(0x35);
        a.sta_zp(0x01);

        // Write our IRQ handler address to the hardware vector $FFFE/$FFFF.
        // With KERNAL banked out, the 6510 reads these from RAM on IRQ.
        a.lda_imm(IRQ_HANDLER & 0xFF);
        a.sta_abs(0xFFFE);
        a.lda_imm(IRQ_HANDLER >> 8);
        a.sta_abs(0xFFFF);

        // Switch to $36 (HIRAM=1) before calling init — keeps I/O visible
        // even if init does AND #$FE on $01 (common banking pattern).
        a.lda_imm(0x36);
        a.sta_zp(0x01);

        // Call SID init routine BEFORE starting the timer.
        a.lda_imm(static_cast<uint8_t>(subtune));
        a.jsr(sid->init_addr);

        // Restore $35 — RAM at $E000+ so IRQ vector reads from our RAM
        // copy at $FFFE.  I/O remains visible for CIA register writes.
        a.lda_imm(0x35);
        a.sta_zp(0x01);

        // Set CIA1 Timer A period
        a.lda_imm(static_cast<uint8_t>(timer_period & 0xFF));
        a.sta_abs(0xDC04);
        a.lda_imm(static_cast<uint8_t>(timer_period >> 8));
        a.sta_abs(0xDC05);

        // Enable CIA1 Timer A interrupt
        a.store_imm(0xDC0D, 0x81);

        // Start Timer A in continuous mode
        a.store_imm(0xDC0E, 0x11);

    } else {
        // ---- PSID play_addr==0 or RSID: tune manages its own IRQ ----

        // For PSID: bank out ROMs so init code in RAM is visible ($01=$35)
        // For RSID: keep all ROMs visible as required by spec ($01=$37)
        uint8_t init_banking = (sid->type == SID_TYPE_RSID) ? 0x37 : 0x35;
        a.lda_imm(init_banking);
        a.sta_zp(0x01);

        // Call init — the tune installs its own interrupt handler
        a.lda_imm(static_cast<uint8_t>(subtune));
        a.jsr(sid->init_addr);

        // For PSID: after init, switch to $36 so the KERNAL IRQ dispatcher
        // at $FF48 is visible.  Many PSID tunes hook $0314/$0315.
        if (sid->type != SID_TYPE_RSID) {
            a.lda_imm(0x36);
            a.sta_zp(0x01);
        }
    }

    // CLI + infinite idle loop
    a.cli();
    a.jmp_self();
}

// =============================================================================
// SID Loader — Payload injection + 6502 player stub
// =============================================================================

void c64_apply_sid_load(C64System* c64, const sid_header_t* sid,
                        const program_data_t* prog, uint16_t subtune) {
    if (!sid || !c64 || !c64->ram) return;

    const char* type_str = (sid->type == SID_TYPE_RSID) ? "RSID" : "PSID";
    printf("C64: %s loader — \"%s\" by %s\n", type_str, sid->name, sid->author);
    printf("C64: load=$%04X init=$%04X play=$%04X songs=%u default=%u\n",
           sid->load_addr, sid->init_addr, sid->play_addr,
           sid->num_songs, sid->start_song);

    uint8_t* ram = c64->ram->memory;
    auto* cpu = CPU(c64->mos6510);

    // ---- Step 1: Write tune payload to C64 RAM ----
    if (prog && prog->data && prog->data_size > 0) {
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
        c64->sid->set_revision(rev);
        printf("C64: SID revision set to %s (from SID file flags)\n",
               rev == SID_REVISION_8580_R5 ? "MOS 8580" : "MOS 6581");
    }

    // ---- Step 3: Determine playback timer period ----
    bool use_cia_rate = false;
    if (subtune < 32) {
        use_cia_rate = (sid->speed_flags >> subtune) & 1;
    }

    // Timer period derived from the system's actual CPU clock and target rate:
    //   VBI: cycles_per_frame  (one play() call per video frame — PAL 19656, NTSC 17095)
    //   CIA: cpu_freq / 60     (PAL ~16421, NTSC ~17045)
    const auto& timing = c64->get_current_timing();
    uint16_t timer_period;
    if (use_cia_rate) {
        timer_period = static_cast<uint16_t>(timing.cpu_frequency_hz / 60);
    } else {
        timer_period = static_cast<uint16_t>(timing.cycles_per_frame);
    }
    printf("C64: Speed flag for subtune %u: %s (timer=%u cycles, cpu=%u Hz)\n",
           subtune + 1, use_cia_rate ? "CIA" : "VBI", timer_period, timing.cpu_frequency_hz);

    // ---- Step 4: Write SID info page to screen RAM ----
    uint8_t* screen_ram = &ram[0x0400];
    uint8_t* color_ram = c64->colorram ? c64->colorram->memory : nullptr;
    if (color_ram) {
        c64_write_sid_info_page(screen_ram, color_ram, sid, subtune, use_cia_rate);
    }

    // ---- Step 5: Inject 6502 player stub ----
    bool needs_timer_irq = (sid->type == SID_TYPE_PSID && sid->play_addr != 0);

    if (needs_timer_irq) {
        build_irq_handler(ram, sid->play_addr, 0x35);
    }
    build_init_stub(ram, sid, subtune, timer_period, needs_timer_irq);

    printf("C64: Player stub at $%04X%s\n", STUB_BASE,
           needs_timer_irq ? ", self-contained IRQ at $0390" : " (init-only)");

    // ---- Step 6: Set CPU to execute the stub ----
    cpu->set(REG_A, (uint8_t)subtune);
    cpu->set(REG_X, 0);
    cpu->set(REG_Y, 0);
    cpu->set(REG_SPL, 0xFF);    // Reset stack
    cpu->set(REG_PC, STUB_BASE);
    cpu->transition_to_fetch();  // Reset pipeline for clean fetch

    printf("C64: PC set to $%04X — subtune %u/%u starting\n",
           STUB_BASE, subtune + 1, sid->num_songs);
}

// =============================================================================
// Subtune Switch — Lightweight re-init without full reload
// =============================================================================

void c64_sid_switch_subtune(C64System* c64, const sid_header_t* sid,
                             const uint8_t* payload, size_t payload_size,
                             uint16_t subtune) {
    if (!sid || !c64 || !c64->ram) return;

    uint8_t* ram = c64->ram->memory;
    auto* cpu = CPU(c64->mos6510);

    // ---- Silence SID: reset all voice state ----
    if (c64->sid) {
        c64->sid->reset();
    }

    // ---- Re-copy payload (in case the tune self-modified during play) ----
    if (payload && payload_size > 0) {
        memcpy(&ram[sid->load_addr], payload, payload_size);
    }

    // ---- Determine timer period ----
    bool use_cia_rate = false;
    if (subtune < 32) {
        use_cia_rate = (sid->speed_flags >> subtune) & 1;
    }
    const auto& timing = c64->get_current_timing();
    uint16_t timer_period;
    if (use_cia_rate) {
        timer_period = static_cast<uint16_t>(timing.cpu_frequency_hz / 60);
    } else {
        timer_period = static_cast<uint16_t>(timing.cycles_per_frame);
    }

    // ---- Update info page ----
    uint8_t* screen_ram = &ram[0x0400];
    uint8_t* color_ram = c64->colorram ? c64->colorram->memory : nullptr;
    if (color_ram) {
        c64_write_sid_info_page(screen_ram, color_ram, sid, subtune, use_cia_rate);
    }

    // ---- Re-inject stub ----
    bool needs_timer_irq = (sid->type == SID_TYPE_PSID && sid->play_addr != 0);
    if (needs_timer_irq) {
        build_irq_handler(ram, sid->play_addr, 0x35);
    }
    build_init_stub(ram, sid, subtune, timer_period, needs_timer_irq);

    // ---- Reset CPU to start of stub ----
    cpu->set(REG_A, (uint8_t)subtune);
    cpu->set(REG_X, 0);
    cpu->set(REG_Y, 0);
    cpu->set(REG_SPL, 0xFF);
    cpu->set(REG_PC, STUB_BASE);
    cpu->transition_to_fetch();

    printf("C64: Switched to subtune %u/%u\n", subtune + 1, sid->num_songs);
}
