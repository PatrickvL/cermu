#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY REGISTER OPERATIONS
// ============================================================================

// ============================================================================
// TRANSFER OPERATIONS
// ============================================================================

// TAX - Transfer A to X
void fam65xx_tax(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->a;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TAY - Transfer A to Y
void fam65xx_tay(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y = cpu->a;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TXA - Transfer X to A
void fam65xx_txa(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->x;
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TYA - Transfer Y to A
void fam65xx_tya(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->y;
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TSX - Transfer Stack Pointer to X
void fam65xx_tsx(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->sp;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// TXS - Transfer X to Stack Pointer
void fam65xx_txs(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->sp = cpu->x;
    // TXS does not affect any flags
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STACK OPERATIONS
// ============================================================================

// PHA - Push Accumulator
void fam65xx_pha(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_push(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// PLA - Pull Accumulator
void fam65xx_pla(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = fam65xx_pull(cpu);
    fam65xx_set_nz_flags(cpu, cpu->a);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// PHP - Push Processor Status
void fam65xx_php(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    fam65xx_push(cpu, cpu->p | FLAG_B | FLAG_U); // B flag set when pushed by PHP
    FAM65XX_OPCODE_FOOTER(cpu);
}

// PLP - Pull Processor Status
void fam65xx_plp(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->p = fam65xx_pull(cpu);
    cpu->p |= FLAG_U; // Unused flag always set
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// INCREMENT/DECREMENT OPERATIONS
// ============================================================================

// INX - Increment X Register
void fam65xx_inx(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x++;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// INY - Increment Y Register
void fam65xx_iny(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y++;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// DEX - Decrement X Register
void fam65xx_dex(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x--;
    fam65xx_set_nz_flags(cpu, cpu->x);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// DEY - Decrement Y Register
void fam65xx_dey(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y--;
    fam65xx_set_nz_flags(cpu, cpu->y);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// MEMORY INCREMENT/DECREMENT OPERATIONS
// ============================================================================

// INC - Increment Memory
void fam65xx_inc_zero_page(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_inc_zero_page_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_inc_absolute(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_inc_absolute_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// DEC - Decrement Memory
void fam65xx_dec_zero_page(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_dec_zero_page_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_dec_absolute(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_dec_absolute_x(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    FAM65XX_INTRA_CYCLE(cpu);
    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}
