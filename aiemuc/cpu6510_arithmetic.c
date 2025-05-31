#include "cpu6510.h"

// ============================================================================
// MOS 6510 ARITHMETIC AND LOGIC INSTRUCTIONS
// ============================================================================

// ADC - Add with Carry
void adc_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_imm_fetch_wait);
}

void adc_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_zp_fetch_wait);
}

void adc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, adc_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_zp_x_fetch_wait);
}

void adc_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, adc_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_abs_fetch_wait);
}

void adc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, adc_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, adc_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_abs_x_fetch_wait);
}

void adc_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_abs_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_abs_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, adc_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, adc_abs_y_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_abs_y_fetch_wait);
}

void adc_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, adc_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, adc_ind_x_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_ind_x_fetch_wait);
}

void adc_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, adc_ind_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_ind_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, adc_ind_y_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, adc_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, adc_ind_y_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_adc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, adc_ind_y_fetch_wait);
}

// SBC - Subtract with Carry
void sbc_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_imm_fetch_wait);
}

void sbc_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_zp_fetch_wait);
}

void sbc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, sbc_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_zp_x_fetch_wait);
}

void sbc_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_abs_fetch_wait);
}

void sbc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, sbc_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_abs_x_fetch_wait);
}

void sbc_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, sbc_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, sbc_abs_y_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_abs_y_fetch_wait);
}

void sbc_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_x_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_ind_x_fetch_wait);
}

void sbc_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_y_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_y_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_y_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, sbc_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, sbc_ind_y_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_sbc(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, sbc_ind_y_fetch_wait);
}

// AND - Logical AND
void and_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_imm_fetch_wait);
}

void and_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, and_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_zp_fetch_wait);
}

void and_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, and_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, and_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_zp_x_fetch_wait);
}

void and_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, and_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, and_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_abs_fetch_wait);
}

void and_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_abs_x_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, and_abs_x_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, and_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, and_abs_x_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_abs_x_fetch_wait);
}

void and_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_abs_y_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, and_abs_y_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, and_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, and_abs_y_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_abs_y_fetch_wait);
}

void and_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, and_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, and_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, and_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, and_ind_x_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_ind_x_fetch_wait);
}

void and_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, and_ind_y_wait1);
    cpu_dev->temp = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, and_ind_y_wait2);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    CPU_READY_OR_STALL(cpu_dev, and_ind_y_wait3);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, and_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, and_ind_y_wait5);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_and(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, and_ind_y_fetch_wait);
}

// ORA - Logical OR
void ora_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_imm_fetch_wait);
}

void ora_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ora_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_zp_fetch_wait);
}

void ora_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ora_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ora_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_zp_x_fetch_wait);
}

void ora_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ora_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, ora_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_abs_fetch_wait);
}

void ora_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_abs_x_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ora_abs_x_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, ora_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, ora_abs_x_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_abs_x_fetch_wait);
}

void ora_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_abs_y_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ora_abs_y_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, ora_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, ora_abs_y_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_abs_y_fetch_wait);
}

void ora_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ora_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, ora_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ora_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, ora_ind_x_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_ind_x_fetch_wait);
}

void ora_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ora_ind_y_wait1);
    cpu_dev->temp = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ora_ind_y_wait2);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    CPU_READY_OR_STALL(cpu_dev, ora_ind_y_wait3);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, ora_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, ora_ind_y_wait5);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_ora(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, ora_ind_y_fetch_wait);
}

// EOR - Exclusive OR
void eor_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_imm_fetch_wait);
}

void eor_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, eor_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_zp_fetch_wait);
}

void eor_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, eor_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, eor_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_zp_x_fetch_wait);
}

void eor_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, eor_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, eor_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_abs_fetch_wait);
}

void eor_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_abs_x_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, eor_abs_x_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, eor_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, eor_abs_x_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_abs_x_fetch_wait);
}

void eor_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_abs_y_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, eor_abs_y_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, eor_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, eor_abs_y_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_abs_y_fetch_wait);
}

void eor_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, eor_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, eor_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, eor_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, eor_ind_x_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_ind_x_fetch_wait);
}

void eor_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, eor_ind_y_wait1);
    cpu_dev->temp = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, eor_ind_y_wait2);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    CPU_READY_OR_STALL(cpu_dev, eor_ind_y_wait3);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, eor_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, eor_ind_y_wait5);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_eor(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, eor_ind_y_fetch_wait);
}

// CMP - Compare Accumulator
void cmp_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_imm_fetch_wait);
}

void cmp_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cmp_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_zp_fetch_wait);
}

void cmp_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_zp_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cmp_zp_x_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->addr_abs); // Dummy read
    cpu_dev->addr_abs = (cpu_dev->addr_abs + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, cmp_zp_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_zp_x_fetch_wait);
}

void cmp_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_abs_fetch_wait);
}

void cmp_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_x_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_x_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, cmp_abs_x_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->x) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->x;
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_x_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_abs_x_fetch_wait);
}

void cmp_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_y_wait1);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_y_wait2);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, cmp_abs_y_wait3);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, cmp_abs_y_wait4);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_abs_y_fetch_wait);
}

void cmp_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_x_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->temp = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_x_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp); // Dummy read
    cpu_dev->temp = (cpu_dev->temp + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_x_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_x_wait4);
    cpu_data = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_x_wait5);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_ind_x_fetch_wait);
}

void cmp_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_y_wait1);
    cpu_dev->temp = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_y_wait2);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->temp);
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_y_wait3);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, (cpu_dev->temp + 1) & 0xFF);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    if ((cpu_dev->addr_abs & 0xFF00) != ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF00)) {
        // Page crossed - extra cycle
        CPU_READY_OR_STALL(cpu_dev, cmp_ind_y_wait4);
        (void)cpu_read_cycle(cpu_dev, (cpu_dev->addr_abs & 0xFF00) | ((cpu_dev->addr_abs + cpu_dev->y) & 0xFF));
    }
    cpu_dev->addr_abs += cpu_dev->y;
    CPU_READY_OR_STALL(cpu_dev, cmp_ind_y_wait5);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cmp(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cmp_ind_y_fetch_wait);
}

// CPX - Compare X Register
void cpx_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cpx_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_cpx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cpx_imm_fetch_wait);
}

void cpx_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cpx_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cpx_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cpx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cpx_zp_fetch_wait);
}

void cpx_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cpx_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cpx_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, cpx_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cpx(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cpx_abs_fetch_wait);
}

// CPY - Compare Y Register  
void cpy_immediate_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cpy_imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    op_cpy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cpy_imm_fetch_wait);
}

void cpy_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cpy_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cpy_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cpy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cpy_zp_fetch_wait);
}

void cpy_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, cpy_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, cpy_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, cpy_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_cpy(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, cpy_abs_fetch_wait);
}

// BIT - Bit Test
void bit_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bit_zp_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, bit_zp_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_bit(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, bit_zp_fetch_wait);
}

void bit_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, bit_abs_wait1);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, bit_abs_wait2);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs |= (cpu_data << 8);
    CPU_READY_OR_STALL(cpu_dev, bit_abs_wait3);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    op_bit(cpu_dev, cpu_data);
    NEXT_INSTRUCTION(cpu_dev, bit_abs_fetch_wait);
}

