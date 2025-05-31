#include "cpu6510.h"

// ============================================================================
// MOS 6510 REGISTER INSTRUCTIONS
// ============================================================================
// Register transfers, increments, and decrements

// Decrement Instructions (alphabetical order)
void dec_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_zp, -1);
}

void dec_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_zpx, -1);
}

void dec_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_abs, -1);
}

void dec_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_absx, -1);
}

void dex_func(cpu6510_state_t* cpu_dev) {
    cpu_register_inc_dec(cpu_dev, &cpu_dev->x, -1);
}

void dey_func(cpu6510_state_t* cpu_dev) {
    cpu_register_inc_dec(cpu_dev, &cpu_dev->y, -1);
}

// Increment Instructions
void inc_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_zp, 1);
}

void inc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_zpx, 1);
}

void inc_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_abs, 1);
}

void inc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_memory_inc_dec(cpu_dev, addr_absx, 1);
}

void inx_func(cpu6510_state_t* cpu_dev) {
    cpu_register_inc_dec(cpu_dev, &cpu_dev->x, 1);
}

void iny_func(cpu6510_state_t* cpu_dev) {
    cpu_register_inc_dec(cpu_dev, &cpu_dev->y, 1);
}

// Transfer Instructions
void tax_func(cpu6510_state_t* cpu_dev) {
    cpu_register_transfer_with_flags(cpu_dev, &cpu_dev->x, cpu_dev->a);
}

void tay_func(cpu6510_state_t* cpu_dev) {
    cpu_register_transfer_with_flags(cpu_dev, &cpu_dev->y, cpu_dev->a);
}

void tsx_func(cpu6510_state_t* cpu_dev) {
    cpu_register_transfer_with_flags(cpu_dev, &cpu_dev->x, cpu_dev->sp);
}

void txa_func(cpu6510_state_t* cpu_dev) {
    cpu_register_transfer_with_flags(cpu_dev, &cpu_dev->a, cpu_dev->x);
}

void txs_func(cpu6510_state_t* cpu_dev) {
    cpu_register_transfer_no_flags(cpu_dev, &cpu_dev->sp, cpu_dev->x);
}

void tya_func(cpu6510_state_t* cpu_dev) {
    cpu_register_transfer_with_flags(cpu_dev, &cpu_dev->a, cpu_dev->y);
}