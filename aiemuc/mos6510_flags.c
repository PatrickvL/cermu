#include "mos6510.h"

// ============================================================================
// MOS 6510 FLAG INSTRUCTIONS
// ============================================================================
// Clear and Set flag instructions (alphabetical order)

// Clear flag instructions
void clc_func(mos6510_t* cpu_dev) {
    mos6510_flag_clear_helper(cpu_dev, FLAG_C);
}

void cld_func(mos6510_t* cpu_dev) {
    mos6510_flag_clear_helper(cpu_dev, FLAG_D);
}

void cli_func(mos6510_t* cpu_dev) {
    mos6510_flag_clear_helper(cpu_dev, FLAG_I);
}

void clv_func(mos6510_t* cpu_dev) {
    mos6510_flag_clear_helper(cpu_dev, FLAG_V);
}

// Set flag instructions
void sec_func(mos6510_t* cpu_dev) {
    mos6510_flag_set_helper(cpu_dev, FLAG_C);
}

void sed_func(mos6510_t* cpu_dev) {
    mos6510_flag_set_helper(cpu_dev, FLAG_D);
}

void sei_func(mos6510_t* cpu_dev) {
    mos6510_flag_set_helper(cpu_dev, FLAG_I);
}
