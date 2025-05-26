#include "cpu6510.h"
#include "bus.h"
#include "c64.h"

// ============================================================================
// MOS 6510 UNOFFICIAL/ILLEGAL INSTRUCTIONS
// ============================================================================

// JAM - Freeze the CPU (0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x92, 0xB2, 0xD2, 0xF2)
void jam_func(void) {
    // CPU freezes - infinite loop until reset
    while (1) {
        // Wait for reset or NMI
        if (bus_state.control_lines & NMI_LINE) {
            cpu6510_nmi();
            break;
        }
    }
}

// SLO - Shift Left and OR (ASL + ORA)
void slo_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, slo_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_zp_wait2);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_zp_wait3); // Dummy write
    // ASL operation
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp <<= 1;
    // ORA operation
    cpu.a |= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_zp_wait4);
    NEXT_INSTRUCTION(slo_zp_fetch_wait);
}

void slo_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, slo_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, slo_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_ind_x_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_ind_x_wait6); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp <<= 1;
    cpu.a |= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_ind_x_wait7);
    NEXT_INSTRUCTION(slo_ind_x_fetch_wait);
}

void slo_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, slo_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, slo_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, slo_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, slo_ind_y_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, slo_ind_y_wait6); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp <<= 1;
    cpu.a |= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, slo_ind_y_wait7);
    NEXT_INSTRUCTION(slo_ind_y_fetch_wait);
}

void slo_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, slo_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, slo_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_abs_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_abs_wait4); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp <<= 1;
    cpu.a |= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_abs_wait5);
    NEXT_INSTRUCTION(slo_abs_fetch_wait);
}

void slo_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, slo_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, slo_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, slo_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, slo_abs_y_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, slo_abs_y_wait5); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp <<= 1;
    cpu.a |= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, slo_abs_y_wait6);
    NEXT_INSTRUCTION(slo_abs_y_fetch_wait);
}

void slo_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, slo_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, slo_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, slo_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, slo_abs_x_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, slo_abs_x_wait5); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp <<= 1;
    cpu.a |= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, slo_abs_x_wait6);
    NEXT_INSTRUCTION(slo_abs_x_fetch_wait);
}

void slo_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, slo_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, slo_zp_x_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_zp_x_wait4); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp <<= 1;
    cpu.a |= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, slo_zp_x_wait5);
    NEXT_INSTRUCTION(slo_zp_x_fetch_wait);
}

// RLA - Rotate Left and AND (ROL + AND)
void rla_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rla_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_zp_wait2);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_zp_wait3); // Dummy write
    // ROL operation
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp = (cpu.temp << 1) | (old_carry ? 1 : 0);
    // AND operation
    cpu.a &= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_zp_wait4);
    NEXT_INSTRUCTION(rla_zp_fetch_wait);
}

void rla_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rla_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, rla_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_ind_x_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_ind_x_wait6); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp = (cpu.temp << 1) | (old_carry ? 1 : 0);
    cpu.a &= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_ind_x_wait7);
    NEXT_INSTRUCTION(rla_ind_x_fetch_wait);
}

void rla_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rla_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, rla_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rla_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rla_ind_y_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rla_ind_y_wait6); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp = (cpu.temp << 1) | (old_carry ? 1 : 0);
    cpu.a &= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rla_ind_y_wait7);
    NEXT_INSTRUCTION(rla_ind_y_fetch_wait);
}

void rla_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rla_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, rla_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_abs_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_abs_wait4); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp = (cpu.temp << 1) | (old_carry ? 1 : 0);
    cpu.a &= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_abs_wait5);
    NEXT_INSTRUCTION(rla_abs_fetch_wait);
}

void rla_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rla_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, rla_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rla_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rla_abs_y_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rla_abs_y_wait5); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp = (cpu.temp << 1) | (old_carry ? 1 : 0);
    cpu.a &= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rla_abs_y_wait6);
    NEXT_INSTRUCTION(rla_abs_y_fetch_wait);
}

