#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY REGISTER OPERATIONS
// ============================================================================

// ============================================================================
// TRANSFER OPERATIONS
// ============================================================================

// TAX - Transfer A to X
FAM65XX_OPCODE_PROTO(fam65xx_op_tax) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->a;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER();
}

// TAY - Transfer A to Y
FAM65XX_OPCODE_PROTO(fam65xx_op_tay) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y = cpu->a;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER();
}

// TXA - Transfer X to A
FAM65XX_OPCODE_PROTO(fam65xx_op_txa) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->x;
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER();
}

// TYA - Transfer Y to A
FAM65XX_OPCODE_PROTO(fam65xx_op_tya) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->y;
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER();
}

// TSX - Transfer Stack Pointer to X
FAM65XX_OPCODE_PROTO(fam65xx_op_tsx) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->sp;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER();
}

// TXS - Transfer X to Stack Pointer
FAM65XX_OPCODE_PROTO(fam65xx_op_txs) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->sp = cpu->x;
    // TXS does not affect any flags
    FAM65XX_OPCODE_FOOTER();
}

// ============================================================================
// STACK OPERATIONS
// ============================================================================

// PHA - Push Accumulator
FAM65XX_OPCODE_PROTO(fam65xx_op_pha) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_push(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER();
}

// PLA - Pull Accumulator
FAM65XX_OPCODE_PROTO(fam65xx_op_pla) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = fam65xx_pull(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER();
}

// PHP - Push Processor Status
FAM65XX_OPCODE_PROTO(fam65xx_op_php) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_push(cpu, cpu->p | FLAG_B | FLAG_U); // B flag set when pushed by PHP
    FAM65XX_OPCODE_FOOTER();
}

// PLP - Pull Processor Status
FAM65XX_OPCODE_PROTO(fam65xx_op_plp) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->p = fam65xx_pull(cpu);
    cpu->p |= FLAG_U; // Unused flag always set
    FAM65XX_OPCODE_FOOTER();
}

// ============================================================================
// INCREMENT/DECREMENT OPERATIONS
// ============================================================================

// INX - Increment X Register
FAM65XX_OPCODE_PROTO(fam65xx_op_inx) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x++;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER();
}

// INY - Increment Y Register
FAM65XX_OPCODE_PROTO(fam65xx_op_iny) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y++;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER();
}

// DEX - Decrement X Register
FAM65XX_OPCODE_PROTO(fam65xx_op_dex) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x--;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER();
}

// DEY - Decrement Y Register
FAM65XX_OPCODE_PROTO(fam65xx_op_dey) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y--;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER();
}

// ============================================================================
// MEMORY INCREMENT/DECREMENT OPERATIONS
// ============================================================================

static inline uint8_t fam65xx_op_inc(fam65xx_t* cpu, uint8_t value) {
    value++;
    fam65xx_set_nz_flags(cpu, value);
    return value;
}

// INC - Increment Memory
FAM65XX_OPCODE_PROTO(fam65xx_op_inc_zero_page) {

    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}

FAM65XX_OPCODE_PROTO(fam65xx_op_inc_zero_page_x) {

    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}

FAM65XX_OPCODE_PROTO(fam65xx_op_inc_absolute) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = (addr_hi << 8) | addr_lo;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}

FAM65XX_OPCODE_PROTO(fam65xx_op_inc_absolute_x) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;

    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}

// DEC - Decrement Memory

static inline uint8_t fam65xx_op_dec(fam65xx_t* cpu, uint8_t value) {
    value--;
    fam65xx_set_nz_flags(cpu, value);
    return value;
}

FAM65XX_OPCODE_PROTO(fam65xx_op_dec_zero_page) {

    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}

FAM65XX_OPCODE_PROTO(fam65xx_op_dec_zero_page_x) {

    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}

FAM65XX_OPCODE_PROTO(fam65xx_op_dec_absolute) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = (addr_hi << 8) | addr_lo;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}

FAM65XX_OPCODE_PROTO(fam65xx_op_dec_absolute_x) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;

    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER();
}
