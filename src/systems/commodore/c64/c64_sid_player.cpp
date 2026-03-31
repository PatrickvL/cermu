// =============================================================================
// C64 SID Player — Implementation
// =============================================================================

#include "systems/commodore/c64/c64_sid_player.hpp"
#include "systems/commodore/c64/c64_screen_utils.hpp"
#include "systems/commodore/c64/c64_constants.hpp"
#include "chip/cpu/fam65xx/fam65xx.hpp"
#include "chip/sound/mos6581.hpp"
#include "chip/cpu/fam65xx/asm6510.hpp"
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

// Memory layout:
//   $0340–$03BF — Init/playback stub (max 128 bytes)
//   irq_addr    — IRQ handler (max 64 bytes, placed dynamically — see find_safe_irq_addr)
//
// Default IRQ candidate ($03C0) sits at the tail of the cassette buffer and is safe
// for the vast majority of SID tunes.  The fallback ($0200, KERNAL workspace) is
// used when the SID payload overlaps the default location.
static constexpr uint16_t STUB_BASE            = 0x0340;
static constexpr uint16_t IRQ_SIZE             = 0x40;  // Max bytes for the IRQ handler
static constexpr uint8_t  CPU_PORT_DDR_DEFAULT = 0x2F;

// Candidate IRQ handler locations (priority order).  find_safe_irq_addr() picks
// the first entry that does not overlap the SID payload or the init stub.
static constexpr uint16_t IRQ_CANDIDATES[] = {
    0x03C0,   // Tail of cassette buffer — default, safe for 99%+ of tunes
    0x0200,   // KERNAL workspace — safe when KERNAL is banked out (PSID)
};

static uint8_t select_sid_banking(uint16_t addr) {
    // Hardware-faithful $01 selection for SID init/play calls
    if (addr >= 0xD000 && addr <= 0xDFFF) return 0x34; // I/O only
    if (addr >= 0xE000 && addr <= 0xFFFA) return 0x35; // RAM at $E000+, I/O
    if (addr >= 0xA000 && addr <= 0xCFFF) return 0x36; // KERNAL ROM, I/O
    return 0x37; // all ROMs visible, I/O
}

/**
 * Return the first IRQ candidate address that does not overlap with:
 *   - the init stub region   [$STUB_BASE, $STUB_BASE + 0x80)
 *   - the SID payload region [load_addr,  load_addr  + data_size)
 */
static uint16_t find_safe_irq_addr(uint16_t load_addr, uint32_t data_size) {
    for (uint16_t cand : IRQ_CANDIDATES) {
        uint32_t cand_end    = cand + IRQ_SIZE;
        uint32_t payload_end = static_cast<uint32_t>(load_addr) + data_size;
        bool payload_ok = (cand_end <= load_addr) || (static_cast<uint32_t>(cand) >= payload_end);
        bool stub_ok    = (cand_end <= STUB_BASE)  || (static_cast<uint32_t>(cand) >= STUB_BASE + 0x80);
        if (payload_ok && stub_ok) return cand;
    }
    printf("C64: WARNING — all IRQ candidates conflict with SID payload; using $%04X\n", IRQ_CANDIDATES[0]);
    return IRQ_CANDIDATES[0];
}

/**
 * Build a self-contained IRQ handler at irq_addr.
 *
 * Fully independent of KERNAL ROM — saves/restores all CPU registers,
 * ensures correct RAM banking, calls the SID play routine, acknowledges
 * the CIA1 timer interrupt, re-arms it, and returns via RTI.
 *
 * Banking strategy:
 *   idle_banking ($35) — used during the idle loop so the CPU reads our
 *     custom IRQ vector from RAM at $FFFE (HIRAM=0 → RAM at $E000+).
 *   play_banking ($36) — used when calling the play routine.  HIRAM=1
 *     keeps I/O visible at $D000 even when tunes do `AND #$FE` on $01
 *     (a common pattern to access RAM under BASIC ROM).  Without HIRAM,
 *     AND #$FE on $35 gives $34 which disables I/O entirely.
 */
