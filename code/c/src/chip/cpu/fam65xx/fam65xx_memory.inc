#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY MEMORY OPERATIONS
// ============================================================================

// ============================================================================
// LOAD OPERATIONS - LDA
// ============================================================================

static inline void fam65xx_op_lda(fam65xx_t* cpu, uint8_t value) {
    cpu->a = value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_immediate) {
    cpu->a = fam65xx_addr_imm(cpu);             // T1: Immediate value fetch
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_zero_page) {
    cpu->a = fam65xx_addr_zp(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_zero_page_x) {
    cpu->a = fam65xx_addr_zpx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_absolute) {
    cpu->a = fam65xx_addr_abs(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_absolute_x) {
    cpu->a = fam65xx_addr_absx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_absolute_y) {
    cpu->a = fam65xx_addr_absy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_indirect_x) {
    cpu->a = fam65xx_addr_indx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_lda_indirect_y) {
    cpu->a = fam65xx_addr_indy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// LOAD OPERATIONS - LDX
// ============================================================================

static inline void fam65xx_op_ldx(fam65xx_t* cpu, uint8_t value) {
    cpu->x = value;
    fam65xx_set_nz_flags(cpu, cpu->x);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldx_immediate) {
    cpu->x = fam65xx_addr_imm(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldx_zero_page) {
    cpu->x = fam65xx_addr_zp(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldx_zero_page_y) {
    cpu->x = fam65xx_addr_zpy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldx_absolute) {
    cpu->x = fam65xx_addr_abs(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldx_absolute_y) {
    cpu->x = fam65xx_addr_absy(cpu);
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// LOAD OPERATIONS - LDY
// ============================================================================

static inline void fam65xx_op_ldy(fam65xx_t* cpu, uint8_t value) {
    cpu->y = value;
    fam65xx_set_nz_flags(cpu, cpu->y);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldy_immediate) {
    cpu->y = fam65xx_addr_imm(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldy_zero_page) {
    cpu->y = fam65xx_addr_zp(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldy_zero_page_x) {
    cpu->y = fam65xx_addr_zpx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldy_absolute) {
    cpu->y = fam65xx_addr_abs(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_ldy_absolute_x) {
    cpu->y = fam65xx_addr_absx(cpu);
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STA
// ============================================================================

FAM65XX_OPCODE_PROTO(fam65xx_op_sta_zero_page) {

    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    fam65xx_write_cycle(cpu, cpu->address, cpu->a);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sta_zero_page_x) {

    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;

    fam65xx_write_cycle(cpu, cpu->address, cpu->a);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sta_absolute) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = (addr_hi << 8) | addr_lo;

    fam65xx_write_cycle(cpu, cpu->address, cpu->a);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sta_absolute_x) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    

    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read

    fam65xx_write_cycle(cpu, cpu->address, cpu->a);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sta_absolute_y) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    

    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read

    fam65xx_write_cycle(cpu, cpu->address, cpu->a);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sta_indirect_x) {

    uint8_t zp_addr = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, zp_addr); // Dummy read
    uint8_t effective_addr = (zp_addr + cpu->x) & 0xFF;
    

    uint8_t addr_lo = fam65xx_read_cycle(cpu, effective_addr);  // Bus read cycle

    uint8_t addr_hi = fam65xx_read_cycle(cpu, (effective_addr + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
    

    fam65xx_write_cycle(cpu, cpu->address, cpu->a);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sta_indirect_y) {

    uint8_t zp_addr = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    

    uint8_t addr_lo = fam65xx_read_cycle(cpu, zp_addr);  // Bus read cycle

    uint8_t addr_hi = fam65xx_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    

    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read

    fam65xx_write_cycle(cpu, cpu->address, cpu->a);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STX
// ============================================================================

FAM65XX_OPCODE_PROTO(fam65xx_op_stx_zero_page) {

    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    fam65xx_write_cycle(cpu, cpu->address, cpu->x);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_stx_zero_page_y) {

    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->y) & 0xFF;

    fam65xx_write_cycle(cpu, cpu->address, cpu->x);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_stx_absolute) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = (addr_hi << 8) | addr_lo;

    fam65xx_write_cycle(cpu, cpu->address, cpu->x);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE OPERATIONS - STY
// ============================================================================

FAM65XX_OPCODE_PROTO(fam65xx_op_sty_zero_page) {

    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    fam65xx_write_cycle(cpu, cpu->address, cpu->y);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sty_zero_page_x) {

    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;

    fam65xx_write_cycle(cpu, cpu->address, cpu->y);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}

FAM65XX_OPCODE_PROTO(fam65xx_op_sty_absolute) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = (addr_hi << 8) | addr_lo;

    fam65xx_write_cycle(cpu, cpu->address, cpu->y);  // T2: Data store
    FAM65XX_OPCODE_FOOTER(cpu);
}
