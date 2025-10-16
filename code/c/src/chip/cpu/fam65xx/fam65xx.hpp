#pragma once
/*
 * fam65xx_modular.hpp - MOS 65xx Family CPU Emulator (Template-Enhanced Version) (C++ Version)
 *
 * This is the main include file for the modular version of the PHI2 split
 * architecture CPU emulator with template-based processor variant support.
 * It includes all component files in the correct order to minimize forward declarations.
 *
 * TEMPLATE ENHANCEMENT OVERVIEW:
 * ==============================
 *
 * This version adds compile-time processor variant support through C++ templates
 * while maintaining full backward compatibility with the existing proven implementation.
 *
 * SUPPORTED PROCESSOR VARIANTS:
 * - MOS 6502 (Original NMOS with illegal opcodes)
 * - MOS 6510 (C64 variant with I/O port)
 * - WDC 65C02 (CMOS version with enhanced instructions)
 * - Rockwell R65C02 (CMOS with bit manipulation)
 * - WDC 65C816 (16-bit extension)
 *
 * TEMPLATE FEATURES:
 * - Compile-time processor feature selection
 * - Zero runtime overhead for processor differences
 * - Processor-specific opcode tables and behaviors
 * - Single source compatibility across all variants
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
 * components for better maintainability while minimizing forward declarations:
 *
 * 1. fam65xx_core.hpp - Core definitions, CPU state, register mappings, API declarations
 * 2. fam65xx_tables.hpp - Includes ALL operation files and defines lookup tables
 *    ├─ fam65xx_addrmodes.hpp - Addressing mode handlers (am_* functions)
 *    ├─ fam65xx_ops.hpp - Arithmetic, bit test, branch, compare operations
 *    ├─ fam65xx_ops_part2.hpp - Control flow and flag operations
 *    ├─ fam65xx_ops_part3.hpp - Load, logic, register, stack, store, transfer operations
 *    ├─ fam65xx_ops_rmw.hpp - Read-modify-write operations (ASL, LSR, ROL, ROR, INC, DEC, illegal RMW)
 *    └─ fam65xx_ops_illegal.hpp - Illegal/undocumented opcodes (LAX, SAX, etc.)
 * 3. fam65xx_impl.hpp - Core implementation (PHI2 handlers, interrupts, tick, init)
 *
 * This structure eliminates forward declarations by having fam65xx_tables.hpp include
 * all operation implementations before defining the lookup tables that reference them.
 */

#include <cstdint>
#include <cstdbool>

/* Include all modular components in dependency order to minimize forward declarations */

/* 1. Core definitions - includes types, utilities, and API declarations */
#ifdef __cplusplus
extern "C" {
#endif

// Include core type definitions
#include "fam65xx_types.hpp"

// Include utility functions
#include "fam65xx_utils.hpp"

// ============================================================================
// API Function Declarations
// ============================================================================

/* Main API functions (callback-based for test runner compatibility) */
bus_state_t fam65xx_init(fam65xx_t* cpu, const fam65xx_desc_t* desc);
bus_state_t fam65xx_reset(fam65xx_t* cpu, bus_state_t pins);
bus_state_t fam65xx_tick(fam65xx_t* cpu, bus_state_t pins);
bool fam65xx_opdone(fam65xx_t* cpu);
bus_state_t fam65xx_bootstrap(fam65xx_t* cpu, bus_state_t pins);

/* CPU state accessor functions (for test runner compatibility) */
void fam65xx_set_a(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_x(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_y(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_s(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_p(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_pc(fam65xx_t* cpu, uint16_t v);

uint8_t fam65xx_a(fam65xx_t* cpu);
uint8_t fam65xx_x(fam65xx_t* cpu);
uint8_t fam65xx_y(fam65xx_t* cpu);
uint8_t fam65xx_s(fam65xx_t* cpu);
uint8_t fam65xx_p(fam65xx_t* cpu);
uint16_t fam65xx_pc(fam65xx_t* cpu);

/* Processor-specific table access functions */
opcode_info_t fam65xx_get_opcode_entry(uint8_t opcode);
cycle_fn_t fam65xx_get_addr_mode_handler(int am_index);

#ifdef __cplusplus
}
#endif

/* 2. Tables and enums - includes ALL operation and addressing mode handlers internally
 *    This eliminates the need for forward declarations by including implementations
 *    before defining the lookup tables that reference them.
 *    NOTE: This contains C++ templates, so it's outside extern "C" */
#include "fam65xx_tables.hpp"

/* 3. Template variants - provides processor-specific CPU template classes */
#include "fam65xx_variants.hpp"

/* 4. Implementation functions - use tables and operation handlers */
#ifdef __cplusplus
extern "C" {
#endif

#include "fam65xx_impl.hpp"

#ifdef __cplusplus
}
#endif