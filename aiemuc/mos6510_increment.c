#include "mos6510.h"

// ============================================================================
// MOS 6510 INCREMENT/DECREMENT INSTRUCTIONS
// ============================================================================

// INX - Increment X Register (0xE8)
void inx_func(mos6510_t* cpu_dev) {
    mos6510_register_inc_dec(cpu_dev, &cpu_dev->x, 1);
}

// INY - Increment Y Register (0xC8)
void iny_func(mos6510_t* cpu_dev) {
    mos6510_register_inc_dec(cpu_dev, &cpu_dev->y, 1);
}

// DEX - Decrement X Register (0xCA)
void dex_func(mos6510_t* cpu_dev) {
    mos6510_register_inc_dec(cpu_dev, &cpu_dev->x, -1);
}

// DEY - Decrement Y Register (0x88)
void dey_func(mos6510_t* cpu_dev) {
    mos6510_register_inc_dec(cpu_dev, &cpu_dev->y, -1);
}

// INC - Increment Memory
void inc_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_rmw_zero_page(cpu_dev, op_inc);
}

void inc_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_rmw_zero_page_x(cpu_dev, op_inc);
}

void inc_absolute_func(mos6510_t* cpu_dev) {
    mos6510_rmw_absolute(cpu_dev, op_inc);
}

void inc_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_rmw_absolute_x(cpu_dev, op_inc);
}

// DEC - Decrement Memory
void dec_zero_page_func(mos6510_t* cpu_dev) {
    mos6510_rmw_zero_page(cpu_dev, op_dec);
}

void dec_zero_page_x_func(mos6510_t* cpu_dev) {
    mos6510_rmw_zero_page_x(cpu_dev, op_dec);
}

void dec_absolute_func(mos6510_t* cpu_dev) {
    mos6510_rmw_absolute(cpu_dev, op_dec);  
}

void dec_absolute_x_func(mos6510_t* cpu_dev) {
    mos6510_rmw_absolute_x(cpu_dev, op_dec);
}
