/**
 * Example showing the complete non-global signal architecture.
 * This demonstrates how a C64 system would be structured with
 * explicit passing of system state to all components.
 */

#include "../../../core/system_lines.h"
#include "../../../core/signal_notification.h"
#include "../../../chip/cpu/mos6510/mos6510_pins.h"
#include "../../../chip/video/mos6567/mos6567_pins.h"

// Forward declarations
typedef struct c64_system_s c64_system_t;
typedef struct mos6510_s mos6510_t;
typedef struct vic_chip_s vic_chip_t;

/**
 * Complete C64 system structure - owns all state, no globals.
 */
struct c64_system_s {
    // System-owned signal state
    system_lines_t system_lines;
    
    // Signal change notification system
    signal_watcher_t* signal_watchers;  // Linked list of all watchers
    
    // Components (each gets access to what it needs)
    mos6510_t* cpu;
    vic_chip_t* vic;
    
    // System memory and other devices...
    uint8_t ram[65536];
    uint8_t rom_basic[8192];
    uint8_t rom_kernal[8192];
};

/**
 * CPU structure - gets system_lines_t pointer, no globals.
 */
struct mos6510_s {
    // CPU registers and state...
    uint8_t a, x, y, sp, status;
    uint16_t pc;
    
    // System interface (passed from system)
    system_lines_t* system_lines;       // Pointer to system-owned line state
    
    // Signal change watchers
    signal_watcher_t irq_watcher;
    signal_watcher_t nmi_watcher;
    
    // CPU-specific state
    bool interrupt_pending;
    bool rdy_stalled;
};

/**
 * VIC-II structure - also gets system_lines_t pointer.
 */
struct vic_chip_s {
    // VIC registers and state...
    uint8_t registers[64];
    
    // System interface (passed from system)
    system_lines_t* system_lines;
    
    // VIC-specific watchers
    signal_watcher_t ba_watcher;        // Watches CPU's BA output
    signal_watcher_t aec_watcher;       // Watches CPU's AEC output
    
    // VIC state
    bool generating_irq;
    bool has_bus_access;
};

// ============================================================================
// USAGE EXAMPLES - ALL NON-GLOBAL
// ============================================================================

/**
 * CPU signal change handler - called when IRQ line changes.
 */
static void cpu_irq_changed(signal_watcher_t* watcher, 
                           uint32_t old_lines, 
                           uint32_t new_lines, 
                           uint32_t changed_mask) {
    mos6510_t* cpu = (mos6510_t*)watcher->context;
    
    // Ultra-fast signal checking using chip-specific macros
    if (MOS6510_PIN_TEST_IRQ(cpu->system_lines)) {
        cpu->interrupt_pending = true;
    }
}

/**
 * VIC signal change handler - called when BA/AEC lines change.
 */
static void vic_bus_control_changed(signal_watcher_t* watcher,
                                  uint32_t old_lines, 
                                  uint32_t new_lines, 
                                  uint32_t changed_mask) {
    vic_chip_t* vic = (vic_chip_t*)watcher->context;
    
    // VIC responds to CPU bus control signals
    if (VIC_PIN_TEST_BA(vic->system_lines) && VIC_PIN_TEST_AEC(vic->system_lines)) {
        vic->has_bus_access = true;
        // VIC can now access memory...
    } else {
        vic->has_bus_access = false;
    }
}

/**
 * CPU execution cycle - ultra-lightweight signal operations.
 */
static inline void cpu_execute_cycle(mos6510_t* cpu) {
    // Ultra-fast RDY check (compiles to single bit test)
    if (!MOS6510_PIN_TEST_RDY(cpu->system_lines)) {
        cpu->rdy_stalled = true;
        return; // Exit immediately if RDY low
    }
    
    cpu->rdy_stalled = false;
    
    // Normal CPU instruction execution...
    // fetch, decode, execute...
    
    // When CPU needs to signal bus availability (very fast)
    if (cpu->rdy_stalled) {
        MOS6510_PIN_SET_BA(cpu->system_lines);     // Assert BA
        MOS6510_PIN_SET_AEC(cpu->system_lines);    // Assert AEC
    } else {
        MOS6510_PIN_CLEAR_BA(cpu->system_lines);   // Clear BA
        MOS6510_PIN_CLEAR_AEC(cpu->system_lines);  // Clear AEC
    }
    
    // The above operations directly modify system_lines->lines
    // No function calls, just bit operations
}

/**
 * VIC execution cycle - also ultra-lightweight.
 */
static inline void vic_execute_cycle(vic_chip_t* vic) {
    // VIC checks bus availability (single bit test)
    if (vic->has_bus_access) {
        // VIC can access memory, generate graphics...
    }
    
    // VIC might need to assert IRQ
    if (vic->generating_irq) {
        VIC_PIN_SET_IRQ(vic->system_lines);        // Very fast IRQ assertion
    } else {
        VIC_PIN_CLEAR_IRQ(vic->system_lines);      // Very fast IRQ clearing
    }
}

/**
 * System initialization - sets up all components with explicit state passing.
 */
void c64_system_init(c64_system_t* system) {
    // Initialize system-owned signal state
    system_lines_init(&system->system_lines);
    system->signal_watchers = NULL;
    
    // Initialize CPU with system interfaces
    system->cpu->system_lines = &system->system_lines;
    
    // Set up CPU's IRQ watcher
    system->cpu->irq_watcher.watch_mask = MOS6510_MASK_IRQ;
    system->cpu->irq_watcher.callback = cpu_irq_changed;
    system->cpu->irq_watcher.context = system->cpu;
    system->cpu->irq_watcher.next = system->signal_watchers;
    system->signal_watchers = &system->cpu->irq_watcher;
    
    // Initialize VIC with system interfaces
    system->vic->system_lines = &system->system_lines;
    
    // Set up VIC's bus control watcher
    system->vic->ba_watcher.watch_mask = VIC_MASK_BA | VIC_MASK_AEC;
    system->vic->ba_watcher.callback = vic_bus_control_changed;
    system->vic->ba_watcher.context = system->vic;
    system->vic->ba_watcher.next = system->signal_watchers;
    system->signal_watchers = &system->vic->ba_watcher;
}

/**
 * System execution cycle - coordinates all components.
 */
void c64_system_cycle(c64_system_t* system) {
    // Save current state for change detection
    uint32_t old_lines = system->system_lines.lines;
    
    // Execute all components (they modify system_lines directly)
    cpu_execute_cycle(system->cpu);
    vic_execute_cycle(system->vic);
    
    // Notify watchers only if signals actually changed
    // This is very fast if nothing changed (common case)
    signal_update_lines(&system->system_lines, 
                       system->system_lines.lines, 
                       system->signal_watchers);
}

// ============================================================================
// PERFORMANCE CHARACTERISTICS
// ============================================================================

/*
 * ULTRA-FAST OPERATIONS (compile to single instructions):
 * - MOS6510_PIN_TEST_RDY(cpu->system_lines)
 * - MOS6510_PIN_SET_BA(cpu->system_lines) 
 * - VIC_PIN_TEST_BA(vic->system_lines)
 * - All other pin test/set/clear operations
 *
 * FAST OPERATIONS (few instructions):
 * - signal_update_lines() when no changes occurred
 * - Direct bit manipulation of system_lines->lines
 *
 * ONLY WHEN NEEDED (infrequent):
 * - Signal change callbacks (only when signals actually change)
 * - Linked list traversal for notification (only when signals change)
 *
 * NO GLOBALS:
 * - All state owned by system structure
 * - Explicit passing of interfaces to components
 * - No hidden dependencies or global variables
 */
