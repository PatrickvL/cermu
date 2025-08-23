#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Remap cycle core exported symbols to avoid conflicts with legacy mos6510
#define mos6510_create mos6510_cycle_create
#define mos6510_destroy mos6510_cycle_destroy
#define mos6510_init    mos6510_cycle_init
#define mos6510_reset   mos6510_cycle_reset
#define mos6510_tick    mos6510_cycle_tick
#include "../../chip/cpu/mos6510_cycle/mos6510_state.h"
#include "../../chip/cpu/mos6510_cycle/mos6510_tick.h"
#include "../../chip/cpu/mos6510_cycle/register_access.h"
#include "../../chip/cpu/mos6510_cycle/cpu_config.h"
// Opaque wrappers used by c64_dual_cpu.c
void* cycle_cpu_create(void) {
    // Create MOS6510 cycle-accurate core with 6510 configuration
    const cpu_config_t* cfg = &CPU_CONFIG_6510;
    return mos6510_create(cfg);
}

void cycle_cpu_destroy(void* cpu) {
    if (cpu) {
    mos6510_destroy((mos6510_state_t*)cpu);
    }
}

void cycle_cpu_reset(void* cpu) {
    if (cpu) {
    mos6510_reset((mos6510_state_t*)cpu);
    }
}

bool cycle_cpu_tick(void* cpu, void* context) {
    if (!cpu || !context) return false;
    return mos6510_tick((mos6510_state_t*)cpu, (tick_context_t*)context);
}

uint16_t cycle_cpu_get_pc(void* cpu) {
    return cpu ? CPU_PC((mos6510_state_t*)cpu) : 0;
}

uint8_t cycle_cpu_get_a(void* cpu) {
    return cpu ? CPU_A((mos6510_state_t*)cpu) : 0;
}

uint8_t cycle_cpu_get_x(void* cpu) {
    return cpu ? CPU_X((mos6510_state_t*)cpu) : 0;
}

uint8_t cycle_cpu_get_y(void* cpu) {
    return cpu ? CPU_Y((mos6510_state_t*)cpu) : 0;
}

uint8_t cycle_cpu_get_sp(void* cpu) {
    return cpu ? CPU_SP((mos6510_state_t*)cpu) : 0;
}

uint8_t cycle_cpu_get_p(void* cpu) {
    return cpu ? CPU_P((mos6510_state_t*)cpu) : 0;
}
// Setters for synchronizing cycle CPU state from legacy CPU
void cycle_cpu_set_pc(void* cpu, uint16_t pc) {
    if (cpu) { SET_CPU_PC((mos6510_state_t*)cpu, pc); }
}
void cycle_cpu_set_a(void* cpu, uint8_t v) {
    if (cpu) { SET_CPU_A((mos6510_state_t*)cpu, v); }
}
void cycle_cpu_set_x(void* cpu, uint8_t v) {
    if (cpu) { SET_CPU_X((mos6510_state_t*)cpu, v); }
}
void cycle_cpu_set_y(void* cpu, uint8_t v) {
    if (cpu) { SET_CPU_Y((mos6510_state_t*)cpu, v); }
}
void cycle_cpu_set_sp(void* cpu, uint8_t v) {
    if (cpu) { SET_CPU_SP((mos6510_state_t*)cpu, v); }
}
void cycle_cpu_set_p(void* cpu, uint8_t v) {
    if (cpu) { SET_CPU_P((mos6510_state_t*)cpu, v); }
}

void* cycle_tick_context_create(void) {
    tick_context_t* ctx = (tick_context_t*)malloc(sizeof(tick_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(tick_context_t));
    return ctx;
}

void cycle_tick_context_init(void* context, bool enable_debug, bool enable_validation) {
    if (!context) return;
    mos6510_tick_context_init((tick_context_t*)context, enable_debug, enable_validation);
}

void cycle_tick_context_destroy(void* context) {
    if (context) free(context);
}
