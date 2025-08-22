#include "c64_dual_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef C64_ENABLE_CYCLE_CPU
#define C64_ENABLE_CYCLE_CPU 0
#endif

#if C64_ENABLE_CYCLE_CPU
// Cycle CPU wrapper functions implemented in c64_dual_cpu_cycle.c
extern void* cycle_cpu_create(void);
extern void  cycle_cpu_destroy(void* cpu);
extern void  cycle_cpu_reset(void* cpu);
extern bool  cycle_cpu_tick(void* cpu, void* context);
extern uint16_t cycle_cpu_get_pc(void* cpu);
extern uint8_t  cycle_cpu_get_a(void* cpu);
extern uint8_t  cycle_cpu_get_x(void* cpu);
extern uint8_t  cycle_cpu_get_y(void* cpu);
extern uint8_t  cycle_cpu_get_sp(void* cpu);
extern uint8_t  cycle_cpu_get_p(void* cpu);
extern void* cycle_tick_context_create(void);
extern void  cycle_tick_context_init(void* context, bool enable_debug, bool enable_validation);
extern void  cycle_tick_context_destroy(void* context);
#endif

// Forward declarations for internal functions
static bus_state_t c64_dual_cpu_execute_validation(c64_dual_cpu_t* dual_cpu, bus_state_t bus_state);
static bus_state_t c64_dual_cpu_execute_benchmark(c64_dual_cpu_t* dual_cpu, bus_state_t bus_state);

// ===== DUAL CPU INITIALIZATION =====

bool c64_dual_cpu_init(c64_dual_cpu_t* dual_cpu, mos6510_t* legacy_cpu) {
    if (!dual_cpu || !legacy_cpu) {
        printf("ERROR: c64_dual_cpu_init - NULL pointer (dual_cpu=%p, legacy_cpu=%p)\n", 
               (void*)dual_cpu, (void*)legacy_cpu);
        return false;
    }
    
    // Clear structure
    memset(dual_cpu, 0, sizeof(*dual_cpu));
    
    // Set initial configuration
    dual_cpu->mode = CPU_MODE_LEGACY_ONLY;
    dual_cpu->validation_enabled = false;
    dual_cpu->performance_monitoring = false;
    dual_cpu->sync_checkpoint_interval = 1000; // Sync every 1000 operations
    
    // Use provided legacy CPU
    dual_cpu->legacy_cpu = legacy_cpu;
    
    // TODO: Create cycle CPU when implementation is ready
    dual_cpu->cycle_cpu = NULL;

#if C64_ENABLE_CYCLE_CPU
    // Allocate tick context via wrapper (opaque)
    void* tctx = cycle_tick_context_create();
    if (!tctx) {
        printf("ERROR: c64_dual_cpu_init - Failed to allocate tick context\n");
        return false;
    }
    dual_cpu->tick_context_data = tctx;
#else
    dual_cpu->tick_context_data = NULL;
#endif
    
    // Initialize performance metrics
    dual_cpu->legacy_instructions = 0;
    dual_cpu->cycle_ticks = 0;
    dual_cpu->legacy_instructions_per_sec = 0.0;
    dual_cpu->cycle_ticks_per_sec = 0.0;
    
    // Initialize validation state
    dual_cpu->validation_failures = 0;
    dual_cpu->consecutive_matches = 0;
    dual_cpu->state_mismatch_detected = false;
    
    // Initialize tick context for cycle CPU
#if C64_ENABLE_CYCLE_CPU
    cycle_tick_context_init(dual_cpu->tick_context_data, false, false);
#endif
    
    printf("Dual CPU system initialized successfully (legacy CPU mode)\n");
    return true;
}

void c64_dual_cpu_destroy(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return;
    
    // Note: We don't destroy the legacy CPU as it's managed externally by the C64 system
    dual_cpu->legacy_cpu = NULL;
    
    if (dual_cpu->cycle_cpu) {
#if C64_ENABLE_CYCLE_CPU
    cycle_cpu_destroy(dual_cpu->cycle_cpu);
#endif
    dual_cpu->cycle_cpu = NULL;
    }
    
    if (dual_cpu->tick_context_data) {
#if C64_ENABLE_CYCLE_CPU
    cycle_tick_context_destroy(dual_cpu->tick_context_data);
#endif
    dual_cpu->tick_context_data = NULL;
    }
    
    printf("INFO: Dual CPU system destroyed\n");
}

