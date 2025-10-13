#pragma once
/*
 * fam65xx_implementation.h - MOS 65xx Family CPU Core Implementation
 * 
 * This file contains the core implementation functions:
 * - PHI2 handlers for read/write operations
 * - Interrupt handling functions
 * - Opcode fetch and transition functions
 * - CPU tick function (main entry point)
 * - CPU initialization and reset functions
 * - API implementations
 */

#include "fam65xx_core.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

// Forward declaration for BRK handler
static bus_state_t op_brk(fam65xx_t* cpu, bus_state_t pins);

/* ============================================================================
 * PHI2 HANDLERS
 * ============================================================================
 * Split PHI2 handlers for reads and writes using register indices.
 * Both return pins with RDY bit cleared if the cycle should be halted.
 */

/* Centralized PHI2 read handler - handles memory reads during PHI2 phase
 * CPU read cycles can be halted by RDY signal, but VIC-II memory reads are always serviced
 *
 * Parameters:
 *   addr_reg: 16-bit register index containing the address to read from
 */
static bus_state_t fam65xx_phi2_read(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg) {
    uint16_t address;

    if (FAM65XX_GET_RDY(pins)) {
        address = cpu->reg16[addr_reg];
        BUS_SET_ADDR(pins, address);
    } else {
        address = BUS_GET_ADDR(pins);
    }

    /* Always perform memory read to service VIC-II even when CPU halted */
    uint8_t current_bus_data = BUS_GET_DATA(pins);
    uint8_t data = cpu->mem_read(cpu->mem_user_data, address, current_bus_data);
    BUS_SET_DATA(pins, data);
    
    return pins;
}

/* Centralized PHI2 write handler - handles memory writes during PHI2 phase
 * Write always proceeds (cannot be halted), but PHI1 can still halt
 *
 * Parameters:
 *   addr_reg: 16-bit register index containing the address to write to
 *   reg_write: 8-bit register index to write from
 */
static bus_state_t fam65xx_phi2_write(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg, reg8_t reg_write) {
    uint16_t address = cpu->reg16[addr_reg];
    
    /* Write cycle - always proceeds regardless of RDY */
    BUS_SET_ADDR(pins, address);
    uint8_t data_byte = cpu->reg8[reg_write];
    BUS_SET_DATA(pins, data_byte);
    cpu->mem_write(cpu->mem_user_data, address, data_byte);
    
    return pins;
}

/* ============================================================================
 * INTERRUPT HANDLING FUNCTIONS
 * ============================================================================
 */

/* Helper function to get interrupt vector address based on BRK flags */
static inline uint16_t fam65xx_get_vector_addr(fam65xx_t* cpu) {
    if (cpu->brk_flags & FAM65XX_BRK_RESET) {
        return 0xFFFC;
    } else if (cpu->brk_flags & FAM65XX_BRK_NMI) {
        return 0xFFFA;
    } else {
        return 0xFFFE;  // BRK/IRQ vector
    }
}

/* Merged interrupt processing function - updates shift register and detects completion
 * Hardware accurate implementation with proper edge detection and I flag checking
 */
static inline bool fam65xx_process_interrupt_detection(fam65xx_t* cpu, bus_state_t pins) {
    /* Use intermediate variable to reduce memory accesses */
    uint32_t shift_reg = cpu->interrupt_shift_register;
    
    /* Shift the register left by one bit */
    shift_reg <<= 1;
    
    /* Sample IRQ line and insert into IRQ bits (active low) */
    if (!(pins & FAM65XX_IRQ)) {
        shift_reg |= (1 << INT_IRQ_START_BIT);
    }
    
    /* NMI Edge Detection - only trigger on falling edge */
    uint8_t nmi_current = (pins & FAM65XX_NMI) ? 1 : 0;
    if (cpu->nmi_prev && !nmi_current) {
        /* Falling edge detected - insert into NMI bits */
        shift_reg |= (1 << INT_NMI_START_BIT);
    }
    cpu->nmi_prev = nmi_current;
    
    /* Sample RESET line and insert into RESET bits (active low) */
    if (!(pins & FAM65XX_RES)) {
        shift_reg |= (1 << INT_RESET_START_BIT);
    }
    
    /* Clear separator bits to prevent cross-over */
    shift_reg &= ~INT_SEPARATOR_MASK;
    
    /* Store back the updated shift register */
    cpu->interrupt_shift_register = shift_reg;
    
    /* Check for completed interrupt sequences in order of priority */
    
    /* Check if RESET has completed shift (3 consecutive cycles) - highest priority */
    if ((shift_reg & INT_RESET_MASK) == INT_RESET_MASK) {
        cpu->brk_flags |= FAM65XX_BRK_RESET;
        return true;
    }
    
    /* Check if NMI has completed shift (3 consecutive cycles) - middle priority */
    if ((shift_reg & INT_NMI_MASK) == INT_NMI_MASK) {
        cpu->brk_flags |= FAM65XX_BRK_NMI;
        return true;
    }
    
    /* Check if IRQ has completed shift (3 consecutive cycles) - lowest priority */
    /* IRQ is masked by the I flag (interrupt disable) */
    if ((shift_reg & INT_IRQ_MASK) == INT_IRQ_MASK && !(CPU_P(cpu) & FLAG_I)) {
        cpu->brk_flags |= FAM65XX_BRK_IRQ;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * OPCODE FETCH AND TRANSITION FUNCTIONS
 * ============================================================================
 */

/* Opcode fetch handler - reads opcode and transitions to appropriate handler */
static bus_state_t fam65xx_opcode_fetch(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read opcode from PC */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    CPU_PC(cpu)++;
    
    /* PHI1: Decode opcode, increment PC, and set up next handler */
    CPU_IR(cpu) = BUS_GET_DATA(pins);
    
    /* Cache the opcode entry (copy once, accessed many times) */
    opcode_info_t opcode_entry = fam65xx_opcode_table[CPU_IR(cpu)];
    cpu->opcode_entry = opcode_entry;
    cpu->cycle_index = 0;
    
    /* Transition based on cached entry */
    int am_index = opcode_entry.am_index;
    if (am_index > AM_IMM) {
        /* Has addressing mode cycles */
        cpu->current_handler = fam65xx_addr_mode_table[am_index];
    } else {
        /* No addressing mode (AM_NON) or immediate mode (AM_IMM), go straight to operation */
        cpu->current_handler = fam65xx_op_handlers[opcode_entry.op_index];
    }
    
    return pins;
}

/* Transition from addressing mode to operation */
static void fam65xx_transition_to_operation(fam65xx_t* cpu) {
    cpu->cycle_index = 0;
    cpu->current_handler = fam65xx_op_handlers[cpu->opcode_entry.op_index];
}

/* Transition from operation back to opcode fetch */
static void fam65xx_transition_to_fetch(fam65xx_t* cpu) {
    cpu->cycle_index = 0;
    cpu->current_handler = fam65xx_opcode_fetch;
}


/* ============================================================================
 * CPU TICK FUNCTION
 * ============================================================================
 * This is the main entry point, called once per system cycle.
 * It executes PHI2, performs memory access, and calls PHI1.
 *
 * RDY checking is centralized here: writes ignore RDY, reads respect it.
 */

bus_state_t fam65xx_tick(fam65xx_t* cpu, bus_state_t pins) {
    /* ========================================================================
     * CENTRALIZED SYNC PIN MANAGEMENT
     * ========================================================================
     */
    
    /* SYNC is asserted during opcode fetch cycles (cycle 0 of instruction fetch) */
    if (cpu->current_handler == fam65xx_opcode_fetch && cpu->cycle_index == 0) {
        pins |= FAM65XX_SYNC;
    } else {
        pins &= ~FAM65XX_SYNC;
    }
    
    /* ========================================================================
     * CYCLE-BY-CYCLE INTERRUPT CHECKING - Hardware Accurate
     * ========================================================================
     */
    
    /* Update interrupt shift register every cycle for hardware-accurate detection */
    if (fam65xx_process_interrupt_detection(cpu, pins)) {
        /* Interrupt detected - check if we should hijack current instruction */
        if (cpu->current_handler != op_brk) {
            /* Not already in interrupt sequence - check for immediate interrupt or hijacking */
            if (cpu->brk_flags & FAM65XX_BRK_RESET) {
                /* RESET has highest priority - immediately start RESET sequence */
                fam65xx_reset(cpu, pins);
                return pins;
            } else if (cpu->current_handler == fam65xx_opcode_fetch && cpu->cycle_index == 0) {
                /* At instruction boundary - start interrupt sequence */
                cpu->current_handler = op_brk;
                cpu->cycle_index = 0;
                return pins;
            } else {
                /* Interrupt will be processed in next cycle */
            }
        }
    }
    
    /* ========================================================================
     * HANDLER EXECUTION - PHI2 calls embedded within each handler
     * ========================================================================
     */
    
    /* Call current handler - embeds PHI2 calls with halt checking */
    pins = cpu->current_handler(cpu, pins);
    
    return pins;
}

/* ============================================================================
 * CPU INITIALIZATION
 * ============================================================================
 */

bus_state_t fam65xx_init(fam65xx_t* cpu, const fam65xx_desc_t* desc) {
    memset(cpu, 0, sizeof(fam65xx_t));
    
    /* Set up memory callbacks */
    if (desc) {
        cpu->mem_read = desc->mem_read;
        cpu->mem_write = desc->mem_write;
        cpu->mem_user_data = desc->mem_user_data;
    }
    
    /* Initialize register layout:
     * ZP high byte (REG_ZPH) = 0x00 (always zero for zero page)
     * SP high byte (REG_SPH) = 0x01 (stack is always in page 1)
     */
    cpu->reg8[REG_ZPH] = 0x00;  /* Zero page high byte */
    cpu->reg8[REG_SPH] = 0x01;  /* Stack pointer high byte */
    
    /* Initialize interrupt state - shift register starts inactive (lines high) */
    cpu->interrupt_shift_register = 0xFFFFFFFF;  /* All bits high = inactive state */
    cpu->nmi_prev = 1;  /* NMI line starts high (inactive) for edge detection */
    
    /* Initialize default CPU state */
    CPU_P(cpu) = FLAG_U | FLAG_I;  /* Set unused and interrupt disable flags */
    CPU_S(cpu) = 0xFD;  /* Default stack pointer after reset */
    
    /* Set up default bus pins state */
    bus_state_t pins = 0;
    pins |= FAM65XX_RDY;   /* Set ready bit */
    pins |= FAM65XX_RW;    /* Set read mode as default state */
    pins |= FAM65XX_IRQ;   /* IRQ line starts HIGH (inactive) */
    pins |= FAM65XX_NMI;   /* NMI line starts HIGH (inactive) */
    pins |= FAM65XX_RES;   /* RESET line starts HIGH (inactive) */
    
    /* CPU is initialized but not executing - caller must call bootstrap or reset */
    cpu->current_handler = NULL;
    cpu->cycle_index = 0;
    
    return pins;
}

/* Start hardware RESET sequence for normal emulation */
bus_state_t fam65xx_reset(fam65xx_t* cpu, bus_state_t pins) {
    /* Initialize interrupt state - shift register starts inactive (lines high) */
    cpu->interrupt_shift_register = 0xFFFFFFFF;  /* All bits high = inactive state */
    cpu->nmi_prev = 1;  /* NMI line starts high (inactive) for edge detection */
    
    /* Set BRK flag to indicate RESET and start interrupt sequence */
    cpu->brk_flags |= FAM65XX_BRK_RESET;
    
    /* Switch to BRK handler starting from cycle 0 for full hardware sequence */
    cpu->current_handler = op_brk;
    cpu->cycle_index = 0;  /* Start from beginning to get proper stack decrements */

    /* Initialize CPU state for RESET - hardware accurate */
    CPU_P(cpu) = FLAG_U | FLAG_I;  /* Set unused and interrupt disable flags */
    /* Note: SP starts at $FF and will be decremented 3 times by BRK handler to $FD */
    CPU_S(cpu) = 0xFF;  /* Hardware starts at $FF, BRK sequence decrements to $FD */
    
    /* PC will be set by BRK handler after reading from RESET vector */
    return pins;
}

/* CPU state accessor functions */
void fam65xx_set_a(fam65xx_t* cpu, uint8_t v) { CPU_A(cpu) = v; }
void fam65xx_set_x(fam65xx_t* cpu, uint8_t v) { CPU_X(cpu) = v; }
void fam65xx_set_y(fam65xx_t* cpu, uint8_t v) { CPU_Y(cpu) = v; }
void fam65xx_set_s(fam65xx_t* cpu, uint8_t v) { CPU_S(cpu) = v; }
void fam65xx_set_p(fam65xx_t* cpu, uint8_t v) { CPU_P(cpu) = v; }
void fam65xx_set_pc(fam65xx_t* cpu, uint16_t v) { CPU_PC(cpu) = v; }

uint8_t fam65xx_a(fam65xx_t* cpu) { return CPU_A(cpu); }
uint8_t fam65xx_x(fam65xx_t* cpu) { return CPU_X(cpu); }
uint8_t fam65xx_y(fam65xx_t* cpu) { return CPU_Y(cpu); }
uint8_t fam65xx_s(fam65xx_t* cpu) { return CPU_S(cpu); }
uint8_t fam65xx_p(fam65xx_t* cpu) { return CPU_P(cpu); }
uint16_t fam65xx_pc(fam65xx_t* cpu) { return CPU_PC(cpu); }

/* Instruction completion detection */
bool fam65xx_opdone(fam65xx_t* cpu) {
    /* Instruction is complete when we're at the fetch handler with cycle_index 0 */
    return (cpu->current_handler == fam65xx_opcode_fetch) && (cpu->cycle_index == 0);
}

/* Bootstrap function for test runner initialization */
bus_state_t fam65xx_bootstrap(fam65xx_t* cpu, bus_state_t pins) {
    /* Set up for immediate instruction execution without RESET sequence */
    pins |= FAM65XX_RDY;   /* Ensure RDY is high for execution */
    pins |= FAM65XX_RW;    /* Ensure RW is set as default state */
    pins |= FAM65XX_IRQ;   /* IRQ line high (inactive) */
    pins |= FAM65XX_NMI;   /* NMI line high (inactive) */
    pins |= FAM65XX_RES;   /* RESET line high (inactive) */
    
    /* Clear any interrupt flags that might have been set */
    cpu->brk_flags = 0;
    
    /* Set up for instruction fetch - CPU ready to execute next instruction */
    cpu->cycle_index = 0;
    cpu->current_handler = fam65xx_opcode_fetch;
    
    return pins;
}


#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif