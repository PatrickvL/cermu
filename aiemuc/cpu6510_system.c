#include "cpu6510.h"
#include "bus.h"
#include "c64.h"

// ============================================================================
// MOS 6510 TRANSFER AND SYSTEM INSTRUCTIONS
// ============================================================================

// TAX - Transfer Accumulator to X (0xAA)
void tax_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, tax_wait);  // Dummy read
    cpu.x = cpu.a;
    cpu_set_zn(cpu.x);
    NEXT_INSTRUCTION(tax_fetch_wait);
}

// TXA - Transfer X to Accumulator (0x8A)
void txa_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, txa_wait);  // Dummy read
    cpu.a = cpu.x;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(txa_fetch_wait);
}

// TAY - Transfer Accumulator to Y (0xA8)
void tay_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, tay_wait);  // Dummy read
    cpu.y = cpu.a;
    cpu_set_zn(cpu.y);
    NEXT_INSTRUCTION(tay_fetch_wait);
}

// TYA - Transfer Y to Accumulator (0x98)
void tya_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, tya_wait);  // Dummy read
    cpu.a = cpu.y;
    cpu_set_zn(cpu.a);
    NEXT_INSTRUCTION(tya_fetch_wait);
}

// TSX - Transfer Stack Pointer to X (0xBA)
void tsx_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, tsx_wait);  // Dummy read
    cpu.x = cpu.sp;
    cpu_set_zn(cpu.x);
    NEXT_INSTRUCTION(tsx_fetch_wait);
}

// TXS - Transfer X to Stack Pointer (0x9A)
void txs_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, txs_wait);  // Dummy read
    cpu.sp = cpu.x;
    NEXT_INSTRUCTION(txs_fetch_wait);
}

// NOP variations - No Operation
void nop_zp_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, nop_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, nop_zp_wait2);  // Read and discard
    NEXT_INSTRUCTION(nop_zp_fetch_wait);
}

void nop_zp_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, nop_zp_x_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, nop_zp_x_wait2); // Dummy read
    cpu.addr_abs = (cpu.addr_abs + cpu.x) & 0xFF;
    WAIT_READY_THEN_READ(cpu.addr_abs, nop_zp_x_wait3);  // Read and discard
    NEXT_INSTRUCTION(nop_zp_x_fetch_wait);
}

void nop_abs_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, nop_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, nop_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, nop_abs_wait3);  // Read and discard
    NEXT_INSTRUCTION(nop_abs_fetch_wait);
}

void nop_abs_x_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, nop_abs_x_wait1);
    cpu.lo = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, nop_abs_x_wait2);
    cpu.hi = bus_state.data;
    cpu.addr_abs = (cpu.hi << 8) | cpu.lo;

    if ((cpu.addr_abs & 0xFF00) != ((cpu.addr_abs + cpu.x) & 0xFF00)) {
        // Page crossed - extra cycle
        WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, nop_abs_x_wait3);
    }

    WAIT_READY_THEN_READ(cpu.addr_abs + cpu.x, nop_abs_x_wait4);
    NEXT_INSTRUCTION(nop_abs_x_fetch_wait);
}

void nop_imm_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, nop_imm_wait);  // Read and discard immediate value
    NEXT_INSTRUCTION(nop_imm_fetch_wait);
}