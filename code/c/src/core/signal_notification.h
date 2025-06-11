#ifndef SIGNAL_NOTIFICATION_H
#define SIGNAL_NOTIFICATION_H

#include <stdint.h>
#include <stddef.h>
#include "system_lines.h"

/**
 * Lightweight signal change notification system.
 * Allows chips to register for notifications when specific signals change,
 * without the overhead of polling or complex callback systems.
 * 
 * NO GLOBALS - all state is passed explicitly.
 */

typedef struct signal_watcher_s signal_watcher_t;

/**
 * Callback function type for signal change notifications.
 * Called only when watched signals actually change.
 * 
 * @param watcher The watcher that detected the change
 * @param old_lines Previous signal state
 * @param new_lines Current signal state  
 * @param changed_mask Bitmask of which signals changed
 */
typedef void (*signal_change_callback_t)(signal_watcher_t* watcher, 
                                        uint32_t old_lines, 
                                        uint32_t new_lines, 
                                        uint32_t changed_mask);

struct signal_watcher_s {
    uint32_t watch_mask;                    // Which signals to watch for changes
    uint32_t last_state;                    // Last known state of watched signals
    signal_change_callback_t callback;      // Function to call on changes
    void* context;                          // User context for callback
    signal_watcher_t* next;                 // Linked list for system
};

/**
 * Update system lines and notify watchers of changes.
 * This is the only function that should be called frequently.
 * Very lightweight - only calls callbacks if signals actually changed.
 * 
 * @param sys_lines Pointer to the system's line state structure
 * @param new_lines New line state to set
 * @param watchers Linked list of signal watchers (can be NULL)
 */
static inline void signal_update_lines(system_lines_t* sys_lines, 
                                      uint32_t new_lines, 
                                      signal_watcher_t* watchers) {
    uint32_t old_lines = sys_lines->lines;
    if (old_lines == new_lines) {
        return; // No change, very fast path
    }
    
    sys_lines->previous_lines = old_lines;
    sys_lines->lines = new_lines;
    uint32_t changed = old_lines ^ new_lines;
    
    // Walk watchers and notify only those watching changed signals
    for (signal_watcher_t* w = watchers; w != NULL; w = w->next) {
        uint32_t relevant_changes = changed & w->watch_mask;
        if (relevant_changes != 0) {
            w->last_state = new_lines & w->watch_mask;
            w->callback(w, old_lines, new_lines, relevant_changes);
        }
    }
}

/**
 * Fast inline test for specific signal states.
 * Use this for the most performance-critical signal checking.
 */
#define SIGNAL_IS_ASSERTED(sys_lines, mask)    SYS_LINES_TEST(sys_lines, mask)
#define SIGNAL_IS_CLEAR(sys_lines, mask)       (!SYS_LINES_TEST(sys_lines, mask))
#define SIGNAL_GET_VALUE(sys_lines, shift, mask) ((SYS_LINES_RAW(sys_lines) >> (shift)) & (mask))

#endif // SIGNAL_NOTIFICATION_H
