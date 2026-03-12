/*
 * inc_lint_prevention.hpp - Simple Lint Prevention for .inc.hpp Files
 *
 * This header disables IDE parsing for .inc.hpp files when they are analyzed
 * standalone, since these files are designed to be included within the
 * fam65xx_t template class context and are not valid C++ on their own.
 *
 * USAGE:
 * ======
 * At the start of each .inc.hpp file:
 *   #include "chip/cpu/fam65xx/operations/inc_lint_prevention.hpp"
 *
 * At the end of each .inc.hpp file:
 *   #include "chip/cpu/fam65xx/operations/inc_lint_prevention_footer.hpp"
 */

#pragma once

#ifndef FAM65XX_TEMPLATE_CONTEXT
// When .inc.hpp files are parsed standalone (outside template context),
// we disable all content analysis by ending the file early.
// This prevents IDE errors since these files are meant to be included
// inside the fam65xx_t class definition.

// Include the types needed for basic IDE symbol resolution
#include "chip/cpu/fam65xx/fam65xx_types.hpp"
#include "core/cermu.hpp"

// Suppress all compiler diagnostics for standalone analysis
CERMU_PRAGMA_SYSTEM_HEADER

// Skip all content when parsed standalone - this prevents IDE errors
#define FAM65XX_SKIP_IMPLEMENTATION

#endif // FAM65XX_TEMPLATE_CONTEXT
