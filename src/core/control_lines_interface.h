#pragma once

#include <cstdint>
#include "system_lines.h"

/**
 * Generic control lines interface for chips that need access to shared bus control lines.
 * This can be used by any chip (CPU, VIC, CIA, etc.) that needs to read or affect control lines.
 * 
 * Uses the system-wide line positions defined in system_lines.h for consistency.
 */
struct control_lines_interface_t {
    /**
     * Callback to get the current state of all control lines.
     * 
     * @param context User-provided context pointer
     * @return Current control line state as bit flags (use SYS_MASK_* constants)
     */
    uint32_t (*get_lines)(void* context);

    /**
     * Callback to set the state of control lines.
     * Sets control lines according to the provided bitmask.
     * 
     * @param context User-provided context pointer
     * @param lines Bitmask of control line states to set (1 = assert, 0 = release)
     */
    void (*set_lines)(void* context, uint32_t lines);

    /**
     * User-provided context pointer passed to all callback functions.
     */
    void* context;
};
