#pragma once
/*
 * fam65xx_modular.h - MOS 65xx Family CPU Emulator (Modular Version)
 * 
 * This is the main include file for the modular version of the PHI2 split
 * architecture CPU emulator. It includes all component files in the correct
 * order to minimize forward declarations.
 * 
 * ARCHITECTURE OVERVIEW:
 * ======================
 * 
 * 1. PIN-BASED BUS SYSTEM
 *    - All chips interact via a shared 64-bit bus state (bus_state_t)
 *    - Bus contains address lines, data lines, and control signals
 *    - Each chip has pin-specific macros to access only its pins
 *    - Generic bus macros for address/data/R/W̅ shared by all
 * 
 * 2. REGISTER ARRAY WITH 16-BIT OVERLAYS
 *    - Registers stored as union of uint8_t[16] and uint16_t[8]
 *    - PC, AD (address latch), and SP are 16-bit pairs on little-endian hosts
 *    - Stack pointer high byte (SPH) is always 0x01, enabling 16-bit SP access
 * 
 * 3. DIRECT PHI2 CALLS
 *    - Each handler makes direct PHI2 calls using register indices
 *    - Split PHI2 handlers: fam65xx_phi2_read() and fam65xx_phi2_write()
 * 
 * 4. ENUM-BASED OPCODE ENCODING
 *    - Opcode table is 256 × 2 bytes with bit fields
 *    - am_index (4 bits), page_cross (1 bit), _reserved (3 bits), rmw (1 bit), op_index (7 bits)
 *    - Addressing modes and operations are separate, combinable
 *    - Flags enable cycle skipping and mode selection
 * 
 * 5. CALLBACK-DRIVEN EXECUTION
 *    - CPU stores current handler function pointer
 *    - Each handler explicitly transitions to next handler
 *    - Opcode fetch → addressing mode → op_index → opcode fetch
 *    - No phase tracking needed - callback pointer IS the phase
 * 
 * 6. SPLIT PHI2 RDY HANDLING
 *    - fam65xx_phi2_read() respects RDY signal
 *    - fam65xx_phi2_write() always proceeds but checks RDY for PHI1 halt
 *    - Handlers check RDY bit after PHI2 calls and return early if set
 * 
 * 7. PAGE CROSS OPTIMIZATION
 *    - Fast detection: (addr1 ^ addr2) & 0x100
 *    - Addressing modes can skip cycle if page_cross flag set and no cross
 *    - Writes never use page_cross flag (always take full cycles)
 * 
 * 8. RMW OPERATION SUPPORT
 *    - Single handler works for both accumulator and memory modes
 *    - RMW flag in opcode entry selects behavior
 *    - Operation logic shared, only register target differs
 * 
 * 9. HARDWARE-ACCURATE TIMING
 *    - Each handler performs exactly one PHI2 access per cycle
 *    - PHI2 calls embedded within handlers with immediate halt checking
 *    - Proper RDY/halt behavior for VIC-II bus arbitration compatibility
 *    - Each system tick = handler execution (PHI2 + memory + PHI1)
 * 
 * MODULAR FILE ORGANIZATION:
 * ==========================
 * 
 * This modular version splits the original monolithic file into logical
 * components for better maintainability:
 * 
 * 1. fam65xx_core.h - Core definitions, CPU state, register mappings, API declarations
 * 3. fam65xx_addrmodes.h - Addressing mode handlers (am_* functions)
 * 9. fam65xx_impl.h - Core implementation (PHI2 handlers, interrupts, tick, init)
 * 4. fam65xx_ops.h - Arithmetic, bit test, branch, compare operations
 * 8. fam65xx_ops_illegal.h - Illegal/undocumented opcodes (LAX, SAX, etc.)
 * 5. fam65xx_ops_part2.h - Control flow and flag operations  
 * 6. fam65xx_ops_part3.h - Load, logic, register, stack, store, transfer operations
 * 7. fam65xx_ops_rmw.h - Read-modify-write operations (ASL, LSR, ROL, ROR, INC, DEC, illegal RMW)
 * 2. fam65xx_tables.h - Lookup tables, enums, opcode table (256 entries)  
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory interface must be implemented by system */
extern uint8_t memory_read(uint16_t addr);
extern void memory_write(uint16_t addr, uint8_t data);

/* Include all modular components in dependency order */

/* 1. Core definitions - must be first (defines types and constants) */
#include "fam65xx_core.h"

/* 2. Addressing mode handlers - use core definitions only */
#include "fam65xx_addrmodes.h"

/* 3. Operation handlers - use core definitions and utility functions */
#include "fam65xx_ops.h"
#include "fam65xx_ops_part2.h"
#include "fam65xx_ops_part3.h"

/* 4. RMW operations - use core definitions and utility functions */
#include "fam65xx_ops_rmw.h"

/* 5. Illegal operations - use core definitions and branch helper */
#include "fam65xx_ops_illegal.h"

/* 6. Tables and enums - defines operation/addressing mode enums and tables */
#include "fam65xx_tables.h"

/* 7. Implementation functions - use tables and operation handlers */
#include "fam65xx_impl.h"

/* Compatibility aliases for test runner integration */
#define cpu_init fam65xx_init
#define cpu_tick fam65xx_tick

#ifdef __cplusplus
}
#endif