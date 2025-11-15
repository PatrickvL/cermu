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

#ifndef FAM65XX_TEMPLATE_CONTEXT
  // Restore compiler diagnostics when parsing standalone
  
  #if defined(_MSC_VER)
    #pragma warning(pop)
  #endif
  
#endif // FAM65XX_TEMPLATE_CONTEXT