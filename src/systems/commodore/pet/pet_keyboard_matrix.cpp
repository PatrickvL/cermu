/*
 * pet_keyboard_matrix.cpp — Commodore PET Keyboard Matrix Data
 *
 * PET 10×8 matrix via PIA1:
 *   PIA Port A bits[3:0] select keyboard row (active-low, directly decoded)
 *   PIA Port B reads the column lines for the selected row
 *
 * Matrix layout based on the standard PET/CBM "graphics" keyboard
 * (Business keyboard variant has the same matrix, just different
 * physical key labelling).
 *
 * Reference: http://www.6502.org/users/andre/petindex/keyboards.html
 *            Commodore PET Service Manual keyboard matrix schematic
 */

#include "systems/commodore/pet/pet_keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"

// ============================================================================
// PET Keyboard Matrix — 10 rows × 8 columns
// ============================================================================
// PIA1 Port A selects the row (active-low, bits 3:0 decoded)
// PIA1 Port B reads the column (PB0–PB7, active-low)
//
// Row = PIA1 decoded row (0–9), Col = PB bit position (0–7).
// These values are used directly by the scan callback:
//   row_open_contacts[hw_row]
//
// PET "graphics keyboard" key assignments:
//
// IMPORTANT: On the PET graphics keyboard, digit keys have the SYMBOL
// as the unshifted character and the DIGIT as the shifted character.
// This is the reverse of modern keyboards.  For example:
//   Key "2": unshifted = '"', shifted = '2'
//   Key "8": unshifted = £ (PETSCII $5C), shifted = '8'
//
// Row 9 is the REPEAT key, connected directly to PIA1 CA1 (not scanned
// through the matrix).  It has no matrix entries.
//
// Hardware matrix:
//       PB0    PB1    PB2    PB3    PB4    PB5    PB6    PB7
// row 0: !      #      %      &      (      ←      HOME   DEL
// row 1: "      $      '      £      )      -      →      ↓
// row 2: Q      E      T      U      O      ↑      STOP   —
// row 3: W      R      Y      I      P      =      /      —
// row 4: A      D      G      J      L      ;      RETURN —
// row 5: S      F      H      K      :      ]      —      —
// row 6: Z      C      B      M      .      LSHIFT —      —
// row 7: X      V      N      ,      @      RSHIFT RVS    —
// row 8: *      £(\)   +      >      ?      SPACE  [      —
// row 9: (REPEAT key — not in matrix, wired to PIA1 CA1)

static const KeyMatrixEntry pet_matrix[] = {
    // row 0: !, #, %, &, (, ←, HOME, DEL
    // Keys "1","3","5","7","9" — unshifted = symbol, shifted = digit
    { 0, 0, '!',               '1'  },
    { 0, 1, '#',               '3'  },
    { 0, 2, '%',               '5'  },
    { 0, 3, '&',               '7'  },
    { 0, 4, '(',               '9'  },
    { 0, 5, UKEY_LEFT_ARROW,    0   },
    { 0, 6, UKEY_CBM_HOME,      0   },
    { 0, 7, UKEY_CBM_DEL,       0   },

    // row 1: ", $, ', £, ), -, CRSR→, CRSR↓
    // Keys "2","4","6","8","0" — unshifted = symbol, shifted = digit
    { 1, 0, '"',               '2'  },
    { 1, 1, '$',               '4'  },
    { 1, 2, '\'',              '6'  },
    { 1, 3, UKEY_POUND_SIGN,   '8'  },
    { 1, 4, ')',               '0'  },
    { 1, 5, '-',                0   },
    { 1, 6, UKEY_CBM_CURSOR_RT, 0   },
    { 1, 7, UKEY_CBM_CURSOR_DN, 0   },

    // row 2: Q, E, T, U, O, ↑, RUN/STOP, (none)
    { 2, 0, 'Q',               'q'  },
    { 2, 1, 'E',               'e'  },
    { 2, 2, 'T',               't'  },
    { 2, 3, 'U',               'u'  },
    { 2, 4, 'O',               'o'  },
    { 2, 5, UKEY_UP_ARROW,    UKEY_PI },
    { 2, 6, UKEY_CBM_RUN_STOP,  0   },

    // row 3: W, R, Y, I, P, =, /, (none)
    { 3, 0, 'W',               'w'  },
    { 3, 1, 'R',               'r'  },
    { 3, 2, 'Y',               'y'  },
    { 3, 3, 'I',               'i'  },
    { 3, 4, 'P',               'p'  },
    { 3, 5, '=',                0   },
    { 3, 6, '/',                0   },

    // row 4: A, D, G, J, L, ;, RETURN, (none)
    { 4, 0, 'A',               'a'  },
    { 4, 1, 'D',               'd'  },
    { 4, 2, 'G',               'g'  },
    { 4, 3, 'J',               'j'  },
    { 4, 4, 'L',               'l'  },
    { 4, 5, ';',                0   },
    { 4, 6, '\r',               0   },
 
    // row 5: S, F, H, K, :, ], (none), (none)
    { 5, 0, 'S',               's'  },
    { 5, 1, 'F',               'f'  },
    { 5, 2, 'H',               'h'  },
    { 5, 3, 'K',               'k'  },
    { 5, 4, ':',               '['  },
    { 5, 5, ']',                0   },

    // row 6: Z, C, B, M, ., LSHIFT, (none), (none)
    { 6, 0, 'Z',               'z'  },
    { 6, 1, 'C',               'c'  },
    { 6, 2, 'B',               'b'  },
    { 6, 3, 'M',               'm'  },
    { 6, 4, '.',                0   },
    { 6, 5, UKEY_CBM_SHIFT_L,   0   },

    // row 7: X, V, N, ,, @, RSHIFT, RVS, (none)
    { 7, 0, 'X',               'x'  },
    { 7, 1, 'V',               'v'  },
    { 7, 2, 'N',               'n'  },
    { 7, 3, ',',               '<'  },
    { 7, 4, '@',                0   },
    { 7, 5, UKEY_CBM_SHIFT_R,   0   },
    { 7, 6, UKEY_CBM_REVERSE,   0   },

    // row 8: *, £, +, >, ?, SPACE, [, (none)
    { 8, 0, '*',                0   },
    { 8, 1, UKEY_POUND_SIGN,    0   },
    { 8, 2, '+',                0   },
    { 8, 3, '>',                0   },
    { 8, 4, '?',                0   },
    { 8, 5, ' ',                0   },
    { 8, 6, '[',                0   },

    // row 9: REPEAT key — wired to PIA1 CA1, not in the matrix scan path.
    // No entries.
};

