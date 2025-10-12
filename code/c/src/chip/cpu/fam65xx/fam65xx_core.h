#pragma once
/*
 * fam65xx_core.h - MOS 65xx Family CPU Core Definitions and API
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
 */

#include <stdint.h>
#include <stdbool.h>

// Include system-wide bus definitions
#include "../../../core/aiemuc.h"
#include "../../../core/system_lines.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Bus State and Pin Definitions
// ============================================================================

// Legacy pin compatibility - map to system_lines.h definitions
#define FAM65XX_RW      BUS_BIT(BUS_RW_BIT)
#define FAM65XX_SYNC    BUS_BIT(BUS_SYNC_BIT)
#define FAM65XX_IRQ     BUS_BIT(BUS_IRQ_BIT)
#define FAM65XX_NMI     BUS_BIT(BUS_NMI_BIT)
#define FAM65XX_RDY     BUS_BIT(BUS_RDY_BIT)
#define FAM65XX_RES     BUS_BIT(BUS_RES_BIT)

// Legacy compatibility macros for FAM65XX bus access
#define FAM65XX_GET_ADDR(p) BUS_GET_ADDR(p)
#define FAM65XX_SET_ADDR(p, d) BUS_SET_ADDR(p, d)
#define FAM65XX_GET_DATA(p) BUS_GET_DATA(p)
#define FAM65XX_SET_DATA(p, d) BUS_SET_DATA(p, d)

// CPU pin access using project definitions
#define FAM65XX_GET_RDY(pins)      ((pins) & FAM65XX_RDY)
#define FAM65XX_SET_SYNC(pins, v)  ((pins) = ((v) ? ((pins) | FAM65XX_SYNC) : ((pins) & ~FAM65XX_SYNC)))
#define FAM65XX_GET_SYNC(pins)     ((pins) & FAM65XX_SYNC)
#define FAM65XX_GET_IRQ(pins)      ((pins) & FAM65XX_IRQ)
#define FAM65XX_GET_NMI(pins)      ((pins) & FAM65XX_NMI)

// ============================================================================
// CPU Flags
// ============================================================================

#define FLAG_C  0x01  // Carry
#define FLAG_Z  0x02  // Zero
#define FLAG_I  0x04  // Interrupt Disable
#define FLAG_D  0x08  // Decimal Mode
#define FLAG_B  0x10  // Break
#define FLAG_U  0x20  // Unused (always 1)
#define FLAG_V  0x40  // Overflow
#define FLAG_N  0x80  // Negative

// BRK flags for interrupt handling
#define FAM65XX_BRK_IRQ     (1<<0)
#define FAM65XX_BRK_NMI     (1<<1)
#define FAM65XX_BRK_RESET   (1<<2)

// Interrupt shift register bit layout - merged system (3 bits per interrupt + separators)
#define INT_IRQ_START_BIT   0   // IRQ uses bits 0-2 (3 bits)
#define INT_IRQ_SEP_BIT     3   // Separator bit after IRQ (bit 3)
#define INT_NMI_START_BIT   4   // NMI uses bits 4-6 (3 bits)
#define INT_NMI_SEP_BIT     7   // Separator bit after NMI (bit 7)
#define INT_RESET_START_BIT 8   // RESET uses bits 8-10 (3 bits)
#define INT_RESET_SEP_BIT   11  // Separator bit after RESET (bit 11)
#define INT_IRQ_MASK        (0x7 << INT_IRQ_START_BIT)     // 3 bits: 0b111
#define INT_NMI_MASK        (0x7 << INT_NMI_START_BIT)     // 3 bits: 0b111
#define INT_RESET_MASK      (0x7 << INT_RESET_START_BIT)   // 3 bits: 0b111
#define INT_SEPARATOR_MASK  ((1 << INT_IRQ_SEP_BIT) | (1 << INT_NMI_SEP_BIT) | (1 << INT_RESET_SEP_BIT))

// ============================================================================
// 8-bit Register indices with endian-aware 16-bit pairs
// ============================================================================

typedef enum {
    // 16-bit aligned register pairs (endian-aware) for memory addresses
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    REG_ZPL,       // Zero page (low byte) - full 16-bit zero page register
    REG_ZPH,       // Zero page (high byte) - always 0x00 for 6502/6510
    REG_SPL,       // Stack pointer (low byte) - full 16-bit stack register
    REG_SPH,       // Stack pointer (high byte) - always 0x01 for 6502/6510
    REG_ABL,       // Address Bus (low byte, even index for little endian)
    REG_ABH,       // Address Bus (high byte)
    REG_PCL,       // Program Counter (low byte, even index for little endian)
    REG_PCH,       // Program Counter (high byte)
#else
    REG_ZPH,       // Zero page (high byte) - always 0x00 for 6502/6510
    REG_ZPL,       // Zero page (low byte) - full 16-bit zero page register
    REG_SPH,       // Stack pointer (high byte) - always 0x01 for 6502/6510
    REG_SPL,       // Stack pointer (low byte) - full 16-bit stack register
    REG_ABH,       // Address Bus (high byte, even index for big endian)
    REG_ABL,       // Address Bus (low byte)
    REG_PCH,       // Program Counter (high byte, even index for big endian)
    REG_PCL,       // Program Counter (low byte)
#endif
    // Public registers
    REG_A,         // Accumulator
    REG_X,         // X index
    REG_Y,         // Y index
    REG_P,         // Processor status
    // Internal registers
    REG_IR,        // Instruction Register (current opcode)
    REG_DL,        // Data latch
    
    REG_COUNT,

    // Compatibility mapping for 8-bit stack pointer
    REG_S = REG_SPL  // Map legacy S register to SPL for compatibility
} reg8_t;

