/*
 * inc_lint_prevention_footer.hpp - Footer for Lint Prevention System
 *
 * This footer restores compiler diagnostics for .inc.hpp files.
 * It must be included at the end of each .inc.hpp file.
 *
 * USAGE:
 * ======
 * At the end of each .inc.hpp file:
 *   #include "inc_lint_prevention_footer.hpp"
 */

#ifndef Z80_TEMPLATE_CONTEXT
// Restore compiler diagnostics when parsing standalone

CERMU_MSVC_WARNING_POP

#endif // Z80_TEMPLATE_CONTEXT