static const KeyCharOverride pet_char_overrides[] = {
    { '\\', 0, 5, KEYMOD_NONE  },   // host backslash → guest ← key
    { '|',  2, 5, KEYMOD_NONE  },   // host pipe → guest ↑ key
    { '^',  1, 3, KEYMOD_NONE  },   // host caret → guest £ key (row 1, PB3)
    { '~',  2, 5, KEYMOD_SHIFT },   // host tilde → guest π (Shift+↑)
    { '{',  0, 4, KEYMOD_NONE  },   // no Commodore { → fall back to (
    { '}',  1, 4, KEYMOD_NONE  },   // no Commodore } → fall back to )
};

// ============================================================================
// Host key bindings — SDL_Keycode → guest char32_t
// ============================================================================

static const HostKeyBinding pet_host_bindings[] = {
    // Modifiers
    { SDLK_LSHIFT,    UKEY_CBM_SHIFT_L   },
    { SDLK_RSHIFT,    UKEY_CBM_SHIFT_R   },
    { SDLK_LCTRL,     UKEY_CBM_REVERSE   },   // PET: CTRL → RVS key

    // Action keys
    { SDLK_TAB,       UKEY_CBM_RUN_STOP  },
    { SDLK_BACKSPACE, UKEY_CBM_DEL       },
    { SDLK_HOME,      UKEY_CBM_HOME      },

    // Navigation
    { SDLK_DOWN,      UKEY_CBM_CURSOR_DN },
    { SDLK_RIGHT,     UKEY_CBM_CURSOR_RT },

    // Non-ASCII character keys
    { SDLK_BACKSLASH, UKEY_LEFT_ARROW    },
    { SDLK_PIPE,      UKEY_UP_ARROW      },
    { SDLK_CARET,     UKEY_POUND_SIGN    },
};

const keyboard_matrix_config_t pet_keyboard_config = {
    .model = KEYBOARD_MODEL_PET,
    .scan_chip = KEYBOARD_SCAN_PIA,
    .rows = PET_KEYBOARD_ROWS,
    .cols = PET_KEYBOARD_COLS,
    .description = "PET 10x8 keyboard matrix (graphics keyboard)",
    .entries = pet_matrix,
    .num_entries = sizeof(pet_matrix) / sizeof(pet_matrix[0]),
    .char_overrides = pet_char_overrides,
    .num_char_overrides = sizeof(pet_char_overrides) / sizeof(pet_char_overrides[0]),
    .host_bindings = pet_host_bindings,
    .num_host_bindings = sizeof(pet_host_bindings) / sizeof(pet_host_bindings[0]),
};
