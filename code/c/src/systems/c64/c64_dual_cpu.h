#ifndef C64_DUAL_CPU_H
#define C64_DUAL_CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "../../chip/cpu/mos6510/mos6510.h"
#include "../../core/system_lines.h"

/**
 * C64 Dual CPU Architecture Support
 * 
 * Enables running both legacy instruction-based CPU and new cycle-accurate CPU
 * simultaneously for validation, comparison, and safe migration.
 * 
 * Key Features:
 * - Runtime selection between legacy and cycle-accurate CPU
 * - Parallel execution for behavioral validation
 * - State synchronization and comparison
 * - Performance benchmarking capabilities
 * - Safe fallback to legacy CPU
 */

// ===== CPU SELECTION MODES =====

typedef enum {
    CPU_MODE_LEGACY_ONLY = 0,      // Use only legacy instruction-based CPU
    CPU_MODE_CYCLE_ONLY = 1,       // Use only cycle-accurate CPU  
    CPU_MODE_VALIDATION = 2,       // Run both CPUs, compare results
    CPU_MODE_BENCHMARK = 3         // Performance comparison mode
} cpu_execution_mode_t;

// ===== PERFORMANCE METRICS =====

typedef struct {
    uint64_t legacy_instructions;       // Instructions executed by legacy CPU
    uint64_t cycle_ticks;               // Cycles executed by cycle-accurate CPU
    double legacy_instructions_per_sec; // Legacy CPU performance
    double cycle_ticks_per_sec;         // Cycle CPU performance
    uint32_t validation_failures;       // Number of validation failures
    uint32_t consecutive_matches;       // Consecutive successful validations
    bool state_mismatch_detected;       // Current state difference status
    cpu_execution_mode_t current_mode;  // Current execution mode
} dual_cpu_metrics_t;

// ===== DUAL CPU STATE STRUCTURE =====

typedef struct {
    // CPU instances
    mos6510_t* legacy_cpu;              // Legacy instruction-based CPU
    void* cycle_cpu;                     // Cycle-accurate CPU (opaque pointer)
    // Internal tick context for cycle CPU (opaque)
    void* tick_context_data;
    
    // Runtime configuration
    cpu_execution_mode_t mode;          // Current execution mode
    bool validation_enabled;            // Enable state validation
    bool performance_monitoring;        // Enable performance metrics
    
    // State synchronization
    uint64_t legacy_instructions;       // Instructions executed by legacy CPU
    uint64_t cycle_ticks;               // Cycles executed by cycle-accurate CPU
    uint64_t sync_checkpoint_interval;  // How often to sync state
    uint64_t last_sync_point;           // Last synchronization point
    
    // Validation state
    bool state_mismatch_detected;       // State difference found
    uint32_t validation_failures;       // Number of validation failures
    uint32_t consecutive_matches;       // Consecutive successful validations
    
    // Performance metrics
    uint64_t legacy_start_cycles;       // Performance measurement start
    uint64_t cycle_start_cycles;        // Performance measurement start
    double legacy_instructions_per_sec; // Legacy CPU performance
    double cycle_ticks_per_sec;         // Cycle CPU performance
    
} c64_dual_cpu_t;

// ===== DUAL CPU MANAGEMENT FUNCTIONS =====

/**
 * Initialize dual CPU system
 * @param dual_cpu Dual CPU state structure
 * @param legacy_cpu Existing legacy CPU instance
 * @return true on success, false on failure
 */
bool c64_dual_cpu_init(c64_dual_cpu_t* dual_cpu, mos6510_t* legacy_cpu);

/**
 * Destroy dual CPU system and free resources
 * @param dual_cpu Dual CPU state structure
 */
void c64_dual_cpu_destroy(c64_dual_cpu_t* dual_cpu);

/**
 * Change CPU execution mode at runtime
 * @param dual_cpu Dual CPU state structure
 * @param new_mode New execution mode
 * @return true on success, false if mode change failed
 */
bool c64_dual_cpu_set_mode(c64_dual_cpu_t* dual_cpu, cpu_execution_mode_t new_mode);

/**
 * Execute one CPU step/instruction based on current mode
 * @param dual_cpu Dual CPU state structure
 * @return true on success, false on failure
 */
bool c64_dual_cpu_step(c64_dual_cpu_t* dual_cpu);

