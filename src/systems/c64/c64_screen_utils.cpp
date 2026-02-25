// =============================================================================
// C64 Screen Text Utilities — Implementation
// =============================================================================

#include "c64_screen_utils.h"

uint8_t c64_ascii_to_screencode(char c) {
    // Used for labels — always uppercase.  In upper/lower charset mode,
    // uppercase glyphs are at screen codes $41–$5A.
    if (c >= 'A' && c <= 'Z') return (uint8_t)(c - 'A' + 0x41);  // $41–$5A (uppercase)
    if (c >= 'a' && c <= 'z') return (uint8_t)(c - 'a' + 0x41);  // $41–$5A (uppercase too)
    if (c >= ' ' && c <= '?') return (uint8_t)c;           // $20–$3F
    if (c == '@') return 0x00;
    return 0x2E;  // '.' for unmapped chars
}

uint8_t c64_latin1_to_screencode(uint8_t ch) {
    // In upper/lower charset mode ($D018 bit 1 set):
    //   Screen codes $01–$1A → lowercase glyphs
    //   Screen codes $41–$5A → uppercase glyphs
    if (ch >= 'A' && ch <= 'Z') return ch - 0x40 + 0x40;  // → $41–$5A (uppercase)
    if (ch >= 'a' && ch <= 'z') return ch - 0x60;           // → $01–$1A (lowercase)
    if (ch >= 0x20 && ch <= 0x3F) return ch;        // space, digits, punctuation
    if (ch == '@') return 0x00;

    // Latin-1 specials
    if (ch == 0xA0) return 0x20;  // non-breaking space → space
    if (ch == 0xA3) return 0x1C;  // £ → C64 native £ sign

    // Latin-1 accented letters (0xC0–0xFF) → accent-stripped screen codes
    // Uppercase accented (0xC0–0xDF) → uppercase screen codes ($41–$5A)
    // Lowercase accented (0xE0–0xFF) → lowercase screen codes ($01–$1A)
    if (ch >= 0xC0) {
        //                      À  Á  Â  Ã  Ä  Å  Æ  Ç  È  É  Ê  Ë  Ì  Í  Î  Ï
        static const uint8_t t[64] = {
            0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x43,0x45,0x45,0x45,0x45,0x49,0x49,0x49,0x49,
        //  Ð  Ñ  Ò  Ó  Ô  Õ  Ö  ×  Ø  Ù  Ú  Û  Ü  Ý  Þ  ß
            0x44,0x4E,0x4F,0x4F,0x4F,0x4F,0x4F,0x2E,0x4F,0x55,0x55,0x55,0x55,0x59,0x2E,0x53,
        //  à  á  â  ã  ä  å  æ  ç  è  é  ê  ë  ì  í  î  ï
            0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x03,0x05,0x05,0x05,0x05,0x09,0x09,0x09,0x09,
        //  ð  ñ  ò  ó  ô  õ  ö  ÷  ø  ù  ú  û  ü  ý  þ  ÿ
            0x04,0x0E,0x0F,0x0F,0x0F,0x0F,0x0F,0x2E,0x0F,0x15,0x15,0x15,0x15,0x19,0x2E,0x19,
        };
        return t[ch - 0xC0];
    }

    return 0x2E;  // everything else → dot
}

uint8_t c64_petscii_to_screencode(uint8_t ch) {
    if (ch < 0x20) return 0x2E;       // Control codes → dot
    if (ch < 0x40) return ch;         // $20–$3F: space, digits, punctuation
    if (ch < 0x60) return ch - 0x40;  // $40–$5F: @, A–Z, [, £, ], ↑, ←
    if (ch < 0x80) return ch - 0x20;  // $60–$7F: graphic characters
    if (ch < 0xA0) return 0x2E;       // $80–$9F: control codes → dot
    if (ch < 0xC0) return ch - 0x40;  // $A0–$BF: reversed/shifted graphics
    if (ch < 0xE0) return ch - 0xC0;  // $C0–$DF: duplicate of $40–$5F
    if (ch < 0xFF) return ch - 0x80;  // $E0–$FE: duplicate of $A0–$BE
    return 0x5E;                      // $FF: π
}

void c64_write_screen_text(uint8_t* screen, uint8_t* color,
                           int row, int col,
                           const char* text, uint8_t color_val) {
    int offset = row * 40 + col;
    for (int i = 0; text[i] && col + i < 40; i++) {
        screen[offset + i] = c64_ascii_to_screencode(text[i]);
        color[offset + i] = color_val;
    }
}

void c64_write_screen_latin1(uint8_t* screen, uint8_t* color,
                              int row, int col,
                              const char* text, uint8_t color_val) {
    int offset = row * 40 + col;
    for (int i = 0; text[i] && col + i < 40; i++) {
        screen[offset + i] = c64_latin1_to_screencode((uint8_t)text[i]);
        color[offset + i] = color_val;
    }
}

void c64_fill_screen_row(uint8_t* screen, uint8_t* color,
                         int row, uint8_t sc, uint8_t col_val) {
    int offset = row * 40;
    for (int i = 0; i < 40; i++) {
        screen[offset + i] = sc;
        color[offset + i] = col_val;
    }
}

int c64_write_hex16(uint8_t* screen, uint8_t* color,
                    int row, int col, uint16_t val, uint8_t col_val) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[6];
    buf[0] = '$';
    buf[1] = hex[(val >> 12) & 0xF];
    buf[2] = hex[(val >> 8) & 0xF];
    buf[3] = hex[(val >> 4) & 0xF];
    buf[4] = hex[val & 0xF];
    buf[5] = '\0';
    int offset = row * 40 + col;
    for (int i = 0; i < 5 && col + i < 40; i++) {
        screen[offset + i] = c64_ascii_to_screencode(buf[i]);
        color[offset + i] = col_val;
    }
    return 5;
}
