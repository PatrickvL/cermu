#include "systems/commodore/c128/c128_keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"

// ============================================================================
// C128 Keyboard Matrix — 8 rows × 11 columns
// ============================================================================
// CIA1 Port A ($DC00) = column select (output, active-low)
// CIA1 Port B ($DC01) = row read (input, active-low)
//
// Row = PB bit position (0–7), Col = PA bit position (0–7) for cols 0–7,
// hardware column index for cols 8–10 (extended via VIC-IIe $D02F).
// These values are used directly by the scan callbacks:
//   row_open_contacts[row] indexed by PB, containing PA bit data
//   col_open_contacts[col] indexed by PA, containing PB bit data
//
// KERNAL keyboard decode table index = PA * 8 + PB (PA-major).
// Columns 0–7: transposed from the C64 PRG format (same KERNAL convention).
// Columns 8–10: C128-specific keys and numeric keypad (VIC-IIe $D02F).
//
// Hardware matrix (C128 PRG, same PA-column/PB-row convention as C64):
//          PA0      PA1     PA2    PA3    PA4    PA5    PA6      PA7     col 8   col 9      col 10
// PB0:     DEL      3       5      7      9      +      £        1       HELP    ESC        ALT
// PB1:     RETURN   W       R      Y      I      P      *        ←       KP8     KP+        KP0
// PB2:     CRSR→    A       D      G      J      L      ;        CTRL    KP5     KP−        KP.
// PB3:     F7       4       6      8      0      -      HOME     2       TAB     LINEFEED   CRSR↑
// PB4:     F1       Z       C      B      M      .      RSHIFT   SPACE   KP2     KP_ENTER   CRSR↓
// PB5:     F3       S       F      H      K      :      =        C=      KP4     KP6        CRSR←
// PB6:     F5       E       T      U      O      @      ↑        Q       KP7     KP9        CRSR→(ext)
// PB7:     CRSR↓    LSHIFT  X      V      N      ,      /        R/S     KP1     KP3        NO_SCROLL

static const KeyMatrixEntry c128_matrix[] = {
    // row 0 (PB0): DEL, 3, 5, 7, 9, +, £, 1, HELP, ESC, ALT
    //              PA0  PA1  PA2  PA3  PA4  PA5  PA6  PA7  ext8  ext9 ext10
    { 0, 0, UKEY_CBM_DEL,        0  },
    { 0, 1, '3',                '#' },
    { 0, 2, '5',                '%' },
    { 0, 3, '7',               '\'' },
    { 0, 4, '9',                ')' },
    { 0, 5, '+',                 0  },
    { 0, 6, UKEY_POUND_SIGN,     0  },
    { 0, 7, '1',                '!' },
    { 0, 8, UKEY_CBM_HELP,       0  },
    { 0, 9, UKEY_ESCAPE,         0  },
    { 0, 10, UKEY_CBM_ALT,       0  },

    // row 1 (PB1): RETURN, W, R, Y, I, P, *, ←, KP8, KP+, KP0
    { 1, 0, '\r',                0  },
    { 1, 1, 'W',                'w' },
    { 1, 2, 'R',                'r' },
    { 1, 3, 'Y',                'y' },
    { 1, 4, 'I',                'i' },
    { 1, 5, 'P',                'p' },
    { 1, 6, '*',                 0  },
    { 1, 7, UKEY_LEFT_ARROW,     0  },
    { 1, 8, UKEY_CBM_KP_8,       0  },
    { 1, 9, UKEY_CBM_KP_PLUS,    0  },
    { 1, 10, UKEY_CBM_KP_0,      0  },

    // row 2 (PB2): CRSR→, A, D, G, J, L, ;, CTRL, KP5, KP−, KP.
    { 2, 0, UKEY_CBM_CURSOR_RT,  0  },
    { 2, 1, 'A',                'a' },
    { 2, 2, 'D',                'd' },
    { 2, 3, 'G',                'g' },
    { 2, 4, 'J',                'j' },
    { 2, 5, 'L',                'l' },
    { 2, 6, ';',                ']' },
    { 2, 7, UKEY_CBM_CTRL,       0  },
    { 2, 8, UKEY_CBM_KP_5,       0  },
    { 2, 9, UKEY_CBM_KP_MINUS,   0  },
    { 2, 10, UKEY_CBM_KP_PERIOD, 0  },

    // row 3 (PB3): F7, 4, 6, 8, 0, -, HOME, 2, TAB, LINEFEED, CRSR↑
    { 3, 0, UKEY_CBM_F7,         0  },
    { 3, 1, '4',                '$' },
    { 3, 2, '6',                '&' },
    { 3, 3, '8',                '(' },
    { 3, 4, '0',                 0  },
    { 3, 5, '-',                 0  },
    { 3, 6, UKEY_CBM_HOME,       0  },
    { 3, 7, '2',                '"' },
    { 3, 8, UKEY_CBM_TAB,        0  },
    { 3, 9, UKEY_CBM_LINE_FEED,  0  },
    { 3, 10, UKEY_CBM_CURSOR_UP, 0  },

    // row 4 (PB4): F1, Z, C, B, M, ., RSHIFT, SPACE, KP2, KP_ENTER, CRSR↓
    { 4, 0, UKEY_CBM_F1,         0  },
    { 4, 1, 'Z',                'z' },
    { 4, 2, 'C',                'c' },
    { 4, 3, 'B',                'b' },
    { 4, 4, 'M',                'm' },
    { 4, 5, '.',                '>' },
    { 4, 6, UKEY_CBM_SHIFT_R,    0  },
    { 4, 7, ' ',                 0  },
    { 4, 8, UKEY_CBM_KP_2,       0  },
    { 4, 9, UKEY_CBM_KP_ENTER,   0  },
    { 4, 10, UKEY_CBM_CURSOR_DN, 0  },

    // row 5 (PB5): F3, S, F, H, K, :, =, C=, KP4, KP6, CRSR←
    { 5, 0, UKEY_CBM_F3,         0  },
    { 5, 1, 'S',                's' },
    { 5, 2, 'F',                'f' },
    { 5, 3, 'H',                'h' },
    { 5, 4, 'K',                'k' },
    { 5, 5, ':',                '[' },
    { 5, 6, '=',                 0  },
    { 5, 7, UKEY_CBM_COMMODORE,  0  },
    { 5, 8, UKEY_CBM_KP_4,       0  },
    { 5, 9, UKEY_CBM_KP_6,       0  },
    { 5, 10, UKEY_CBM_CURSOR_LT, 0  },

    // row 6 (PB6): F5, E, T, U, O, @, ↑, Q, KP7, KP9, CRSR→(ext)
    { 6, 0, UKEY_CBM_F5,         0  },
    { 6, 1, 'E',                'e' },
    { 6, 2, 'T',                't' },
    { 6, 3, 'U',                'u' },
    { 6, 4, 'O',                'o' },
    { 6, 5, '@',                 0  },
    { 6, 6, UKEY_UP_ARROW,  UKEY_PI },
    { 6, 7, 'Q',                'q' },
    { 6, 8, UKEY_CBM_KP_7,       0  },
    { 6, 9, UKEY_CBM_KP_9,       0  },
    { 6, 10, UKEY_CBM_CURSOR_RT, 0  },

    // row 7 (PB7): CRSR↓, LSHIFT, X, V, N, comma, /, RUN/STOP, KP1, KP3, NO_SCROLL
    { 7, 0, UKEY_CBM_CURSOR_DN,  0  },
    { 7, 1, UKEY_CBM_SHIFT_L,    0  },
    { 7, 2, 'X',                'x' },
    { 7, 3, 'V',                'v' },
    { 7, 4, 'N',                'n' },
    { 7, 5, ',',                '<' },
    { 7, 6, '/',                '?' },
    { 7, 7, UKEY_CBM_RUN_STOP,   0  },
    { 7, 8, UKEY_CBM_KP_1,       0  },
    { 7, 9, UKEY_CBM_KP_3,       0  },
    { 7, 10, UKEY_CBM_NO_SCROLL, 0  },
};

static const KeyCharOverride c128_char_overrides[] = {
    { '\\', 1, 7, KEYMOD_NONE  },   // host backslash → guest ← key (PB1/PA7)
    { '|',  6, 6, KEYMOD_NONE  },   // host pipe → guest ↑ key (PB6/PA6)
    { '^',  0, 6, KEYMOD_NONE  },   // host caret → guest £ key (PB0/PA6)
    { '~',  6, 6, KEYMOD_SHIFT },   // host tilde → guest π (Shift+↑, PB6/PA6)
    { '{',  3, 3, KEYMOD_SHIFT },   // no Commodore { → fall back to ( (PB3/PA3)
    { '}',  0, 4, KEYMOD_SHIFT },   // no Commodore } → fall back to ) (PB0/PA4)
};

// ============================================================================
// Host key bindings — SDL_Keycode → guest char32_t
// ============================================================================

static const HostKeyBinding c128_host_bindings[] = {
    // Modifiers
    { SDLK_LSHIFT,     UKEY_CBM_SHIFT_L    },
    { SDLK_RSHIFT,     UKEY_CBM_SHIFT_R    },
    { SDLK_LCTRL,      UKEY_CBM_CTRL       },
    { SDLK_LGUI,       UKEY_CBM_COMMODORE  },
    { SDLK_RALT,       UKEY_CBM_ALT        },

    // Action keys
    { SDLK_TAB,        UKEY_CBM_RUN_STOP   },
    { SDLK_BACKSPACE,  UKEY_CBM_DEL        },
    { SDLK_HOME,       UKEY_CBM_HOME       },
    { SDLK_ESCAPE,     UKEY_ESCAPE         },

    // Navigation — C64-compatible cursor keys as primary targets
    { SDLK_DOWN,       UKEY_CBM_CURSOR_DN  },
    { SDLK_RIGHT,      UKEY_CBM_CURSOR_RT  },
    // Dedicated C128 cursor keys (col 10) also mapped:
    { SDLK_UP,         UKEY_CBM_CURSOR_UP  },
    { SDLK_LEFT,       UKEY_CBM_CURSOR_LT  },

    // Function keys
    { SDLK_F1,         UKEY_CBM_F1         },
    { SDLK_F3,         UKEY_CBM_F3         },
    { SDLK_F5,         UKEY_CBM_F5         },
    { SDLK_F7,         UKEY_CBM_F7         },
    { SDLK_F9,         UKEY_CBM_HELP       },

    // Non-ASCII character keys
    { SDLK_BACKSLASH,  UKEY_LEFT_ARROW     },
    { SDLK_PIPE,       UKEY_UP_ARROW       },
    { SDLK_CARET,      UKEY_POUND_SIGN     },

    // C128-specific keys
    { SDLK_KP_ENTER,   UKEY_CBM_LINE_FEED  },

    // Numeric keypad
    { SDLK_KP_0,       UKEY_CBM_KP_0       },
    { SDLK_KP_1,       UKEY_CBM_KP_1       },
    { SDLK_KP_2,       UKEY_CBM_KP_2       },
    { SDLK_KP_3,       UKEY_CBM_KP_3       },
    { SDLK_KP_4,       UKEY_CBM_KP_4       },
    { SDLK_KP_5,       UKEY_CBM_KP_5       },
    { SDLK_KP_6,       UKEY_CBM_KP_6       },
    { SDLK_KP_7,       UKEY_CBM_KP_7       },
    { SDLK_KP_8,       UKEY_CBM_KP_8       },
    { SDLK_KP_9,       UKEY_CBM_KP_9       },
    { SDLK_KP_PLUS,    UKEY_CBM_KP_PLUS    },
    { SDLK_KP_MINUS,   UKEY_CBM_KP_MINUS   },
    { SDLK_KP_PERIOD,  UKEY_CBM_KP_PERIOD  },
};

const keyboard_matrix_config_t c128_keyboard_config = {
    .model = KEYBOARD_MODEL_C128,
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C128_KEYBOARD_ROWS,
    .cols = C128_KEYBOARD_COLS,
    .description = "C128 11x8 keyboard matrix (CIA 1, C64-compatible + extended)",
    .entries = c128_matrix,
    .num_entries = sizeof(c128_matrix) / sizeof(c128_matrix[0]),
    .char_overrides = c128_char_overrides,
    .num_char_overrides = sizeof(c128_char_overrides) / sizeof(c128_char_overrides[0]),
    .host_bindings = c128_host_bindings,
    .num_host_bindings = sizeof(c128_host_bindings) / sizeof(c128_host_bindings[0]),
};
