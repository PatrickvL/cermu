/*
 * MOS 6502 Cycle-Accurate Emulator
 * 
 * ARCHITECTURE OVERVIEW:
 * ======================
 * 
 * 1. PIN-BASED BUS SYSTEM
 *    - All chips interact via a shared 64-bit bus state (Pins)
 *    - Bus contains address lines, data lines, and control signals
 *    - Each chip has pin-specific macros to access only its pins
 *    - Generic bus macros for address/data/R/W̅ shared by all
 * 
 * 2. REGISTER ARRAY WITH 16-BIT OVERLAYS
 *    - Registers stored as union of uint8_t[16] and uint16_t[8]
 *    - PC, AD (address latch), and SP are 16-bit pairs on little-endian hosts
 *    - Stack pointer high byte (SPH) is always 0x01, enabling 16-bit SP access
 *    - Register index 0 (REG_DUMMY) marks read cycles in metadata
 * 
 * 3. METADATA-DRIVEN PHI2
 *    - Each cycle has metadata: address source + register index
 *    - Generic PHI2 handler computes address and sets R/W̅ from metadata
 *    - If register index is 0 (REG_DUMMY) → read cycle
 *    - If register index > 0 → write cycle using that register
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
 * 6. CENTRALIZED RDY CHECKING
 *    - cpu_tick checks RDY once: in read path only
 *    - Write cycles always proceed (ignore RDY)
 *    - PHI1 handlers never check RDY (already handled)
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
 *    - PHI2: Set up address, R/W̅, data (for writes)
 *    - Memory access: Read or write based on R/W̅
 *    - PHI1: Process result, update CPU state
 *    - Each system tick = one full cycle (PHI2 + memory + PHI1)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================================
 * BUS STATE AND PIN DEFINITIONS
 * ============================================================================
 * The bus is a 64-bit packed value containing all signal lines.
 * Generic bus macros access address/data/R/W̅ that all chips can see.
 * Chip-specific pin macros access control signals private to each chip.
 */

typedef uint64_t Pins;

/* Generic Bus Lines (shared by all chips) */
#define BUS_ADDR_SHIFT   0
#define BUS_ADDR_MASK    0x000000000000FFFF
#define BUS_DATA_SHIFT   16
#define BUS_DATA_MASK    0x0000000000FF0000
#define BUS_RW_SHIFT     24
#define BUS_RW_MASK      0x0000000001000000

#define BUS_GET_ADDR(pins)       ((uint16_t)(((pins) >> BUS_ADDR_SHIFT) & 0xFFFF))
#define BUS_SET_ADDR(pins, addr) ((pins) = ((pins) & ~BUS_ADDR_MASK) | (((uint64_t)(addr) & 0xFFFF) << BUS_ADDR_SHIFT))
#define BUS_GET_DATA(pins)       ((uint8_t)(((pins) >> BUS_DATA_SHIFT) & 0xFF))
#define BUS_SET_DATA(pins, data) ((pins) = ((pins) & ~BUS_DATA_MASK) | (((uint64_t)(data) & 0xFF) << BUS_DATA_SHIFT))
#define BUS_GET_RW(pins)         ((((pins) >> BUS_RW_SHIFT) & 1) != 0)
#define BUS_SET_RW(pins, val)    ((pins) = ((pins) & ~BUS_RW_MASK) | (((uint64_t)(val) & 1) << BUS_RW_SHIFT))

/* CPU-Specific Pins */
#define CPU_PIN_SYNC     25
#define CPU_PIN_RDY      26
#define CPU_PIN_IRQ      27
#define CPU_PIN_NMI      28

#define CPU_GET_RDY(pins)      (((pins) >> CPU_PIN_RDY) & 1)
#define CPU_SET_SYNC(pins, v)  ((pins) = ((pins) & ~(1ULL << CPU_PIN_SYNC)) | (((uint64_t)(v) & 1) << CPU_PIN_SYNC))
#define CPU_GET_SYNC(pins)     (((pins) >> CPU_PIN_SYNC) & 1)
#define CPU_GET_IRQ(pins)      (((pins) >> CPU_PIN_IRQ) & 1)
#define CPU_GET_NMI(pins)      (((pins) >> CPU_PIN_NMI) & 1)

/* VIC-II-Specific Pins (for multi-chip systems) */
#define VIC_PIN_BA       30
#define VIC_PIN_AEC      31

#define VIC_SET_BA(pins, v)    ((pins) = ((pins) & ~(1ULL << VIC_PIN_BA)) | (((uint64_t)(v) & 1) << VIC_PIN_BA))
#define VIC_SET_AEC(pins, v)   ((pins) = ((pins) & ~(1ULL << VIC_PIN_AEC)) | (((uint64_t)(v) & 1) << VIC_PIN_AEC))
#define VIC_GET_BA(pins)       (((pins) >> VIC_PIN_BA) & 1)
#define VIC_GET_AEC(pins)      (((pins) >> VIC_PIN_AEC) & 1)

/* ============================================================================
 * REGISTER ARRAY LAYOUT
 * ============================================================================
 * Registers are stored as a union of 8-bit and 16-bit arrays.
 * PC, AD, and SP are 16-bit pairs (little-endian host assumed).
 * Register 0 (REG_DUMMY) is unused and marks read cycles in metadata.
 * Stack pointer high byte is always 0x01, enabling direct 16-bit SP access.
 */

/* 8-bit register indices - arranged to align with 16-bit register pairs
 * Layout: ZP(0,1), SP(2,3), AB(4,5), PC(6,7), then others(8+)
 * REG_DUMMY is 0, which overlaps with ZP high byte (always 0x00)
 */
// ============================================================================
// 8-bit Register indices with endian-aware 16-bit pairs
// ============================================================================

enum {
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
};

// Compatibility mapping for 8-bit stack pointer
#define REG_S     REG_SPL  // Map legacy S register to SPL for compatibility
#define REG_DUMMY REG_SPL  // Never used as write source, marks read cycles (ZP high)