/**
 * Execute one CPU instruction (legacy mode only)
 * @param dual_cpu Dual CPU state structure
 * @return true on success, false on failure
 */
bool c64_dual_cpu_step_instruction(c64_dual_cpu_t* dual_cpu);

/**
 * Get the current CPU execution mode
 * @param dual_cpu Dual CPU state structure
 * @return Current execution mode
 */
cpu_execution_mode_t c64_dual_cpu_get_mode(const c64_dual_cpu_t* dual_cpu);
/**
 * Get human-readable name for a CPU execution mode
 * @param mode CPU execution mode enum
 * @return Const string like "LEGACY_ONLY", "CYCLE_ONLY", "VALIDATION", "BENCHMARK"
 */
const char* c64_dual_cpu_mode_str(cpu_execution_mode_t mode);

/**
 * Execute CPU based on current mode
 * @param dual_cpu Dual CPU state structure
 * @param bus_state Current bus state (for cycle-accurate CPU)
 * @return Updated bus state
 */
bus_state_t c64_dual_cpu_execute(c64_dual_cpu_t* dual_cpu, bus_state_t bus_state);

/**
 * Synchronize state between both CPUs
 * @param dual_cpu Dual CPU state structure
 * @return true if CPUs are in sync, false if mismatch detected
 */
bool c64_dual_cpu_synchronize_state(c64_dual_cpu_t* dual_cpu);

/**
 * Validate that both CPUs have identical architectural state
 * @param dual_cpu Dual CPU state structure
 * @return true if state matches, false if mismatch detected
 */
bool c64_dual_cpu_validate_state(c64_dual_cpu_t* dual_cpu);

// ===== PERFORMANCE MONITORING =====

/**
 * Start performance measurement
 * @param dual_cpu Dual CPU state structure
 */
void c64_dual_cpu_start_benchmark(c64_dual_cpu_t* dual_cpu);

/**
 * Stop performance measurement and calculate metrics
 * @param dual_cpu Dual CPU state structure
 * @param elapsed_seconds Wall clock time elapsed
 */
void c64_dual_cpu_stop_benchmark(c64_dual_cpu_t* dual_cpu, double elapsed_seconds);

/**
 * Print performance comparison results
 * @param dual_cpu Dual CPU state structure
 */
void c64_dual_cpu_print_benchmark_results(const c64_dual_cpu_t* dual_cpu);

// ===== STATE COMPARISON UTILITIES =====

/**
 * Compare CPU registers between legacy and cycle-accurate CPUs
 * @param dual_cpu Dual CPU state structure
 * @return true if registers match, false otherwise
 */
bool c64_dual_cpu_compare_registers(const c64_dual_cpu_t* dual_cpu);

/**
 * Compare CPU flags between both CPUs
 * @param dual_cpu Dual CPU state structure  
 * @return true if flags match, false otherwise
 */
bool c64_dual_cpu_compare_flags(const c64_dual_cpu_t* dual_cpu);

/**
 * Print detailed state comparison when mismatch is detected
 * @param dual_cpu Dual CPU state structure
 */
void c64_dual_cpu_print_state_mismatch(const c64_dual_cpu_t* dual_cpu);

/**
 * Get performance metrics for both CPUs
 * @param dual_cpu Dual CPU state structure
 * @param metrics Output metrics structure
 */
void c64_dual_cpu_get_metrics(const c64_dual_cpu_t* dual_cpu, dual_cpu_metrics_t* metrics);

// ===== INTEGRATION HELPERS =====

/**
 * Get active CPU instance for external interfaces
 * @param dual_cpu Dual CPU state structure
 * @return Pointer to active CPU (legacy or cycle-accurate)
 */
void* c64_dual_cpu_get_active_cpu(const c64_dual_cpu_t* dual_cpu);

/**
 * Check if cycle-accurate CPU is being used
 * @param dual_cpu Dual CPU state structure
 * @return true if using cycle-accurate CPU, false if using legacy
 */
bool c64_dual_cpu_is_using_cycle_cpu(const c64_dual_cpu_t* dual_cpu);

/**
 * Reset both CPUs to initial state
 * @param dual_cpu Dual CPU state structure
 */
void c64_dual_cpu_reset(c64_dual_cpu_t* dual_cpu);

#endif // C64_DUAL_CPU_H
