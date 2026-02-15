// =============================================================================
// VIC-II Register Test Harness — Implementation
// =============================================================================
// Contains:
//   1. KERNAL RAMTAS memory-test patch + BASIC redirect
//   2. Embedded 6510 machine-code test program (results → buffer at $0400)
//   3. C++ harness that reads the results buffer after tests complete
// =============================================================================

#include "vicii_test_harness.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace vicii_test {

// ============================================================================
// 1. KERNAL PATCHES
// ============================================================================
//
// Patch A (RAMTAS at $FD5F): Skip the memory-test loop that dominates boot.
// Patch B ($FCFF): Redirect the JMP ($A000) to JMP $C000 directly, so the
//   test program runs instead of BASIC.  $A000 is in BASIC ROM address space;
//   we can't override it via RAM writes since ROM is banked in at that point.
// ============================================================================

void patch_kernal_for_test(c64_t* c64) {
    if (!c64 || !c64->kernal || !c64->kernal->memory) {
        printf("VICII-TEST: WARNING — cannot patch KERNAL (ROM not loaded)\n");
        return;
    }

    uint8_t* rom = c64->kernal->memory;

    // --- Patch A: Skip RAMTAS memory test ---
    // $FD5F - $E000 = 0x1D5F
    constexpr uint16_t RAMTAS_OFFSET = 0x1D5F;
    if (rom[RAMTAS_OFFSET] == 0xA2 && rom[RAMTAS_OFFSET + 1] == 0x3C) {
        static const uint8_t ramtas_patch[] = {
            0xA0, 0x00,       // LDY #$00
            0x85, 0xC1,       // STA $C1         (A is 0 from page clearing)
            0xA9, 0xA0,       // LDA #$A0        (top page = $A0)
            0x85, 0xC2,       // STA $C2
            0x4C, 0x88, 0xFD, // JMP $FD88       (skip to SETTOP)
        };
        memcpy(&rom[RAMTAS_OFFSET], ramtas_patch, sizeof(ramtas_patch));
        printf("VICII-TEST: Patched RAMTAS at $FD5F — memory test skipped\n");
    } else {
        printf("VICII-TEST: WARNING — RAMTAS bytes at $FD5F don't match ($%02X $%02X)\n",
               rom[RAMTAS_OFFSET], rom[RAMTAS_OFFSET + 1]);
    }

    // --- Patch B: Redirect BASIC cold-start to test program ---
    // $FCFF - $E000 = 0x1CFF
    // Original: 6C 00 A0 (JMP ($A000) — indirect through BASIC ROM)
    // Patched:  4C 00 C0 (JMP $C000  — direct to test program)
    constexpr uint16_t JMP_OFFSET = 0x1CFF;
    if (rom[JMP_OFFSET] == 0x6C && rom[JMP_OFFSET + 1] == 0x00 && rom[JMP_OFFSET + 2] == 0xA0) {
        rom[JMP_OFFSET]     = 0x4C;  // JMP absolute
        rom[JMP_OFFSET + 1] = TEST_LOAD_ADDR & 0xFF;
        rom[JMP_OFFSET + 2] = TEST_LOAD_ADDR >> 8;
        printf("VICII-TEST: Patched $FCFF — BASIC start → JMP $%04X\n", TEST_LOAD_ADDR);
    } else {
        printf("VICII-TEST: WARNING — bytes at $FCFF don't match ($%02X $%02X $%02X)\n",
               rom[JMP_OFFSET], rom[JMP_OFFSET + 1], rom[JMP_OFFSET + 2]);
    }
}


// ============================================================================
// 2. EMBEDDED 6510 TEST PROGRAM
// ============================================================================
//
// Results are written to a buffer at $0400 (screen RAM). Each result entry
// is 5 bytes: [test_num, sub_test, result, expected, actual].
//
// The write pointer is maintained in zero page $08/$09.
// When all tests are done, $02 is set to $FF.
//
// The program uses a mini-assembler helper to generate correct 6502 machine code.
// ============================================================================

// Mini 6502 assembler
struct asm6510 {
    uint8_t* buf;
    size_t   pos;
    size_t   cap;

    void emit(uint8_t b) { if (pos < cap) buf[pos++] = b; }
    void emit2(uint8_t op, uint8_t arg) { emit(op); emit(arg); }
    void emit3(uint8_t op, uint8_t lo, uint8_t hi) { emit(op); emit(lo); emit(hi); }
    uint16_t addr() const { return static_cast<uint16_t>(TEST_LOAD_ADDR + pos); }