bool c64_dual_cpu_set_mode(c64_dual_cpu_t* dual_cpu, cpu_execution_mode_t new_mode) {
    if (!dual_cpu) {
        printf("ERROR: c64_dual_cpu_set_mode - NULL dual_cpu pointer\n");
        return false;
    }
    
    if (dual_cpu->mode == new_mode) {
        return true; // Already in requested mode
    }
    
    printf("INFO: Switching CPU mode from %d to %d\n", dual_cpu->mode, new_mode);
    
    // Create cycle-accurate CPU if switching to a mode that needs it
    if ((new_mode != CPU_MODE_LEGACY_ONLY) && (!dual_cpu->cycle_cpu)) {
#if !C64_ENABLE_CYCLE_CPU
        printf("ERROR: Cycle CPU not enabled in build. Reconfigure with -DC64_ENABLE_CYCLE_CPU=ON.\n");
        return false;
#else
        dual_cpu->cycle_cpu = cycle_cpu_create(); // Use wrapper function
        if (!dual_cpu->cycle_cpu) {
            printf("ERROR: Failed to create cycle-accurate CPU for mode switch\n");
            return false;
        }
        // Note: cycle_cpu_create already initializes the CPU
        if (!dual_cpu->tick_context_data) {
            dual_cpu->tick_context_data = cycle_tick_context_create();
            if (dual_cpu->tick_context_data) cycle_tick_context_init(dual_cpu->tick_context_data, false, false);
        }
#endif
    }
    
    // Update configuration
    dual_cpu->mode = new_mode;
    dual_cpu->validation_enabled = (new_mode == CPU_MODE_VALIDATION);
    dual_cpu->performance_monitoring = (new_mode == CPU_MODE_BENCHMARK);
    
    // Reset counters
    dual_cpu->legacy_instructions = 0;
    dual_cpu->cycle_ticks = 0;
    dual_cpu->validation_failures = 0;
    dual_cpu->consecutive_matches = 0;
    dual_cpu->state_mismatch_detected = false;
    dual_cpu->last_sync_point = 0;
    
    return true;
}

bool c64_dual_cpu_step(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return false;
    
    switch (dual_cpu->mode) {
        case CPU_MODE_LEGACY_ONLY:
            if (dual_cpu->legacy_cpu) {
                return mos6510_step(dual_cpu->legacy_cpu);
            }
            return false;
            
        case CPU_MODE_CYCLE_ONLY:
            // Step cycle CPU when enabled
#if C64_ENABLE_CYCLE_CPU
            if (dual_cpu->cycle_cpu) {
                bool ticked = cycle_cpu_tick(dual_cpu->cycle_cpu, dual_cpu->tick_context_data);
                if (ticked) dual_cpu->cycle_ticks++;
                return ticked;
            }
#endif
            return false;
            
        case CPU_MODE_VALIDATION:
        case CPU_MODE_BENCHMARK:
            // For validation and benchmark modes, step legacy CPU and cycle CPU (when available)
            if (dual_cpu->legacy_cpu) {
                bool result = mos6510_step(dual_cpu->legacy_cpu);
                if (result) {
                    dual_cpu->legacy_instructions++;
                }

                // Step cycle-accurate CPU when enabled
#if C64_ENABLE_CYCLE_CPU
                if (dual_cpu->cycle_cpu) {
                    if (cycle_cpu_tick(dual_cpu->cycle_cpu, dual_cpu->tick_context_data)) {
                        dual_cpu->cycle_ticks++;
                    }
                }
#endif

                // In validation mode, periodically synchronize state (compare + counters)
                if (dual_cpu->mode == CPU_MODE_VALIDATION &&
                    (dual_cpu->legacy_instructions % dual_cpu->sync_checkpoint_interval) == 0) {
                    (void)c64_dual_cpu_synchronize_state(dual_cpu);
                }

                return result;
            }
            return false;
            
        default:
            return false;
    }
}

