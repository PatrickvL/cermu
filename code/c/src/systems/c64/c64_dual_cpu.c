#include "c64_dual_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    mos6510_tick_context_init(&dual_cpu->tick_context, false, false);
    
    printf("Dual CPU system initialized successfully (legacy CPU mode)\n");
    return true;
}

void c64_dual_cpu_destroy(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return;
    
    // Note: We don't destroy the legacy CPU as it's managed externally by the C64 system
    dual_cpu->legacy_cpu = NULL;
    
    if (dual_cpu->cycle_cpu) {
        mos6510_destroy(dual_cpu->cycle_cpu);
        dual_cpu->cycle_cpu = NULL;
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
        dual_cpu->cycle_cpu = mos6510_create(NULL); // Use default 6510 config
        if (!dual_cpu->cycle_cpu) {
            printf("ERROR: Failed to create cycle-accurate CPU for mode switch\n");
            return false;
        }
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
            // TODO: Implement cycle CPU stepping when API is available
            printf("Cycle-only CPU mode not yet implemented\n");
            return false;
            
        case CPU_MODE_VALIDATION:
        case CPU_MODE_BENCHMARK:
            // For validation and benchmark modes, step legacy CPU and compare
            if (dual_cpu->legacy_cpu) {
                bool result = mos6510_step(dual_cpu->legacy_cpu);
                dual_cpu->legacy_instructions++;
                
                // TODO: Also step cycle CPU and validate when available
                
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
                if (mos6510_tick(dual_cpu->cycle_cpu, &dual_cpu->tick_context)) {
                    dual_cpu->cycle_ticks++;
                }
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
        if (mos6510_tick(dual_cpu->cycle_cpu, &dual_cpu->tick_context)) {
            dual_cpu->cycle_ticks++;
        }
    }
    
    // Perform validation at checkpoints
    if ((dual_cpu->legacy_instructions % dual_cpu->sync_checkpoint_interval) == 0) {
        if (c64_dual_cpu_validate_state(dual_cpu)) {
            dual_cpu->consecutive_matches++;
        } else {
            dual_cpu->validation_failures++;
            dual_cpu->consecutive_matches = 0;
            dual_cpu->state_mismatch_detected = true;
            
            printf("WARNING: CPU state validation failed at instruction %llu\n", 
                   (unsigned long long)dual_cpu->legacy_instructions);
            c64_dual_cpu_print_state_mismatch(dual_cpu);
        }
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
        if (mos6510_tick(dual_cpu->cycle_cpu, &dual_cpu->tick_context)) {
            dual_cpu->cycle_ticks++;
        }
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
    
    // Get cycle CPU registers
    uint16_t cycle_pc = mos6510_get_pc(dual_cpu->cycle_cpu);
    uint8_t cycle_a = mos6510_get_a(dual_cpu->cycle_cpu);
    uint8_t cycle_x = mos6510_get_x(dual_cpu->cycle_cpu);
    uint8_t cycle_y = mos6510_get_y(dual_cpu->cycle_cpu);
    uint8_t cycle_sp = mos6510_get_sp(dual_cpu->cycle_cpu);
    
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
    uint8_t cycle_flags = mos6510_get_p(dual_cpu->cycle_cpu);
    
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
    printf("  [State access API not yet implemented]\n");
    
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
    return (dual_cpu->mode == CPU_MODE_CYCLE_ONLY);
}

void c64_dual_cpu_reset(c64_dual_cpu_t* dual_cpu) {
    if (!dual_cpu) return;
    
    if (dual_cpu->legacy_cpu) {
        mos6510_reset(dual_cpu->legacy_cpu);
    }
    
    if (dual_cpu->cycle_cpu) {
        mos6510_reset(dual_cpu->cycle_cpu);
    }
    
    // Reset counters
    dual_cpu->legacy_instructions = 0;
    dual_cpu->cycle_ticks = 0;
    dual_cpu->validation_failures = 0;
    dual_cpu->consecutive_matches = 0;
    dual_cpu->state_mismatch_detected = false;
    
    printf("INFO: Dual CPU system reset\n");
}
