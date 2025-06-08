#include "mos6510.h"

// Macros for register transfer/inc/dec instructions
#define DEFINE_REG_XFER(fn, dest, src) \
    void fn(mos6510_t* cpu) { mos6510_register_transfer_with_flags(cpu, &cpu->dest, cpu->src); }
#define DEFINE_REG_XFER_NOFLAG(fn, dest, src) \
    void fn(mos6510_t* cpu) { mos6510_register_transfer_no_flags(cpu, &cpu->dest, cpu->src); }
#define DEFINE_REG_INCDEC(fn, reg, delta) \
    void fn(mos6510_t* cpu) { mos6510_register_inc_dec(cpu, &cpu->reg, delta); }

// ============================================================================
// MOS 6510 REGISTER INSTRUCTIONS
// ============================================================================
// Register transfers, increments, and decrements

DEFINE_REG_INCDEC(inx_func, x, 1) // INX - Increment X Register (0xE8)
DEFINE_REG_INCDEC(iny_func, y, 1) // INY - Increment Y Register (0xC8)
DEFINE_REG_INCDEC(dex_func, x, -1) // DEX - Decrement X Register (0xCA)
DEFINE_REG_INCDEC(dey_func, y, -1) // DEY - Decrement Y Register (0x88)

// INC - Increment Memory
void inc_zero_page_func(mos6510_t* cpu) {
    mos6510_rmw_zero_page(cpu, op_inc);
}

void inc_zero_page_x_func(mos6510_t* cpu) {
    mos6510_rmw_zero_page_x(cpu, op_inc);
}

void inc_absolute_func(mos6510_t* cpu) {
    mos6510_rmw_absolute(cpu, op_inc);
}

void inc_absolute_x_func(mos6510_t* cpu) {
    mos6510_rmw_absolute_x(cpu, op_inc);
}

// DEC - Decrement Memory
void dec_zero_page_func(mos6510_t* cpu) {
    mos6510_rmw_zero_page(cpu, op_dec);
}

void dec_zero_page_x_func(mos6510_t* cpu) {
    mos6510_rmw_zero_page_x(cpu, op_dec);
}

void dec_absolute_func(mos6510_t* cpu) {
    mos6510_rmw_absolute(cpu, op_dec);  
}

void dec_absolute_x_func(mos6510_t* cpu) {
    mos6510_rmw_absolute_x(cpu, op_dec);
}

// Transfer Instructions
DEFINE_REG_XFER(tax_func, x, a) // TAX - Transfer A to X (0xAA)
DEFINE_REG_XFER(tay_func, y, a)  // TAY - Transfer A to Y (0xA8)
DEFINE_REG_XFER(tsx_func, x, sp) // TSX - Transfer Stack Pointer to X (0xBA)
DEFINE_REG_XFER(txa_func, a, x) // TXA - Transfer X to A (0x8A)
DEFINE_REG_XFER_NOFLAG(txs_func, sp, x) // TXS - Transfer X to Stack Pointer (0x9A)
DEFINE_REG_XFER(tya_func, a, y) // TYA - Transfer Y to A (0x98)