bool c64_dual_cpu_step_instruction(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu || !dual_cpu->legacy_cpu) return false;
    
    // This function is specifically for single instruction stepping (legacy only)
    bool result = mos6510_step(dual_cpu->legacy_cpu);
    if (result) {
        dual_cpu->legacy_instructions++;
    }
    return result;
}

cpu_execution_mode_t c64_dual_cpu_get_mode(const c64_dual_cpu_t* dual_cpu) {
    return dual_cpu ? dual_cpu->mode : CPU_MODE_LEGACY_ONLY;
}

// ===== CPU EXECUTION =====

bus_state_t c64_dual_cpu_execute(c64_dual_cpu_t* dual_cpu, bus_state_t bus_state) {
    if (!dual_cpu) {
        printf("ERROR: c64_dual_cpu_execute - NULL dual_cpu pointer\n");
        return bus_state;
    }
    
    switch (dual_cpu->mode) {
        case CPU_MODE_LEGACY_ONLY:
            // Execute legacy CPU only
            if (mos6510_step(dual_cpu->legacy_cpu)) {
                dual_cpu->legacy_instructions++;
            }
            return bus_state; // Legacy CPU doesn't use bus_state directly
            
        case CPU_MODE_CYCLE_ONLY:
            // Execute cycle-accurate CPU only
            if (dual_cpu->cycle_cpu) {
#if C64_ENABLE_CYCLE_CPU
                if (cycle_cpu_tick(dual_cpu->cycle_cpu, dual_cpu->tick_context_data)) {
                    dual_cpu->cycle_ticks++;
                }
#endif
            }
            return bus_state;
            
        case CPU_MODE_VALIDATION:
            // Execute both CPUs and validate
            return c64_dual_cpu_execute_validation(dual_cpu, bus_state);
            
        case CPU_MODE_BENCHMARK:
            // Execute both CPUs for performance comparison
            return c64_dual_cpu_execute_benchmark(dual_cpu, bus_state);
            
        default:
            printf("ERROR: Unknown CPU execution mode %d\n", dual_cpu->mode);
            return bus_state;
    }
}

// ===== VALIDATION MODE EXECUTION =====

static bus_state_t c64_dual_cpu_execute_validation(c64_dual_cpu_t* dual_cpu, bus_state_t bus_state) {
    // Execute legacy CPU
    bool legacy_success = mos6510_step(dual_cpu->legacy_cpu);
    if (legacy_success) {
        dual_cpu->legacy_instructions++;
    }
    
    // Execute cycle-accurate CPU
            if (dual_cpu->cycle_cpu) {
#if C64_ENABLE_CYCLE_CPU
                if (cycle_cpu_tick(dual_cpu->cycle_cpu, dual_cpu->tick_context_data)) {
                    dual_cpu->cycle_ticks++;
                }
#endif
            }
    
    // Perform validation at checkpoints (synchronize state)
    if ((dual_cpu->legacy_instructions % dual_cpu->sync_checkpoint_interval) == 0) {
        (void)c64_dual_cpu_synchronize_state(dual_cpu);
    }
    
    return bus_state;
}

// ===== BENCHMARK MODE EXECUTION =====

static bus_state_t c64_dual_cpu_execute_benchmark(c64_dual_cpu_t* dual_cpu, bus_state_t bus_state) {
    // Execute legacy CPU
    if (mos6510_step(dual_cpu->legacy_cpu)) {
        dual_cpu->legacy_instructions++;
    }
    
    // Execute cycle-accurate CPU
    if (dual_cpu->cycle_cpu) {
#if C64_ENABLE_CYCLE_CPU
        if (cycle_cpu_tick(dual_cpu->cycle_cpu, dual_cpu->tick_context_data)) {
            dual_cpu->cycle_ticks++;
        }
#endif
    }
    
    return bus_state;
}

// ===== STATE VALIDATION =====

bool c64_dual_cpu_validate_state(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu || !dual_cpu->legacy_cpu || !dual_cpu->cycle_cpu) {
        return false;
    }
    
    // Compare architectural state between both CPUs
    bool registers_match = c64_dual_cpu_compare_registers(dual_cpu);
    bool flags_match = c64_dual_cpu_compare_flags(dual_cpu);
    
    return registers_match && flags_match;
}