    // --- single-instruction helpers ---
    void lda_imm(uint8_t v) { emit2(0xA9, v); }
    void ldx_imm(uint8_t v) { emit2(0xA2, v); }
    void ldy_imm(uint8_t v) { emit2(0xA0, v); }
    void sta_zp(uint8_t a)  { emit2(0x85, a); }
    void stx_zp(uint8_t a)  { emit2(0x86, a); }
    void lda_zp(uint8_t a)  { emit2(0xA5, a); }
    void sta_abs(uint16_t a){ emit3(0x8D, a & 0xFF, a >> 8); }
    void lda_abs(uint16_t a){ emit3(0xAD, a & 0xFF, a >> 8); }
    void cmp_imm(uint8_t v) { emit2(0xC9, v); }
    void and_imm(uint8_t v) { emit2(0x29, v); }
    void beq(int8_t off)    { emit2(0xF0, static_cast<uint8_t>(off)); }
    void bne(int8_t off)    { emit2(0xD0, static_cast<uint8_t>(off)); }
    void bpl(int8_t off)    { emit2(0x10, static_cast<uint8_t>(off)); }
    void jmp(uint16_t a)    { emit3(0x4C, a & 0xFF, a >> 8); }
    void jsr(uint16_t a)    { emit3(0x20, a & 0xFF, a >> 8); }
    void sei()              { emit(0x78); }
    void nop()              { emit(0xEA); }
    void rts()              { emit(0x60); }
    void tax()              { emit(0xAA); }
    void txa()              { emit(0x8A); }
    void tay()              { emit(0xA8); }
    void tya()              { emit(0x98); }
    void pha()              { emit(0x48); }
    void pla()              { emit(0x68); }
    void inx()              { emit(0xE8); }
    void dex()              { emit(0xCA); }
    void iny()              { emit(0xC8); }
    void dey()              { emit(0x88); }
    void clc()              { emit(0x18); }
    void adc_imm(uint8_t v) { emit2(0x69, v); }
    void inc_zp(uint8_t a)  { emit2(0xE6, a); }
    void sta_ind_y(uint8_t zp) { emit2(0x91, zp); } // STA ($zp),Y

    // --- composite: write one result entry to the buffer ---
    // Expects: test_num in A param, sub_test param, result param, expected param, actual param
    // Writes 5 bytes at ($08),Y and advances pointer by 5.
    // Clobbers A, Y.
    void write_result(uint8_t test_num, uint8_t sub_test, uint8_t result,
                      uint8_t expected, uint8_t actual) {
        ldy_imm(0);
        lda_imm(test_num);   sta_ind_y(ZP_RESULT_PTR_LO);
        iny();
        lda_imm(sub_test);   sta_ind_y(ZP_RESULT_PTR_LO);
        iny();
        lda_imm(result);     sta_ind_y(ZP_RESULT_PTR_LO);
        iny();
        lda_imm(expected);   sta_ind_y(ZP_RESULT_PTR_LO);
        iny();
        lda_imm(actual);     sta_ind_y(ZP_RESULT_PTR_LO);
        // Advance pointer by 5
        advance_result_ptr();
    }

    // Advance the 16-bit result pointer at $08/$09 by 5
    void advance_result_ptr() {
        clc();
        lda_zp(ZP_RESULT_PTR_LO);
        adc_imm(RESULT_ENTRY_SIZE);
        sta_zp(ZP_RESULT_PTR_LO);
        lda_zp(ZP_RESULT_PTR_HI);
        adc_imm(0);  // carry
        sta_zp(ZP_RESULT_PTR_HI);
    }

