#include "cpu6510.h"
#include "bus.h"
#include "c64.h"

// ============================================================================
// MOS 6510 UNOFFICIAL/ILLEGAL INSTRUCTIONS
// ============================================================================

// JAM - Freeze the CPU (0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x92, 0xB2, 0xD2, 0xF2)
void jam_func(cpu6510_state_t* cpu_dev) {
    // CPU freezes - infinite loop until reset
    while (1) {
        // Wait for reset or NMI
        if (bus.control_lines & NMI_LINE) {
            cpu6510_nmi(cpu_dev);
            break;
        }
    }
}

// SLO - Shift Left and OR (ASL + ORA)
void slo_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_zp_wait2);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_zp_wait3); // Dummy write
    // ASL operation
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp <<= 1;
    // ORA operation
    cpu_dev->a |= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, slo_zp_fetch_wait);
}

void slo_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, slo_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_ind_x_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_ind_x_wait6); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp <<= 1;
    cpu_dev->a |= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_ind_x_wait7);
    NEXT_INSTRUCTION(cpu_dev, slo_ind_x_fetch_wait);
}

void slo_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, slo_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, slo_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, slo_ind_y_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, slo_ind_y_wait6); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp <<= 1;
    cpu_dev->a |= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, slo_ind_y_wait7);
    NEXT_INSTRUCTION(cpu_dev, slo_ind_y_fetch_wait);
}

void slo_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_abs_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_abs_wait4); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp <<= 1;
    cpu_dev->a |= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, slo_abs_fetch_wait);
}

void slo_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, slo_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, slo_abs_y_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, slo_abs_y_wait5); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp <<= 1;
    cpu_dev->a |= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, slo_abs_y_wait6);
    NEXT_INSTRUCTION(cpu_dev, slo_abs_y_fetch_wait);
}

void slo_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, slo_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, slo_abs_x_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, slo_abs_x_wait5); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp <<= 1;
    cpu_dev->a |= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, slo_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, slo_abs_x_fetch_wait);
}

void slo_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, slo_zp_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, slo_zp_x_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_zp_x_wait4); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp <<= 1;
    cpu_dev->a |= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, slo_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, slo_zp_x_fetch_wait);
}

// RLA - Rotate Left and AND (ROL + AND)
void rla_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_zp_wait2);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_zp_wait3); // Dummy write
    // ROL operation
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp = (cpu_dev->temp << 1) | (old_carry ? 1 : 0);
    // AND operation
    cpu_dev->a &= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, rla_zp_fetch_wait);
}

void rla_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, rla_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_ind_x_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_ind_x_wait6); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp = (cpu_dev->temp << 1) | (old_carry ? 1 : 0);
    cpu_dev->a &= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_ind_x_wait7);
    NEXT_INSTRUCTION(cpu_dev, rla_ind_x_fetch_wait);
}

void rla_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, rla_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rla_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rla_ind_y_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rla_ind_y_wait6); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp = (cpu_dev->temp << 1) | (old_carry ? 1 : 0);
    cpu_dev->a &= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rla_ind_y_wait7);
    NEXT_INSTRUCTION(cpu_dev, rla_ind_y_fetch_wait);
}

void rla_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_abs_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_abs_wait4); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp = (cpu_dev->temp << 1) | (old_carry ? 1 : 0);
    cpu_dev->a &= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, rla_abs_fetch_wait);
}

void rla_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rla_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rla_abs_y_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rla_abs_y_wait5); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp = (cpu_dev->temp << 1) | (old_carry ? 1 : 0);
    cpu_dev->a &= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rla_abs_y_wait6);
    NEXT_INSTRUCTION(cpu_dev, rla_abs_y_fetch_wait);
}

void rla_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, rla_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, rla_abs_x_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, rla_abs_x_wait5); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp = (cpu_dev->temp << 1) | (old_carry ? 1 : 0);
    cpu_dev->a &= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, rla_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, rla_abs_x_fetch_wait);
}

void rla_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rla_zp_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rla_zp_x_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_zp_x_wait4); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x80);
    cpu_dev->temp = (cpu_dev->temp << 1) | (old_carry ? 1 : 0);
    cpu_dev->a &= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rla_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, rla_zp_x_fetch_wait);
}

// SRE - Shift Right and EOR (LSR + EOR)
void sre_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_zp_wait2);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_zp_wait3); // Dummy write
    // LSR operation
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp >>= 1;
    // EOR operation
    cpu_dev->a ^= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, sre_zp_fetch_wait);
}

void sre_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, sre_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_ind_x_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_ind_x_wait6); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp >>= 1;
    cpu_dev->a ^= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_ind_x_wait7);
    NEXT_INSTRUCTION(cpu_dev, sre_ind_x_fetch_wait);
}