bool c64_dual_cpu_compare_registers(const c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu || !dual_cpu->legacy_cpu || !dual_cpu->cycle_cpu) {
        return false;
    }
    
    // Compare PC, A, X, Y, SP registers
    uint16_t legacy_pc = dual_cpu->legacy_cpu->base.pc;
    uint8_t legacy_a = dual_cpu->legacy_cpu->base.a;
    uint8_t legacy_x = dual_cpu->legacy_cpu->base.x;
    uint8_t legacy_y = dual_cpu->legacy_cpu->base.y;
    uint8_t legacy_sp = dual_cpu->legacy_cpu->base.sp;
    
    // Get cycle CPU registers using wrapper functions
    uint16_t cycle_pc = 0; uint8_t cycle_a = 0, cycle_x = 0, cycle_y = 0, cycle_sp = 0;
#if C64_ENABLE_CYCLE_CPU
    cycle_pc = cycle_cpu_get_pc(dual_cpu->cycle_cpu);
    cycle_a = cycle_cpu_get_a(dual_cpu->cycle_cpu);
    cycle_x = cycle_cpu_get_x(dual_cpu->cycle_cpu);
    cycle_y = cycle_cpu_get_y(dual_cpu->cycle_cpu);
    cycle_sp = cycle_cpu_get_sp(dual_cpu->cycle_cpu);
#endif
    
    return (legacy_pc == cycle_pc &&
            legacy_a == cycle_a &&
            legacy_x == cycle_x &&
            legacy_y == cycle_y &&
            legacy_sp == cycle_sp);
}

bool c64_dual_cpu_compare_flags(const c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu || !dual_cpu->legacy_cpu || !dual_cpu->cycle_cpu) {
        return false;
    }
    
    // Compare processor status flags
    uint8_t legacy_flags = dual_cpu->legacy_cpu->base.p;
    uint8_t cycle_flags = 0;
#if C64_ENABLE_CYCLE_CPU
    cycle_flags = cycle_cpu_get_p(dual_cpu->cycle_cpu);
#endif
    
    return (legacy_flags == cycle_flags);
}

void c64_dual_cpu_get_metrics(const c64_dual_cpu_t* dual_cpu, dual_cpu_metrics_t* metrics) {
    if (!dual_cpu || !metrics) return;
    
    metrics->legacy_instructions = dual_cpu->legacy_instructions;
    metrics->cycle_ticks = dual_cpu->cycle_ticks;
    metrics->legacy_instructions_per_sec = dual_cpu->legacy_instructions_per_sec;
    metrics->cycle_ticks_per_sec = dual_cpu->cycle_ticks_per_sec;
    metrics->validation_failures = dual_cpu->validation_failures;
    metrics->consecutive_matches = dual_cpu->consecutive_matches;
    metrics->state_mismatch_detected = dual_cpu->state_mismatch_detected;
    metrics->current_mode = dual_cpu->mode;
}

void c64_dual_cpu_print_state_mismatch(const c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu || !dual_cpu->legacy_cpu) {
        return;
    }
    
    printf("=== CPU State Mismatch Debug ===\n");
    printf("Legacy CPU State:\n");
    printf("  PC=0x%04X A=0x%02X X=0x%02X Y=0x%02X SP=0x%02X P=0x%02X\n",
           dual_cpu->legacy_cpu->base.pc,
           dual_cpu->legacy_cpu->base.a,
           dual_cpu->legacy_cpu->base.x,
           dual_cpu->legacy_cpu->base.y,
           dual_cpu->legacy_cpu->base.sp,
           dual_cpu->legacy_cpu->base.p);
    printf("Cycle-Accurate CPU State:\n");
#if C64_ENABLE_CYCLE_CPU
    if (dual_cpu->cycle_cpu) {
        printf("  PC=0x%04X A=0x%02X X=0x%02X Y=0x%02X SP=0x%02X P=0x%02X\n",
               cycle_cpu_get_pc(dual_cpu->cycle_cpu),
               cycle_cpu_get_a(dual_cpu->cycle_cpu),
               cycle_cpu_get_x(dual_cpu->cycle_cpu),
               cycle_cpu_get_y(dual_cpu->cycle_cpu),
               cycle_cpu_get_sp(dual_cpu->cycle_cpu),
               cycle_cpu_get_p(dual_cpu->cycle_cpu));
    } else {
        printf("  [Cycle CPU not instantiated]\n");
    }
