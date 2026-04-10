#include "systems/commodore/vic20/vic20_keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"

// ============================================================================
// VIC-20 Keyboard Matrix — 8×8
// ============================================================================
// VIA Port B ($9120) = column select (output, active-low)
// VIA Port A ($9121) = row read (input, active-low)
//
// Row = PB bit position (0–7), Col = PA bit position (0–7).
// These values are used directly by the scan callbacks:
//   row_open_contacts[PB_bit], col_open_contacts[PA_bit]
//
// NOTE: On VIC-20 hardware, PB drives columns and PA reads rows —
// the reverse naming from C64.  The scan callback indexes
// row_open_contacts[PB_bit] → returns PA bit values.
//
// The VIC-20 reference matrix is conventionally printed as [PA row][PB col],
// but entries here are stored as {PB_bit, PA_bit} to match the scan code.
//
// Reference (transposed to PB rows × PA cols):
//          PA0     PA1     PA2    PA3    PA4    PA5    PA6      PA7
// PB0:     1       3       5      7      9      +      £        DEL
// PB1:     ←       W       R      Y      I      P      *        RETURN
// PB2:     CTRL    A       D      G      J      L      ;        CRSR→
// PB3:     R/S     LSHIFT  X      V      N      ,      /        CRSR↓
// PB4:     SPACE   Z       C      B      M      .      RSHIFT   F1
// PB5:     C=      S       F      H      K      :      =        F3
// PB6:     Q       E       T      U      O      @      ↑        F5
// PB7:     2       4       6      8      0      -      HOME     F7

static const KeyMatrixEntry vic20_matrix[] = {
    // row 0 (PB0): 1, 3, 5, 7, 9, +, £, DEL
    //              PA0  PA1  PA2  PA3  PA4  PA5  PA6  PA7
    { 0, 0, '1',              '!'  },
    { 0, 1, '3',              '#'  },
    { 0, 2, '5',              '%'  },
    { 0, 3, '7',             '\''  },
    { 0, 4, '9',              ')'  },
    { 0, 5, '+',               0   },
    { 0, 6, UKEY_POUND_SIGN,   0   },
    { 0, 7, UKEY_CBM_DEL,      0   },

    // row 1 (PB1): ←, W, R, Y, I, P, *, RETURN
    { 1, 0, UKEY_LEFT_ARROW,   0   },
    { 1, 1, 'W',              'w'  },
    { 1, 2, 'R',              'r'  },
    { 1, 3, 'Y',              'y'  },
    { 1, 4, 'I',              'i'  },
    { 1, 5, 'P',              'p'  },
    { 1, 6, '*',               0   },
    { 1, 7, '\r',              0   },

    // row 2 (PB2): CTRL, A, D, G, J, L, ;, CRSR→
    { 2, 0, UKEY_CBM_CTRL,     0   },
    { 2, 1, 'A',              'a'  },
    { 2, 2, 'D',              'd'  },
    { 2, 3, 'G',              'g'  },
    { 2, 4, 'J',              'j'  },
    { 2, 5, 'L',              'l'  },
    { 2, 6, ';',              ']'  },
    { 2, 7, UKEY_CBM_CURSOR_RT, 0  },

    // row 3 (PB3): RUN/STOP, LSHIFT, X, V, N, comma, /, CRSR↓
    { 3, 0, UKEY_CBM_RUN_STOP, 0   },
    { 3, 1, UKEY_CBM_SHIFT_L,  0   },
    { 3, 2, 'X',              'x'  },
    { 3, 3, 'V',              'v'  },
    { 3, 4, 'N',              'n'  },
    { 3, 5, ',',              '<'  },
    { 3, 6, '/',              '?'  },
    { 3, 7, UKEY_CBM_CURSOR_DN, 0  },

    // row 4 (PB4): SPACE, Z, C, B, M, ., RSHIFT, F1
    { 4, 0, ' ',               0   },
    { 4, 1, 'Z',              'z'  },
    { 4, 2, 'C',              'c'  },
    { 4, 3, 'B',              'b'  },
    { 4, 4, 'M',              'm'  },
    { 4, 5, '.',              '>'  },
    { 4, 6, UKEY_CBM_SHIFT_R,  0   },
    { 4, 7, UKEY_CBM_F1,       0   },

    // row 5 (PB5): C=, S, F, H, K, :, =, F3
    { 5, 0, UKEY_CBM_COMMODORE, 0  },
    { 5, 1, 'S',              's'  },
    { 5, 2, 'F',              'f'  },
    { 5, 3, 'H',              'h'  },
    { 5, 4, 'K',              'k'  },
    { 5, 5, ':',              '['  },
    { 5, 6, '=',               0   },
    { 5, 7, UKEY_CBM_F3,       0   },

    // row 6 (PB6): Q, E, T, U, O, @, ↑, F5
    { 6, 0, 'Q',              'q'  },
    { 6, 1, 'E',              'e'  },
    { 6, 2, 'T',              't'  },
    { 6, 3, 'U',              'u'  },
    { 6, 4, 'O',              'o'  },
    { 6, 5, '@',               0   },
    { 6, 6, UKEY_UP_ARROW, UKEY_PI },
    { 6, 7, UKEY_CBM_F5,       0   },

    // row 7 (PB7): 2, 4, 6, 8, 0, -, HOME, F7
    { 7, 0, '2',              '"'  },
    { 7, 1, '4',              '$'  },
    { 7, 2, '6',              '&'  },
    { 7, 3, '8',              '('  },
    { 7, 4, '0',               0   },
    { 7, 5, '-',               0   },
    { 7, 6, UKEY_CBM_HOME,      0  },
    { 7, 7, UKEY_CBM_F7,        0  },
};