// 16-bit register indices (native endian compatible)
enum {
    REG_ZP = REG_ZPL / 2,  // Zero page (16 bits) - full zero page register
    REG_SP = REG_SPL / 2,  // Stack pointer as 16-bit (SPL in low, 0x01 in high)
    REG_AB = REG_ABL / 2,  // Address Bus Latch as 16-bit (ADL/ADH pair)
    REG_PC = REG_PCL / 2,  // Program counter / PC as 16-bit (PCL/PCH pair)
};

/* Processor status flags */
#define FLAG_C  0x01   /* Carry */
#define FLAG_Z  0x02   /* Zero */
#define FLAG_I  0x04   /* Interrupt disable */
#define FLAG_D  0x08   /* Decimal mode */
#define FLAG_B  0x10   /* Break command */
#define FLAG_U  0x20   /* Unused (always 1) */
#define FLAG_V  0x40   /* Overflow */
#define FLAG_N  0x80   /* Negative */

/* ============================================================================
 * ADDRESSING MODE AND OPERATION ENUMS
 * ============================================================================
 * Addressing modes describe how operands are fetched.
 * Operations describe what the CPU does with those operands.
 * These are combined in the opcode table to minimize redundancy.
 * 
 * AM_NON (0): No addressing mode handler needed
 *   - Used by: Implicit, Immediate, Accumulator, and Relative modes
 *   - These modes either have no operand, operand in next byte, or 
 *     operate directly on registers without memory access
 */

typedef enum {
    AM_NON = 0, /* No addressing handler (Implicit/Immediate/Accumulator/Relative) */
    AM_ZER,     /* Zero Page - operand at $00nn */
    AM_ZPX,     /* Zero Page,X - operand at ($00nn + X) & 0xFF */
    AM_ZPY,     /* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
    AM_ABS,     /* Absolute - operand at $nnnn */
    AM_ABX,     /* Absolute,X - operand at $nnnn + X */
    AM_ABY,     /* Absolute,Y - operand at $nnnn + Y */
    AM_IND,     /* Indirect - jump target at ($nnnn) */
    AM_INX,     /* Indexed Indirect - operand at (($nn + X) & 0xFF) */
    AM_INY,     /* Indirect Indexed - operand at ($nn) + Y */
    AM_COUNT
} AddrMode;

/* Aliases for documentation/clarity (all map to AM_NON) */
#define AM_IMP  AM_NON  /* Implied/Implicit - no operand */
#define AM_IMM  AM_NON  /* Immediate - operand is next byte */
#define AM_ACC  AM_NON  /* Accumulator - operate on A register */
#define AM_REL  AM_NON  /* Relative - branch offset */

typedef enum {
    OP_LDA, OP_LDX, OP_LDY,
    OP_STA, OP_STX, OP_STY,
    OP_ADC, OP_SBC,
    OP_AND, OP_ORA, OP_EOR,
    OP_CMP, OP_CPX, OP_CPY,
    OP_ASL, OP_LSR, OP_ROL, OP_ROR,
    OP_INC, OP_DEC,
    OP_INX, OP_INY, OP_DEX, OP_DEY,
    OP_TAX, OP_TAY, OP_TXA, OP_TYA, OP_TSX, OP_TXS,
    OP_PHA, OP_PHP, OP_PLA, OP_PLP,
    OP_BCC, OP_BCS, OP_BEQ, OP_BNE, OP_BMI, OP_BPL, OP_BVC, OP_BVS,
    OP_CLC, OP_SEC, OP_CLI, OP_SEI, OP_CLD, OP_SED, OP_CLV,
    OP_JMP, OP_JSR, OP_RTS, OP_RTI, OP_BRK,
    OP_BIT, OP_NOP, OP_JAM,
    // 65C02 enhancements
    OP_BRA,
    // Illegal opcodes - combination instructions
    OP_LAX, OP_SAX, OP_DCP, OP_ISC, OP_SLO, OP_RLA, OP_SRE, OP_RRA,
    // Illegal opcodes - special accumulator operations
    OP_ANC, OP_ASR, OP_ARR, OP_SBX,
    // Illegal opcodes - store with AND operations
    OP_SHA, OP_SHS, OP_SHX, OP_SHY, OP_LAS,
    // Illegal opcodes - special operations
    OP_XAA,

    OP_COUNT
} Operation;

/* ============================================================================
 * CYCLE METADATA
 * ============================================================================
 * Each cycle is described by metadata:
 * - addr_source: How to compute the address for this cycle
 * - reg_index: Which register to write (0 = read cycle)
 * 
 * The generic PHI2 handler uses this metadata to set up the bus.
 */

typedef struct {
    uint8_t addr_source;   /* Address computation method */
    uint8_t reg_index;     /* Register to write (0 = read) */
} CycleMetadata;

/* Hardware-accurate address sources for metadata
 * These correspond directly to 16-bit register indices and actual hardware
 * multiplexers that select which register drives the address bus during each cycle.
 */
#define ADDR_ZP          REG16_ZP   /* Zero Page (0x00xx + ADL) */
#define ADDR_STACK       REG16_SP   /* Stack Pointer (0x01xx + S) */
#define ADDR_ABL         REG16_AB   /* Address Bus Latch (16-bit computed address) */
#define ADDR_PC          REG16_PC   /* Program Counter */

/* Aliases for documentation purposes - all map to hardware address sources */
#define ADDR_PC_INC      ADDR_PC    /* PC (will be incremented in PHI1) */
#define ADDR_ABS         ADDR_ABL   /* Absolute addressing uses Address Bus Latch */
#define ADDR_ABS_X       ADDR_ABL   /* Absolute,X uses ABL (after X added in PHI1) */
#define ADDR_ABS_Y       ADDR_ABL   /* Absolute,Y uses ABL (after Y added in PHI1) */
#define ADDR_ZP_X        ADDR_ZP    /* Zero Page,X uses ZP (after X added to ADL) */
#define ADDR_ZP_Y        ADDR_ZP    /* Zero Page,Y uses ZP (after Y added to ADL) */
#define ADDR_IND_X       ADDR_ABL   /* Indexed Indirect uses ABL (computed address) */
#define ADDR_IND_Y       ADDR_ABL   /* Indirect Indexed uses ABL (computed address) */

/* ============================================================================
 * OPCODE ENCODING
 * ============================================================================
 * Opcode table is 256 entries × 2 bytes = 512 bytes.
 * Each entry packs addressing mode, op_index, and flags using bit fields.
 * 
 * page_cross: If set, addressing mode can skip a cycle if no page boundary crossed
 * rmw: If set, op_index uses Read-Modify-Write (3 cycles on memory)
 */