void rla_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rla_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, rla_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, rla_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, rla_abs_x_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, rla_abs_x_wait5); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp = (cpu.temp << 1) | (old_carry ? 1 : 0);
    cpu.a &= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, rla_abs_x_wait6);
    NEXT_INSTRUCTION(rla_abs_x_fetch_wait);
}

void rla_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rla_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, rla_zp_x_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_zp_x_wait4); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x80);
    cpu.temp = (cpu.temp << 1) | (old_carry ? 1 : 0);
    cpu.a &= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rla_zp_x_wait5);
    NEXT_INSTRUCTION(rla_zp_x_fetch_wait);
}

// SRE - Shift Right and EOR (LSR + EOR)
void sre_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sre_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_zp_wait2);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_zp_wait3); // Dummy write
    // LSR operation
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp >>= 1;
    // EOR operation
    cpu.a ^= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_zp_wait4);
    NEXT_INSTRUCTION(sre_zp_fetch_wait);
}

void sre_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sre_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, sre_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_ind_x_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_ind_x_wait6); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp >>= 1;
    cpu.a ^= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_ind_x_wait7);
    NEXT_INSTRUCTION(sre_ind_x_fetch_wait);
}

void sre_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sre_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, sre_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, sre_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, sre_ind_y_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, sre_ind_y_wait6); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp >>= 1;
    cpu.a ^= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, sre_ind_y_wait7);
    NEXT_INSTRUCTION(sre_ind_y_fetch_wait);
}

void sre_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sre_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sre_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_abs_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_abs_wait4); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp >>= 1;
    cpu.a ^= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_abs_wait5);
    NEXT_INSTRUCTION(sre_abs_fetch_wait);
}

void sre_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sre_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sre_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, sre_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, sre_abs_y_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, sre_abs_y_wait5); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp >>= 1;
    cpu.a ^= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, sre_abs_y_wait6);
    NEXT_INSTRUCTION(sre_abs_y_fetch_wait);
}

void sre_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sre_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sre_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, sre_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, sre_abs_x_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, sre_abs_x_wait5); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp >>= 1;
    cpu.a ^= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, sre_abs_x_wait6);
    NEXT_INSTRUCTION(sre_abs_x_fetch_wait);
}

void sre_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sre_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, sre_zp_x_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_zp_x_wait4); // Dummy write
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp >>= 1;
    cpu.a ^= cpu.temp;
    cpu_set_zn(cpu.a);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, sre_zp_x_wait5);
    NEXT_INSTRUCTION(sre_zp_x_fetch_wait);
}

// RRA - Rotate Right and ADC (ROR + ADC)
void rra_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rra_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_zp_wait2);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_zp_wait3); // Dummy write
    // ROR operation
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp = (cpu.temp >> 1) | (old_carry ? 0x80 : 0);
    // ADC operation
    op_adc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_zp_wait4);
    NEXT_INSTRUCTION(rra_zp_fetch_wait);
}

void rra_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rra_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, rra_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_ind_x_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_ind_x_wait6); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp = (cpu.temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_ind_x_wait7);
    NEXT_INSTRUCTION(rra_ind_x_fetch_wait);
}

void rra_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rra_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, rra_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rra_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rra_ind_y_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rra_ind_y_wait6); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp = (cpu.temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rra_ind_y_wait7);
    NEXT_INSTRUCTION(rra_ind_y_fetch_wait);
}

void rra_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rra_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, rra_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_abs_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_abs_wait4); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp = (cpu.temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_abs_wait5);
    NEXT_INSTRUCTION(rra_abs_fetch_wait);
}

void rra_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rra_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, rra_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rra_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, rra_abs_y_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rra_abs_y_wait5); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp = (cpu.temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, rra_abs_y_wait6);
    NEXT_INSTRUCTION(rra_abs_y_fetch_wait);
}

