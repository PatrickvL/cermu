// m680x0_ops.inc.hpp — Instruction operation handlers (aggregator)
//
// Includes all split operation files in dependency order.
// Must be included inside m680x0_t class body with M680X0_TEMPLATE_CONTEXT defined.

// EA evaluation helpers (used by all instruction groups)
#include "chip/cpu/m680x0/operations/m680x0_ea.inc.hpp"

// Move instructions (Groups 1/2/3, 7)
#include "chip/cpu/m680x0/operations/m680x0_move_ops.inc.hpp"

// Arithmetic, logic, and shift instructions (Groups 0, 5, 8, 9, B, C, D, E)
#include "chip/cpu/m680x0/operations/m680x0_arith_ops.inc.hpp"

// Control flow instructions (Groups 4, 6)
#include "chip/cpu/m680x0/operations/m680x0_ctrl_ops.inc.hpp"

