#pragma once
/*
 * fam65xx_ops_65c02.hpp - 65C02 Enhanced Operations
 *
 * This file implements the additional operations introduced in the 65C02:
 * - STZ (Store Zero)
 * - TRB/TSB (Test and Reset/Set Bits)
 * - PHX/PHY/PLX/PLY (Push/Pull X/Y)
 * - WAI/STP (Wait/Stop)
 * - Enhanced BIT instruction with immediate mode
 * - BRA (Branch Always)
 * - RMB/SMB (Reset/Set Memory Bit) - Rockwell variant
 * - BBR/BBS (Branch on Bit Reset/Set) - Rockwell variant
 */

#include "fam65xx_core.hpp"
#include "fam65xx_helpers.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

// ============================================================================
// 65C02 ENHANCED OPERATIONS
// ============================================================================

// Forward declarations for missing functions
static void fam65xx_transition_to_fetch(fam65xx_t* cpu);
static bus_state_t fam65xx_phi2_read(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg);
static bus_state_t fam65xx_phi2_write(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg, reg8_t data_reg);

// Helper macros for RDY checking and fetch transition
#define FAM65XX_RDY_CHECK(pins) FAM65XX_GET_RDY(pins)
#define FETCH_NEXT_OP(cpu, pins) do { fam65xx_transition_to_fetch(cpu); return pins; } while(0)

// STZ - Store Zero
static inline bus_state_t op_stz(fam65xx_t* cpu, bus_state_t pins) {
    CPU_DL(cpu) = 0;  // Store zero in data latch
    return fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
}

// TRB - Test and Reset Bits
static inline bus_state_t op_trb(fam65xx_t* cpu, bus_state_t pins) {
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    const uint8_t data = BUS_GET_DATA(pins);
    const uint8_t result = data & ~CPU_A(cpu);
    
    // Set Z flag based on A & data
    CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_Z) | ((CPU_A(cpu) & data) ? 0 : FLAG_Z);
    
    CPU_DL(cpu) = result;
    return fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
}

// TSB - Test and Set Bits
static inline bus_state_t op_tsb(fam65xx_t* cpu, bus_state_t pins) {
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    const uint8_t data = BUS_GET_DATA(pins);
    const uint8_t result = data | CPU_A(cpu);
    
    // Set Z flag based on A & data
    CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_Z) | ((CPU_A(cpu) & data) ? 0 : FLAG_Z);
    
    CPU_DL(cpu) = result;
    return fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
}

// PHX - Push X Register
static inline bus_state_t op_phx(fam65xx_t* cpu, bus_state_t pins) {
    CPU_AB(cpu) = CPU_SP(cpu);
    CPU_S(cpu)--;
    return fam65xx_phi2_write(cpu, pins, REG_AB, REG_X);
}

// PHY - Push Y Register
static inline bus_state_t op_phy(fam65xx_t* cpu, bus_state_t pins) {
    CPU_AB(cpu) = CPU_SP(cpu);
    CPU_S(cpu)--;
    return fam65xx_phi2_write(cpu, pins, REG_AB, REG_Y);
}

// PLX - Pull X Register
static inline bus_state_t op_plx(fam65xx_t* cpu, bus_state_t pins) {
    CPU_S(cpu)++;
    CPU_AB(cpu) = CPU_SP(cpu);
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    CPU_X(cpu) = BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    
    FETCH_NEXT_OP(cpu, pins);
}

// PLY - Pull Y Register
static inline bus_state_t op_ply(fam65xx_t* cpu, bus_state_t pins) {
    CPU_S(cpu)++;
    CPU_AB(cpu) = CPU_SP(cpu);
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    CPU_Y(cpu) = BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_Y(cpu));
    
    FETCH_NEXT_OP(cpu, pins);
}

// WAI - Wait for Interrupt
static inline bus_state_t op_wai(fam65xx_t* cpu, bus_state_t pins) {
    // Set WAI state - CPU stops until interrupt occurs
    cpu->brk_flags |= FAM65XX_BRK_IRQ;  // Mark as waiting for interrupt
    FETCH_NEXT_OP(cpu, pins);
}

// STP - Stop
static inline bus_state_t op_stp(fam65xx_t* cpu, bus_state_t pins) {
    // Set STP state - CPU stops until reset
    cpu->brk_flags |= FAM65XX_BRK_RESET;  // Mark as stopped
    return pins;  // Stop execution
}

// ============================================================================
// ROCKWELL 65C02 BIT MANIPULATION OPERATIONS
// ============================================================================

// RMB0-RMB7 - Reset Memory Bit
#define DEFINE_RMB_OP(bit) \
static inline bus_state_t op_rmb##bit(fam65xx_t* cpu, bus_state_t pins) { \
    pins = fam65xx_phi2_read(cpu, pins, REG_AB); \
    if (!FAM65XX_RDY_CHECK(pins)) return pins; \
    const uint8_t data = BUS_GET_DATA(pins); \
    CPU_DL(cpu) = data & ~(1 << (bit)); \
    return fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL); \
}

