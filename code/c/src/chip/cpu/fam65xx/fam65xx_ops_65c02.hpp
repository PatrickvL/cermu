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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

// ============================================================================
// 65C02 ENHANCED OPERATIONS
// ============================================================================

// STZ - Store Zero
static inline bus_state_t op_stz(fam65xx_t* cpu, bus_state_t pins) {
    BUS_SET_DATA(pins, 0);
    return fam65xx_phi2_write(cpu, pins);
}

// TRB - Test and Reset Bits
static inline bus_state_t op_trb(fam65xx_t* cpu, bus_state_t pins) {
    const uint8_t data = BUS_GET_DATA(pins);
    const uint8_t result = data & ~cpu->A;
    
    // Set Z flag based on A & data
    cpu->P = (cpu->P & ~FLAG_Z) | ((cpu->A & data) ? 0 : FLAG_Z);
    
    BUS_SET_DATA(pins, result);
    return fam65xx_phi2_write(cpu, pins);
}

// TSB - Test and Set Bits
static inline bus_state_t op_tsb(fam65xx_t* cpu, bus_state_t pins) {
    const uint8_t data = BUS_GET_DATA(pins);
    const uint8_t result = data | cpu->A;
    
    // Set Z flag based on A & data
    cpu->P = (cpu->P & ~FLAG_Z) | ((cpu->A & data) ? 0 : FLAG_Z);
    
    BUS_SET_DATA(pins, result);
    return fam65xx_phi2_write(cpu, pins);
}

// PHX - Push X Register
static inline bus_state_t op_phx(fam65xx_t* cpu, bus_state_t pins) {
    BUS_SET_ADDR(pins, 0x0100 | cpu->S);
    BUS_SET_DATA(pins, cpu->X);
    cpu->S--;
    return fam65xx_phi2_write(cpu, pins);
}

// PHY - Push Y Register
static inline bus_state_t op_phy(fam65xx_t* cpu, bus_state_t pins) {
    BUS_SET_ADDR(pins, 0x0100 | cpu->S);
    BUS_SET_DATA(pins, cpu->Y);
    cpu->S--;
    return fam65xx_phi2_write(cpu, pins);
}

// PLX - Pull X Register
static inline bus_state_t op_plx(fam65xx_t* cpu, bus_state_t pins) {
    cpu->S++;
    BUS_SET_ADDR(pins, 0x0100 | cpu->S);
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    cpu->X = BUS_GET_DATA(pins);
    cpu->P = (cpu->P & ~(FLAG_N | FLAG_Z)) | (cpu->X & FLAG_N) | ((cpu->X == 0) ? FLAG_Z : 0);
    
    return fam65xx_fetch_next_op(cpu, pins);
}

// PLY - Pull Y Register
static inline bus_state_t op_ply(fam65xx_t* cpu, bus_state_t pins) {
    cpu->S++;
    BUS_SET_ADDR(pins, 0x0100 | cpu->S);
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    cpu->Y = BUS_GET_DATA(pins);
    cpu->P = (cpu->P & ~(FLAG_N | FLAG_Z)) | (cpu->Y & FLAG_N) | ((cpu->Y == 0) ? FLAG_Z : 0);
    
    return fam65xx_fetch_next_op(cpu, pins);
}

// WAI - Wait for Interrupt
static inline bus_state_t op_wai(fam65xx_t* cpu, bus_state_t pins) {
    // Set WAI state - CPU stops until interrupt occurs
    cpu->bcd_enabled = true;  // Reuse a flag to indicate WAI state
    return fam65xx_fetch_next_op(cpu, pins);
}

// STP - Stop
static inline bus_state_t op_stp(fam65xx_t* cpu, bus_state_t pins) {
    // Set STP state - CPU stops until reset
    cpu->jam_enabled = true;  // Reuse a flag to indicate STP state
    return pins;  // Stop execution
}

// ============================================================================
// ROCKWELL 65C02 BIT MANIPULATION OPERATIONS
// ============================================================================

// RMB0-RMB7 - Reset Memory Bit
#define DEFINE_RMB_OP(bit) \
static inline bus_state_t op_rmb##bit(fam65xx_t* cpu, bus_state_t pins) { \
    const uint8_t data = BUS_GET_DATA(pins); \
    BUS_SET_DATA(pins, data & ~(1 << (bit))); \
    return fam65xx_phi2_write(cpu, pins); \
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
    const uint8_t data = BUS_GET_DATA(pins); \
    BUS_SET_DATA(pins, data | (1 << (bit))); \
    return fam65xx_phi2_write(cpu, pins); \
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
    const uint8_t data = BUS_GET_DATA(pins); \
    if (!(data & (1 << (bit)))) { \
        /* Bit is reset, take branch */ \
        return fam65xx_branch_taken(cpu, pins); \
    } else { \
        /* Bit is set, branch not taken */ \
        return fam65xx_fetch_next_op(cpu, pins); \
    } \
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
    const uint8_t data = BUS_GET_DATA(pins); \
    if (data & (1 << (bit))) { \
        /* Bit is set, take branch */ \
        return fam65xx_branch_taken(cpu, pins); \
    } else { \
        /* Bit is reset, branch not taken */ \
        return fam65xx_fetch_next_op(cpu, pins); \
    } \
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
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    const uint8_t zp_addr = BUS_GET_DATA(pins);
    
    // Read low byte of indirect address
    BUS_SET_ADDR(pins, zp_addr);
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    const uint8_t addr_lo = BUS_GET_DATA(pins);
    
    // Read high byte of indirect address  
    BUS_SET_ADDR(pins, (zp_addr + 1) & 0xFF);
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    const uint8_t addr_hi = BUS_GET_DATA(pins);
    
    // Set final address
    const uint16_t final_addr = addr_lo | (addr_hi << 8);
    BUS_SET_ADDR(pins, final_addr);
    
    return pins;
}

// Zero Page Relative for BBR/BBS - nn,label  
static inline bus_state_t am_zpr(fam65xx_t* cpu, bus_state_t pins) {
    // First fetch zero page address
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    const uint8_t zp_addr = BUS_GET_DATA(pins);
    
    // Read data at zero page address for bit test
    BUS_SET_ADDR(pins, zp_addr);
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    // Store the data for the bit test operation
    cpu->AD = BUS_GET_DATA(pins);
    
    // Now fetch the branch offset
    BUS_SET_ADDR(pins, cpu->PC++);
    pins = fam65xx_phi2_read(cpu, pins);
    if (!BUS_RDY(pins)) return pins;
    
    // Store branch offset for potential branch
    cpu->IR = BUS_GET_DATA(pins);
    
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif