#include "systems/commodore/c64/c64_keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"

// ============================================================================
// C64 Keyboard Matrix — 8×8
// ============================================================================
// CIA1 Port A ($DC00) = column select (output, active-low)
// CIA1 Port B ($DC01) = row read (input, active-low)
//
// Row = PB bit position (0–7), Col = PA bit position (0–7).
// These values are used directly by the scan callbacks:
//   row_open_contacts[row] indexed by PB, containing PA bit data
//   col_open_contacts[col] indexed by PA, containing PB bit data
//
// KERNAL keyboard decode table index = PA * 8 + PB (PA-major).
// The KERNAL scans PA columns (outer loop via $DC00 writes) and reads
// PB rows (inner loop via LSR on $DC01 data).
//
// Hardware matrix (C64 PRG, Table of Keyboard Matrix Values):
//          PA0      PA1     PA2    PA3    PA4    PA5    PA6      PA7
// PB0:     DEL      3       5      7      9      +      £        1
// PB1:     RETURN   W       R      Y      I      P      *        ←
// PB2:     CRSR→    A       D      G      J      L      ;        CTRL
// PB3:     F7       4       6      8      0      -      HOME     2
// PB4:     F1       Z       C      B      M      .      RSHIFT   SPACE
// PB5:     F3       S       F      H      K      :      =        C=
// PB6:     F5       E       T      U      O      @      ↑        Q
// PB7:     CRSR↓    LSHIFT  X      V      N      ,      /        RUNSTOP

static const KeyMatrixEntry c64_matrix[] = {
    // row 0 (PB0): DEL, 3, 5, 7, 9, +, £, 1
    //              PA0  PA1  PA2  PA3  PA4  PA5  PA6  PA7
    { 0, 0, UKEY_CBM_DEL,       0   },
    { 0, 1, '3',               '#'  },
    { 0, 2, '5',               '%'  },
    { 0, 3, '7',              '\''  },
    { 0, 4, '9',               ')'  },
    { 0, 5, '+',                0   },
    { 0, 6, UKEY_POUND_SIGN,    0   },
    { 0, 7, '1',               '!'  },

    // row 1 (PB1): RETURN, W, R, Y, I, P, *, ←
    { 1, 0, '\r',               0   },
    { 1, 1, 'W',               'w'  },
    { 1, 2, 'R',               'r'  },
    { 1, 3, 'Y',               'y'  },
    { 1, 4, 'I',               'i'  },
    { 1, 5, 'P',               'p'  },
    { 1, 6, '*',                0   },
    { 1, 7, UKEY_LEFT_ARROW,    0   },

    // row 2 (PB2): CRSR→, A, D, G, J, L, ;, CTRL
    { 2, 0, UKEY_CBM_CURSOR_RT, 0   },
    { 2, 1, 'A',               'a'  },
    { 2, 2, 'D',               'd'  },
    { 2, 3, 'G',               'g'  },
    { 2, 4, 'J',               'j'  },
    { 2, 5, 'L',               'l'  },
    { 2, 6, ';',               ']'  },
    { 2, 7, UKEY_CBM_CTRL,      0   },

    // row 3 (PB3): F7, 4, 6, 8, 0, -, HOME, 2
    { 3, 0, UKEY_CBM_F7,        0   },
    { 3, 1, '4',               '$'  },
    { 3, 2, '6',               '&'  },
    { 3, 3, '8',               '('  },
    { 3, 4, '0',                0   },
    { 3, 5, '-',                0   },
    { 3, 6, UKEY_CBM_HOME,      0   },
    { 3, 7, '2',               '"'  },

    // row 4 (PB4): F1, Z, C, B, M, ., RSHIFT, SPACE
    { 4, 0, UKEY_CBM_F1,        0   },
    { 4, 1, 'Z',               'z'  },
    { 4, 2, 'C',               'c'  },
    { 4, 3, 'B',               'b'  },
    { 4, 4, 'M',               'm'  },
    { 4, 5, '.',               '>'  },
    { 4, 6, UKEY_CBM_SHIFT_R,   0   },
    { 4, 7, ' ',                0   },

    // row 5 (PB5): F3, S, F, H, K, :, =, C=
    { 5, 0, UKEY_CBM_F3,        0   },
    { 5, 1, 'S',               's'  },
    { 5, 2, 'F',               'f'  },
    { 5, 3, 'H',               'h'  },
    { 5, 4, 'K',               'k'  },
    { 5, 5, ':',               '['  },
    { 5, 6, '=',                0   },
    { 5, 7, UKEY_CBM_COMMODORE, 0   },

    // row 6 (PB6): F5, E, T, U, O, @, ↑, Q
    { 6, 0, UKEY_CBM_F5,        0   },
    { 6, 1, 'E',               'e'  },
    { 6, 2, 'T',               't'  },
    { 6, 3, 'U',               'u'  },
    { 6, 4, 'O',               'o'  },
    { 6, 5, '@',                0   },
    { 6, 6, UKEY_UP_ARROW, UKEY_PI  },
    { 6, 7, 'Q',               'q'  },

    // row 7 (PB7): CRSR↓, LSHIFT, X, V, N, comma, /, RUNSTOP
    { 7, 0, UKEY_CBM_CURSOR_DN, 0   },
    { 7, 1, UKEY_CBM_SHIFT_L,   0   },
    { 7, 2, 'X',               'x'  },
    { 7, 3, 'V',               'v'  },
    { 7, 4, 'N',               'n'  },
    { 7, 5, ',',               '<'  },
    { 7, 6, '/',               '?'  },
    { 7, 7, UKEY_CBM_RUN_STOP,  0   },
};

// ============================================================================
// Host key bindings — SDL_Keycode → guest char32_t
// ============================================================================

static const HostKeyBinding c64_host_bindings[] = {
    // Modifiers
    { SDLK_LSHIFT,    UKEY_CBM_SHIFT_L   },
    { SDLK_RSHIFT,    UKEY_CBM_SHIFT_R   },
    { SDLK_LCTRL,     UKEY_CBM_CTRL      },
    { SDLK_LGUI,      UKEY_CBM_COMMODORE },

    // Action keys
    { SDLK_TAB,        UKEY_CBM_RUN_STOP },
    { SDLK_BACKSPACE,  UKEY_CBM_DEL      },
    { SDLK_HOME,       UKEY_CBM_HOME     },

    // Navigation
    { SDLK_DOWN,       UKEY_CBM_CURSOR_DN },
    { SDLK_RIGHT,      UKEY_CBM_CURSOR_RT },

    // Function keys
    { SDLK_F1,         UKEY_CBM_F1       },
    { SDLK_F3,         UKEY_CBM_F3       },
    { SDLK_F5,         UKEY_CBM_F5       },
    { SDLK_F7,         UKEY_CBM_F7       },

    // Non-ASCII character keys
    { SDLK_BACKSLASH,  UKEY_LEFT_ARROW   },
    { SDLK_PIPE,       UKEY_UP_ARROW     },
    { SDLK_CARET,      UKEY_POUND_SIGN   },
};

// ============================================================================
// Character overrides
// ============================================================================

static const KeyCharOverride c64_char_overrides[] = {
    { '\\', 1, 7, KEYMOD_NONE  },   // host backslash → guest ← key (PB1/PA7)
    { '|',  6, 6, KEYMOD_NONE  },   // host pipe → guest ↑ key (PB6/PA6)
    { '^',  0, 6, KEYMOD_NONE  },   // host caret → guest £ key (PB0/PA6)
    { '~',  6, 6, KEYMOD_SHIFT },   // host tilde → guest π (Shift+↑, PB6/PA6)

    // Host chars with no Commodore equivalent → fall back
    { '{',  3, 3, KEYMOD_SHIFT },   // no Commodore { → fall back to ( (PB3/PA3)
    { '}',  0, 4, KEYMOD_SHIFT },   // no Commodore } → fall back to ) (PB0/PA4)
};

const keyboard_matrix_config_t c64_keyboard_config = {
    .model = KEYBOARD_MODEL_C64,
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C64_KEYBOARD_ROWS,
    .cols = C64_KEYBOARD_COLS,
    .description = "C64 8x8 keyboard matrix",
    .entries = c64_matrix,
    .num_entries = sizeof(c64_matrix) / sizeof(c64_matrix[0]),
    .char_overrides = c64_char_overrides,
    .num_char_overrides = sizeof(c64_char_overrides) / sizeof(c64_char_overrides[0]),
    .host_bindings = c64_host_bindings,
    .num_host_bindings = sizeof(c64_host_bindings) / sizeof(c64_host_bindings[0]),
};
