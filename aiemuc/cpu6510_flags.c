#include "cpu6510.h"
#include "bus.h"
#include "c64.h"

// ============================================================================
// MOS 6510 FLAG INSTRUCTIONS
// ============================================================================

// CLC - Clear Carry Flag (0x18)
void clc_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, clc_wait);  // Dummy read
    cpu.p &= ~FLAG_C;
    NEXT_INSTRUCTION(clc_fetch_wait);
}

// SEC - Set Carry Flag (0x38)
void sec_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, sec_wait);  // Dummy read
    cpu.p |= FLAG_C;
    NEXT_INSTRUCTION(sec_fetch_wait);
}

// CLI - Clear Interrupt Disable Flag (0x58)
void cli_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, cli_wait);  // Dummy read
    cpu.p &= ~FLAG_I;
    NEXT_INSTRUCTION(cli_fetch_wait);
}

// SEI - Set Interrupt Disable Flag (0x78)
void sei_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, sei_wait);  // Dummy read
    cpu.p |= FLAG_I;
    NEXT_INSTRUCTION(sei_fetch_wait);
}

// CLD - Clear Decimal Mode Flag (0xD8)
void cld_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, cld_wait);  // Dummy read
    cpu.p &= ~FLAG_D;
    NEXT_INSTRUCTION(cld_fetch_wait);
}

// SED - Set Decimal Mode Flag (0xF8)
void sed_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, sed_wait);  // Dummy read
    cpu.p |= FLAG_D;
    NEXT_INSTRUCTION(sed_fetch_wait);
}

// CLV - Clear Overflow Flag (0xB8)
void clv_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, clv_wait);  // Dummy read
    cpu.p &= ~FLAG_V;
    NEXT_INSTRUCTION(clv_fetch_wait);
}