typedef struct {
    uint16_t am_index    : 4;   // Addressing mode index (0-15, bits 0-3, nibble-aligned)
    uint16_t _reserved   : 3;   // Reserved bits (bits 4-6)
    uint16_t page_cross  : 1;   // Can skip cycle if no page cross (bit 7)
    uint16_t rmw         : 1;   // Read-Modify-Write op_index (bit 8)
    uint16_t op_index    : 7;   // Operation index (0-127, bits 9-15, byte-extractable with >> 9)
} opcode_info_t;

/* ============================================================================
 * CPU STATE
 * ============================================================================
 * The CPU maintains:
 * - Register array (with 16-bit overlays for PC, AD, SP)
 * - Current opcode and cached opcode entry
 * - Current cycle index within instruction
 * - Current handler function and metadata array
 */

typedef struct CPU6502 CPU6502;
typedef Pins (*CycleFunc)(CPU6502* cpu, Pins pins);

struct CPU6502 {
    /* Register array - union allows both 8-bit and 16-bit access */
    union {
        uint8_t reg8[REG_COUNT];        /* 8-bit register access */
        uint16_t reg16[REG_COUNT / 2];  /* 16-bit pair access (little-endian) */
    };
    
    /* Current execution state */
    opcode_info_t opcode_entry;       /* Cached opcode entry (copied once) */
    uint8_t cycle_index;              /* Current cycle within instruction */
    CycleFunc current_handler;        /* Current PHI1 handler */
    CycleMetadata* current_metadata;  /* Current cycle metadata array */
    
    /* Cycle counter */
    uint64_t cycles;
};

/* Accessor macros for cleaner code */
#define CPU_ZP(cpu)    ((cpu)->reg16[REG_ZP])   /* Zero page address (ADL + 0x00xx) */
#define CPU_SP(cpu)    ((cpu)->reg16[REG_SP])   /* Stack pointer (SPL + 0x01xx) */
#define CPU_AB(cpu)    ((cpu)->reg16[REG_AB])   /* Address Bus Latch (ABL/ABH) */
#define CPU_PC(cpu)    ((cpu)->reg16[REG_PC])   /* Program Counter (PCL/PCH) */

/* Individual byte access - using the new register layout */
#define CPU_ADL(cpu)   ((cpu)->reg8[REG_ADL])     /* Address Bus Latch Low */
#define CPU_ADH(cpu)   ((cpu)->reg8[REG_ADH])     /* Address Bus Latch High */
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

/* ============================================================================
 * ADDRESSING MODE AND OPERATION DESCRIPTORS
 * ============================================================================
 * These tables describe each addressing mode and op_index.
 * They're indexed by the enum values to get handlers and metadata.
 */

typedef struct {
    CycleMetadata* metadata;
    CycleFunc handler;
} AddrModeDesc;

typedef struct {
    CycleMetadata* rmw_metadata;    /* For RMW mode (3 cycles) */
    CycleMetadata* normal_metadata; /* For register/normal mode */
    CycleFunc handler;
} OperationDesc;

/* Forward declarations */
static Pins opcode_fetch(CPU6502* cpu, Pins pins);
static void transition_to_operation(CPU6502* cpu);
static void transition_to_fetch(CPU6502* cpu);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/* Fast page cross detection using XOR and bit 8 check */
static inline bool page_crossed(uint16_t addr1, uint16_t addr2) {
    return (addr1 ^ addr2) & 0x0100;
}

/* Update N and Z flags based on value */
static inline void update_nz_flags(CPU6502* cpu, uint8_t value) {
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z)) |
                 (value & FLAG_N) |
                 (value == 0 ? FLAG_Z : 0);
}

/* Memory interface (to be implemented by system) */
extern uint8_t memory_read(uint16_t addr);
extern void memory_write(uint16_t addr, uint8_t data);


/* ============================================================================
 * ADDRESSING MODE HANDLERS
 * ============================================================================
 * These handlers prepare the address/data for operations.
 * They transition to the op_index handler when complete.
 */

/* Zero Page - operand at $00nn */
static CycleMetadata addr_zp_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY }
};

static Pins addr_zp(CPU6502* cpu, Pins pins) {
    CPU_ADL(cpu) = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    transition_to_operation(cpu);
    return pins;
}

/* Absolute - operand at $nnnn */
static CycleMetadata addr_abs_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },  /* Read low byte */
    { ADDR_PC_INC, REG_DUMMY }   /* Read high byte */
};

static Pins addr_abs(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
        case 1:
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page,X - operand at ($00nn + X) & 0xFF */
static CycleMetadata addr_zpx_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },  /* Read base address */
    { ADDR_ZP_X, REG_DUMMY }     /* Dummy read while adding X */
};

static Pins addr_zpx(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
        case 1:
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_X(cpu)) & 0xFF;
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
static CycleMetadata addr_zpy_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },
    { ADDR_ZP_Y, REG_DUMMY }
};

static Pins addr_zpy(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
        case 1:
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_Y(cpu)) & 0xFF;
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,X - operand at $nnnn + X (may skip cycle if no page cross) */
static CycleMetadata addr_abx_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },
    { ADDR_PC_INC, REG_DUMMY },
    { ADDR_ABS_X, REG_DUMMY }  /* Page cross penalty cycle */
};

static Pins addr_abx(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1: {
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            
            uint16_t base = CPU_AD(cpu);
            uint16_t effective = base + CPU_X(cpu);
            CPU_AD(cpu) = effective;
            
            /* Skip cycle 2 if page_cross flag set and no page crossed */
            if (cpu->opcode_entry.page_cross && !page_crossed(base, effective)) {
                transition_to_operation(cpu);
            }
            break;
        }
            
        case 2:
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,Y - operand at $nnnn + Y (may skip cycle if no page cross) */
static CycleMetadata addr_aby_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },
    { ADDR_PC_INC, REG_DUMMY },
    { ADDR_ABS_Y, REG_DUMMY }
};

static Pins addr_aby(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1: {
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            
            uint16_t base = CPU_AD(cpu);
            uint16_t effective = base + CPU_Y(cpu);
            CPU_AD(cpu) = effective;
            
            if (cpu->opcode_entry.page_cross && !page_crossed(base, effective)) {
                transition_to_operation(cpu);
            }
            break;
        }
            
        case 2:
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indirect - used only by JMP ($nnnn) */
static CycleMetadata addr_ind_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },  /* Read pointer low byte */
    { ADDR_PC_INC, REG_DUMMY },  /* Read pointer high byte */
    { ADDR_ABS, REG_DUMMY },     /* Read target low byte */
    { ADDR_ABS, REG_DUMMY }      /* Read target high byte */
};

static Pins addr_ind(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Read low byte of pointer address */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* Read high byte of pointer address */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 2:
            /* Read low byte of target address from (pointer) */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            
            /* IMPORTANT: 6502 bug - if pointer is at page boundary (e.g., $xxFF),
             * high byte is read from $xx00 instead of $(xx+1)00
             * To emulate this bug: increment only low byte for next read */
            CPU_ADL(cpu) = (CPU_ADL(cpu) + 1) & 0xFF;
            break;
            
        case 3:
            /* Read high byte of target address */
            CPU_ADL(cpu) = CPU_DL(cpu);
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indexed Indirect - operand at (($nn + X) & 0xFF) */
static CycleMetadata addr_idx_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },  /* Read pointer */
    { ADDR_ZP, REG_DUMMY },      /* Dummy read */
    { ADDR_ZP_X, REG_DUMMY },    /* Read low byte of target */
    { ADDR_ZP_X, REG_DUMMY }     /* Read high byte of target */
};

static Pins addr_idx(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
        case 1:
            /* Dummy cycle */
            break;
        case 2:
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_X(cpu) + 1) & 0xFF;
            break;
        case 3:
            CPU_ADL(cpu) = CPU_DL(cpu);
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indirect Indexed - operand at ($nn) + Y (may skip cycle if no page cross) */
static CycleMetadata addr_idy_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY },  /* Read pointer */
    { ADDR_ZP, REG_DUMMY },      /* Read low byte */
    { ADDR_ZP, REG_DUMMY },      /* Read high byte */
    { ADDR_IND_Y, REG_DUMMY }    /* Page cross penalty */
};

