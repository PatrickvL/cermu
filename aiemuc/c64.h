#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>

// Compiler optimization hints
#ifdef _MSC_VER
#define likely(x)   (x)
#define unlikely(x) (x)
#else
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// ============================================================================
// GLOBAL DEVICE INSTANCES - Declared in c64.c
// Note: Full types are defined in device headers, accessed where needed
// ============================================================================

// Global device instances - centralized in c64.c
// Files that need access to these devices should include globals.h

// Main emulator functions
void c64_init(void);
void c64_emulate_frame(void);

#endif // C64_H