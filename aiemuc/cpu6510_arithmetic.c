#include "cpu6510.h"
#include "bus.h"
#include "c64.h"

// ============================================================================
// MOS 6510 ARITHMETIC AND LOGIC INSTRUCTIONS
// ============================================================================

// ADC - Add with Carry
void adc_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_imm_wait);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_imm_fetch_wait);
}

void adc_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_zp_wait2);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_zp_fetch_wait);
}

void adc_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_zp_x_wait3);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_zp_x_fetch_wait);
}

void adc_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, adc_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_abs_wait3);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_abs_fetch_wait);
}

void adc_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, adc_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), adc_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_abs_x_wait4);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_abs_x_fetch_wait);
}

void adc_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, adc_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), adc_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_abs_y_wait4);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_abs_y_fetch_wait);
}

void adc_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, adc_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, adc_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, adc_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_ind_x_wait5);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_ind_x_fetch_wait);
}

void adc_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, adc_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, adc_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), adc_ind_y_wait4);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, adc_ind_y_wait5);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_ind_y_fetch_wait);
}

// SBC - Subtract with Carry
void sbc_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_imm_wait);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_imm_fetch_wait);
}

void sbc_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_zp_wait2);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_zp_fetch_wait);
}

void sbc_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_zp_x_wait3);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_zp_x_fetch_wait);
}

void sbc_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sbc_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_abs_wait3);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_abs_fetch_wait);
}

void sbc_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sbc_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), sbc_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_abs_x_wait4);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_abs_x_fetch_wait);
}

void sbc_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sbc_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), sbc_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_abs_y_wait4);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_abs_y_fetch_wait);
}

void sbc_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, sbc_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, sbc_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, sbc_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_ind_x_wait5);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_ind_x_fetch_wait);
}

void sbc_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sbc_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, sbc_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, sbc_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), sbc_ind_y_wait4);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, sbc_ind_y_wait5);
    op_sbc(bus_state.data);
    NEXT_INSTRUCTION(sbc_ind_y_fetch_wait);
}

// AND - Logical AND
void and_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_imm_wait);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_imm_fetch_wait);
}

void and_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, and_zp_wait2);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_zp_fetch_wait);
}

void and_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, and_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, and_zp_x_wait3);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_zp_x_fetch_wait);
}

void and_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, and_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, and_abs_wait3);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_abs_fetch_wait);
}

void and_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, and_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), and_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, and_abs_x_wait4);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_abs_x_fetch_wait);
}

void and_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, and_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), and_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, and_abs_y_wait4);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_abs_y_fetch_wait);
}

void and_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, and_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, and_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, and_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, and_ind_x_wait5);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_ind_x_fetch_wait);
}

void and_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, and_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, and_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), and_ind_y_wait4);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, and_ind_y_wait5);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_ind_y_fetch_wait);
}

// ORA - Logical OR
void ora_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_imm_wait);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_imm_fetch_wait);
}

void ora_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_zp_wait2);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_zp_fetch_wait);
}

void ora_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_zp_x_wait3);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_zp_x_fetch_wait);
}

void ora_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ora_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_abs_wait3);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_abs_fetch_wait);
}

void ora_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ora_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), ora_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_abs_x_wait4);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_abs_x_fetch_wait);
}

void ora_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, ora_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), ora_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_abs_y_wait4);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_abs_y_fetch_wait);
}

void ora_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, ora_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, ora_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, ora_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_ind_x_wait5);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_ind_x_fetch_wait);
}

void ora_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, ora_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, ora_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, ora_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), ora_ind_y_wait4);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, ora_ind_y_wait5);
    op_ora(bus_state.data);
    NEXT_INSTRUCTION(ora_ind_y_fetch_wait);
}

// EOR - Exclusive OR
void eor_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_imm_wait);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_imm_fetch_wait);
}

void eor_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_zp_wait2);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_zp_fetch_wait);
}

void eor_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_zp_x_wait3);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_zp_x_fetch_wait);
}

void eor_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, eor_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_abs_wait3);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_abs_fetch_wait);
}

void eor_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, eor_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), eor_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_abs_x_wait4);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_abs_x_fetch_wait);
}

void eor_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, eor_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), eor_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_abs_y_wait4);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_abs_y_fetch_wait);
}

void eor_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, eor_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, eor_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, eor_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_ind_x_wait5);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_ind_x_fetch_wait);
}

void eor_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, eor_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, eor_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, eor_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), eor_ind_y_wait4);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, eor_ind_y_wait5);
    op_eor(bus_state.data);
    NEXT_INSTRUCTION(eor_ind_y_fetch_wait);
}

// CMP - Compare Accumulator
void cmp_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_imm_wait);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_imm_fetch_wait);
}

void cmp_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_zp_wait2);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_zp_fetch_wait);
}

void cmp_zero_page_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_zp_x_wait3);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_zp_x_fetch_wait);
}

void cmp_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, cmp_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_abs_wait3);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_abs_fetch_wait);
}

void cmp_absolute_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, cmp_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.x) & 0xFF), cmp_abs_x_wait3);
    }
    
    cpu.addr_abs += cpu.x;
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_abs_x_wait4);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_abs_x_fetch_wait);
}

void cmp_absolute_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_abs_y_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, cmp_abs_y_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), cmp_abs_y_wait3);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_abs_y_wait4);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_abs_y_fetch_wait);
}

void cmp_indirect_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_ind_x_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, cmp_ind_x_wait2); // Dummy read
    cpu.temp = (cpu.temp + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.temp, cmp_ind_x_wait3);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, cmp_ind_x_wait4);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_ind_x_wait5);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_ind_x_fetch_wait);
}

void cmp_indirect_y_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cmp_ind_y_wait1);
    cpu.temp = bus_state.data;
    WAIT_READY_THEN_READ(cpu.temp, cmp_ind_y_wait2);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ((cpu.temp + 1) & 0xFF, cmp_ind_y_wait3);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;
    
    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.y) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ((cpu.addr_abs & 0xFF00) | ((cpu.addr_abs + cpu.y) & 0xFF), cmp_ind_y_wait4);
    }
    
    cpu.addr_abs += cpu.y;
    WAIT_READY_THEN_READ(cpu.addr_abs, cmp_ind_y_wait5);
    op_cmp(bus_state.data);
    NEXT_INSTRUCTION(cmp_ind_y_fetch_wait);
}

// CPX - Compare X Register
void cpx_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cpx_imm_wait);
    op_cpx(bus_state.data);
    NEXT_INSTRUCTION(cpx_imm_fetch_wait);
}

void cpx_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cpx_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, cpx_zp_wait2);
    op_cpx(bus_state.data);
    NEXT_INSTRUCTION(cpx_zp_fetch_wait);
}

void cpx_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cpx_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, cpx_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, cpx_abs_wait3);
    op_cpx(bus_state.data);
    NEXT_INSTRUCTION(cpx_abs_fetch_wait);
}

// CPY - Compare Y Register
void cpy_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cpy_imm_wait);
    op_cpy(bus_state.data);
    NEXT_INSTRUCTION(cpy_imm_fetch_wait);
}

void cpy_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cpy_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, cpy_zp_wait2);
    op_cpy(bus_state.data);
    NEXT_INSTRUCTION(cpy_zp_fetch_wait);
}

void cpy_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, cpy_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, cpy_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, cpy_abs_wait3);
    op_cpy(bus_state.data);
    NEXT_INSTRUCTION(cpy_abs_fetch_wait);
}