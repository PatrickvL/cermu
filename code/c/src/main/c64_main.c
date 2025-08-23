#include "../systems/c64/c64.h"
#include "../systems/c64/c64_config.h"
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    // Create system configuration (PAL by default)
    c64_config_t config = {
        .vicii_standard = VIC_PAL,
        .rom_config = NULL  // Use default ROM paths
    };
    
    // Initialize the C64 system and get the instance
    c64_t* c64 = c64_system_create(&config);
    if (!c64) {
        printf("Failed to initialize C64 system!\n");
        return 1;
    }

    // Basic validation smoke test using dual-CPU framework
    printf("C64 emulator initialized. Starting validation smoke test...\n");

    // Switch to VALIDATION mode and set a small checkpoint interval
    if (!c64_set_cpu_mode(c64, CPU_MODE_VALIDATION)) {
        printf("Failed to set CPU validation mode\n");
    }
    c64_set_validation_checkpoint_interval(c64, 100);

    // Execute a fixed number of legacy steps; cycle core ticks alongside in validation mode
    const uint64_t steps = 5000;
    for (uint64_t i = 0; i < steps; i++) {
        if (!c64_cpu_step(c64)) {
            printf("CPU step failed at #%llu\n", (unsigned long long)i);
            break;
        }
    }

    // Force a final sync/compare and print metrics
    (void)c64_validate_sync(c64);
    dual_cpu_metrics_t m = {0};
    c64_get_cpu_metrics(c64, &m);
    printf("Mode=%s legacy_instr=%llu cycle_ticks=%llu failures=%u matches=%u\n",
           c64_dual_cpu_mode_str(m.current_mode),
           (unsigned long long)m.legacy_instructions,
           (unsigned long long)m.cycle_ticks,
           m.validation_failures,
           m.consecutive_matches);

    // Clean up
    c64_system_destroy(c64);
    return 0;
}