#else
    printf("  [Cycle CPU not enabled in build]\n");
#endif
    
    
#if C64_ENABLE_CYCLE_CPU
    // Per-field mismatch diagnostics
    if (dual_cpu->cycle_cpu && dual_cpu->legacy_cpu) {
        uint16_t lpc = dual_cpu->legacy_cpu->base.pc;
        uint16_t cpc = cycle_cpu_get_pc(dual_cpu->cycle_cpu);
        uint8_t  la  = dual_cpu->legacy_cpu->base.a;
        uint8_t  ca  = cycle_cpu_get_a(dual_cpu->cycle_cpu);
        uint8_t  lx  = dual_cpu->legacy_cpu->base.x;
        uint8_t  cx  = cycle_cpu_get_x(dual_cpu->cycle_cpu);
        uint8_t  ly  = dual_cpu->legacy_cpu->base.y;
        uint8_t  cy  = cycle_cpu_get_y(dual_cpu->cycle_cpu);
        uint8_t  lsp = dual_cpu->legacy_cpu->base.sp;
        uint8_t  csp = cycle_cpu_get_sp(dual_cpu->cycle_cpu);
        uint8_t  lp  = dual_cpu->legacy_cpu->base.p;
        uint8_t  cp  = cycle_cpu_get_p(dual_cpu->cycle_cpu);

        bool any = false;
        if (lpc != cpc) { printf("DIFF PC : L=0x%04X C=0x%04X\n", lpc, cpc); any = true; }
        if (la  != ca ) { printf("DIFF A  : L=0x%02X C=0x%02X\n",  la,  ca ); any = true; }
        if (lx  != cx ) { printf("DIFF X  : L=0x%02X C=0x%02X\n",  lx,  cx ); any = true; }
        if (ly  != cy ) { printf("DIFF Y  : L=0x%02X C=0x%02X\n",  ly,  cy ); any = true; }
        if (lsp != csp) { printf("DIFF SP : L=0x%02X C=0x%02X\n",  lsp, csp); any = true; }
        if (lp  != cp ) { printf("DIFF P  : L=0x%02X C=0x%02X\n",  lp,  cp ); any = true; }

        if (!any) {
            printf("No per-field architectural mismatches detected.\n");
        }
    }
#endif
    printf("Instructions executed: %llu\n", (unsigned long long)dual_cpu->legacy_instructions);
    printf("Cycles executed: %llu\n", (unsigned long long)dual_cpu->cycle_ticks);
    printf("================================\n");
}

// ===== PERFORMANCE MONITORING =====

void c64_dual_cpu_start_benchmark(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return;
    
    dual_cpu->legacy_start_cycles = clock();
    dual_cpu->cycle_start_cycles = clock();
    dual_cpu->legacy_instructions = 0;
    dual_cpu->cycle_ticks = 0;
    
    printf("INFO: Benchmark started\n");
}

void c64_dual_cpu_stop_benchmark(c64_dual_cpu_t* dual_cpu, double elapsed_seconds) {
    if (!dual_cpu) return;
    
    if (elapsed_seconds > 0.0) {
        dual_cpu->legacy_instructions_per_sec = dual_cpu->legacy_instructions / elapsed_seconds;
        dual_cpu->cycle_ticks_per_sec = dual_cpu->cycle_ticks / elapsed_seconds;
    }
    
    printf("INFO: Benchmark completed\n");
}