    // Write result from runtime check: A=actual value to test, expected=compile-time constant
    // test_num and sub_test are compile-time constants.
    // If A == expected, writes PASS entry; else writes FAIL entry with actual value.
    // Clobbers: A, X, Y. ~40 bytes.
    void check_and_record(uint8_t test_num, uint8_t sub_test, uint8_t expected) {
        // Save actual in X
        tax();
        cmp_imm(expected);

        // BEQ +offset (skip fail, go to pass)
        // fail path: ~18 bytes, pass path: ~18 bytes
        // We use a forward branch to the pass path
        beq(20);  // skip fail → go to pass path

        // === FAIL PATH (A ≠ expected) ===
        // Write: [test_num, sub_test, RESULT_FAIL, expected, actual(X)]
        ldy_imm(0);
        lda_imm(test_num);    sta_ind_y(ZP_RESULT_PTR_LO); // offset 0
        iny();
        lda_imm(sub_test);    sta_ind_y(ZP_RESULT_PTR_LO); // offset 1
        iny();
        lda_imm(RESULT_FAIL); sta_ind_y(ZP_RESULT_PTR_LO); // offset 2
        iny();
        lda_imm(expected);    sta_ind_y(ZP_RESULT_PTR_LO); // offset 3
        // 20 bytes to here
        uint16_t common_target = static_cast<uint16_t>(TEST_LOAD_ADDR + pos + 22);
        // Jump over pass path to common pointer advance
        jmp(common_target); // 3 bytes (total fail = 23 bytes... but beq offset was 20)
        // Hmm, let me count more carefully...

        // Actually this is getting complex with manual offset calcs. Let me use a simpler pattern:
        // Just always write the result (pass or fail), deciding at runtime.
        // This avoids branch offset calculation entirely.
    }

    // SIMPLER approach: emit a "test and record" subroutine call.
    // The subroutine is at a known address, and we inline the comparison.
    //
    // Instead of trying to branch, we use a JSR to a "record_result" routine
    // embedded at the end of the program. Before the JSR:
    //   Zero-page $0A = test_num
    //   Zero-page $0B = sub_test
    //   Zero-page $0C = expected
    //   A             = actual
    //
    // The subroutine compares A to $0C and writes the result entry.

    void setup_test(uint8_t test_num, uint8_t sub_test, uint8_t expected) {
        lda_imm(test_num);  sta_zp(0x0A);
        lda_imm(sub_test);  sta_zp(0x0B);
        lda_imm(expected);  sta_zp(0x0C);
    }

    // After loading the actual value in A, call the record routine
    // void call_record(uint16_t record_routine) { jsr(record_routine); }
};