void sre_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, sre_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, sre_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, sre_ind_y_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, sre_ind_y_wait6); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp >>= 1;
    cpu_dev->a ^= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, sre_ind_y_wait7);
    NEXT_INSTRUCTION(cpu_dev, sre_ind_y_fetch_wait);
}

void sre_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_abs_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_abs_wait4); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp >>= 1;
    cpu_dev->a ^= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, sre_abs_fetch_wait);
}

void sre_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, sre_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, sre_abs_y_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, sre_abs_y_wait5); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp >>= 1;
    cpu_dev->a ^= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, sre_abs_y_wait6);
    NEXT_INSTRUCTION(cpu_dev, sre_abs_y_fetch_wait);
}

void sre_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, sre_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, sre_abs_x_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, sre_abs_x_wait5); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp >>= 1;
    cpu_dev->a ^= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, sre_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, sre_abs_x_fetch_wait);
}

void sre_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sre_zp_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sre_zp_x_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_zp_x_wait4); // Dummy write
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp >>= 1;
    cpu_dev->a ^= cpu_dev->temp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, sre_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, sre_zp_x_fetch_wait);
}

// RRA - Rotate Right and ADC (ROR + ADC)
void rra_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_zp_wait2);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_zp_wait3); // Dummy write
    // ROR operation
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp = (cpu_dev->temp >> 1) | (old_carry ? 0x80 : 0);
    // ADC operation
    op_adc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, rra_zp_fetch_wait);
}

void rra_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, rra_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_ind_x_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_ind_x_wait6); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp = (cpu_dev->temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_ind_x_wait7);
    NEXT_INSTRUCTION(cpu_dev, rra_ind_x_fetch_wait);
}

void rra_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, rra_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rra_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rra_ind_y_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rra_ind_y_wait6); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp = (cpu_dev->temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rra_ind_y_wait7);
    NEXT_INSTRUCTION(cpu_dev, rra_ind_y_fetch_wait);
}

void rra_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_abs_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_abs_wait4); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp = (cpu_dev->temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, rra_abs_fetch_wait);
}

void rra_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rra_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, rra_abs_y_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rra_abs_y_wait5); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp = (cpu_dev->temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, rra_abs_y_wait6);
    NEXT_INSTRUCTION(cpu_dev, rra_abs_y_fetch_wait);
}

void rra_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, rra_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, rra_abs_x_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, rra_abs_x_wait5); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp = (cpu_dev->temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, rra_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, rra_abs_x_fetch_wait);
}

void rra_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, rra_zp_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, rra_zp_x_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_zp_x_wait4); // Dummy write
    bool old_carry = cpu_get_flag(cpu_dev, FLAG_C);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->temp & 0x01);
    cpu_dev->temp = (cpu_dev->temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, rra_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, rra_zp_x_fetch_wait);
}

// SAX - Store A AND X
void sax_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sax_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a & cpu_dev->x, sax_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sax_zp_fetch_wait);
}

void sax_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sax_zp_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sax_zp_y_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a & cpu_dev->x, sax_zp_y_wait3);
    NEXT_INSTRUCTION(cpu_dev, sax_zp_y_fetch_wait);
}

void sax_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sax_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sax_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a & cpu_dev->x, sax_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, sax_abs_fetch_wait);
}

void sax_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, sax_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sax_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, sax_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, sax_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->a & cpu_dev->x, sax_ind_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, sax_ind_x_fetch_wait);
}

// LAX - Load A and X
void lax_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_zp_wait2);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_zp_fetch_wait);
}

void lax_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_zp_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_zp_y_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_zp_y_wait3);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_zp_y_fetch_wait);
}

void lax_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_abs_wait3);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_abs_fetch_wait);
}

void lax_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, lax_abs_y_wait3);
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, lax_abs_y_wait4);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_abs_y_fetch_wait);
}

void lax_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, lax_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_ind_x_wait5);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_ind_x_fetch_wait);
}

void lax_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = ((cpu_dev->hi << 8) | cpu_dev->lo) + cpu_dev->x;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_abs_x_wait3);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_abs_x_fetch_wait);
}

void lax_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_zp_x_wait1);
    cpu_dev->addr_abs = (bus.data + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_zp_x_wait2);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_zp_x_fetch_wait);
}

void lax_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, lax_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, lax_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, lax_ind_y_wait4);
    }
    
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, lax_ind_y_wait5);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_ind_y_fetch_wait);
}

void lax_immediate_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, lax_imm_wait);
    cpu_dev->a = cpu_dev->x = bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, lax_imm_fetch_wait);
}