void rra_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rra_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, rra_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, rra_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, rra_abs_x_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, rra_abs_x_wait5); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp = (cpu.temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, rra_abs_x_wait6);
    NEXT_INSTRUCTION(rra_abs_x_fetch_wait);
}

void rra_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, rra_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, rra_zp_x_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_zp_x_wait4); // Dummy write
    bool old_carry = cpu_get_flag(FLAG_C);
    cpu_set_flag(FLAG_C, cpu.temp & 0x01);
    cpu.temp = (cpu.temp >> 1) | (old_carry ? 0x80 : 0);
    op_adc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, rra_zp_x_wait5);
    NEXT_INSTRUCTION(rra_zp_x_fetch_wait);
}

// SAX - Store A AND X
void sax_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sax_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a & cpu.x, sax_zp_wait2);
    NEXT_INSTRUCTION(sax_zp_fetch_wait);
}

void sax_zero_page_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sax_zp_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sax_zp_y_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a & cpu.x, sax_zp_y_wait3);
    NEXT_INSTRUCTION(sax_zp_y_fetch_wait);
}

void sax_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sax_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sax_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a & cpu.x, sax_abs_wait3);
    NEXT_INSTRUCTION(sax_abs_fetch_wait);
}

void sax_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sax_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sax_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, sax_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, sax_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a & cpu.x, sax_ind_x_wait5);
    NEXT_INSTRUCTION(sax_ind_x_fetch_wait);
}

// LAX - Load A and X
void lax_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_zp_wait2);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_zp_fetch_wait);
}

void lax_zero_page_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_zp_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_zp_y_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.y) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_zp_y_wait3);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_zp_y_fetch_wait);
}

void lax_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, lax_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_abs_wait3);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_abs_fetch_wait);
}

void lax_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, lax_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, lax_abs_y_wait3);
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, lax_abs_y_wait4);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_abs_y_fetch_wait);
}

void lax_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, lax_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_ind_x_wait5);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_ind_x_fetch_wait);
}

void lax_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, lax_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = ((cpu.hi << 8) | cpu.lo) + cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_abs_x_wait3);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_abs_x_fetch_wait);
}

void lax_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_zp_x_wait1);
    cpu.addr_abs = (bus_state.data + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_zp_x_wait2);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_zp_x_fetch_wait);
}

void lax_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lax_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, lax_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, lax_ind_y_wait4);
    }
    
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, lax_ind_y_wait5);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_ind_y_fetch_wait);
}

void lax_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lax_imm_wait);
    cpu.a = cpu.x = bus_state.data;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(lax_imm_fetch_wait);
}

// DCP - Decrement and Compare (DEC + CMP)
void dcp_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dcp_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_zp_wait2);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_zp_wait3); // Dummy write
    cpu.temp--;
    // Compare with A
    uint16_t result = cpu.a - cpu.temp;
    cpu_set_flag(FLAG_C, result < 0x100);
    cpu_set_zn(result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_zp_wait4);
    NEXT_INSTRUCTION(dcp_zp_fetch_wait);
}

void dcp_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dcp_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, dcp_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_ind_x_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_ind_x_wait6); // Dummy write
    cpu.temp--;
    uint16_t result = cpu.a - cpu.temp;
    cpu_set_flag(FLAG_C, result < 0x100);
    cpu_set_zn(result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_ind_x_wait7);
    NEXT_INSTRUCTION(dcp_ind_x_fetch_wait);
}

void dcp_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dcp_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, dcp_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, dcp_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, dcp_ind_y_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, dcp_ind_y_wait6); // Dummy write
    cpu.temp--;
    uint16_t result = cpu.a - cpu.temp;
    cpu_set_flag(FLAG_C, result < 0x100);
    cpu_set_zn(result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, dcp_ind_y_wait7);
    NEXT_INSTRUCTION(dcp_ind_y_fetch_wait);
}