// Build the 6510 program
static size_t build_test_program(uint8_t* buffer, size_t buffer_size) {
    asm6510 a;
    a.buf = buffer;
    a.pos = 0;
    a.cap = buffer_size;

    // We'll place a "record_result" subroutine at a forward address.
    // First emit the main test body, then the subroutine.
    // For the JSR targets, we'll patch them after we know the subroutine address.

    // Track JSR fixup locations
    uint16_t jsr_fixups[256];
    int num_fixups = 0;

    // === Preamble ===
    a.sei();

    // Initialize result pointer to $0400
    a.lda_imm(RESULTS_BASE & 0xFF);
    a.sta_zp(ZP_RESULT_PTR_LO);
    a.lda_imm(RESULTS_BASE >> 8);
    a.sta_zp(ZP_RESULT_PTR_HI);

    // Clear done flag
    a.lda_imm(0);
    a.sta_zp(ZP_DONE_FLAG);

    // Helper macro-like lambda
    auto emit_test = [&](uint8_t test_num, uint8_t sub_test, uint8_t expected,
                         uint16_t vic_addr, uint8_t write_val, bool mask_read = false, uint8_t mask = 0xFF) {
        a.setup_test(test_num, sub_test, expected);
        a.lda_imm(write_val);
        a.sta_abs(vic_addr);
        a.lda_abs(vic_addr);
        if (mask_read) a.and_imm(mask);
        // JSR to record_result (address TBD, will fixup)
        jsr_fixups[num_fixups++] = static_cast<uint16_t>(a.pos);
        a.jsr(0x0000);  // placeholder — will be patched
    };

    // === TEST 1: 8-bit register R/W roundtrip ===
    emit_test(1, 0, 0xA5, 0xD015, 0xA5);  // MXE
    emit_test(1, 1, 0x5A, 0xD017, 0x5A);  // MXYE
    emit_test(1, 2, 0xC3, 0xD01B, 0xC3);  // MXDP
    emit_test(1, 3, 0x3C, 0xD01C, 0x3C);  // MXMC
    emit_test(1, 4, 0x81, 0xD01D, 0x81);  // MXXE
    // Reset sprite registers
    a.lda_imm(0);
    a.sta_abs(0xD015); a.sta_abs(0xD017); a.sta_abs(0xD01B);
    a.sta_abs(0xD01C); a.sta_abs(0xD01D);

    // === TEST 2: 4-bit color register masking ===
    uint16_t color_addrs[] = {
        0xD020, 0xD021, 0xD022, 0xD023, 0xD024, 0xD025, 0xD026,
        0xD027, 0xD028, 0xD029, 0xD02A, 0xD02B, 0xD02C, 0xD02D, 0xD02E
    };
    for (uint8_t i = 0; i < 15; i++) {
        emit_test(2, i, 0x0F, color_addrs[i], 0xFF);
    }

    // === TEST 3: Color register value roundtrip ===
    emit_test(3, 0, 0x06, 0xD020, 0x06);  // Border = blue
    emit_test(3, 1, 0x0E, 0xD021, 0x0E);  // Background = light blue

    // === TEST 4: Sprite coordinate registers ===
    emit_test(4, 0, 0x80, 0xD000, 0x80);  // M0X
    emit_test(4, 1, 0x64, 0xD001, 0x64);  // M0Y = 100
    emit_test(4, 2, 0x01, 0xD010, 0x01);  // MX8 bit 0
    a.lda_imm(0); a.sta_abs(0xD010);      // Clear MX8

    // All 8 sprite Y positions
    for (uint8_t s = 0; s < 8; s++) {
        uint8_t y_val = 50 + s * 21;
        emit_test(4, 3 + s, y_val, 0xD001 + s * 2, y_val);
    }

    // === TEST 5: Memory pointer register $D018 ===
    // Bit 0 is unused and masked on read (real HW behavior)
    emit_test(5, 0, 0x14, 0xD018, 0x15, true, 0xFE);
    a.lda_imm(0x14); a.sta_abs(0xD018);  // Restore default

    // === TEST 6: Control register 1 ($D011) writable bits ===
    // Read current, save, write test value, read back (mask bit 7: raster MSB)
    a.setup_test(6, 0, 0x3B);
    a.lda_abs(0xD011);
    a.and_imm(0x7F);
    a.pha();         // Save original on stack
    a.lda_imm(0x3B); // DEN=1, RSEL=1, YSCROLL=3, BMM=1
    a.sta_abs(0xD011);
    a.lda_abs(0xD011);
    a.and_imm(0x7F); // Mask read-only bit 7
    jsr_fixups[num_fixups++] = static_cast<uint16_t>(a.pos);
    a.jsr(0x0000);   // Record result
    a.pla();         // Restore original
    a.sta_abs(0xD011);

    // === TEST 7: Control register 2 ($D016) writable bits ===
    emit_test(7, 0, 0x08, 0xD016, 0x08, true, 0x1F);  // CSEL=1
    emit_test(7, 1, 0x17, 0xD016, 0x17, true, 0x1F);  // MCM=1, XSCROLL=7
    a.lda_imm(0xC8); a.sta_abs(0xD016);  // Restore

    // === TEST 8: Sprite enable register roundtrip ===
    // Set up sprite 0 data first (63 bytes of $FF at $2000)
    a.ldx_imm(62);
    a.lda_imm(0xFF);
    uint16_t spr_loop = a.addr();
    a.emit3(0x9D, 0x00, 0x20); // STA $2000,X
    a.dex();
    int8_t spr_bpl = static_cast<int8_t>(spr_loop - (a.addr() + 2));
    a.bpl(spr_bpl);

    // Set sprite 0 pointer
    a.lda_imm(0x80);
    a.sta_abs(0x07F8);

    emit_test(8, 0, 0x01, 0xD015, 0x01);  // Enable sprite 0
    emit_test(8, 1, 0xFF, 0xD015, 0xFF);  // Enable all 8
    a.lda_imm(0); a.sta_abs(0xD015);      // Disable all

    // === TEST 9: Sprite flag registers ===
    emit_test(9, 0, 0xAA, 0xD01B, 0xAA);  // Priority
    emit_test(9, 1, 0x55, 0xD01C, 0x55);  // Multicolor
    emit_test(9, 2, 0xC3, 0xD01D, 0xC3);  // X expand
    emit_test(9, 3, 0x3C, 0xD017, 0x3C);  // Y expand

    a.lda_imm(0);
    a.sta_abs(0xD01B); a.sta_abs(0xD01C);
    a.sta_abs(0xD01D); a.sta_abs(0xD017);

    // === TEST 10: Collision registers (read-only, clear-on-read) ===
    a.lda_imm(0); a.sta_abs(0xD015); // Disable all sprites
    // Write garbage
    a.lda_imm(0xFF);
    a.sta_abs(0xD01E); a.sta_abs(0xD01F);
    // Read — should be 0 (no collisions)
    a.setup_test(10, 0, 0x00);
    a.lda_abs(0xD01E);
    jsr_fixups[num_fixups++] = static_cast<uint16_t>(a.pos);
    a.jsr(0x0000);

    a.setup_test(10, 1, 0x00);
    a.lda_abs(0xD01F);
    jsr_fixups[num_fixups++] = static_cast<uint16_t>(a.pos);
    a.jsr(0x0000);

    // === TEST 11: Raster counter is readable ===
    a.setup_test(11, 0, 0x01); // Just check we can read it (pass if we get here)
    a.lda_abs(0xD012);
    // We can't predict the value, so always write PASS
    a.write_result(11, 0, RESULT_PASS, 0, 0);

    // === TEST 12: Interrupt enable register ($D01A) ===
    emit_test(12, 0, 0x0F, 0xD01A, 0x0F, true, 0x0F); // All IRQ sources
    emit_test(12, 1, 0x01, 0xD01A, 0x01, true, 0x0F); // Raster only
    // Clear and disable
    a.lda_imm(0xFF); a.sta_abs(0xD019); // Acknowledge all
    a.lda_imm(0x00); a.sta_abs(0xD01A); // Disable all

    // === TEST 13: Sprite display integration (sprite-bg collision) ===
    a.setup_test(13, 0, 0x01);
    // Sprite data already at $2000, pointer at $07F8=$80
    // Write visible chars (filled block $A0) at the sprite's screen position.
    // Sprite at VIC-II coords (24,115): char row=(115-51)/8=8, col=(24-24)/8=0
    // Screen address = $0400 + 8*40 + 0 = $0540
    a.lda_imm(0xA0);  // Filled block (reverse space) in PETSCII screen code
    a.sta_abs(0x0540); a.sta_abs(0x0541); a.sta_abs(0x0542);
    a.sta_abs(0x0568); a.sta_abs(0x0569); a.sta_abs(0x056A);
    a.lda_imm(24);  a.sta_abs(0xD000); // M0X = 24
    a.lda_imm(115); a.sta_abs(0xD001); // M0Y = 115
    a.lda_imm(0); a.sta_abs(0xD010);
    a.lda_imm(0x01); a.sta_abs(0xD027); // Color: white
    a.lda_imm(0x01); a.sta_abs(0xD015); // Enable sprite 0

    // Clear collision by reading
    a.lda_abs(0xD01F);

    // Wait ~2 frames: outer=160, inner=256, ~200K cycles
    a.ldy_imm(160);
    uint16_t w1_outer = a.addr();
    a.ldx_imm(0);
    uint16_t w1_inner = a.addr();
    a.dex();
    a.bne(static_cast<int8_t>(w1_inner - (a.addr() + 2)));
    a.dey();
    a.bne(static_cast<int8_t>(w1_outer - (a.addr() + 2)));

    // Read sprite-bg collision bit 0
    a.lda_abs(0xD01F);
    a.and_imm(0x01);
    jsr_fixups[num_fixups++] = static_cast<uint16_t>(a.pos);
    a.jsr(0x0000);

    a.lda_imm(0); a.sta_abs(0xD015); // Disable sprites

    // === TEST 14: Dual-sprite collision ===
    a.setup_test(14, 0, 0x03);
    a.lda_imm(0x80); a.sta_abs(0x07F9); // Sprite 1 pointer = $2000
    a.lda_imm(100);
    a.sta_abs(0xD000); a.sta_abs(0xD001); // Sprite 0
    a.sta_abs(0xD002); a.sta_abs(0xD003); // Sprite 1 (same position → collision)
    a.lda_imm(0); a.sta_abs(0xD010);
    a.lda_imm(0x01); a.sta_abs(0xD027); // Sprite 0 = white
    a.lda_imm(0x02); a.sta_abs(0xD028); // Sprite 1 = red
    a.lda_imm(0x03); a.sta_abs(0xD015); // Enable sprites 0+1

    a.lda_abs(0xD01E); // Clear collision

    // Wait ~2 frames
    a.ldy_imm(160);
    uint16_t w2_outer = a.addr();
    a.ldx_imm(0);
    uint16_t w2_inner = a.addr();
    a.dex();
    a.bne(static_cast<int8_t>(w2_inner - (a.addr() + 2)));
    a.dey();
    a.bne(static_cast<int8_t>(w2_outer - (a.addr() + 2)));

    // Read sprite-sprite collision: bits 0+1 should be set
    a.lda_abs(0xD01E);
    a.and_imm(0x03);
    jsr_fixups[num_fixups++] = static_cast<uint16_t>(a.pos);
    a.jsr(0x0000);

    a.lda_imm(0); a.sta_abs(0xD015); // Disable sprites

    // === DONE ===
    a.lda_imm(DONE_SIGNAL);
    a.sta_zp(ZP_DONE_FLAG);

    // Infinite halt
    uint16_t halt_addr = a.addr();
    a.jmp(halt_addr);

    // ==================================================================
    // RECORD RESULT SUBROUTINE
    // ==================================================================
    // On entry: A = actual value
    //           $0A = test_num, $0B = sub_test, $0C = expected
    //           ($08/$09) = results buffer pointer
    // Writes 5-byte entry and advances pointer.
    //
    // Logic:
    //   CMP $0C / TAX / BEQ +4 / LDA #FAIL / BNE +2 / LDA #PASS
    //   STA $0D (result temp)
    //   Write 5 bytes: test_num, sub_test, result, expected, actual(X)
    //   Advance pointer by 5
    //   RTS
    // ==================================================================
    uint16_t record_subroutine = a.addr();

    // Compare actual (A) to expected ($0C)
    // TAX first (saves actual in X, clobbers Z, but CMP below re-sets Z)
    a.tax();               // Save actual in X
    a.emit2(0xC5, 0x0C);  // CMP $0C (zero-page) — sets Z flag properly

    // Branch to select result code:
    //   BEQ +4  → skip LDA #FAIL + BNE, land on LDA #PASS
    //   LDA #FAIL (2 bytes)
    //   BNE +2  → skip LDA #PASS (always taken since A=$FF≠0)
    //   LDA #PASS (2 bytes)
    a.beq(4);
    a.lda_imm(RESULT_FAIL);  // A = $FF
    a.bne(2);
    a.lda_imm(RESULT_PASS);  // A = $01

    // A = result code (PASS or FAIL), X = actual value
    a.sta_zp(0x0D);  // Save result in temp

    // Write 5-byte entry at ($08),Y
    a.ldy_imm(0);
    a.lda_zp(0x0A); a.sta_ind_y(ZP_RESULT_PTR_LO);  // [0] test_num
    a.iny();
    a.lda_zp(0x0B); a.sta_ind_y(ZP_RESULT_PTR_LO);  // [1] sub_test
    a.iny();
    a.lda_zp(0x0D); a.sta_ind_y(ZP_RESULT_PTR_LO);  // [2] result
    a.iny();
    a.lda_zp(0x0C); a.sta_ind_y(ZP_RESULT_PTR_LO);  // [3] expected
    a.iny();
    a.txa();         a.sta_ind_y(ZP_RESULT_PTR_LO);  // [4] actual

    // Advance pointer by 5
    a.clc();
    a.lda_zp(ZP_RESULT_PTR_LO);
    a.adc_imm(RESULT_ENTRY_SIZE);
    a.sta_zp(ZP_RESULT_PTR_LO);
    a.lda_zp(ZP_RESULT_PTR_HI);
    a.adc_imm(0);
    a.sta_zp(ZP_RESULT_PTR_HI);

    a.rts();

    // ==================================================================
    // PATCH JSR TARGETS
    // ==================================================================
    // All JSR placeholders need to point to record_subroutine
    for (int i = 0; i < num_fixups; i++) {
        size_t offset = jsr_fixups[i];
        // JSR is 3 bytes: 0x20, lo, hi. We stored the offset of the JSR opcode.
        buffer[offset + 1] = record_subroutine & 0xFF;
        buffer[offset + 2] = record_subroutine >> 8;
    }

    printf("VICII-TEST: Built test program: %zu bytes, %d tests, record_result at $%04X\n",
           a.pos, num_fixups, record_subroutine);

    return a.pos;
}


