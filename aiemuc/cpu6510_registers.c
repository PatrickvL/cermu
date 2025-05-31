#include "cpu6510.h"

// ============================================================================
// MOS 6510 REGISTER INSTRUCTIONS
// ============================================================================
// Register transfers, increments, and decrements

// Decrement Instructions (alphabetical order)
void dec_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_zp(cpu_dev);
    uint8_t result = fetched - 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, dec_zp_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, dec_zp_fetch_wait);
}

void dec_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_zpx(cpu_dev);
    uint8_t result = fetched - 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, dec_zpx_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, dec_zpx_fetch_wait);
}

void dec_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_abs(cpu_dev);
    uint8_t result = fetched - 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, dec_abs_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, dec_abs_fetch_wait);
}

void dec_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_absx(cpu_dev);
    uint8_t result = fetched - 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, dec_absx_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, dec_absx_fetch_wait);
}

void dex_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dex_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x--;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, dex_fetch_wait);
}

void dey_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, dey_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y--;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, dey_fetch_wait);
}

// Increment Instructions
void inc_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_zp(cpu_dev);
    uint8_t result = fetched + 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, inc_zp_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, inc_zp_fetch_wait);
}

void inc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_zpx(cpu_dev);
    uint8_t result = fetched + 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, inc_zpx_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, inc_zpx_fetch_wait);
}

void inc_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_abs(cpu_dev);
    uint8_t result = fetched + 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, inc_abs_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, inc_abs_fetch_wait);
}

void inc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t fetched = addr_absx(cpu_dev);
    uint8_t result = fetched + 1;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, inc_absx_write_wait);
    cpu_set_zn(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, inc_absx_fetch_wait);
}

void inx_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, inx_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x++;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, inx_fetch_wait);
}

void iny_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, iny_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y++;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, iny_fetch_wait);
}

// Transfer Instructions
void tax_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tax_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x = cpu_dev->a;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, tax_fetch_wait);
}

void tay_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tay_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->y = cpu_dev->a;
    cpu_set_zn(cpu_dev, cpu_dev->y);
    NEXT_INSTRUCTION(cpu_dev, tay_fetch_wait);
}

void tsx_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tsx_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->x = cpu_dev->sp;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, tsx_fetch_wait);
}

void txa_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, txa_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = cpu_dev->x;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, txa_fetch_wait);
}

void txs_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, txs_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->sp = cpu_dev->x;
    NEXT_INSTRUCTION(cpu_dev, txs_fetch_wait);
}

void tya_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tya_wait);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = cpu_dev->y;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, tya_fetch_wait);
}