DEFINE_RMB_OP(0)
DEFINE_RMB_OP(1) 
DEFINE_RMB_OP(2)
DEFINE_RMB_OP(3)
DEFINE_RMB_OP(4)
DEFINE_RMB_OP(5)
DEFINE_RMB_OP(6)
DEFINE_RMB_OP(7)

// SMB0-SMB7 - Set Memory Bit
#define DEFINE_SMB_OP(bit) \
static inline bus_state_t op_smb##bit(fam65xx_t* cpu, bus_state_t pins) { \
    pins = fam65xx_phi2_read(cpu, pins, REG_AB); \
    if (!FAM65XX_RDY_CHECK(pins)) return pins; \
    const uint8_t data = BUS_GET_DATA(pins); \
    CPU_DL(cpu) = data | (1 << (bit)); \
    return fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL); \
}

DEFINE_SMB_OP(0)
DEFINE_SMB_OP(1)
DEFINE_SMB_OP(2) 
DEFINE_SMB_OP(3)
DEFINE_SMB_OP(4)
DEFINE_SMB_OP(5)
DEFINE_SMB_OP(6)
DEFINE_SMB_OP(7)

// BBR0-BBR7 - Branch on Bit Reset
#define DEFINE_BBR_OP(bit) \
static inline bus_state_t op_bbr##bit(fam65xx_t* cpu, bus_state_t pins) { \
    const uint8_t data = CPU_DL(cpu); \
    if (!(data & (1 << (bit)))) { \
        /* Bit is reset, take branch */ \
        int8_t offset = (int8_t)CPU_IR(cpu); \
        CPU_PC(cpu) += offset; \
    } \
    FETCH_NEXT_OP(cpu, pins); \
}

DEFINE_BBR_OP(0)
DEFINE_BBR_OP(1)
DEFINE_BBR_OP(2)
DEFINE_BBR_OP(3) 
DEFINE_BBR_OP(4)
DEFINE_BBR_OP(5)
DEFINE_BBR_OP(6)
DEFINE_BBR_OP(7)

// BBS0-BBS7 - Branch on Bit Set
#define DEFINE_BBS_OP(bit) \
static inline bus_state_t op_bbs##bit(fam65xx_t* cpu, bus_state_t pins) { \
    const uint8_t data = CPU_DL(cpu); \
    if (data & (1 << (bit))) { \
        /* Bit is set, take branch */ \
        int8_t offset = (int8_t)CPU_IR(cpu); \
        CPU_PC(cpu) += offset; \
    } \
    FETCH_NEXT_OP(cpu, pins); \
}

DEFINE_BBS_OP(0)
DEFINE_BBS_OP(1)
DEFINE_BBS_OP(2)
DEFINE_BBS_OP(3)
DEFINE_BBS_OP(4)
DEFINE_BBS_OP(5)
DEFINE_BBS_OP(6)
DEFINE_BBS_OP(7)

// ============================================================================
// ENHANCED ADDRESSING MODES FOR 65C02
// ============================================================================

// Zero Page Indirect - ($nn)
static inline bus_state_t am_zpi(fam65xx_t* cpu, bus_state_t pins) {
    // Fetch zero page address
    CPU_AB(cpu) = CPU_PC(cpu)++;
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    const uint8_t zp_addr = BUS_GET_DATA(pins);
    
    // Read low byte of indirect address
    CPU_AB(cpu) = zp_addr;
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    const uint8_t addr_lo = BUS_GET_DATA(pins);
    
    // Read high byte of indirect address
    CPU_AB(cpu) = (zp_addr + 1) & 0xFF;
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    const uint8_t addr_hi = BUS_GET_DATA(pins);
    
    // Set final address
    CPU_AB(cpu) = addr_lo | (addr_hi << 8);
    
    return pins;
}

// Zero Page Relative for BBR/BBS - nn,label
static inline bus_state_t am_zpr(fam65xx_t* cpu, bus_state_t pins) {
    // First fetch zero page address
    CPU_AB(cpu) = CPU_PC(cpu)++;
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    const uint8_t zp_addr = BUS_GET_DATA(pins);
    
    // Read data at zero page address for bit test
    CPU_AB(cpu) = zp_addr;
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    // Store the data for the bit test operation
    CPU_DL(cpu) = BUS_GET_DATA(pins);
    
    // Now fetch the branch offset
    CPU_AB(cpu) = CPU_PC(cpu)++;
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_RDY_CHECK(pins)) return pins;
    
    // Store branch offset for potential branch
    CPU_IR(cpu) = BUS_GET_DATA(pins);
    
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif