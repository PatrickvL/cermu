#include "systems/commodore/c16/c16_keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"

// ============================================================================
// C16 / Plus/4 Keyboard Matrix — 8×8
// ============================================================================
// TED PIO2 ($FD30) selects which row(s) to scan (active-low output).
// TED register $FF08 reads the column result (active-low input).
//
// Row = PIO2 bit position (0–7), Col = result bit position (0–7).
// These values are used directly by the scan callback:
//   row_open_contacts[PIO2_bit]
//
// Hardware matrix (VICE Plus4 gtk3_pos.vkm reference):
//       col 0       col 1      col 2      col 3    col 4    col 5    col 6     col 7
// row 0: INST/DEL   RETURN     £          HELP/F7  F1       F2       F3        @
// row 1: 3          W          A          4        Z        S        E         SHIFTs
// row 2: 5          R          D          6        C        F        T         X
// row 3: 7          Y          G          8        B        H        U         V
// row 4: 9          I          J          0        M        K        O         N
// row 5: CRSR↓      P          L          CRSR↑    .        :        -         ,
// row 6: CRSR←      *          ;          CRSR→    ESC      =        +         /
// row 7: 1          HOME       CTRL       2        SPACE    C=       Q         RUN/STOP
//
// Key differences from C64:
//   - Dedicated cursor keys (UP, DOWN, LEFT, RIGHT — no SHIFT required)
//   - ESC key at (6,4)
//   - Both SHIFT keys wired to same position (1,7)
//   - Function keys: F1 (0,4), F2 (0,5), F3 (0,6), HELP/F7 (0,3)
//   - No separate RESTORE key in matrix (wired directly to NMI)

static const KeyMatrixEntry c16_matrix[] = {
    // row 0: DEL, RETURN, £, HELP/F7, F1, F2, F3, @
    { 0, 0, UKEY_CBM_DEL,       0  },
    { 0, 1, '\r',               0  },
    { 0, 2, UKEY_POUND_SIGN,    0  },
    { 0, 3, UKEY_CBM_F7,        0  },
    { 0, 4, UKEY_CBM_F1,        0  },
    { 0, 5, UKEY_CBM_F2,        0  },
    { 0, 6, UKEY_CBM_F3,        0  },
    { 0, 7, '@',                0  },

    // row 1: 3, W, A, 4, Z, S, E, SHIFT
    { 1, 0, '3',               '#' },
    { 1, 1, 'W',               'w' },
    { 1, 2, 'A',               'a' },
    { 1, 3, '4',               '$' },
    { 1, 4, 'Z',               'z' },
    { 1, 5, 'S',               's' },
    { 1, 6, 'E',               'e' },
    { 1, 7, UKEY_CBM_SHIFT_L,   0  },

    // row 2: 5, R, D, 6, C, F, T, X
    { 2, 0, '5',               '%' },
    { 2, 1, 'R',               'r' },
    { 2, 2, 'D',               'd' },
    { 2, 3, '6',               '&' },
    { 2, 4, 'C',               'c' },
    { 2, 5, 'F',               'f' },
    { 2, 6, 'T',               't' },
    { 2, 7, 'X',               'x' },

    // row 3: 7, Y, G, 8, B, H, U, V
    { 3, 0, '7',              '\'' },
    { 3, 1, 'Y',               'y' },
    { 3, 2, 'G',               'g' },
    { 3, 3, '8',               '(' },
    { 3, 4, 'B',               'b' },
    { 3, 5, 'H',               'h' },
    { 3, 6, 'U',               'u' },
    { 3, 7, 'V',               'v' },

    // row 4: 9, I, J, 0, M, K, O, N
    { 4, 0, '9',               ')' },
    { 4, 1, 'I',               'i' },
    { 4, 2, 'J',               'j' },
    { 4, 3, '0',                0  },
    { 4, 4, 'M',               'm' },
    { 4, 5, 'K',               'k' },
    { 4, 6, 'O',               'o' },
    { 4, 7, 'N',               'n' },

    // row 5: CRSR↓, P, L, CRSR↑, ., :, -, ,
    { 5, 0, UKEY_CBM_CURSOR_DN, 0  },
    { 5, 1, 'P',               'p' },
    { 5, 2, 'L',               'l' },
    { 5, 3, UKEY_CBM_CURSOR_UP, 0  },
    { 5, 4, '.',               '>' },
    { 5, 5, ':',               '[' },
    { 5, 6, '-',                0  },
    { 5, 7, ',',               '<' },

    // row 6: CRSR←, *, ;, CRSR→, ESC, =, +, /
    { 6, 0, UKEY_CBM_CURSOR_LT, 0  },
    { 6, 1, '*',                0  },
    { 6, 2, ';',               ']' },
    { 6, 3, UKEY_CBM_CURSOR_RT, 0  },
    { 6, 4, UKEY_ESCAPE,        0  },
    { 6, 5, '=',                0  },
    { 6, 6, '+',                0  },
    { 6, 7, '/',               '?' },

    // row 7: 1, HOME, CTRL, 2, SPACE, C=, Q, RUN/STOP
    { 7, 0, '1',               '!' },
    { 7, 1, UKEY_CBM_HOME,      0  },
    { 7, 2, UKEY_CBM_CTRL,      0  },
    { 7, 3, '2',               '"' },
    { 7, 4, ' ',                0  },
    { 7, 5, UKEY_CBM_COMMODORE, 0  },
    { 7, 6, 'Q',               'q' },
    { 7, 7, UKEY_CBM_RUN_STOP,  0  },
};

