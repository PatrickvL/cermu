/*
 * inc_lint_prevention_footer.hpp - Footer for Lint Prevention System
 *
 * This footer restores compiler diagnostics for .inc.hpp files.
 * It must be included at the end of each .inc.hpp file.
 *
 * USAGE:
 * ======
 * At the end of each .inc.hpp file:
 *   #include "chip/cpu/m680x0/operations/inc_lint_prevention_footer.hpp"
 */

#ifndef M680X0_TEMPLATE_CONTEXT
// Restore compiler diagnostics when parsing standalone

CERMU_MSVC_WARNING_POP

#endif // M680X0_TEMPLATE_CONTEXT
