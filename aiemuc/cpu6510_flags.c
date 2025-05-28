#include "cpu6510.h"
#include "bus.h"

// ============================================================================
// MOS 6510 FLAG INSTRUCTIONS
// ============================================================================

// CLC - Clear Carry Flag (0x18)
void clc_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, clc_wait);  // Dummy read
    cpu_dev->p &= ~FLAG_C;
    NEXT_INSTRUCTION(cpu_dev, clc_fetch_wait);
}

// SEC - Set Carry Flag (0x38)
void sec_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, sec_wait);  // Dummy read
    cpu_dev->p |= FLAG_C;
    NEXT_INSTRUCTION(cpu_dev, sec_fetch_wait);
}

// CLI - Clear Interrupt Disable Flag (0x58)
void cli_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, cli_wait);  // Dummy read
    cpu_dev->p &= ~FLAG_I;
    NEXT_INSTRUCTION(cpu_dev, cli_fetch_wait);
}

// SEI - Set Interrupt Disable Flag (0x78)
void sei_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, sei_wait);  // Dummy read
    cpu_dev->p |= FLAG_I;
    NEXT_INSTRUCTION(cpu_dev, sei_fetch_wait);
}

// CLD - Clear Decimal Mode Flag (0xD8)
void cld_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, cld_wait);  // Dummy read
    cpu_dev->p &= ~FLAG_D;
    NEXT_INSTRUCTION(cpu_dev, cld_fetch_wait);
}

// SED - Set Decimal Mode Flag (0xF8)
void sed_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, sed_wait);  // Dummy read
    cpu_dev->p |= FLAG_D;
    NEXT_INSTRUCTION(cpu_dev, sed_fetch_wait);
}

// CLV - Clear Overflow Flag (0xB8)
void clv_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, clv_wait);  // Dummy read
    cpu_dev->p &= ~FLAG_V;
    NEXT_INSTRUCTION(cpu_dev, clv_fetch_wait);
}