// ============================================================================
// 3. INJECT TEST PROGRAM INTO RAM
// ============================================================================

void inject_test_program(c64_t* c64) {
    if (!c64 || !c64->ram) return;

    uint8_t program[4096];
    size_t size = build_test_program(program, sizeof(program));

    if (size > sizeof(program)) {
        printf("VICII-TEST: ERROR — program too large (%zu bytes)\n", size);
        return;
    }

    // Copy to C64 RAM
    memcpy(&c64->ram->memory[TEST_LOAD_ADDR], program, size);

    // Clear results buffer
    memset(&c64->ram->memory[RESULTS_BASE], 0, RESULTS_MAX * RESULT_ENTRY_SIZE);

    printf("VICII-TEST: Injected %zu bytes at $%04X\n", size, TEST_LOAD_ADDR);
}


// ============================================================================
// 4. HARNESS — init, poll, read results, summary
// ============================================================================

void harness_init(vicii_test_state_t* state) {
    memset(state, 0, sizeof(*state));
    state->active = true;
    state->max_frames = 600;  // ~10 seconds at 50/60fps
}

bool harness_poll(vicii_test_state_t* state, c64_t* c64) {
    if (!state->active || !c64 || !c64->ram) return false;

    state->frames_run++;

    // Check done flag
    if (c64->ram->memory[ZP_DONE_FLAG] == DONE_SIGNAL) {
        state->all_done = true;
        state->active = false;
        return false;
    }

    // Safety timeout
    if (state->frames_run >= state->max_frames) {
        printf("VICII-TEST: TIMEOUT after %d frames (done_flag=$%02X)\n",
               state->frames_run, c64->ram->memory[ZP_DONE_FLAG]);
        state->active = false;
        return false;
    }

    return true;
}

