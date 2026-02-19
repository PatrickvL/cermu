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
 *   #include "inc_lint_prevention.hpp"
 *
 * At the end of each .inc.hpp file:
 *   #include "inc_lint_prevention_footer.hpp"
 */

#pragma once

#ifndef FAM65XX_TEMPLATE_CONTEXT
// When .inc.hpp files are parsed standalone (outside template context),
// we disable all content analysis by ending the file early.
// This prevents IDE errors since these files are meant to be included
// inside the fam65xx_t class definition.

// Include the types needed for basic IDE symbol resolution
#include "../fam65xx_types.h"

// Suppress all compiler diagnostics for standalone analysis
#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__)
#pragma GCC system_header
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif

// Skip all content when parsed standalone - this prevents IDE errors
#define FAM65XX_SKIP_IMPLEMENTATION

#endif // FAM65XX_TEMPLATE_CONTEXT