static Pins addr_idy(CPU6502* cpu, Pins pins) {
    switch (cpu->cycle_index++) {
        case 0:
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_ADL(cpu) = (CPU_ADL(cpu) + 1) & 0xFF;
            break;
            
        case 2: {
            uint16_t base = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            uint16_t effective = base + CPU_Y(cpu);
            CPU_AD(cpu) = effective;
            
            if (cpu->opcode_entry.page_cross && !page_crossed(base, effective)) {
                transition_to_operation(cpu);
            }
            break;
        }
            
        case 3:
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* ============================================================================
 * OPERATION HANDLERS
 * ============================================================================
 * These handlers perform the actual CPU operations.
 * They transition to opcode fetch when complete.
 */

/* --- Load Operations --- */

static CycleMetadata op_lda_cycles[] = {
    { ADDR_ABS, REG_DUMMY }  /* Read from address */
};

static Pins op_lda(CPU6502* cpu, Pins pins) {
    CPU_A(cpu) = BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* LDA immediate uses TEMP from addressing mode */
static Pins op_lda_imm(CPU6502* cpu, Pins pins) {
    CPU_A(cpu) = CPU_DL(cpu);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static CycleMetadata op_ldx_cycles[] = {
    { ADDR_ABS, REG_DUMMY }
};

static Pins op_ldx(CPU6502* cpu, Pins pins) {
    CPU_X(cpu) = BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static Pins op_ldx_imm(CPU6502* cpu, Pins pins) {
    CPU_X(cpu) = CPU_DL(cpu);
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static CycleMetadata op_ldy_cycles[] = {
    { ADDR_ABS, REG_DUMMY }
};

static Pins op_ldy(CPU6502* cpu, Pins pins) {
    CPU_Y(cpu) = BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_Y(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static Pins op_ldy_imm(CPU6502* cpu, Pins pins) {
    CPU_Y(cpu) = CPU_DL(cpu);
    update_nz_flags(cpu, CPU_Y(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* --- Store Operations --- */

static CycleMetadata op_sta_cycles[] = {
    { ADDR_ABS, REG_A }  /* Write A to address */
};

static Pins op_sta(CPU6502* cpu, Pins pins) {
    transition_to_fetch(cpu);
    return pins;
}

static CycleMetadata op_stx_cycles[] = {
    { ADDR_ABS, REG_X }
};

static Pins op_stx(CPU6502* cpu, Pins pins) {
    transition_to_fetch(cpu);
    return pins;
}

static CycleMetadata op_sty_cycles[] = {
    { ADDR_ABS, REG_Y }
};

static Pins op_sty(CPU6502* cpu, Pins pins) {
    transition_to_fetch(cpu);
    return pins;
}

/* --- Arithmetic Operations --- */

static CycleMetadata op_adc_cycles[] = {
    { ADDR_ABS, REG_DUMMY }
};

static Pins op_adc(CPU6502* cpu, Pins pins) {
    uint8_t operand = BUS_GET_DATA(pins);
    uint16_t result = CPU_A(cpu) + operand + (CPU_P(cpu) & FLAG_C ? 1 : 0);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    update_nz_flags(cpu, CPU_A(cpu));
    
    if (result > 0xFF) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ result) & (operand ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    transition_to_fetch(cpu);
    return pins;
}

static Pins op_adc_imm(CPU6502* cpu, Pins pins) {
    uint8_t operand = CPU_DL(cpu);
    uint16_t result = CPU_A(cpu) + operand + (CPU_P(cpu) & FLAG_C ? 1 : 0);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    update_nz_flags(cpu, CPU_A(cpu));
    
    if (result > 0xFF) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ result) & (operand ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    transition_to_fetch(cpu);
    return pins;
}

/* --- RMW Operations --- */
/* These operations work for both accumulator and memory modes.
 * The RMW flag in opcode_entry determines which mode.
 * Accumulator mode: 1 cycle, operate on A register
 * Memory mode: 3 cycles (read, write original, write modified)
 */

static CycleMetadata op_rmw_cycles[] = {
    { ADDR_ABS, REG_DUMMY },   /* Read value */
    { ADDR_ABS, REG_DL },    /* Write original (dummy) */
    { ADDR_ABS, REG_DL }     /* Write modified */
};

static CycleMetadata op_acc_cycles[] = {
    { ADDR_PC, REG_DUMMY }  /* Dummy read */
};

/* ASL - Arithmetic Shift Left */
// Macro to handle RMW boilerplate
#define RMW_HANDLER_START(cpu, pins, reg_idx_var) \
    uint8_t reg_idx_var; \
    if ((cpu)->opcode_entry.rmw) { \
        switch ((cpu)->cycle_index++) { \
            case 0: \
                CPU_DL(cpu) = BUS_GET_DATA(pins); \
                return pins; \
            case 1: \
                reg_idx_var = REG_DL; \
                break; \
            case 2: \
                transition_to_fetch(cpu); \
                return pins; \
        } \
    } else { \
        reg_idx_var = REG_A; \
    }

    
#define RMW_HANDLER_END(cpu) \
    if (!(cpu)->opcode_entry.rmw) { \
        transition_to_fetch(cpu); \
    }

Pins op_asl(CPU6502* cpu, Pins pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    /* Common op_index logic */
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value <<= 1;
    cpu->reg8[reg_idx] = value;
    update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* LSR - Logical Shift Right */
static Pins op_lsr(CPU6502* cpu, Pins pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value >>= 1;
    cpu->reg8[reg_idx] = value;
    update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* ROL - Rotate Left */
static Pins op_rol(CPU6502* cpu, Pins pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 1 : 0;
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value << 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* ROR - Rotate Right */
static Pins op_ror(CPU6502* cpu, Pins pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 1 : 0;
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value >> 1) | (old_carry << 7);
    cpu->reg8[reg_idx] = value;
    update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* INC - Increment Memory (RMW only, no accumulator mode) */
#define RMW_ONLY_START(cpu, pins, reg_idx_var) \
    uint8_t reg_idx_var = REG_DL; \
    switch ((cpu)->cycle_index++) { \
        case 0: \
            cpu->reg8[reg_idx_var] = BUS_GET_DATA(pins); \
            return pins; \
        case 1: \
            break; \
        case 2: \
            transition_to_fetch(cpu); \
            return pins; \
    }

Pins op_inc(CPU6502* cpu, Pins pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    cpu->reg8[reg_idx]++;
    update_nz_flags(cpu, cpu->reg8[reg_idx]);
    return pins;
}    

/* DEC - Decrement Memory (RMW only, no accumulator mode) */
static Pins op_dec(CPU6502* cpu, Pins pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    cpu->reg8[reg_idx]--;
    update_nz_flags(cpu, cpu->reg8[reg_idx]);
    return pins;
}

/* ============================================================================
 * LOOKUP TABLES
 * ============================================================================
 */

/* Addressing mode table */
static AddrModeDesc addr_mode_table[AM_COUNT] = {
    { NULL, NULL, },  // AM_NON : No handler needed
    { addr_zp_cycles,  addr_zp }, // AM_ZER
    { addr_zpx_cycles, addr_zpx }, // AM_ZPX
    { addr_zpy_cycles, addr_zpy }, // AM_ZPY
    { addr_abs_cycles, addr_abs }, // AM_ABS
    { addr_abx_cycles, addr_abx }, // AM_ABX
    { addr_aby_cycles, addr_aby }, // AM_ABY
    { addr_ind_cycles, addr_ind }, // AM_IND
    { addr_idx_cycles, addr_idx }, // AM_INX
    { addr_idy_cycles, addr_idy }, // AM_INY
};

/* Operation table */
static OperationDesc operation_table[OP_COUNT] = {
    [OP_LDA] = { op_lda_cycles, NULL, op_lda },  /* Immediate mode uses special handler */
    [OP_LDX] = { op_ldx_cycles, NULL, op_ldx },
    [OP_LDY] = { op_ldy_cycles, NULL, op_ldy },
    [OP_STA] = { op_sta_cycles, NULL, op_sta },
    [OP_STX] = { op_stx_cycles, NULL, op_stx },
    [OP_STY] = { op_sty_cycles, NULL, op_sty },
    [OP_ADC] = { op_adc_cycles, NULL, op_adc },
    [OP_ASL] = { op_rmw_cycles, op_acc_cycles, op_asl },
    [OP_LSR] = { op_rmw_cycles, op_acc_cycles, op_lsr },
    [OP_ROL] = { op_rmw_cycles, op_acc_cycles, op_rol },
    [OP_ROR] = { op_rmw_cycles, op_acc_cycles, op_ror },
    [OP_INC] = { op_rmw_cycles, NULL, op_inc },
    [OP_DEC] = { op_rmw_cycles, NULL, op_dec },
};

// Compact macro for opcode_info_t opcode definition - creates properly formatted bitfield entries
#define OP(am_index, page_cross, op_index, rmw_flag) {(am_index), 0, (page_cross), (rmw_flag), (op_index)}

// Merged opcode lookup table - 8 entries per line for readability
static const opcode_info_t opcode_table[256] = {
    OP(AM_NON,0,OP_BRK,0), OP(AM_INX,0,OP_ORA,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_SLO,1), OP(AM_ZER,0,OP_NOP,0), OP(AM_ZER,0,OP_ORA,0), OP(AM_ZER,0,OP_ASL,1), OP(AM_ZER,0,OP_SLO,1),
    OP(AM_NON,0,OP_PHP,0), OP(AM_IMM,0,OP_ORA,0), OP(AM_ACC,0,OP_ASL,0), OP(AM_IMM,0,OP_ANC,0), OP(AM_ABS,0,OP_NOP,0), OP(AM_ABS,0,OP_ORA,0), OP(AM_ABS,0,OP_ASL,1), OP(AM_ABS,0,OP_SLO,1),
    OP(AM_REL,0,OP_BPL,0), OP(AM_INY,1,OP_ORA,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_SLO,1), OP(AM_ZPX,0,OP_NOP,0), OP(AM_ZPX,0,OP_ORA,0), OP(AM_ZPX,0,OP_ASL,1), OP(AM_ZPX,0,OP_SLO,1),
    OP(AM_NON,0,OP_CLC,0), OP(AM_ABY,1,OP_ORA,0), OP(AM_NON,0,OP_NOP,0), OP(AM_ABY,0,OP_SLO,1), OP(AM_ABX,0,OP_NOP,0), OP(AM_ABX,1,OP_ORA,0), OP(AM_ABX,0,OP_ASL,1), OP(AM_ABX,0,OP_SLO,1),
    OP(AM_NON,0,OP_JSR,0), OP(AM_INX,0,OP_AND,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_RLA,1), OP(AM_ZER,0,OP_BIT,0), OP(AM_ZER,0,OP_AND,0), OP(AM_ZER,0,OP_ROL,1), OP(AM_ZER,0,OP_RLA,1),
    OP(AM_NON,0,OP_PLP,0), OP(AM_IMM,0,OP_AND,0), OP(AM_ACC,0,OP_ROL,0), OP(AM_IMM,0,OP_ANC,0), OP(AM_ABS,0,OP_BIT,0), OP(AM_ABS,0,OP_AND,0), OP(AM_ABS,0,OP_ROL,1), OP(AM_ABS,0,OP_RLA,1),
    OP(AM_REL,0,OP_BMI,0), OP(AM_INY,1,OP_AND,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_RLA,1), OP(AM_ZPX,0,OP_NOP,0), OP(AM_ZPX,0,OP_AND,0), OP(AM_ZPX,0,OP_ROL,1), OP(AM_ZPX,0,OP_RLA,1),
    OP(AM_NON,0,OP_SEC,0), OP(AM_ABY,1,OP_AND,0), OP(AM_NON,0,OP_NOP,0), OP(AM_ABY,0,OP_RLA,1), OP(AM_ABX,0,OP_NOP,0), OP(AM_ABX,1,OP_AND,0), OP(AM_ABX,0,OP_ROL,1), OP(AM_ABX,0,OP_RLA,1),
    OP(AM_NON,0,OP_RTI,0), OP(AM_INX,0,OP_EOR,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_SRE,1), OP(AM_ZER,0,OP_NOP,0), OP(AM_ZER,0,OP_EOR,0), OP(AM_ZER,0,OP_LSR,1), OP(AM_ZER,0,OP_SRE,1),
    OP(AM_NON,0,OP_PHA,0), OP(AM_IMM,0,OP_EOR,0), OP(AM_ACC,0,OP_LSR,0), OP(AM_IMM,0,OP_ASR,0), OP(AM_ABS,0,OP_JMP,0), OP(AM_ABS,0,OP_EOR,0), OP(AM_ABS,0,OP_LSR,1), OP(AM_ABS,0,OP_SRE,1),
    OP(AM_REL,0,OP_BVC,0), OP(AM_INY,1,OP_EOR,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_SRE,1), OP(AM_ZPX,0,OP_NOP,0), OP(AM_ZPX,0,OP_EOR,0), OP(AM_ZPX,0,OP_LSR,1), OP(AM_ZPX,0,OP_SRE,1),
    OP(AM_NON,0,OP_CLI,0), OP(AM_ABY,1,OP_EOR,0), OP(AM_NON,0,OP_NOP,0), OP(AM_ABY,0,OP_SRE,1), OP(AM_ABX,0,OP_NOP,0), OP(AM_ABX,1,OP_EOR,0), OP(AM_ABX,0,OP_LSR,1), OP(AM_ABX,0,OP_SRE,1),
    OP(AM_NON,0,OP_RTS,0), OP(AM_INX,0,OP_ADC,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INX,0,OP_RRA,1), OP(AM_ZER,0,OP_NOP,0), OP(AM_ZER,0,OP_ADC,0), OP(AM_ZER,0,OP_ROR,1), OP(AM_ZER,0,OP_RRA,1),
    OP(AM_NON,0,OP_PLA,0), OP(AM_IMM,0,OP_ADC,0), OP(AM_ACC,0,OP_ROR,0), OP(AM_IMM,0,OP_ARR,0), OP(AM_IND,0,OP_JMP,0), OP(AM_ABS,0,OP_ADC,0), OP(AM_ABS,0,OP_ROR,1), OP(AM_ABS,0,OP_RRA,1),
    OP(AM_REL,0,OP_BVS,0), OP(AM_INY,1,OP_ADC,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_RRA,1), OP(AM_ZPX,0,OP_NOP,0), OP(AM_ZPX,0,OP_ADC,0), OP(AM_ZPX,0,OP_ROR,1), OP(AM_ZPX,0,OP_RRA,1),
    OP(AM_NON,0,OP_SEI,0), OP(AM_ABY,1,OP_ADC,0), OP(AM_NON,0,OP_NOP,0), OP(AM_ABY,0,OP_RRA,1), OP(AM_ABX,0,OP_NOP,0), OP(AM_ABX,1,OP_ADC,0), OP(AM_ABX,0,OP_ROR,1), OP(AM_ABX,0,OP_RRA,1),
    OP(AM_IMM,0,OP_NOP,0), OP(AM_INX,0,OP_STA,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_INX,0,OP_SAX,0), OP(AM_ZER,0,OP_STY,0), OP(AM_ZER,0,OP_STA,0), OP(AM_ZER,0,OP_STX,0), OP(AM_ZER,0,OP_SAX,0),
    OP(AM_NON,0,OP_DEY,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_NON,0,OP_TXA,0), OP(AM_IMM,0,OP_XAA,0), OP(AM_ABS,0,OP_STY,0), OP(AM_ABS,0,OP_STA,0), OP(AM_ABS,0,OP_STX,0), OP(AM_ABS,0,OP_SAX,0),
    OP(AM_REL,0,OP_BCC,0), OP(AM_INY,0,OP_STA,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_SHA,0), OP(AM_ZPX,0,OP_STY,0), OP(AM_ZPX,0,OP_STA,0), OP(AM_ZPY,0,OP_STX,0), OP(AM_ZPY,0,OP_SAX,0),
    OP(AM_NON,0,OP_TYA,0), OP(AM_ABY,0,OP_STA,0), OP(AM_NON,0,OP_TXS,0), OP(AM_ABY,0,OP_SHS,0), OP(AM_ABX,0,OP_SHY,0), OP(AM_ABX,0,OP_STA,0), OP(AM_ABY,0,OP_SHX,0), OP(AM_ABY,0,OP_SHA,0),
    OP(AM_IMM,0,OP_LDY,0), OP(AM_INX,0,OP_LDA,0), OP(AM_IMM,0,OP_LDX,0), OP(AM_INX,0,OP_LAX,0), OP(AM_ZER,0,OP_LDY,0), OP(AM_ZER,0,OP_LDA,0), OP(AM_ZER,0,OP_LDX,0), OP(AM_ZER,0,OP_LAX,0),
    OP(AM_NON,0,OP_TAY,0), OP(AM_IMM,0,OP_LDA,0), OP(AM_NON,0,OP_TAX,0), OP(AM_IMM,0,OP_LAX,0), OP(AM_ABS,0,OP_LDY,0), OP(AM_ABS,0,OP_LDA,0), OP(AM_ABS,0,OP_LDX,0), OP(AM_ABS,0,OP_LAX,0),
    OP(AM_REL,0,OP_BCS,0), OP(AM_INY,1,OP_LDA,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_LAX,0), OP(AM_ZPX,0,OP_LDY,0), OP(AM_ZPX,0,OP_LDA,0), OP(AM_ZPY,0,OP_LDX,0), OP(AM_ZPY,0,OP_LAX,0),
    OP(AM_NON,0,OP_CLV,0), OP(AM_ABY,1,OP_LDA,0), OP(AM_NON,0,OP_TSX,0), OP(AM_ABY,0,OP_LAS,0), OP(AM_ABX,1,OP_LDY,0), OP(AM_ABX,1,OP_LDA,0), OP(AM_ABY,1,OP_LDX,0), OP(AM_ABY,0,OP_LAX,0),
    OP(AM_IMM,0,OP_CPY,0), OP(AM_INX,0,OP_CMP,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_INX,0,OP_DCP,1), OP(AM_ZER,0,OP_CPY,0), OP(AM_ZER,0,OP_CMP,0), OP(AM_ZER,0,OP_DEC,1), OP(AM_ZER,0,OP_DCP,1),
    OP(AM_NON,0,OP_INY,0), OP(AM_IMM,0,OP_CMP,0), OP(AM_NON,0,OP_DEX,0), OP(AM_IMM,0,OP_SBX,0), OP(AM_ABS,0,OP_CPY,0), OP(AM_ABS,0,OP_CMP,0), OP(AM_ABS,0,OP_DEC,1), OP(AM_ABS,0,OP_DCP,1),
    OP(AM_REL,0,OP_BNE,0), OP(AM_INY,1,OP_CMP,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_DCP,1), OP(AM_ZPX,0,OP_NOP,0), OP(AM_ZPX,0,OP_CMP,0), OP(AM_ZPX,0,OP_DEC,1), OP(AM_ZPX,0,OP_DCP,1),
    OP(AM_NON,0,OP_CLD,0), OP(AM_ABY,1,OP_CMP,0), OP(AM_NON,0,OP_NOP,0), OP(AM_ABY,0,OP_DCP,1), OP(AM_ABX,0,OP_NOP,0), OP(AM_ABX,1,OP_CMP,0), OP(AM_ABX,0,OP_DEC,1), OP(AM_ABX,0,OP_DCP,1),
    OP(AM_IMM,0,OP_CPX,0), OP(AM_INX,0,OP_SBC,0), OP(AM_IMM,0,OP_NOP,0), OP(AM_INX,0,OP_ISC,1), OP(AM_ZER,0,OP_CPX,0), OP(AM_ZER,0,OP_SBC,0), OP(AM_ZER,0,OP_INC,1), OP(AM_ZER,0,OP_ISC,1),
    OP(AM_NON,0,OP_INX,0), OP(AM_IMM,0,OP_SBC,0), OP(AM_NON,0,OP_NOP,0), OP(AM_IMM,0,OP_SBC,0), OP(AM_ABS,0,OP_CPX,0), OP(AM_ABS,0,OP_SBC,0), OP(AM_ABS,0,OP_INC,1), OP(AM_ABS,0,OP_ISC,1),
    OP(AM_REL,0,OP_BEQ,0), OP(AM_INY,1,OP_SBC,0), OP(AM_NON,0,OP_JAM,0), OP(AM_INY,0,OP_ISC,1), OP(AM_ZPX,0,OP_NOP,0), OP(AM_ZPX,0,OP_SBC,0), OP(AM_ZPX,0,OP_INC,1), OP(AM_ZPX,0,OP_ISC,1),
    OP(AM_NON,0,OP_SED,0), OP(AM_ABY,1,OP_SBC,0), OP(AM_NON,0,OP_NOP,0), OP(AM_ABY,0,OP_ISC,1), OP(AM_ABX,0,OP_NOP,0), OP(AM_ABX,1,OP_SBC,0), OP(AM_ABX,0,OP_INC,1), OP(AM_ABX,0,OP_ISC,1)
};

// Clean up the compact opcode macro
#undef OP

/* ============================================================================
 * OPCODE FETCH AND TRANSITION FUNCTIONS
 * ============================================================================
 */

static CycleMetadata opcode_fetch_cycles[] = {
    { ADDR_PC_INC, REG_DUMMY }
};

static Pins opcode_fetch(CPU6502* cpu, Pins pins) {
    cpu->regs[REG_IR] = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    
    /* Cache the opcode entry (copy once, accessed many times) */
    opcode_info_t opcode_entry = opcode_table[cpu->regs[REG_IR]];
    cpu->opcode_entry = opcode_entry;
    cpu->cycle_index = 0;
    
    /* Transition based on cached entry */
    int am_index = opcode_entry.am_index;   
    if (am_index > AM_NON) {
        AddrModeDesc* am_desc = &addr_mode_table[am_index];
        /* Has addressing mode cycles */
        cpu->current_handler = am_desc->handler;
        cpu->current_metadata = am_desc->metadata;
    } else {
        /* No addressing mode, go straight to op_index */
        OperationDesc* op_desc = &operation_table[opcode_entry.op_index];
        cpu->current_handler = op_desc->handler;
        
        /* Select metadata based on cached RMW flag */
        if (opcode_entry.rmw) {
            cpu->current_metadata = op_desc->rmw_metadata;
        } else {
            cpu->current_metadata = op_desc->normal_metadata;
        }
    }
    
    return pins;
}

static void transition_to_operation(CPU6502* cpu) {
    OperationDesc* op_desc = &operation_table[cpu->opcode_entry.op_index];
    
    cpu->cycle_index = 0;
    cpu->current_handler = op_desc->handler;
    
    /* Select metadata based on RMW flag */
    if (cpu->opcode_entry.rmw) {
        cpu->current_metadata = op_desc->rmw_metadata;
    } else {
        cpu->current_metadata = op_desc->normal_metadata;
    }
}

static void transition_to_fetch(CPU6502* cpu) {
    cpu->cycle_index = 0;
    cpu->current_handler = opcode_fetch;
    cpu->current_metadata = opcode_fetch_cycles;
}

/* ============================================================================
 * CPU TICK FUNCTION
 * ============================================================================
 * This is the main entry point, called once per system cycle.
 * It executes PHI2, performs memory access, and calls PHI1.
 *
 * RDY checking is centralized here: writes ignore RDY, reads respect it.
 */

Pins cpu_tick(CPU6502* cpu, Pins pins) {
    /* Get metadata for current cycle */
    CycleMetadata* meta = &cpu->current_metadata[cpu->cycle_index];
    
    /* ========================================================================
     * PHI2 PHASE START - Set up address bus, R/W̅, and data (for writes)
     * ========================================================================
     * This is metadata-driven and:
     * - Sets SYNC if cycle_index == 0 (opcode fetch)
     * - Computes address based on addr_source
     * - Sets R/W̅ based on reg_index (0 = read, >0 = write)
     * - For writes, puts register value on data bus
     */
    
    /* Check if this is a write cycle - elegant: REG_DUMMY is 0, and reg 0 high byte
     * (which overlaps ZP high byte) is always 0, so any non-zero reg_index is a write */
    bool is_write = (meta->reg_index != REG_DUMMY);
    uint16_t bus_address;

    if (CPU_GET_RDY(pins) || is_write ) {    
        /* Set SYNC only on cycle 0 (opcode fetch) */
        CPU_SET_SYNC(pins, cpu->cycle_index == 0);
        
        /* Hardware-accurate address source selection using 16-bit register indices
        * This directly maps to the 16-bit register array, making it very efficient.
        * Arithmetic operations are handled by addressing mode handlers in PHI1.
        */
        bus_address = cpu->reg16[meta->addr_source];
        
        /* Set address on bus */
        BUS_SET_ADDR(pins, bus_address);
    } else {
        bus_address  = BUS_GET_ADDR(pins);
    }

    /* ========================================================================
    * PHI2 PHASE END
    * ========================================================================
    */
   
    /* ========================================================================
    * MEMORY ACCESS PHASE START - Perform actual memory read/write
    * ========================================================================
    */

    /* Set R/W̅ and data */
    if (is_write) {
        BUS_SET_DATA(pins, cpu->reg8[meta->reg_index]);
  
        /* Write: always proceeds regardless of RDY */
        memory_write(bus_address, BUS_GET_DATA(pins));
        
    } else {
        /* Read: perform memory access */
        BUS_SET_DATA(pins, memory_read(bus_address));
        
    }
    /* ========================================================================
     * MEMORY ACCESS PHASE END
     * ========================================================================
     */
    
    /* ========================================================================
     * PHI1 PHASE START - Process result, update CPU state
     * ========================================================================
     */
    
    /* Check RDY: if low (even during write), CPU PHI1 is halted - don't call PHI1 handler */
    if (CPU_GET_RDY(pins)) {
        /* Call current handler (only if not halted) */
        pins = cpu->current_handler(cpu, pins);
        cpu->cycles++;
    }

    /* ========================================================================
     * PHI1 PHASE END
     * ========================================================================
     */
    
    return pins;
}

/* ============================================================================
 * CPU INITIALIZATION
 * ============================================================================
 */

void cpu_init(CPU6502* cpu) {
    memset(cpu, 0, sizeof(CPU6502));
    
    /* Initialize register layout:
     * ZP high byte (REG_ZPH/REG_DUMMY) = 0x00 (always zero for zero page)
     * SP high byte (REG_SPH) = 0x01 (stack is always in page 1)
     * S register (stack pointer low) = 0xFF (stack starts at top)
     */
    cpu->reg8[REG_ZPH] = 0x00;  /* Zero page high byte - same as REG_DUMMY */
    cpu->reg8[REG_SPH] = 0x01;  /* Stack pointer high byte */
    CPU_S(cpu) = 0xFF;          /* Stack pointer low byte */
    
    /* Set processor status unused flag to 1 */
    CPU_P(cpu) = FLAG_U;
    
    /* Start at opcode fetch */
    cpu->current_handler = opcode_fetch;
    cpu->current_metadata = opcode_fetch_cycles;
    cpu->cycle_index = 0;
}

/* ============================================================================
 * SYSTEM TICK (MULTI-CHIP)
 * ============================================================================
 * Example of how to integrate CPU with other chips like VIC-II.
 * The glue logic function would translate VIC signals to CPU RDY.
 */

#if 0  /* Example code, not compiled */

Pins glue_logic(Pins pins) {
    bool ba = VIC_GET_BA(pins);
    bool aec = VIC_GET_AEC(pins);
    
    /* When VIC takes bus (AEC low), halt CPU */
    if (!aec) {
        pins = (pins & ~(1ULL << CPU_PIN_RDY)) | (0ULL << CPU_PIN_RDY);
    } else {
        pins = (pins & ~(1ULL << CPU_PIN_RDY)) | (1ULL << CPU_PIN_RDY);
    }
    
    return pins;
}

Pins system_tick(CPU6502* cpu, VIC* vic, Pins pins) {
    /* 1. VIC determines if it needs the bus */
    pins = vic_tick(vic, pins);
    
    /* 2. Glue logic translates signals */
    pins = glue_logic(pins);
    
    /* 3. CPU tick */
    pins = cpu_tick(cpu, pins);
    
    return pins;
}

#endif