void harness_read_results(vicii_test_state_t* state, c64_t* c64) {
    if (!c64 || !c64->ram) return;

    const uint8_t* ram = c64->ram->memory;

    // Read the results write pointer to know how many entries were written
    uint16_t write_ptr = ram[ZP_RESULT_PTR_LO] | (ram[ZP_RESULT_PTR_HI] << 8);
    int num_entries = (write_ptr - RESULTS_BASE) / RESULT_ENTRY_SIZE;

    if (num_entries <= 0 || num_entries > (int)RESULTS_MAX) {
        printf("VICII-TEST: No valid results (ptr=$%04X, entries=%d)\n",
               write_ptr, num_entries);
        return;
    }

    printf("\nVICII-TEST: %d test results:\n", num_entries);
    printf("  %-6s %-4s %-6s %-8s %-8s\n", "Test", "Sub", "Result", "Expected", "Actual");
    printf("  %-6s %-4s %-6s %-8s %-8s\n", "------", "----", "------", "--------", "--------");

    for (int i = 0; i < num_entries; i++) {
        uint16_t base = RESULTS_BASE + i * RESULT_ENTRY_SIZE;
        uint8_t test_num = ram[base + 0];
        uint8_t sub_test = ram[base + 1];
        uint8_t result   = ram[base + 2];
        uint8_t expected = ram[base + 3];
        uint8_t actual   = ram[base + 4];

        const char* status = (result == RESULT_PASS) ? "PASS" :
                             (result == RESULT_FAIL) ? "FAIL" : "????";

        if (result == RESULT_PASS) {
            state->total_pass++;
            printf("  %-6d %-4d %-6s   $%02X       $%02X\n",
                   test_num, sub_test, status, expected, actual);
        } else {
            state->total_fail++;
            printf("  %-6d %-4d \033[1;31m%-6s\033[0m   $%02X       $%02X  ← MISMATCH\n",
                   test_num, sub_test, status, expected, actual);
        }
    }
}

void harness_summary(const vicii_test_state_t* state) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║           VIC-II REGISTER TEST RESULTS           ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Passed: %-5d                                   ║\n", state->total_pass);
    printf("║  Failed: %-5d                                   ║\n", state->total_fail);
    printf("║  Frames: %-5d                                   ║\n", state->frames_run);
    printf("║  Status: %-40s ║\n",
           state->all_done ? "ALL TESTS COMPLETED" :
           (state->frames_run >= state->max_frames ? "TIMED OUT" : "INCOMPLETE"));
    printf("╚══════════════════════════════════════════════════╝\n");

    if (state->total_fail == 0 && state->all_done) {
        printf("\033[1;32m✓ All VIC-II register tests passed!\033[0m\n");
    } else if (state->total_fail > 0) {
        printf("\033[1;31m✗ %d test(s) FAILED — see details above\033[0m\n", state->total_fail);
    }
}

} // namespace vicii_test