static const KeyCharOverride c16_char_overrides[] = {
    { '\\', 6, 5, KEYMOD_SHIFT },   // host backslash → guest ← (Shift+=)
    { '|',  4, 3, KEYMOD_SHIFT },   // host pipe → guest ↑ (Shift+0)
    { '^',  0, 2, KEYMOD_NONE  },   // host caret → guest £ key
    { '~',  6, 5, KEYMOD_CBM   },   // host tilde → guest π (C=+=)

    // Host chars with no Commodore equivalent → fall back
    { '{',  3, 3, KEYMOD_SHIFT },   // no Commodore { → fall back to (
    { '}',  4, 0, KEYMOD_SHIFT },   // no Commodore } → fall back to )
};

static const HostKeyBinding c16_host_bindings[] = {
    { SDLK_LSHIFT,    UKEY_CBM_SHIFT_L   },
    { SDLK_RSHIFT,    UKEY_CBM_SHIFT_L   },  // both wired to same position
    { SDLK_LCTRL,     UKEY_CBM_CTRL      },
    { SDLK_LGUI,      UKEY_CBM_COMMODORE },
    { SDLK_TAB,       UKEY_CBM_RUN_STOP  },
    { SDLK_BACKSPACE, UKEY_CBM_DEL       },
    { SDLK_HOME,      UKEY_CBM_HOME      },
    { SDLK_DOWN,      UKEY_CBM_CURSOR_DN },
    { SDLK_UP,        UKEY_CBM_CURSOR_UP },
    { SDLK_RIGHT,     UKEY_CBM_CURSOR_RT },
    { SDLK_LEFT,      UKEY_CBM_CURSOR_LT },
    { SDLK_ESCAPE,    UKEY_ESCAPE        },
    { SDLK_F1,        UKEY_CBM_F1        },
    { SDLK_F2,        UKEY_CBM_F2        },
    { SDLK_F3,        UKEY_CBM_F3        },
    { SDLK_F7,        UKEY_CBM_F7        },
    { SDLK_CARET,     UKEY_POUND_SIGN    },
};

const keyboard_matrix_config_t c16_keyboard_config = {
    .model = KEYBOARD_MODEL_PLUS4_C16,
    .scan_chip = KEYBOARD_SCAN_TED,
    .rows = C16_KEYBOARD_ROWS,
    .cols = C16_KEYBOARD_COLS,
    .description = "C16/Plus4 8x8 keyboard matrix (TED 7360)",
    .entries = c16_matrix,
    .num_entries = sizeof(c16_matrix) / sizeof(c16_matrix[0]),
    .char_overrides = c16_char_overrides,
    .num_char_overrides = sizeof(c16_char_overrides) / sizeof(c16_char_overrides[0]),
    .host_bindings = c16_host_bindings,
    .num_host_bindings = sizeof(c16_host_bindings) / sizeof(c16_host_bindings[0]),
};
