#include "mos6502_family_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY REGISTER OPERATIONS
// ============================================================================

// ============================================================================
// TRANSFER OPERATIONS
// ============================================================================

// TAX - Transfer A to X
void mos6502_family_tax(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->a;
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// TAY - Transfer A to Y
void mos6502_family_tay(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y = cpu->a;
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// TXA - Transfer X to A
void mos6502_family_txa(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->x;
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// TYA - Transfer Y to A
void mos6502_family_tya(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = cpu->y;
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// TSX - Transfer Stack Pointer to X
void mos6502_family_tsx(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x = cpu->sp;
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// TXS - Transfer X to Stack Pointer
void mos6502_family_txs(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->sp = cpu->x;
    // TXS does not affect any flags
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STACK OPERATIONS
// ============================================================================

// PHA - Push Accumulator
void mos6502_family_pha(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_push(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// PLA - Pull Accumulator
void mos6502_family_pla(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = mos6502_family_pull(cpu);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// PHP - Push Processor Status
void mos6502_family_php(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_push(cpu, cpu->p | FLAG_B | FLAG_U); // B flag set when pushed by PHP
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// PLP - Pull Processor Status
void mos6502_family_plp(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->p = mos6502_family_pull(cpu);
    cpu->p |= FLAG_U; // Unused flag always set
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// INCREMENT/DECREMENT OPERATIONS
// ============================================================================

// INX - Increment X Register
void mos6502_family_inx(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x++;
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// INY - Increment Y Register
void mos6502_family_iny(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y++;
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// DEX - Decrement X Register
void mos6502_family_dex(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->x--;
    mos6502_family_set_nz_flags(cpu, cpu->x);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// DEY - Decrement Y Register
void mos6502_family_dey(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->y--;
    mos6502_family_set_nz_flags(cpu, cpu->y);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// MEMORY INCREMENT/DECREMENT OPERATIONS
// ============================================================================

// INC - Increment Memory
void mos6502_family_inc_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_inc_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_inc_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_inc_absolute_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value++;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// DEC - Decrement Memory
void mos6502_family_dec_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_dec_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_dec_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_dec_absolute_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    value--;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    mos6502_family_set_nz_flags(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}