void dcp_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dcp_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, dcp_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_abs_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_abs_wait4); // Dummy write
    cpu.temp--;
    uint16_t result = cpu.a - cpu.temp;
    cpu_set_flag(FLAG_C, result < 0x100);
    cpu_set_zn(result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_abs_wait5);
    NEXT_INSTRUCTION(dcp_abs_fetch_wait);
}

void dcp_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dcp_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, dcp_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, dcp_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, dcp_abs_y_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, dcp_abs_y_wait5); // Dummy write
    cpu.temp--;
    uint16_t result = cpu.a - cpu.temp;
    cpu_set_flag(FLAG_C, result < 0x100);
    cpu_set_zn(result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, dcp_abs_y_wait6);
    NEXT_INSTRUCTION(dcp_abs_y_fetch_wait);
}

void dcp_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dcp_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, dcp_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, dcp_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, dcp_abs_x_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, dcp_abs_x_wait5); // Dummy write
    cpu.temp--;
    uint16_t result = cpu.a - cpu.temp;
    cpu_set_flag(FLAG_C, result < 0x100);
    cpu_set_zn(result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, dcp_abs_x_wait6);
    NEXT_INSTRUCTION(dcp_abs_x_fetch_wait);
}

void dcp_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, dcp_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, dcp_zp_x_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_zp_x_wait4); // Dummy write
    cpu.temp--;
    uint16_t result = cpu.a - cpu.temp;
    cpu_set_flag(FLAG_C, result < 0x100);
    cpu_set_zn(result & 0xFF);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, dcp_zp_x_wait5);
    NEXT_INSTRUCTION(dcp_zp_x_fetch_wait);
}

// ISC/ISB - Increment and Subtract with Carry (INC + SBC)
void isc_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, isc_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_zp_wait2);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_zp_wait3); // Dummy write
    cpu.temp++;
    // SBC operation
    op_sbc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_zp_wait4);
    NEXT_INSTRUCTION(isc_zp_fetch_wait);
}

void isc_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, isc_ind_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_ind_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, isc_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_ind_x_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_ind_x_wait6); // Dummy write
    cpu.temp++;
    op_sbc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_ind_x_wait7);
    NEXT_INSTRUCTION(isc_ind_x_fetch_wait);
}

void isc_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, isc_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, isc_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, isc_ind_y_wait4); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, isc_ind_y_wait5);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, isc_ind_y_wait6); // Dummy write
    cpu.temp++;
    op_sbc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, isc_ind_y_wait7);
    NEXT_INSTRUCTION(isc_ind_y_fetch_wait);
}

void isc_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, isc_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, isc_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_abs_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_abs_wait4); // Dummy write
    cpu.temp++;
    op_sbc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_abs_wait5);
    NEXT_INSTRUCTION(isc_abs_fetch_wait);
}

void isc_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, isc_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, isc_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, isc_abs_y_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.y, isc_abs_y_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, isc_abs_y_wait5); // Dummy write
    cpu.temp++;
    op_sbc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.y, cpu.temp, isc_abs_y_wait6);
    NEXT_INSTRUCTION(isc_abs_y_fetch_wait);
}

void isc_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, isc_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, isc_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, isc_abs_x_wait3); // Page cross
    }
    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, isc_abs_x_wait4);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, isc_abs_x_wait5); // Dummy write
    cpu.temp++;
    op_sbc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs + cpu.x, cpu.temp, isc_abs_x_wait6);
    NEXT_INSTRUCTION(isc_abs_x_fetch_wait);
}

void isc_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, isc_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, isc_zp_x_wait3);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_zp_x_wait4); // Dummy write
    cpu.temp++;
    op_sbc(cpu.temp);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.temp, isc_zp_x_wait5);
    NEXT_INSTRUCTION(isc_zp_x_fetch_wait);
}