// 16-bit register indices (native endian compatible)
typedef enum {
    REG_ZP = REG_ZPL / 2,  // Zero page (16 bits) - full zero page register
    REG_SP = REG_SPL / 2,  // Stack pointer as 16-bit (SPL in low, 0x01 in high)
    REG_AB = REG_ABL / 2,  // Address Bus Latch as 16-bit (ADL/ADH pair)
    REG_PC = REG_PCL / 2,  // Program counter / PC as 16-bit (PCL/PCH pair)
} reg16_t;

// ============================================================================
// Opcode Encoding
// ============================================================================

typedef struct {
    uint16_t am_index    : 4;   // Addressing mode index (0-15, bits 0-3, nibble-aligned)
    uint16_t _reserved   : 3;   // Reserved bits (bits 4-6)
    uint16_t page_cross  : 1;   // Can skip cycle if no page cross (bit 7)
    uint16_t rmw         : 1;   // Read-Modify-Write op_index (bit 8)
    uint16_t op_index    : 7;   // Operation index (0-127, bits 9-15, byte-extractable with >> 9)
} opcode_info_t;

// ============================================================================
// CPU State
// ============================================================================

typedef struct fam65xx_t fam65xx_t;
typedef bus_state_t (*cycle_fn_t)(fam65xx_t* cpu, bus_state_t pins);

struct fam65xx_t {
    /* Register array - union allows both 8-bit and 16-bit access */
    union {
        uint8_t reg8[REG_COUNT];        /* 8-bit register access */
        uint16_t reg16[REG_COUNT / 2];  /* 16-bit pair access (little-endian) */
    };
    
    /* Current execution state */
    opcode_info_t opcode_entry;       /* Cached opcode entry (copied once) */
    uint8_t cycle_index;              /* Current cycle within instruction */
    cycle_fn_t current_handler;        /* Current PHI1 handler */
    
    /* Interrupt state - merged shift register system */
    uint32_t interrupt_shift_register; /* Combined shift register for all interrupt types */
    uint8_t brk_flags;                /* BRK/IRQ/NMI/RESET flags */
    uint8_t nmi_prev;                 /* Previous NMI line state for edge detection
                                       * NOTE: Consider storing complete previous pins state
                                       * for edge detection of all signals if needed in future */
};

/* Accessor macros for cleaner code */
#define CPU_ZP(cpu)    ((cpu)->reg16[REG_ZP])   /* Zero page address (ADL + 0x00xx) */
#define CPU_SP(cpu)    ((cpu)->reg16[REG_SP])   /* Stack pointer (SPL + 0x01xx) */
#define CPU_AB(cpu)    ((cpu)->reg16[REG_AB])   /* Address Bus Latch (ABL/ABH) */
#define CPU_PC(cpu)    ((cpu)->reg16[REG_PC])   /* Program Counter (PCL/PCH) */

/* Individual byte access - using the new register layout */
#define CPU_ADL(cpu)   ((cpu)->reg8[REG_ABL])     /* Address Bus Latch Low */
#define CPU_ADH(cpu)   ((cpu)->reg8[REG_ABH])     /* Address Bus Latch High */
#define CPU_PCL(cpu)   ((cpu)->reg8[REG_PCL])     /* Program Counter Low */
#define CPU_PCH(cpu)   ((cpu)->reg8[REG_PCH])     /* Program Counter High */

#define CPU_A(cpu)     ((cpu)->reg8[REG_A])
#define CPU_X(cpu)     ((cpu)->reg8[REG_X])
#define CPU_Y(cpu)     ((cpu)->reg8[REG_Y])
#define CPU_P(cpu)     ((cpu)->reg8[REG_P])
#define CPU_S(cpu)     ((cpu)->reg8[REG_SPL])
#define CPU_IR(cpu)    ((cpu)->reg8[REG_IR])
#define CPU_DL(cpu)    ((cpu)->reg8[REG_DL])

/* Legacy aliases for compatibility */
#define CPU_AD(cpu)    CPU_AB(cpu)  /* Address latch as 16-bit - now maps to AB */

// ============================================================================
// Utility Functions
// ============================================================================

/* Fast page cross detection using XOR and bit 8 check */
static inline bool fam65xx_page_crossed(uint16_t addr1, uint16_t addr2) {
    return (addr1 ^ addr2) & 0x0100;
}

/* Update N and Z flags based on value */
static inline void fam65xx_update_nz_flags(fam65xx_t* cpu, uint8_t value) {
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z)) |
                 (value & FLAG_N) |
                 (value == 0 ? FLAG_Z : 0);
}

// ============================================================================
// API Function Declarations
// ============================================================================

/* Forward declarations only for functions referenced in core.h itself */
#ifdef CHIPS_IMPL
/* These functions are needed by the API functions below - others are now included via tables */
static inline uint16_t fam65xx_get_vector_addr(fam65xx_t* cpu);
static void fam65xx_transition_to_operation(fam65xx_t* cpu);
static void fam65xx_transition_to_fetch(fam65xx_t* cpu);
#endif

/* Main API functions (PHI2 split implementation) */
void fam65xx_init(fam65xx_t* cpu);
bus_state_t fam65xx_tick(fam65xx_t* cpu, bus_state_t pins);

/* Memory interface (to be implemented by system) */
extern uint8_t memory_read(uint16_t addr);
extern void memory_write(uint16_t addr, uint8_t data);

/* Compatibility aliases for test runner integration */
#define cpu_init fam65xx_init
#define cpu_tick fam65xx_tick

/* Legacy compatibility aliases */
#define page_crossed fam65xx_page_crossed
#define update_nz_flags fam65xx_update_nz_flags

#ifdef __cplusplus
}
#endif