// Example usage in MOS6510 CPU implementation - NO GLOBALS

#include "mos6510_pins.h"
#include "../../../core/signal_notification.h"

typedef struct {
    // CPU state...
    system_lines_t* system_lines;       // Pointer to system-owned line state
    signal_watcher_t* signal_watchers;  // Linked list of watchers
    signal_watcher_t irq_watcher;       // IRQ change watcher
    signal_watcher_t nmi_watcher;       // NMI change watcher
    uint8_t interrupt_pending;
    bool bus_available;
    // ... other state
} mos6510_t;

// IRQ signal change handler
static void mos6510_irq_changed(signal_watcher_t* watcher, 
                               uint32_t old_lines, 
                               uint32_t new_lines, 
                               uint32_t changed_mask) {
    mos6510_t* cpu = (mos6510_t*)watcher->context;
    
    if (new_lines & MOS6510_MASK_IRQ) {
        // IRQ asserted - set interrupt pending flag
        cpu->interrupt_pending |= 0x01; // IRQ_PENDING bit
    }
    // Handle IRQ deassertion if needed...
}

// Example of lightweight signal operations in CPU execution
static inline void mos6510_execute_cycle(mos6510_t* cpu) {
    // Very fast signal checking - no function calls
    if (!MOS6510_PIN_TEST_RDY(cpu->system_lines)) {
        return; // RDY low, CPU stalled - exit immediately
    }
    
    // Normal CPU execution...
    
    // When CPU needs to change output signals (also very fast)
    if (cpu->bus_available) {
        MOS6510_PIN_SET_BA(cpu->system_lines);
    } else {
        MOS6510_PIN_CLEAR_BA(cpu->system_lines);
    }
    
    // Signal update with automatic notification (only calls callbacks if changed)
    signal_update_lines(cpu->system_lines, cpu->system_lines->lines, cpu->signal_watchers);
}

// CPU initialization
void mos6510_init(mos6510_t* cpu, system_lines_t* system_lines) {
    cpu->system_lines = system_lines;
    
    // Set up IRQ watching
    cpu->irq_watcher.watch_mask = MOS6510_MASK_IRQ;
    cpu->irq_watcher.callback = mos6510_irq_changed;
    cpu->irq_watcher.context = cpu;
    cpu->irq_watcher.last_state = 0;
    
    // Similarly for NMI...
}