static void build_irq_handler(uint8_t* ram, uint16_t irq_addr,
                               uint16_t play_addr, uint8_t idle_banking) {
    asm6510 a(ram + irq_addr, IRQ_SIZE, irq_addr);

    a.pha();
    a.txa();
    a.pha();
    a.tya();
    a.pha();

    // Ensure predictable I/O/banking state before each play() call.
    a.lda_imm(CPU_PORT_DDR_DEFAULT); // Hardware-faithful DDR; bits 0-2 remain outputs
    a.sta_zp(0x00);

    if (play_addr >= 0xE000) {
        // High-memory play routines live under KERNAL ROM; force RAM visible.
        a.lda_imm(0x35);
        a.sta_zp(0x01);
    } else {
        // Keep current RAM/ROM choice but guarantee I/O visibility for SID MMIO.
        a.lda_zp(0x01);
        a.ora_imm(0x04);          // CHAREN=1 => I/O visible at $D000-$DFFF
        a.sta_zp(0x01);
    }

    // Common SID player convention: play() is called with A=0.
    a.lda_imm(0x00);

    a.jsr(play_addr);             // JSR play_addr

    a.lda_abs(c64_constants::CIA1_ICR);            // LDA $DC0D (acknowledge CIA1)

    // Keep our playback IRQ source alive even if the tune touches CIA1 control.
    // Some players alter $DC0D/$DC0E; re-arming here prevents one-shot silence.
    a.store_imm(c64_constants::CIA1_ICR, 0x81);    // Enable Timer A interrupt mask
    a.store_imm(0xDC0E, 0x11);                     // Start Timer A, continuous mode

    // Restore idle banking before RTI so the next IRQ vector fetch still
    // reaches our RAM vector at $FFFE/$FFFF.
    a.lda_imm(CPU_PORT_DDR_DEFAULT);
    a.sta_zp(0x00);
    a.lda_imm(idle_banking);
    a.sta_zp(0x01);

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
static void build_init_stub(uint8_t* ram, uint16_t irq_addr,
                             const sid_header_t* sid,
                             uint16_t subtune,
                             uint16_t timer_period,
                             bool needs_timer_irq) {
    asm6510 a(ram + STUB_BASE, 0x80, STUB_BASE);

    a.sei();

    // Set VIC-II border ($D020) = dark blue, background ($D021) = black
    a.store_imm(0xD020, 0x06);
    a.store_imm(0xD021, 0x00);

    // Disable ALL CIA interrupt sources and acknowledge any pending.
    // This prevents leftover KERNAL timer/keyboard IRQs from firing
    // before our handler is installed.
    a.store_imm(c64_constants::CIA1_ICR, 0x7F);
    a.store_imm(c64_constants::CIA2_ICR, 0x7F);
    a.lda_abs(c64_constants::CIA1_ICR);            // ack CIA1
    a.lda_abs(c64_constants::CIA2_ICR);            // ack CIA2

    // Determine if we must force $01=$35 for all phases (RAM at $E000+ needed)
    bool force_kernal_ram = (sid->load_addr >= 0xE000) || (sid->init_addr >= 0xE000) || (sid->play_addr >= 0xE000);
    if (needs_timer_irq) {
        // ---- PSID with play_addr != 0: we manage the playback IRQ ----

        // Bank out KERNAL, keep I/O visible ($01=$35) so we can write
        // our IRQ vector to RAM at $FFFE/$FFFF and the CPU will read it.
        a.lda_imm(CPU_PORT_DDR_DEFAULT); // Use C64 default DDR
        a.sta_zp(0x00);               // STA $00
        a.lda_imm(0x35);
        a.sta_zp(0x01);

        // Write our IRQ handler address to the hardware vector $FFFE/$FFFF.
        // With KERNAL banked out, the 6510 reads these from RAM on IRQ.
        a.lda_imm(irq_addr & 0xFF);
        a.sta_abs(0xFFFE);
        a.lda_imm(irq_addr >> 8);
        a.sta_abs(0xFFFF);

        // Keep classic PSID init banking: $01=$35 for broad compatibility.
        a.lda_imm(CPU_PORT_DDR_DEFAULT); // Use C64 default DDR
        a.sta_zp(0x00);               // STA $00
        a.lda_imm(0x35);
        a.sta_zp(0x01);

        // Call SID init routine BEFORE starting the timer.
        a.lda_imm(static_cast<uint8_t>(subtune));
        a.jsr(sid->init_addr);

        // Always restore $35 after init
        a.lda_imm(CPU_PORT_DDR_DEFAULT); // Use C64 default DDR
        a.sta_zp(0x00);               // STA $00
        a.lda_imm(0x35);
        a.sta_zp(0x01);

        // Set CIA1 Timer A period
        a.lda_imm(static_cast<uint8_t>(timer_period & 0xFF));
        a.sta_abs(0xDC04);
        a.lda_imm(static_cast<uint8_t>(timer_period >> 8));
        a.sta_abs(0xDC05);

        // Enable CIA1 Timer A interrupt
        a.store_imm(c64_constants::CIA1_ICR, 0x81);

        // Start Timer A in continuous mode
        a.store_imm(0xDC0E, 0x11);

    } else {
        // ---- PSID play_addr==0 or RSID: tune manages its own IRQ ----

        // For PSID: bank out ROMs so init code in RAM is visible ($01=$35)
        // For RSID: keep all ROMs visible as required by spec ($01=$37)
        uint8_t init_banking = (sid->type == SID_TYPE_RSID) ? 0x37 : select_sid_banking(sid->init_addr);
        a.lda_imm(CPU_PORT_DDR_DEFAULT); // Use C64 default DDR
        a.sta_zp(0x00);               // STA $00
        a.lda_imm(init_banking);
        a.sta_zp(0x01);

        // Call init — the tune installs its own interrupt handler
        a.lda_imm(static_cast<uint8_t>(subtune));
        a.jsr(sid->init_addr);

        // For PSID: after init, always set $01=$36 (unless KERNAL RAM is needed, then $35)
        if (sid->type != SID_TYPE_RSID) {
            a.lda_imm(CPU_PORT_DDR_DEFAULT); // Use C64 default DDR
            a.sta_zp(0x00);               // STA $00
            a.lda_imm(force_kernal_ram ? 0x35 : 0x36);
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
    if (!sid || !c64 || !c64->ram || !c64->cpu) return;
    if (!c64->ram->data()) return;

    const char* type_str = (sid->type == SID_TYPE_RSID) ? "RSID" : "PSID";
    printf("C64: %s loader \u2014 \"%s\" by %s\n", type_str, sid->name, sid->author);
    printf("C64: load=$%04X init=$%04X play=$%04X songs=%u default=%u\n",
           sid->load_addr, sid->init_addr, sid->play_addr,
           sid->num_songs, sid->start_song);

    uint8_t* ram = c64->ram->data();
    auto& cpu = *c64->cpu;

    // ---- Step 1: Write tune payload to C64 RAM ----
    // If the SID needs RAM at $E000–$FFFF (KERNAL area), bank out KERNAL before copying.
    bool needs_kernal_ram = (sid->load_addr >= 0xE000) || (sid->init_addr >= 0xE000) || (sid->play_addr >= 0xE000);
    uint8_t orig_bank = 0x37;
    if (needs_kernal_ram) {
        orig_bank = cpu.read_io_port();
        cpu.write_io_data(0x35); // RAM at $E000–$FFFF, I/O visible
    }
    const size_t payload_size = (prog && prog->data) ? prog->data_size : 0;
    if (payload_size > 0) {
        memcpy(&ram[sid->load_addr], prog->data, payload_size);
    }
    if (needs_kernal_ram) {
        cpu.write_io_data(orig_bank); // Restore original banking
    }

    // ---- Safe IRQ handler placement ----
    // Pick a 64-byte region that does not overlap the SID payload or the init stub.
    const bool needs_timer_irq = (sid->type == SID_TYPE_PSID && sid->play_addr != 0);
    const uint16_t irq_addr    = needs_timer_irq
        ? find_safe_irq_addr(sid->load_addr, static_cast<uint32_t>(payload_size))
        : 0;

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

    // ---- Step 4: Optional SID info page (only when it won't clobber tune RAM) ----
    const uint16_t screen_base = 0x0400;
    const uint32_t screen_end  = 0x07E8; // 1000-byte text screen: $0400-$07E7
    const uint32_t load_end    = static_cast<uint32_t>(sid->load_addr) + payload_size;
    const bool screen_overlap  = (payload_size > 0)
                               && (load_end > static_cast<uint32_t>(screen_base))
                               && (static_cast<uint32_t>(sid->load_addr) < screen_end);

    if (!screen_overlap) {
        uint8_t* screen_ram = &ram[screen_base];
        // Color RAM is hardwired at IO space ($D800–$DBFF).
        uint8_t* color_ram  = c64->colorram ? c64->colorram->memory : nullptr;
        c64_write_sid_info_page(screen_ram, color_ram, sid, subtune, use_cia_rate);

        // Restore charset/font selection for readable screen codes while
        // preserving the current screen base nibble managed by the tune.
        if (c64->vicii) {
            vicii_base_t& vicii_chip = *c64->vicii;
            uint8_t d018 = vicii_chip.regs_[0x18];
            d018 = static_cast<uint8_t>((d018 & 0xF0) | 0x06);
            vicii_chip.regs_[0x18] = d018;
            vicii_chip.memory.cb_base = 0x1800;
        }
    } else {
        printf("C64: SID info page skipped (payload overlaps screen RAM $0400-$07E7)\n");
    }

    // ---- Step 5: Inject 6502 player stub ----
    if (needs_timer_irq) {
        build_irq_handler(ram, irq_addr, sid->play_addr, 0x35);
        printf("C64: Player stub at $%04X, IRQ handler at $%04X\n", STUB_BASE, irq_addr);
    } else {
        printf("C64: Player stub at $%04X (init-only)\n", STUB_BASE);
    }
    build_init_stub(ram, irq_addr, sid, subtune, timer_period, needs_timer_irq);

    // ---- Step 6: Set CPU to execute the stub ----
    if (needs_kernal_ram) {
        cpu.write_io_data(0x35);
    }
    cpu.set(A, (uint8_t)subtune);
    cpu.set(X, 0);
    cpu.set(Y, 0);
    cpu.set(SPL, 0xFF);    // Reset stack
    cpu.set(PC, STUB_BASE);
    cpu.transition_to_fetch();  // Reset pipeline for clean fetch

    printf("C64: PC set to $%04X — subtune %u/%u starting\n",
           STUB_BASE, subtune + 1, sid->num_songs);
}

// =============================================================================
// Subtune Switch — Lightweight re-init without full reload
// =============================================================================

void c64_sid_switch_subtune(C64System* c64, const sid_header_t* sid,
                             const uint8_t* payload, size_t payload_size,
                             uint16_t subtune) {
    if (!sid || !c64 || !c64->ram || !c64->cpu) return;
    if (!c64->ram->data()) return;

    uint8_t* ram = c64->ram->data();
    auto& cpu = *c64->cpu;

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

    // ---- Optional info page refresh (only when it won't clobber tune RAM) ----
    const uint16_t screen_base = 0x0400;
    const uint32_t screen_end  = 0x07E8; // 1000-byte text screen: $0400-$07E7
    const uint32_t load_end    = static_cast<uint32_t>(sid->load_addr) + payload_size;
    const bool screen_overlap  = (payload_size > 0)
                               && (load_end > static_cast<uint32_t>(screen_base))
                               && (static_cast<uint32_t>(sid->load_addr) < screen_end);

    if (!screen_overlap) {
        uint8_t* screen_ram = &ram[screen_base];
        // Color RAM is hardwired at IO space ($D800–$DBFF).
        uint8_t* color_ram  = c64->colorram ? c64->colorram->memory : nullptr;
        c64_write_sid_info_page(screen_ram, color_ram, sid, subtune, use_cia_rate);

        // Restore charset/font selection for readable screen codes while
        // preserving the current screen base nibble managed by the tune.
        if (c64->vicii) {
            vicii_base_t& vicii_chip = *c64->vicii;
            uint8_t d018 = vicii_chip.regs_[0x18];
            d018 = static_cast<uint8_t>((d018 & 0xF0) | 0x06);
            vicii_chip.regs_[0x18] = d018;
            vicii_chip.memory.cb_base = 0x1800;
        }
    } else {
        printf("C64: SID info page refresh skipped (payload overlaps screen RAM $0400-$07E7)\n");
    }

    // ---- Re-inject stub ----
    const bool needs_timer_irq = (sid->type == SID_TYPE_PSID && sid->play_addr != 0);
    const uint16_t irq_addr    = needs_timer_irq
        ? find_safe_irq_addr(sid->load_addr, static_cast<uint32_t>(payload_size))
        : 0;
    if (needs_timer_irq) {
        build_irq_handler(ram, irq_addr, sid->play_addr, 0x35);
    }
    build_init_stub(ram, irq_addr, sid, subtune, timer_period, needs_timer_irq);

    // ---- Reset CPU to start of stub ----
    cpu.set(A, (uint8_t)subtune);
    cpu.set(X, 0);
    cpu.set(Y, 0);
    cpu.set(SPL, 0xFF);
    cpu.set(PC, STUB_BASE);
    cpu.transition_to_fetch();

    printf("C64: Switched to subtune %u/%u\n", subtune + 1, sid->num_songs);
}