// NOP variants
void nop_func(void) { /* true NOP, does nothing */ NEXT_INSTRUCTION(nop_fetch_wait); }
void nop_zero_page_func(void) { NEXT_INSTRUCTION(nop_fetch_wait); }
void nop_zero_page_x_func(void) { NEXT_INSTRUCTION(nop_fetch_wait); }
void nop_absolute_func(void) { NEXT_INSTRUCTION(nop_fetch_wait); }
void nop_absolute_x_func(void) { NEXT_INSTRUCTION(nop_fetch_wait); }
void nop_immediate_func(void) { NEXT_INSTRUCTION(nop_fetch_wait); }

// Single-byte immediate illegal opcodes
void anc_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, anc_imm_wait);
    cpu.a &= bus_state.data;
    cpu_set_zn(cpu.a);
    cpu_set_flag(FLAG_C, cpu.a & 0x80);
    NEXT_INSTRUCTION(anc_imm_fetch_wait);
}

void alr_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, alr_imm_wait);
    cpu.a &= bus_state.data;
    cpu_set_flag(FLAG_C, cpu.a & 0x01);
    cpu.a >>= 1;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(alr_imm_fetch_wait);
}

void arr_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, arr_imm_wait);
    cpu.a &= bus_state.data;
    cpu.a = (cpu.a >> 1) | (cpu_get_flag(FLAG_C) ? 0x80 : 0);
    cpu_set_zn(cpu.a);
    cpu_set_flag(FLAG_C, cpu.a & 0x40);
    cpu_set_flag(FLAG_V, ((cpu.a >> 5) ^ (cpu.a >> 6)) & 1);
    NEXT_INSTRUCTION(arr_imm_fetch_wait);
}

void xaa_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, xaa_imm_wait);
    cpu.a = (cpu.a | 0xEE) & cpu.x & bus_state.data; // Unstable, but this is a common guess
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(xaa_imm_fetch_wait);
}

void axs_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, axs_imm_wait);
    uint8_t val = cpu.a & cpu.x;
    uint16_t result = val - bus_state.data;
    cpu.x = result & 0xFF;
    cpu_set_zn(cpu.x);
    cpu_set_flag(FLAG_C, result < 0x100);
    NEXT_INSTRUCTION(axs_imm_fetch_wait);
}

// Unstable/undocumented opcodes
void ahx_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ahx_ind_y_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ahx_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.addr_abs + 1) & 0xFF, ahx_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = ((cpu.hi << 8) | cpu.lo) + cpu.y;
    uint8_t val = cpu.a & cpu.x & ((cpu.addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, val, ahx_ind_y_wait4);
    NEXT_INSTRUCTION(ahx_ind_y_fetch_wait);
}

void tas_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, tas_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, tas_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = ((cpu.hi << 8) | cpu.lo) + cpu.y;
    cpu.sp = cpu.a & cpu.x;
    uint8_t val = cpu.sp & ((cpu.addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, val, tas_abs_y_wait3);
    NEXT_INSTRUCTION(tas_abs_y_fetch_wait);
}

void shy_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, shy_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, shy_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = ((cpu.hi << 8) | cpu.lo) + cpu.x;
    uint8_t val = cpu.y & ((cpu.addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, val, shy_abs_x_wait3);
    NEXT_INSTRUCTION(shy_abs_x_fetch_wait);
}

void shx_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, shx_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, shx_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = ((cpu.hi << 8) | cpu.lo) + cpu.y;
    uint8_t val = cpu.x & ((cpu.addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, val, shx_abs_y_wait3);
    NEXT_INSTRUCTION(shx_abs_y_fetch_wait);
}

void ahx_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ahx_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ahx_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = ((cpu.hi << 8) | cpu.lo) + cpu.y;
    uint8_t val = cpu.a & cpu.x & ((cpu.addr_abs >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, val, ahx_abs_y_wait3);
    NEXT_INSTRUCTION(ahx_abs_y_fetch_wait);
}

void las_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, las_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, las_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = ((cpu.hi << 8) | cpu.lo) + cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, las_abs_y_wait3);
    cpu.a = cpu.x = cpu.sp = bus_state.data & cpu.sp;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(las_abs_y_fetch_wait);
}