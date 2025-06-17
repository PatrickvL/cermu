#include "mos6502_family_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY MEMORY OPERATIONS
// ============================================================================

// ============================================================================
// LOAD OPERATIONS - LDA
// ============================================================================

void mos6502_family_lda_immediate(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_imm(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lda_zero_page(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_zp(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lda_zero_page_x(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_zpx(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lda_absolute(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_abs(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lda_absolute_x(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_absx(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lda_absolute_y(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_absy(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lda_indirect_x(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_indx(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lda_indirect_y(mos6502_family_t* cpu) {
    cpu->a = mos6502_family_addr_indy(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// LOAD OPERATIONS - LDX
// ============================================================================

void mos6502_family_ldx_immediate(mos6502_family_t* cpu) {
    cpu->x = mos6502_family_addr_imm(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldx_zero_page(mos6502_family_t* cpu) {
    cpu->x = mos6502_family_addr_zp(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldx_zero_page_y(mos6502_family_t* cpu) {
    cpu->x = mos6502_family_addr_zpy(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldx_absolute(mos6502_family_t* cpu) {
    cpu->x = mos6502_family_addr_abs(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldx_absolute_y(mos6502_family_t* cpu) {
    cpu->x = mos6502_family_addr_absy(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// LOAD OPERATIONS - LDY
// ============================================================================

void mos6502_family_ldy_immediate(mos6502_family_t* cpu) {
    cpu->y = mos6502_family_addr_imm(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldy_zero_page(mos6502_family_t* cpu) {
    cpu->y = mos6502_family_addr_zp(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldy_zero_page_x(mos6502_family_t* cpu) {
    cpu->y = mos6502_family_addr_zpx(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldy_absolute(mos6502_family_t* cpu) {
    cpu->y = mos6502_family_addr_abs(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ldy_absolute_x(mos6502_family_t* cpu) {
    cpu->y = mos6502_family_addr_absx(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STA
// ============================================================================

void mos6502_family_sta_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sta_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sta_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sta_absolute_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sta_absolute_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sta_indirect_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, zp_addr); // Dummy read
    uint8_t effective_addr = (zp_addr + cpu->x) & 0xFF;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, effective_addr);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (effective_addr + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sta_indirect_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc++);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, zp_addr);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STX
// ============================================================================

void mos6502_family_stx_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_stx_zero_page_y(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->y) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_stx_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STY
// ============================================================================

void mos6502_family_sty_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sty_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_sty_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}
