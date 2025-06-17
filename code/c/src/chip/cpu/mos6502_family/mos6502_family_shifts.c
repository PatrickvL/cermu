#include "mos6502_family_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY SHIFT OPERATIONS
// ============================================================================

// ============================================================================
// SHIFT LEFT - ASL (using inline RMW helpers for performance)
// ============================================================================

// ASL - Arithmetic Shift Left (Accumulator)
void mos6502_family_asl_accumulator(mos6502_family_t* cpu) {
    mos6502_family_rmw_accumulator(cpu, mos6502_family_op_asl);
}

// ASL - Arithmetic Shift Left (Zero Page)
void mos6502_family_asl_zero_page(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page(cpu, mos6502_family_op_asl);
}

void mos6502_family_asl_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page_x(cpu, mos6502_family_op_asl);
}

void mos6502_family_asl_absolute(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute(cpu, mos6502_family_op_asl);
}

void mos6502_family_asl_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute_x(cpu, mos6502_family_op_asl);
}
    
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_asl_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_asl_absolute_x(mos6502_family_t* cpu) {
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
    
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// LOGICAL SHIFT RIGHT - LSR
// ============================================================================

// LSR - Logical Shift Right (Accumulator)
void mos6502_family_lsr_accumulator(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    
    mos6502_family_set_flag(cpu, FLAG_C, (cpu->a & 0x01) != 0);
    cpu->a >>= 1;
    mos6502_family_set_nz_flags(cpu, cpu->a);
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// LSR - Logical Shift Right (Zero Page)
void mos6502_family_lsr_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lsr_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lsr_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_lsr_absolute_x(mos6502_family_t* cpu) {
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
    
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// ROTATE LEFT - ROL
// ============================================================================

// ROL - Rotate Left (Accumulator)
void mos6502_family_rol_accumulator(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (cpu->a & 0x80) != 0);
    cpu->a = (cpu->a << 1) | (old_carry ? 1 : 0);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ROL - Rotate Left (Zero Page)
void mos6502_family_rol_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (old_carry ? 1 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_rol_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (old_carry ? 1 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_rol_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (old_carry ? 1 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_rol_absolute_x(mos6502_family_t* cpu) {
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
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (old_carry ? 1 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// ROTATE RIGHT - ROR
// ============================================================================

// ROR - Rotate Right (Accumulator)
void mos6502_family_ror_accumulator(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (cpu->a & 0x01) != 0);
    cpu->a = (cpu->a >> 1) | (old_carry ? 0x80 : 0);
    mos6502_family_set_nz_flags(cpu, cpu->a);
    
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ROR - Rotate Right (Zero Page)
void mos6502_family_ror_zero_page(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ror_zero_page_x(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ror_absolute(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Dummy write
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

void mos6502_family_ror_absolute_x(mos6502_family_t* cpu) {
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
    
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}
