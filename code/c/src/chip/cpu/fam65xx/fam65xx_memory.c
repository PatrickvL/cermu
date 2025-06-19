#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY MEMORY OPERATIONS
// ============================================================================

// ============================================================================
// LOAD OPERATIONS - LDA
// ============================================================================

void fam65xx_lda_immediate(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_imm(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_lda_zero_page(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_zp(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_lda_zero_page_x(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_zpx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_lda_absolute(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_abs(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_lda_absolute_x(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_absx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_lda_absolute_y(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_absy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_lda_indirect_x(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_indx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_lda_indirect_y(fam65xx_t* cpu) {
    cpu->a = fam65xx_addr_indy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// LOAD OPERATIONS - LDX
// ============================================================================

void fam65xx_ldx_immediate(fam65xx_t* cpu) {
    cpu->x = fam65xx_addr_imm(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldx_zero_page(fam65xx_t* cpu) {
    cpu->x = fam65xx_addr_zp(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldx_zero_page_y(fam65xx_t* cpu) {
    cpu->x = fam65xx_addr_zpy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldx_absolute(fam65xx_t* cpu) {
    cpu->x = fam65xx_addr_abs(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldx_absolute_y(fam65xx_t* cpu) {
    cpu->x = fam65xx_addr_absy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// LOAD OPERATIONS - LDY
// ============================================================================

void fam65xx_ldy_immediate(fam65xx_t* cpu) {
    cpu->y = fam65xx_addr_imm(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldy_zero_page(fam65xx_t* cpu) {
    cpu->y = fam65xx_addr_zp(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldy_zero_page_x(fam65xx_t* cpu) {
    cpu->y = fam65xx_addr_zpx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldy_absolute(fam65xx_t* cpu) {
    cpu->y = fam65xx_addr_abs(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_ldy_absolute_x(fam65xx_t* cpu) {
    cpu->y = fam65xx_addr_absx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STA
// ============================================================================

void fam65xx_sta_zero_page(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sta_zero_page_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sta_absolute(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sta_absolute_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sta_absolute_y(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sta_indirect_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t zp_addr = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, zp_addr); // Dummy read
    uint8_t effective_addr = (zp_addr + cpu->x) & 0xFF;
    
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, effective_addr);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, (effective_addr + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
    
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sta_indirect_y(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t zp_addr = fam65xx_read_cycle(cpu, cpu->pc++);
    
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, zp_addr);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STX
// ============================================================================

void fam65xx_stx_zero_page(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_stx_zero_page_y(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->y) & 0xFF;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_stx_absolute(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STY
// ============================================================================

void fam65xx_sty_zero_page(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sty_zero_page_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_sty_absolute(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}
