#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY REGISTER OPERATIONS
// ============================================================================

// ============================================================================
// TRANSFER OPERATIONS
// ============================================================================

// TAX - Transfer A to X
void fam65xx_op_tax(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->a;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TAY - Transfer A to Y
void fam65xx_op_tay(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y = cpu->a;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TXA - Transfer X to A
void fam65xx_op_txa(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->x;
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TYA - Transfer Y to A
void fam65xx_op_tya(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->y;
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TSX - Transfer Stack Pointer to X
void fam65xx_op_tsx(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->sp;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TXS - Transfer X to Stack Pointer
void fam65xx_op_txs(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->sp = cpu->x;
    // TXS does not affect any flags
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STACK OPERATIONS
// ============================================================================

// PHA - Push Accumulator
void fam65xx_op_pha(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_push(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// PLA - Pull Accumulator
void fam65xx_op_pla(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = fam65xx_pull(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// PHP - Push Processor Status
void fam65xx_op_php(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_push(cpu, cpu->p | FLAG_B | FLAG_U); // B flag set when pushed by PHP
    FAM65XX_OPCODE_FOOTER(cpu);
}

// PLP - Pull Processor Status
void fam65xx_op_plp(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->p = fam65xx_pull(cpu);
    cpu->p |= FLAG_U; // Unused flag always set
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// INCREMENT/DECREMENT OPERATIONS
// ============================================================================

// INX - Increment X Register
void fam65xx_op_inx(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x++;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// INY - Increment Y Register
void fam65xx_op_iny(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y++;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// DEX - Decrement X Register
void fam65xx_op_dex(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x--;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// DEY - Decrement Y Register
void fam65xx_op_dey(fam65xx_t* cpu) {

    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y--;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// MEMORY INCREMENT/DECREMENT OPERATIONS
// ============================================================================

// INC - Increment Memory
void fam65xx_op_inc_zero_page(fam65xx_t* cpu) {

    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_inc_zero_page_x(fam65xx_t* cpu) {

    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_inc_absolute(fam65xx_t* cpu) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = (addr_hi << 8) | addr_lo;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_inc_absolute_x(fam65xx_t* cpu) {

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
    FAM65XX_OPCODE_FOOTER(cpu);
}

// DEC - Decrement Memory
void fam65xx_op_dec_zero_page(fam65xx_t* cpu) {

    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_dec_zero_page_x(fam65xx_t* cpu) {

    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_dec_absolute(fam65xx_t* cpu) {

    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch

    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = (addr_hi << 8) | addr_lo;

    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch

    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;

    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_dec_absolute_x(fam65xx_t* cpu) {

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
    FAM65XX_OPCODE_FOOTER(cpu);
}