static const KeyCharOverride vic20_char_overrides[] = {
    { '\\', 1, 0, KEYMOD_NONE  },   // host backslash → guest ← key (PB1/PA0)
    { '|',  6, 6, KEYMOD_NONE  },   // host pipe → guest ↑ key (PB6/PA6)
    { '^',  0, 6, KEYMOD_NONE  },   // host caret → guest £ key (PB0/PA6)
    { '~',  6, 6, KEYMOD_SHIFT },   // host tilde → guest π (Shift+↑, PB6/PA6)
    { '{',  7, 3, KEYMOD_SHIFT },   // no Commodore { → fall back to ( (PB7/PA3)
    { '}',  0, 4, KEYMOD_SHIFT },   // no Commodore } → fall back to ) (PB0/PA4)
};

static const HostKeyBinding vic20_host_bindings[] = {
    { SDLK_LSHIFT,    UKEY_CBM_SHIFT_L   },
    { SDLK_RSHIFT,    UKEY_CBM_SHIFT_R   },
    { SDLK_LCTRL,     UKEY_CBM_CTRL      },
    { SDLK_LGUI,      UKEY_CBM_COMMODORE },
    { SDLK_TAB,       UKEY_CBM_RUN_STOP  },
    { SDLK_BACKSPACE, UKEY_CBM_DEL       },
    { SDLK_HOME,      UKEY_CBM_HOME      },
    { SDLK_DOWN,      UKEY_CBM_CURSOR_DN },
    { SDLK_RIGHT,     UKEY_CBM_CURSOR_RT },
    { SDLK_F1,        UKEY_CBM_F1        },
    { SDLK_F3,        UKEY_CBM_F3        },
    { SDLK_F5,        UKEY_CBM_F5        },
    { SDLK_F7,        UKEY_CBM_F7        },
    { SDLK_BACKSLASH, UKEY_LEFT_ARROW    },
    { SDLK_PIPE,      UKEY_UP_ARROW      },
    { SDLK_CARET,     UKEY_POUND_SIGN    },
};

const keyboard_matrix_config_t vic20_keyboard_config = {
    .model = KEYBOARD_MODEL_VIC20,
    .scan_chip = KEYBOARD_SCAN_VIA,
    .rows = VIC20_KEYBOARD_ROWS,
    .cols = VIC20_KEYBOARD_COLS,
    .description = "VIC-20 8x8 keyboard matrix",
    .entries = vic20_matrix,
    .num_entries = sizeof(vic20_matrix) / sizeof(vic20_matrix[0]),
    .char_overrides = vic20_char_overrides,
    .num_char_overrides = sizeof(vic20_char_overrides) / sizeof(vic20_char_overrides[0]),
    .host_bindings = vic20_host_bindings,
    .num_host_bindings = sizeof(vic20_host_bindings) / sizeof(vic20_host_bindings[0]),
};