// DCP - Decrement and Compare (DEC + CMP)
void dcp_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_zp_wait2);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_zp_wait3); // Dummy write
    cpu_dev->temp--;
    // Compare with A
    uint16_t result = cpu_dev->a - cpu_dev->temp;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_set_zn(cpu_dev, result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, dcp_zp_fetch_wait);
}

void dcp_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, dcp_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_ind_x_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_ind_x_wait6); // Dummy write
    cpu_dev->temp--;
    uint16_t result = cpu_dev->a - cpu_dev->temp;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_set_zn(cpu_dev, result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_ind_x_wait7);
    NEXT_INSTRUCTION(cpu_dev, dcp_ind_x_fetch_wait);
}

void dcp_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, dcp_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, dcp_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, dcp_ind_y_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, dcp_ind_y_wait6); // Dummy write
    cpu_dev->temp--;
    uint16_t result = cpu_dev->a - cpu_dev->temp;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_set_zn(cpu_dev, result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, dcp_ind_y_wait7);
    NEXT_INSTRUCTION(cpu_dev, dcp_ind_y_fetch_wait);
}

void dcp_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_abs_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_abs_wait4); // Dummy write
    cpu_dev->temp--;
    uint16_t result = cpu_dev->a - cpu_dev->temp;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_set_zn(cpu_dev, result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, dcp_abs_fetch_wait);
}

void dcp_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, dcp_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, dcp_abs_y_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, dcp_abs_y_wait5); // Dummy write
    cpu_dev->temp--;
    uint16_t result = cpu_dev->a - cpu_dev->temp;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_set_zn(cpu_dev, result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, dcp_abs_y_wait6);
    NEXT_INSTRUCTION(cpu_dev, dcp_abs_y_fetch_wait);
}

void dcp_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, dcp_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, dcp_abs_x_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, dcp_abs_x_wait5); // Dummy write
    cpu_dev->temp--;
    uint16_t result = cpu_dev->a - cpu_dev->temp;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_set_zn(cpu_dev, result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, dcp_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, dcp_abs_x_fetch_wait);
}

void dcp_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, dcp_zp_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, dcp_zp_x_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_zp_x_wait4); // Dummy write
    cpu_dev->temp--;
    uint16_t result = cpu_dev->a - cpu_dev->temp;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_set_zn(cpu_dev, result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, dcp_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, dcp_zp_x_fetch_wait);
}

// ISC/ISB - Increment and Subtract with Carry (INC + SBC)
void isc_zero_page_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_zp_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_zp_wait2);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_zp_wait3); // Dummy write
    cpu_dev->temp++;
    // SBC operation
    op_sbc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_zp_wait4);
    NEXT_INSTRUCTION(cpu_dev, isc_zp_fetch_wait);
}

void isc_indirect_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_ind_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_ind_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_ind_x_wait3);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, isc_ind_x_wait4);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_ind_x_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_ind_x_wait6); // Dummy write
    cpu_dev->temp++;
    op_sbc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_ind_x_wait7);
    NEXT_INSTRUCTION(cpu_dev, isc_ind_x_fetch_wait);
}

void isc_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, isc_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, isc_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, isc_ind_y_wait5);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, isc_ind_y_wait6); // Dummy write
    cpu_dev->temp++;
    op_sbc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, isc_ind_y_wait7);
    NEXT_INSTRUCTION(cpu_dev, isc_ind_y_fetch_wait);
}

void isc_absolute_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_abs_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_abs_wait2);
    cpu_dev->addr_abs |= (bus.data << 8);
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_abs_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_abs_wait4); // Dummy write
    cpu_dev->temp++;
    op_sbc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_abs_wait5);
    NEXT_INSTRUCTION(cpu_dev, isc_abs_fetch_wait);
}

void isc_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, isc_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, isc_abs_y_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, isc_abs_y_wait5); // Dummy write
    cpu_dev->temp++;
    op_sbc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->y, cpu_dev->temp, isc_abs_y_wait6);
    NEXT_INSTRUCTION(cpu_dev, isc_abs_y_fetch_wait);
}

void isc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, isc_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, isc_abs_x_wait4);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, isc_abs_x_wait5); // Dummy write
    cpu_dev->temp++;
    op_sbc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs + cpu_dev->x, cpu_dev->temp, isc_abs_x_wait6);
    NEXT_INSTRUCTION(cpu_dev, isc_abs_x_fetch_wait);
}

void isc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, isc_zp_x_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_zp_x_wait2); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, isc_zp_x_wait3);
    cpu_dev->temp = bus.data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_zp_x_wait4); // Dummy write
    cpu_dev->temp++;
    op_sbc(cpu_dev, cpu_dev->temp);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->temp, isc_zp_x_wait5);
    NEXT_INSTRUCTION(cpu_dev, isc_zp_x_fetch_wait);
}