void c64_dual_cpu_print_benchmark_results(const c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return;
    
    printf("=== CPU Performance Benchmark Results ===\n");
    printf("Legacy CPU:\n");
    printf("  Instructions executed: %llu\n", (unsigned long long)dual_cpu->legacy_instructions);
    printf("  Instructions per second: %.2f\n", dual_cpu->legacy_instructions_per_sec);
    
    printf("Cycle-Accurate CPU:\n"); 
    printf("  Cycles executed: %llu\n", (unsigned long long)dual_cpu->cycle_ticks);
    printf("  Cycles per second: %.2f\n", dual_cpu->cycle_ticks_per_sec);
    
    if (dual_cpu->legacy_instructions_per_sec > 0) {
        double performance_ratio = dual_cpu->cycle_ticks_per_sec / dual_cpu->legacy_instructions_per_sec;
        printf("Performance ratio (cycle/legacy): %.2fx\n", performance_ratio);
    }
    
    printf("==========================================\n");
}

// ===== UTILITY FUNCTIONS =====

void* c64_dual_cpu_get_active_cpu(const c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return NULL;
    
    switch (dual_cpu->mode) {
        case CPU_MODE_LEGACY_ONLY:
        case CPU_MODE_VALIDATION:
        case CPU_MODE_BENCHMARK:
            return dual_cpu->legacy_cpu;
            
        case CPU_MODE_CYCLE_ONLY:
            return dual_cpu->cycle_cpu;
            
        default:
            return dual_cpu->legacy_cpu; // Default to legacy for safety
    }
}

bool c64_dual_cpu_is_using_cycle_cpu(const c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return false;
    // Report true whenever the cycle core is active in any mode
    return (dual_cpu->cycle_cpu != NULL) && (dual_cpu->mode != CPU_MODE_LEGACY_ONLY);
}

void c64_dual_cpu_reset(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return;
    
    if (dual_cpu->legacy_cpu) {
        mos6510_reset(dual_cpu->legacy_cpu);
    }
    
    if (dual_cpu->cycle_cpu) {
#if C64_ENABLE_CYCLE_CPU
        cycle_cpu_reset(dual_cpu->cycle_cpu);
#endif
    }
    
    // Reset counters
    dual_cpu->legacy_instructions = 0;
    dual_cpu->cycle_ticks = 0;
    dual_cpu->validation_failures = 0;
    dual_cpu->consecutive_matches = 0;
    dual_cpu->state_mismatch_detected = false;
    
    printf("INFO: Dual CPU system reset\n");
}

// Human-readable mode string helper
const char* c64_dual_cpu_mode_str(cpu_execution_mode_t mode) {
    switch (mode) {
        case CPU_MODE_LEGACY_ONLY: return "LEGACY_ONLY";
        case CPU_MODE_CYCLE_ONLY:  return "CYCLE_ONLY";
        case CPU_MODE_VALIDATION:  return "VALIDATION";
        case CPU_MODE_BENCHMARK:   return "BENCHMARK";
        default:                   return "UNKNOWN";
    }
}

// ===== STATE SYNCHRONIZATION =====
bool c64_dual_cpu_synchronize_state(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return false;

    // Both CPUs must exist to synchronize/validate
    if (!dual_cpu->legacy_cpu || !dual_cpu->cycle_cpu) {
        return false;
    }

    // For now, perform validation and update counters; later we can copy state when APIs allow.
    bool ok = c64_dual_cpu_validate_state(dual_cpu);
    if (ok) {
        dual_cpu->consecutive_matches++;
        dual_cpu->last_sync_point = dual_cpu->legacy_instructions;
        dual_cpu->state_mismatch_detected = false;
    } else {
        dual_cpu->validation_failures++;
        dual_cpu->consecutive_matches = 0;
        dual_cpu->state_mismatch_detected = true;
        printf("WARNING: c64_dual_cpu_synchronize_state: state mismatch at instruction %llu\n",
               (unsigned long long)dual_cpu->legacy_instructions);
        c64_dual_cpu_print_state_mismatch(dual_cpu);
    }
    return ok;
}

// ===== VALIDATION CONFIGURATION =====
void c64_dual_cpu_set_checkpoint_interval(c64_dual_cpu_t* dual_cpu, uint64_t interval) {
    if (!dual_cpu) return;
    if (interval == 0) interval = 1; // coerce to minimum 1
    dual_cpu->sync_checkpoint_interval = interval;
}

uint64_t c64_dual_cpu_get_checkpoint_interval(const c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return 0;
    return dual_cpu->sync_checkpoint_interval;
}