// NOP variants
void nop_func(cpu6510_state_t* cpu_dev) { NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait); }
void nop_zero_page_func(cpu6510_state_t* cpu_dev) { NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait); }
void nop_zero_page_x_func(cpu6510_state_t* cpu_dev) { NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait); }
void nop_absolute_func(cpu6510_state_t* cpu_dev) { NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait); }
void nop_absolute_x_func(cpu6510_state_t* cpu_dev) { NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait); }
void nop_immediate_func(cpu6510_state_t* cpu_dev) { NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait); }

// Single-byte immediate illegal opcodes
void anc_immediate_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, anc_imm_wait);
    cpu_dev->a &= bus.data;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x80);
    NEXT_INSTRUCTION(cpu_dev, anc_imm_fetch_wait);
}

void alr_immediate_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, alr_imm_wait);
    cpu_dev->a &= bus.data;
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x01);
    cpu_dev->a >>= 1;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, alr_imm_fetch_wait);
}

void arr_immediate_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, arr_imm_wait);
    cpu_dev->a &= bus.data;
    cpu_dev->a = (cpu_dev->a >> 1) | (cpu_get_flag(cpu_dev, FLAG_C) ? 0x80 : 0);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x40);
    cpu_set_flag(cpu_dev, FLAG_V, ((cpu_dev->a >> 5) ^ (cpu_dev->a >> 6)) & 1);
    NEXT_INSTRUCTION(cpu_dev, arr_imm_fetch_wait);
}

void xaa_immediate_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, xaa_imm_wait);
    cpu_dev->a = (cpu_dev->a | 0xEE) & cpu_dev->x & bus.data; // Unstable, but this is a common guess
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, xaa_imm_fetch_wait);
}

void axs_immediate_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, axs_imm_wait);
    uint8_t val = cpu_dev->a & cpu_dev->x;
    uint16_t result = val - bus.data;
    cpu_dev->x = result & 0xFF;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    NEXT_INSTRUCTION(cpu_dev, axs_imm_fetch_wait);
}

// Unstable/undocumented opcodes
void ahx_indirect_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ahx_ind_y_wait1);
    cpu_dev->addr_abs = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, ahx_ind_y_wait2);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, (cpu_dev->addr_abs + 1) & 0xFF, ahx_ind_y_wait3);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = ((cpu_dev->hi << 8) | cpu_dev->lo) + cpu_dev->y;
    uint8_t val = cpu_dev->a & cpu_dev->x & ((cpu_dev->addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, val, ahx_ind_y_wait4);
    NEXT_INSTRUCTION(cpu_dev, ahx_ind_y_fetch_wait);
}

void tas_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, tas_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, tas_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = ((cpu_dev->hi << 8) | cpu_dev->lo) + cpu_dev->y;
    cpu_dev->sp = cpu_dev->a & cpu_dev->x;
    uint8_t val = cpu_dev->sp & ((cpu_dev->addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, val, tas_abs_y_wait3);
    NEXT_INSTRUCTION(cpu_dev, tas_abs_y_fetch_wait);
}

void shy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, shy_abs_x_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, shy_abs_x_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = ((cpu_dev->hi << 8) | cpu_dev->lo) + cpu_dev->x;
    uint8_t val = cpu_dev->y & ((cpu_dev->addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, val, shy_abs_x_wait3);
    NEXT_INSTRUCTION(cpu_dev, shy_abs_x_fetch_wait);
}

void shx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, shx_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, shx_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = ((cpu_dev->hi << 8) | cpu_dev->lo) + cpu_dev->y;
    uint8_t val = cpu_dev->x & ((cpu_dev->addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, val, shx_abs_y_wait3);
    NEXT_INSTRUCTION(cpu_dev, shx_abs_y_fetch_wait);
}

void ahx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ahx_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, ahx_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = ((cpu_dev->hi << 8) | cpu_dev->lo) + cpu_dev->y;
    uint8_t val = cpu_dev->a & cpu_dev->x & ((cpu_dev->addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, val, ahx_abs_y_wait3);
    NEXT_INSTRUCTION(cpu_dev, ahx_abs_y_fetch_wait);
}

void las_absolute_y_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, las_abs_y_wait1);
    cpu_dev->lo = bus.data;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, las_abs_y_wait2);
    cpu_dev->hi = bus.data;
    cpu_dev->addr_abs = ((cpu_dev->hi << 8) | cpu_dev->lo) + cpu_dev->y;
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->addr_abs, las_abs_y_wait3);
    cpu_dev->a = cpu_dev->x = cpu_dev->sp = bus.data & cpu_dev->sp;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, las_abs_y_fetch_wait);
}