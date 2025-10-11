/*
 * MOS 6502 Cycle-Accurate Emulator
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
 * 3. METADATA-DRIVEN PHI2
 *    - Each cycle has metadata: address source + register index
 *    - Generic PHI2 handler computes address and sets R/W̅ from metadata
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

typedef uint64_t bus_state_t;

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

/* CPU-Specific bus_state_t */
#define CPU_PIN_SYNC     25
#define CPU_PIN_RDY      26
#define CPU_PIN_IRQ      27
#define CPU_PIN_NMI      28
#define CPU_PIN_HALT     63    /* Reserved bit for cycle halting */

#define CPU_GET_RDY(pins)      (((pins) >> CPU_PIN_RDY) & 1)
#define CPU_SET_SYNC(pins, v)  ((pins) = ((pins) & ~(1ULL << CPU_PIN_SYNC)) | (((uint64_t)(v) & 1) << CPU_PIN_SYNC))
#define CPU_GET_SYNC(pins)     (((pins) >> CPU_PIN_SYNC) & 1)
#define CPU_GET_IRQ(pins)      (((pins) >> CPU_PIN_IRQ) & 1)
#define CPU_GET_NMI(pins)      (((pins) >> CPU_PIN_NMI) & 1)
#define CPU_GET_HALT(pins)     (((pins) >> CPU_PIN_HALT) & 1)
#define CPU_SET_HALT(pins, v)  ((pins) = ((pins) & ~(1ULL << CPU_PIN_HALT)) | (((uint64_t)(v) & 1) << CPU_PIN_HALT))

/* VIC-II-Specific bus_state_t (for multi-chip systems) */
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
 * Stack pointer high byte is always 0x01, enabling direct 16-bit SP access.
 */

/* 8-bit register indices - arranged to align with 16-bit register pairs
 * Layout: ZP(0,1), SP(2,3), AB(4,5), PC(6,7), then others(8+)
 */
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
typedef bus_state_t (*cycle_fn_t)(CPU6502* cpu, bus_state_t pins);

struct CPU6502 {
    /* Register array - union allows both 8-bit and 16-bit access */
    union {
        uint8_t reg8[REG_COUNT];        /* 8-bit register access */
        uint16_t reg16[REG_COUNT / 2];  /* 16-bit pair access (little-endian) */
    };
    
    /* Current execution state */
    opcode_info_t opcode_entry;       /* Cached opcode entry (copied once) */
    uint8_t cycle_index;              /* Current cycle within instruction */
    cycle_fn_t current_handler;        /* Current PHI1 handler */
    
    /* Cycle counter */
    uint64_t cycles;
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

/* Forward declarations */
static bus_state_t opcode_fetch(CPU6502* cpu, bus_state_t pins);
static void transition_to_operation(CPU6502* cpu);
static void transition_to_fetch(CPU6502* cpu);

/* PHI2 Handler declarations */
static bus_state_t cpu_phi2_read(CPU6502* cpu, bus_state_t pins, reg16_t addr_reg);
static bus_state_t cpu_phi2_write(CPU6502* cpu, bus_state_t pins, reg16_t addr_reg, reg8_t reg_write);

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
 * PHI2 HANDLER
 * ============================================================================
 * Centralized PHI2 handler that takes address register index and data byte.
 * Returns pins with HALT bit set if the cycle should be halted.
 */

/* Centralized PHI2 read handler - handles memory reads during PHI2 phase
 * CPU read cycles can be halted by RDY signal, but VIC-II memory reads are always serviced
 *
 * Parameters:
 *   addr_reg: 16-bit register index containing the address to read from
 */
static bus_state_t cpu_phi2_read(CPU6502* cpu, bus_state_t pins, reg16_t addr_reg) {
    /* Set SYNC if this is cycle 0 (opcode fetch) */
    CPU_SET_SYNC(pins, cpu->cycle_index == 0);
    
    uint16_t address;

    if (CPU_GET_RDY(pins)) {
        address = cpu->reg16[addr_reg];
        BUS_SET_ADDR(pins, address);
        CPU_SET_HALT(pins, 0);
    } else {
        address = BUS_GET_ADDR(pins);
        CPU_SET_HALT(pins, 1);
    }

    /* Always perform memory read to service VIC-II even when CPU halted */
    BUS_SET_DATA(pins, memory_read(address));
    
    return pins;
}

/* Centralized PHI2 write handler - handles memory writes during PHI2 phase
 * Write always proceeds (cannot be halted), but PHI1 can still halt
 *
 * Parameters:
 *   addr_reg: 16-bit register index containing the address to write to
 *   reg_write: 8-bit register index to write from
 */
static bus_state_t cpu_phi2_write(CPU6502* cpu, bus_state_t pins, reg16_t addr_reg, reg8_t reg_write) {
    /* Set SYNC if this is cycle 0 (opcode fetch) */
    CPU_SET_SYNC(pins, cpu->cycle_index == 0);
    
    uint16_t address = cpu->reg16[addr_reg];
    
    /* Write cycle - always proceeds regardless of RDY */
    BUS_SET_ADDR(pins, address);
    uint8_t data_byte = cpu->reg8[reg_write];
    BUS_SET_DATA(pins, data_byte);
    memory_write(address, data_byte);
    
    /* Check RDY for PHI1 halt - even writes can have PHI1 delayed */
    if (!CPU_GET_RDY(pins)) {
        CPU_SET_HALT(pins, 1);
        return pins;
    }
    
    CPU_SET_HALT(pins, 0);
    return pins;
}

/* ============================================================================
 * ADDRESSING MODE HANDLERS
 * ============================================================================
 * These handlers prepare the address/data for operations.
 * They transition to the op_index handler when complete.
 */

/* Zero Page - operand at $00nn */
static bus_state_t addr_zp(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read from PC and increment */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Store operand address in ADL and increment PC */
    CPU_ADL(cpu) = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    transition_to_operation(cpu);
    return pins;
}

/* Absolute - operand at $nnnn */
static bus_state_t addr_abs(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store low byte and increment PC */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* PHI2: Read high byte from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store high byte and increment PC */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page,X - operand at ($00nn + X) & 0xFF */
static bus_state_t addr_zpx(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read base address from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store base address and increment PC */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP while adding X */
            pins = cpu_phi2_read(cpu, pins, REG_ZP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Add X to address */
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_X(cpu)) & 0xFF;
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
static bus_state_t addr_zpy(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read base address from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store base address and increment PC */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP while adding Y */
            pins = cpu_phi2_read(cpu, pins, REG_ZP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Add Y to address */
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_Y(cpu)) & 0xFF;
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,X - operand at $nnnn + X (may skip cycle if no page cross) */
static bus_state_t addr_abx(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store low byte */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1: {
            /* PHI2: Read high byte from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store high byte and add X */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            
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
            /* PHI2: Page cross penalty cycle */
            pins = cpu_phi2_read(cpu, pins, REG_AB);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Complete addressing */
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,Y - operand at $nnnn + Y (may skip cycle if no page cross) */
static bus_state_t addr_aby(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store low byte */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1: {
            /* PHI2: Read high byte from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store high byte and add Y */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            
            uint16_t base = CPU_AD(cpu);
            uint16_t effective = base + CPU_Y(cpu);
            CPU_AD(cpu) = effective;
            
            if (cpu->opcode_entry.page_cross && !page_crossed(base, effective)) {
                transition_to_operation(cpu);
            }
            break;
        }
            
        case 2:
            /* PHI2: Page cross penalty cycle */
            pins = cpu_phi2_read(cpu, pins, REG_AB);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Complete addressing */
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indirect - used only by JMP ($nnnn) */
static bus_state_t addr_ind(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte of pointer address from PC */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store pointer low byte */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read high byte of pointer address from PC */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store pointer high byte */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 2:
            /* PHI2: Read low byte of target address from (pointer) */
            pins = cpu_phi2_read(cpu, pins, REG_AB);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store target low byte and prepare for 6502 bug */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            
            /* IMPORTANT: 6502 bug - if pointer is at page boundary (e.g., $xxFF),
             * high byte is read from $xx00 instead of $(xx+1)00
             * To emulate this bug: increment only low byte for next read */
            CPU_ADL(cpu) = (CPU_ADL(cpu) + 1) & 0xFF;
            break;
            
        case 3:
            /* PHI2: Read high byte of target address */
            pins = cpu_phi2_read(cpu, pins, REG_AB);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Assemble final target address */
            CPU_ADL(cpu) = CPU_DL(cpu);
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indexed Indirect - operand at (($nn + X) & 0xFF) */
static bus_state_t addr_idx(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read pointer from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store pointer */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP */
            pins = cpu_phi2_read(cpu, pins, REG_ZP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Dummy cycle - no operation */
            break;
            
        case 2:
            /* PHI2: Read low byte of target from ZP+X */
            pins = cpu_phi2_read(cpu, pins, REG_ZP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store target low byte and increment pointer */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_X(cpu) + 1) & 0xFF;
            break;
            
        case 3:
            /* PHI2: Read high byte of target from ZP+X+1 */
            pins = cpu_phi2_read(cpu, pins, REG_ZP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Assemble final address */
            CPU_ADL(cpu) = CPU_DL(cpu);
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indirect Indexed - operand at ($nn) + Y (may skip cycle if no page cross) */
static bus_state_t addr_idy(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read pointer from PC and increment */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store pointer */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read low byte from ZP pointer */
            pins = cpu_phi2_read(cpu, pins, REG_ZP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store low byte and increment pointer */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_ADL(cpu) = (CPU_ADL(cpu) + 1) & 0xFF;
            break;
            
        case 2: {
            /* PHI2: Read high byte from ZP pointer+1 */
            pins = cpu_phi2_read(cpu, pins, REG_ZP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Calculate effective address with Y */
            uint16_t base = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            uint16_t effective = base + CPU_Y(cpu);
            CPU_AD(cpu) = effective;
            
            if (cpu->opcode_entry.page_cross && !page_crossed(base, effective)) {
                transition_to_operation(cpu);
            }
            break;
        }
            
        case 3:
            /* PHI2: Page cross penalty cycle */
            pins = cpu_phi2_read(cpu, pins, REG_AB);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Complete addressing */
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

static bus_state_t op_lda(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Load accumulator and set flags */
    CPU_A(cpu) = BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* LDA immediate uses TEMP from addressing mode */
static bus_state_t op_lda_imm(CPU6502* cpu, bus_state_t pins) {
    CPU_A(cpu) = CPU_DL(cpu);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_ldx(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Load X register and set flags */
    CPU_X(cpu) = BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_ldx_imm(CPU6502* cpu, bus_state_t pins) {
    CPU_X(cpu) = CPU_DL(cpu);
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_ldy(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Load Y register and set flags */
    CPU_Y(cpu) = BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_Y(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_ldy_imm(CPU6502* cpu, bus_state_t pins) {
    CPU_Y(cpu) = CPU_DL(cpu);
    update_nz_flags(cpu, CPU_Y(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* --- Store Operations --- */

static bus_state_t op_sta(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write A to target address */
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_A);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Complete instruction */
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_stx(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write X to target address */
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_X);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Complete instruction */
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_sty(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write Y to target address */
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_Y);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Complete instruction */
    transition_to_fetch(cpu);
    return pins;
}

/* --- Arithmetic Operations --- */

static bus_state_t op_adc(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform ADC operation */
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

static bus_state_t op_adc_imm(CPU6502* cpu, bus_state_t pins) {
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

static bus_state_t op_sbc(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform SBC operation */
    uint8_t operand = BUS_GET_DATA(pins);
    uint16_t result = CPU_A(cpu) - operand - (CPU_P(cpu) & FLAG_C ? 0 : 1);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    update_nz_flags(cpu, CPU_A(cpu));
    
    if (result < 0x100) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ operand) & (a_old ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_sbc_imm(CPU6502* cpu, bus_state_t pins) {
    uint8_t operand = CPU_DL(cpu);
    uint16_t result = CPU_A(cpu) - operand - (CPU_P(cpu) & FLAG_C ? 0 : 1);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    update_nz_flags(cpu, CPU_A(cpu));
    
    if (result < 0x100) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ operand) & (a_old ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    transition_to_fetch(cpu);
    return pins;
}

/* --- Logic Operations --- */

static bus_state_t op_and(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform AND operation */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_and_imm(CPU6502* cpu, bus_state_t pins) {
    CPU_A(cpu) &= CPU_DL(cpu);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_ora(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform ORA operation */
    CPU_A(cpu) |= BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_ora_imm(CPU6502* cpu, bus_state_t pins) {
    CPU_A(cpu) |= CPU_DL(cpu);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_eor(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform EOR operation */
    CPU_A(cpu) ^= BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_eor_imm(CPU6502* cpu, bus_state_t pins) {
    CPU_A(cpu) ^= CPU_DL(cpu);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* --- Compare Operations --- */

static bus_state_t op_cmp(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform CMP operation */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = CPU_A(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_A(cpu) >= data ? FLAG_C : 0);
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_cmp_imm(CPU6502* cpu, bus_state_t pins) {
    uint8_t data = CPU_DL(cpu);
    uint16_t result = CPU_A(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_A(cpu) >= data ? FLAG_C : 0);
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_cpx(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform CPX operation */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = CPU_X(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_X(cpu) >= data ? FLAG_C : 0);
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_cpx_imm(CPU6502* cpu, bus_state_t pins) {
    uint8_t data = CPU_DL(cpu);
    uint16_t result = CPU_X(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_X(cpu) >= data ? FLAG_C : 0);
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_cpy(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform CPY operation */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = CPU_Y(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_Y(cpu) >= data ? FLAG_C : 0);
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_cpy_imm(CPU6502* cpu, bus_state_t pins) {
    uint8_t data = CPU_DL(cpu);
    uint16_t result = CPU_Y(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_Y(cpu) >= data ? FLAG_C : 0);
    transition_to_fetch(cpu);
    return pins;
}

/* --- Register Operations --- */

static bus_state_t op_inx(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Increment X register */
    CPU_X(cpu)++;
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_iny(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Increment Y register */
    CPU_Y(cpu)++;
    update_nz_flags(cpu, CPU_Y(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_dex(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Decrement X register */
    CPU_X(cpu)--;
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_dey(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Decrement Y register */
    CPU_Y(cpu)--;
    update_nz_flags(cpu, CPU_Y(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* --- Transfer Operations --- */

static bus_state_t op_tax(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Transfer A to X */
    CPU_X(cpu) = CPU_A(cpu);
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_tay(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Transfer A to Y */
    CPU_Y(cpu) = CPU_A(cpu);
    update_nz_flags(cpu, CPU_Y(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_txa(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Transfer X to A */
    CPU_A(cpu) = CPU_X(cpu);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_tya(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Transfer Y to A */
    CPU_A(cpu) = CPU_Y(cpu);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_tsx(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Transfer S to X */
    CPU_X(cpu) = CPU_S(cpu);
    update_nz_flags(cpu, CPU_X(cpu));
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_txs(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Transfer X to S */
    CPU_S(cpu) = CPU_X(cpu);
    transition_to_fetch(cpu);
    return pins;
}

/* --- RMW Operations --- */
/* These operations work for both accumulator and memory modes.
 * The RMW flag in opcode_entry determines which mode.
 * Accumulator mode: 1 cycle, operate on A register
 * Memory mode: 3 cycles (read, write original, write modified)
 */

/* ============================================================================
 * RMW MACROS
 * ============================================================================
 * These macros handle the boilerplate for Read-Modify-Write operations.
 * They work with both accumulator mode (1 cycle) and memory mode (3 cycles).
 */

/* RMW handler for operations that support both accumulator and memory modes */
#define RMW_HANDLER_START(cpu, pins, reg_idx_var) \
    uint8_t reg_idx_var; \
    if ((cpu)->opcode_entry.rmw) { \
        switch ((cpu)->cycle_index++) { \
            case 0: \
                pins = cpu_phi2_read(cpu, pins, REG_AB); \
                if (CPU_GET_HALT(pins)) return pins; \
                CPU_DL(cpu) = BUS_GET_DATA(pins); \
                break; \
            case 1: \
                pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL); \
                if (CPU_GET_HALT(pins)) return pins; \
                reg_idx_var = REG_DL; \
                break; \
            case 2: \
                pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL); \
                if (CPU_GET_HALT(pins)) return pins; \
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

/* RMW handler for memory-only operations (no accumulator mode) */
#define RMW_ONLY_START(cpu, pins, reg_idx_var) \
    uint8_t reg_idx_var = REG_DL; \
    switch ((cpu)->cycle_index++) { \
        case 0: \
            pins = cpu_phi2_read(cpu, pins, REG_AB); \
            if (CPU_GET_HALT(pins)) return pins; \
            CPU_DL(cpu) = BUS_GET_DATA(pins); \
            break; \
        case 1: \
            pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL); \
            if (CPU_GET_HALT(pins)) return pins; \
            break; \
        case 2: \
            pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL); \
            if (CPU_GET_HALT(pins)) return pins; \
            transition_to_fetch(cpu); \
            return pins; \
    }

/* ASL - Arithmetic Shift Left */
static bus_state_t op_asl(CPU6502* cpu, bus_state_t pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
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
static bus_state_t op_lsr(CPU6502* cpu, bus_state_t pins) {
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
static bus_state_t op_rol(CPU6502* cpu, bus_state_t pins) {
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
static bus_state_t op_ror(CPU6502* cpu, bus_state_t pins) {
    RMW_HANDLER_START(cpu, pins, reg_idx);
    
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 0x80 : 0;
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value >> 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    update_nz_flags(cpu, value);
    
    RMW_HANDLER_END(cpu);
    return pins;
}

/* INC - Increment Memory (RMW only, no accumulator mode) */
static bus_state_t op_inc(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    cpu->reg8[reg_idx]++;
    update_nz_flags(cpu, cpu->reg8[reg_idx]);
    return pins;
}

/* DEC - Decrement Memory (RMW only, no accumulator mode) */
static bus_state_t op_dec(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    cpu->reg8[reg_idx]--;
    update_nz_flags(cpu, cpu->reg8[reg_idx]);
    return pins;
}

/* ============================================================================
 * ILLEGAL RMW OPERATIONS
 * ============================================================================
 * These are combination operations that perform two operations in sequence.
 * All are memory-only RMW operations (no accumulator mode).
 */

/* SLO - ASL + ORA (Shift Left and OR) */
static bus_state_t op_slo(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform ASL */
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value <<= 1;
    cpu->reg8[reg_idx] = value;
    
    /* Perform ORA with A */
    CPU_A(cpu) |= value;
    update_nz_flags(cpu, CPU_A(cpu));
    return pins;
}

/* RLA - ROL + AND (Rotate Left and AND) */
static bus_state_t op_rla(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform ROL */
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 1 : 0;
    if (value & 0x80) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value << 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    
    /* Perform AND with A */
    CPU_A(cpu) &= value;
    update_nz_flags(cpu, CPU_A(cpu));
    return pins;
}

/* SRE - LSR + EOR (Shift Right and EOR) */
static bus_state_t op_sre(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform LSR */
    uint8_t value = cpu->reg8[reg_idx];
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value >>= 1;
    cpu->reg8[reg_idx] = value;
    
    /* Perform EOR with A */
    CPU_A(cpu) ^= value;
    update_nz_flags(cpu, CPU_A(cpu));
    return pins;
}

/* RRA - ROR + ADC (Rotate Right and Add with Carry) */
static bus_state_t op_rra(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform ROR */
    uint8_t value = cpu->reg8[reg_idx];
    uint8_t old_carry = (CPU_P(cpu) & FLAG_C) ? 0x80 : 0;
    if (value & 0x01) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    value = (value >> 1) | old_carry;
    cpu->reg8[reg_idx] = value;
    
    /* Perform ADC with A */
    uint8_t operand = value;
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
    return pins;
}

/* DCP - DEC + CMP (Decrement and Compare) */
static bus_state_t op_dcp(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform DEC */
    cpu->reg8[reg_idx]--;
    
    /* Perform CMP with A */
    uint8_t value = cpu->reg8[reg_idx];
    uint16_t result = CPU_A(cpu) - value;
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_A(cpu) >= value ? FLAG_C : 0);
    return pins;
}

/* ISC - INC + SBC (Increment and Subtract with Carry) */
static bus_state_t op_isc(CPU6502* cpu, bus_state_t pins) {
    RMW_ONLY_START(cpu, pins, reg_idx);
    
    /* Perform INC */
    cpu->reg8[reg_idx]++;
    
    /* Perform SBC with A */
    uint8_t operand = cpu->reg8[reg_idx];
    uint16_t result = CPU_A(cpu) - operand - (CPU_P(cpu) & FLAG_C ? 0 : 1);
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    update_nz_flags(cpu, CPU_A(cpu));
    if (result < 0x100) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    if (((a_old ^ operand) & (a_old ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    return pins;
}

/* ============================================================================
 * STACK OPERATIONS
 * ============================================================================
 */

/* PHA - Push Accumulator */
static bus_state_t op_pha(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Write A to stack and decrement SP */
            pins = cpu_phi2_write(cpu, pins, REG_SP, REG_A);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* PHP - Push Processor Status */
static bus_state_t op_php(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Write P|B|U to stack and decrement SP */
            CPU_DL(cpu) = CPU_P(cpu) | FLAG_B | FLAG_U;
            pins = cpu_phi2_write(cpu, pins, REG_SP, REG_DL);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* PLA - Pull Accumulator */
static bus_state_t op_pla(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            break;
            
        case 1:
            /* Dummy cycle for stack pointer increment */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Increment stack pointer */
            CPU_S(cpu)++;
            break;
            
        case 2:
            /* PHI2: Read from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store in A and set flags */
            CPU_A(cpu) = BUS_GET_DATA(pins);
            update_nz_flags(cpu, CPU_A(cpu));
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* PLP - Pull Processor Status */
static bus_state_t op_plp(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            break;
            
        case 1:
            /* Dummy cycle for stack pointer increment */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Increment stack pointer */
            CPU_S(cpu)++;
            break;
            
        case 2:
            /* PHI2: Read from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store in P (clear B, set U) */
            CPU_P(cpu) = (BUS_GET_DATA(pins) & ~FLAG_B) | FLAG_U;
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* ============================================================================
 * BRANCH OPERATIONS
 * ============================================================================
 */

/* Helper function for branch operations */
static bus_state_t op_branch_helper(CPU6502* cpu, bus_state_t pins, uint8_t flag_mask, bool flag_value) {
    switch (cpu->cycle_index++) {
        case 0: {
            /* PHI2: Read branch offset from PC */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Check branch condition */
            bool branch_taken = ((CPU_P(cpu) & flag_mask) != 0) == flag_value;
            CPU_PC(cpu)++;
            
            if (!branch_taken) {
                transition_to_fetch(cpu);
                return pins;
            }
            
            /* Calculate target address */
            int8_t offset = (int8_t)BUS_GET_DATA(pins);
            uint16_t target = CPU_PC(cpu) + offset;
            CPU_AB(cpu) = target;
            
            /* Skip cycle 2 if no page cross and page_cross flag set */
            if (cpu->opcode_entry.page_cross && !page_crossed(CPU_PC(cpu), target)) {
                CPU_PC(cpu) = target;
                transition_to_fetch(cpu);
                return pins;
            }
            break;
        }
        
        case 1:
            /* PHI2: Page cross penalty cycle */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Set final PC */
            CPU_PC(cpu) = CPU_AB(cpu);
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

static bus_state_t op_bcc(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_C, false);
}

static bus_state_t op_bcs(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_C, true);
}

static bus_state_t op_beq(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_Z, true);
}

static bus_state_t op_bne(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_Z, false);
}

static bus_state_t op_bmi(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_N, true);
}

static bus_state_t op_bpl(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_N, false);
}

static bus_state_t op_bvc(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_V, false);
}

static bus_state_t op_bvs(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, FLAG_V, true);
}

/* ============================================================================
 * FLAG OPERATIONS
 * ============================================================================
 */

static bus_state_t op_clc(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Clear carry flag */
    CPU_P(cpu) &= ~FLAG_C;
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_sec(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Set carry flag */
    CPU_P(cpu) |= FLAG_C;
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_cli(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Clear interrupt disable flag */
    CPU_P(cpu) &= ~FLAG_I;
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_sei(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Set interrupt disable flag */
    CPU_P(cpu) |= FLAG_I;
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_cld(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Clear decimal mode flag */
    CPU_P(cpu) &= ~FLAG_D;
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_sed(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Set decimal mode flag */
    CPU_P(cpu) |= FLAG_D;
    transition_to_fetch(cpu);
    return pins;
}

static bus_state_t op_clv(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Clear overflow flag */
    CPU_P(cpu) &= ~FLAG_V;
    transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * CONTROL FLOW OPERATIONS
 * ============================================================================
 */

/* JMP - Jump */
static bus_state_t op_jmp(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Dummy read (JMP target address set by addressing mode) */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Set PC to target address */
    CPU_PC(cpu) = CPU_AB(cpu);
    transition_to_fetch(cpu);
    return pins;
}

/* JSR - Jump to Subroutine */
static bus_state_t op_jsr(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte of target address */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store low byte */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* PHI2: Dummy read from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            break;
            
        case 2:
            /* PHI2: Push PCH to stack */
            pins = cpu_phi2_write(cpu, pins, REG_SP, REG_PCH);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 3:
            /* PHI2: Push PCL to stack */
            pins = cpu_phi2_write(cpu, pins, REG_SP, REG_PCL);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 4:
            /* PHI2: Read high byte of target address */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Set PC to target address */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu) = CPU_AB(cpu);
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* RTS - Return from Subroutine */
static bus_state_t op_rts(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            break;
            
        case 1:
            /* Dummy cycle for stack pointer increment */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Increment stack pointer */
            CPU_S(cpu)++;
            break;
            
        case 2:
            /* PHI2: Read PCL from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store PCL and increment SP */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_S(cpu)++;
            break;
            
        case 3:
            /* PHI2: Read PCH from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Assemble PC */
            CPU_PC(cpu) = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            break;
            
        case 4:
            /* PHI2: Dummy read from PC */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Increment PC and transition to fetch */
            CPU_PC(cpu)++;
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* RTI - Return from Interrupt */
static bus_state_t op_rti(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            break;
            
        case 1:
            /* Dummy cycle for stack pointer increment */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Increment stack pointer */
            CPU_S(cpu)++;
            break;
            
        case 2:
            /* PHI2: Read P from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store P and increment SP */
            CPU_P(cpu) = (BUS_GET_DATA(pins) & ~FLAG_B) | FLAG_U;
            CPU_S(cpu)++;
            break;
            
        case 3:
            /* PHI2: Read PCL from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store PCL and increment SP */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_S(cpu)++;
            break;
            
        case 4:
            /* PHI2: Read PCH from stack */
            pins = cpu_phi2_read(cpu, pins, REG_SP);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Assemble PC and transition to fetch */
            CPU_PC(cpu) = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* BRK - Break / Software Interrupt */
static bus_state_t op_brk(CPU6502* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Dummy read from PC+1 */
            pins = cpu_phi2_read(cpu, pins, REG_PC);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Increment PC to PC+2 for return address */
            CPU_PC(cpu) += 2;
            break;
            
        case 1:
            /* PHI2: Push PCH to stack */
            pins = cpu_phi2_write(cpu, pins, REG_SP, REG_PCH);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 2:
            /* PHI2: Push PCL to stack */
            pins = cpu_phi2_write(cpu, pins, REG_SP, REG_PCL);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 3:
            /* PHI2: Push P|B|U to stack */
            CPU_DL(cpu) = CPU_P(cpu) | FLAG_B | FLAG_U;
            pins = cpu_phi2_write(cpu, pins, REG_SP, REG_DL);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Decrement stack pointer and set I flag */
            CPU_S(cpu)--;
            CPU_P(cpu) |= FLAG_I;
            
            /* Set up vector address (0xFFFE for BRK/IRQ) */
            CPU_AB(cpu) = 0xFFFE;
            break;
            
        case 4:
            /* PHI2: Read vector low byte */
            pins = cpu_phi2_read(cpu, pins, REG_AB);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Store vector low byte and increment address */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_AB(cpu)++;
            break;
            
        case 5:
            /* PHI2: Read vector high byte */
            pins = cpu_phi2_read(cpu, pins, REG_AB);
            if (CPU_GET_HALT(pins)) return pins;
            
            /* PHI1: Set PC to vector address and transition to fetch */
            CPU_PC(cpu) = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* ============================================================================
 * BIT TEST AND MISC OPERATIONS
 * ============================================================================
 */

/* BIT - Bit Test */
static bus_state_t op_bit(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Perform BIT test */
    uint8_t data = BUS_GET_DATA(pins);
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_V | FLAG_Z)) |
                 (data & FLAG_N) |
                 (data & FLAG_V) |
                 ((CPU_A(cpu) & data) == 0 ? FLAG_Z : 0);
    transition_to_fetch(cpu);
    return pins;
}

/* NOP - No Operation */
static bus_state_t op_nop(CPU6502* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Do nothing, just transition to fetch */
    transition_to_fetch(cpu);
    return pins;
}

/* JAM - Jam/Halt CPU */
static bus_state_t op_jam(CPU6502* cpu, bus_state_t pins) {
    /* JAM instruction - CPU halts indefinitely */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    /* Don't transition to fetch - stay in JAM state */
    return pins;
}

/* ============================================================================
 * 65C02 AND ILLEGAL OPERATIONS
 * ============================================================================
 */

/* BRA - Branch Always (65C02) */
static bus_state_t op_bra(CPU6502* cpu, bus_state_t pins) {
    return op_branch_helper(cpu, pins, 0xFF, true); /* Always branch */
}

/* LAX - Load A and X */
static bus_state_t op_lax(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Load both A and X */
    uint8_t data = BUS_GET_DATA(pins);
    CPU_A(cpu) = data;
    CPU_X(cpu) = data;
    update_nz_flags(cpu, data);
    transition_to_fetch(cpu);
    return pins;
}

/* SAX - Store A & X */
static bus_state_t op_sax(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write A&X to target address */
    CPU_DL(cpu) = CPU_A(cpu) & CPU_X(cpu);
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Complete instruction */
    transition_to_fetch(cpu);
    return pins;
}

/* ANC - AND with Carry */
static bus_state_t op_anc(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address or use immediate */
    if (cpu->opcode_entry.am_index == 0) {
        /* Immediate mode - read from PC */
        pins = cpu_phi2_read(cpu, pins, REG_PC);
        if (CPU_GET_HALT(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode */
        pins = cpu_phi2_read(cpu, pins, REG_AB);
        if (CPU_GET_HALT(pins)) return pins;
    }
    
    /* PHI1: Perform AND and set carry to bit 7 */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_A(cpu));
    CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_C) | ((CPU_A(cpu) & 0x80) ? FLAG_C : 0);
    transition_to_fetch(cpu);
    return pins;
}

/* ASR - AND + LSR */
static bus_state_t op_asr(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address or use immediate */
    if (cpu->opcode_entry.am_index == 0) {
        /* Immediate mode - read from PC */
        pins = cpu_phi2_read(cpu, pins, REG_PC);
        if (CPU_GET_HALT(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode */
        pins = cpu_phi2_read(cpu, pins, REG_AB);
        if (CPU_GET_HALT(pins)) return pins;
    }
    
    /* PHI1: Perform AND then LSR */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_C) | ((CPU_A(cpu) & 0x01) ? FLAG_C : 0);
    CPU_A(cpu) >>= 1;
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* ARR - AND + ROR */
static bus_state_t op_arr(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address or use immediate */
    if (cpu->opcode_entry.am_index == 0) {
        /* Immediate mode - read from PC */
        pins = cpu_phi2_read(cpu, pins, REG_PC);
        if (CPU_GET_HALT(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode */
        pins = cpu_phi2_read(cpu, pins, REG_AB);
        if (CPU_GET_HALT(pins)) return pins;
    }
    
    /* PHI1: Perform AND then ROR with complex flag behavior */
    uint8_t carry = (CPU_P(cpu) & FLAG_C) ? 0x80 : 0;
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    CPU_A(cpu) = (CPU_A(cpu) >> 1) | carry;
    update_nz_flags(cpu, CPU_A(cpu));
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_C | FLAG_V)) |
                 ((CPU_A(cpu) & 0x40) ? FLAG_C : 0) |
                 (((CPU_A(cpu) & 0x40) ^ ((CPU_A(cpu) & 0x20) << 1)) ? FLAG_V : 0);
    transition_to_fetch(cpu);
    return pins;
}

/* SBX - (A & X) - operand -> X */
static bus_state_t op_sbx(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address or use immediate */
    if (cpu->opcode_entry.am_index == 0) {
        /* Immediate mode - read from PC */
        pins = cpu_phi2_read(cpu, pins, REG_PC);
        if (CPU_GET_HALT(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode */
        pins = cpu_phi2_read(cpu, pins, REG_AB);
        if (CPU_GET_HALT(pins)) return pins;
    }
    
    /* PHI1: Perform (A & X) - operand -> X */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = (CPU_A(cpu) & CPU_X(cpu)) - data;
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 ((result < 0x100) ? FLAG_C : 0);
    CPU_X(cpu) = result & 0xFF;
    transition_to_fetch(cpu);
    return pins;
}

/* SHA - Store A & X & (H+1) */
static bus_state_t op_sha(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write A&X&(H+1) to target address */
    uint8_t high = (CPU_AB(cpu) >> 8) + 1;
    CPU_DL(cpu) = CPU_A(cpu) & CPU_X(cpu) & high;
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Complete instruction */
    transition_to_fetch(cpu);
    return pins;
}

/* SHS - Store A & X & (H+1), set SP to A & X */
static bus_state_t op_shs(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write A&X&(H+1) to target address */
    uint8_t high = (CPU_AB(cpu) >> 8) + 1;
    uint8_t val = CPU_A(cpu) & CPU_X(cpu) & high;
    CPU_DL(cpu) = val;
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Set SP to A & X */
    CPU_S(cpu) = CPU_A(cpu) & CPU_X(cpu);
    transition_to_fetch(cpu);
    return pins;
}

/* SHX - Store X & (H+1) */
static bus_state_t op_shx(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write X&(H+1) to target address */
    uint8_t high = (CPU_AB(cpu) >> 8) + 1;
    CPU_DL(cpu) = CPU_X(cpu) & high;
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Complete instruction */
    transition_to_fetch(cpu);
    return pins;
}

/* SHY - Store Y & (H+1) */
static bus_state_t op_shy(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Write Y&(H+1) to target address */
    uint8_t high = (CPU_AB(cpu) >> 8) + 1;
    CPU_DL(cpu) = CPU_Y(cpu) & high;
    pins = cpu_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Complete instruction */
    transition_to_fetch(cpu);
    return pins;
}

/* LAS - Load A, X, SP with (operand & SP) */
static bus_state_t op_las(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = cpu_phi2_read(cpu, pins, REG_AB);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Load A, X, SP with (operand & SP) */
    uint8_t val = BUS_GET_DATA(pins) & CPU_S(cpu);
    CPU_A(cpu) = val;
    CPU_X(cpu) = val;
    CPU_S(cpu) = val;
    update_nz_flags(cpu, val);
    transition_to_fetch(cpu);
    return pins;
}

/* XAA - Transfer X to A, then AND with immediate */
static bus_state_t op_xaa(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read operand (immediate mode only) */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: X -> A, then A & operand */
    CPU_PC(cpu)++;
    CPU_A(cpu) = CPU_X(cpu) & BUS_GET_DATA(pins);
    update_nz_flags(cpu, CPU_A(cpu));
    transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * LOOKUP TABLES
 * ============================================================================
 */

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

/* Addressing mode table - now just function pointers */
static const cycle_fn_t addr_mode_table[AM_COUNT] = {
    NULL,     // AM_NON : No handler needed
    addr_zp,  // AM_ZER
    addr_zpx, // AM_ZPX
    addr_zpy, // AM_ZPY
    addr_abs, // AM_ABS
    addr_abx, // AM_ABX
    addr_aby, // AM_ABY
    addr_ind, // AM_IND
    addr_idx, // AM_INX
    addr_idy, // AM_INY
};

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

/* Operation table - now just function pointers */
static const cycle_fn_t op_handlers[OP_COUNT] = {
    op_lda, // OP_LDA
    op_ldx, // OP_LDX
    op_ldy, // OP_LDY,
    op_sta, // OP_STA
    op_stx, // OP_STX
    op_sty, // OP_STY,
    op_adc, // OP_ADC
    op_sbc, // OP_SBC
    op_and, // OP_AND
    op_ora, // OP_ORA
    op_eor, // OP_EOR
    op_cmp, // OP_CMP
    op_cpx, // OP_CPX
    op_cpy, // OP_CPY
    op_asl, // OP_ASL
    op_lsr, // OP_LSR
    op_rol, // OP_ROL
    op_ror, // OP_ROR
    op_inc, // OP_INC
    op_dec, // OP_DEC
    op_inx, // OP_INX
    op_iny, // OP_INY
    op_dex, // OP_DEX
    op_dey, // OP_DEY
    op_tax, // OP_TAX
    op_tay, // OP_TAY
    op_txa, // OP_TXA
    op_tya, // OP_TYA
    op_tsx, // OP_TSX
    op_txs, // OP_TXS
    op_pha, // OP_PHA
    op_php, // OP_PHP
    op_pla, // OP_PLA
    op_plp, // OP_PLP
    op_bcc, // OP_BCC
    op_bcs, // OP_BCS
    op_beq, // OP_BEQ
    op_bne, // OP_BNE
    op_bmi, // OP_BMI
    op_bpl, // OP_BPL
    op_bvc, // OP_BVC
    op_bvs, // OP_BVS
    op_clc, // OP_CLC
    op_sec, // OP_SEC
    op_cli, // OP_CLI
    op_sei, // OP_SEI
    op_cld, // OP_CLD
    op_sed, // OP_SED
    op_clv, // OP_CLV
    op_jmp, // OP_JMP
    op_jsr, // OP_JSR
    op_rts, // OP_RTS
    op_rti, // OP_RTI
    op_brk, // OP_BRK
    op_bit, // OP_BIT
    op_nop, // OP_NOP
    op_jam, // OP_JAM
    // 65C02 enhancements
    op_bra, // OP_BRA
    // Illegal opcodes - combination instructions
    op_lax, // OP_LAX
    op_sax, // OP_SAX
    op_dcp, // OP_DCP
    op_isc, // OP_ISC
    op_slo, // OP_SLO
    op_rla, // OP_RLA
    op_sre, // OP_SRE
    op_rra, // OP_RRA
    // Illegal opcodes - special accumulator operations
    op_anc, // OP_ANC
    op_asr, // OP_ASR
    op_arr, // OP_ARR
    op_sbx, // OP_SBX
    // Illegal opcodes - store with AND operations
    op_sha, // OP_SHA
    op_shs, // OP_SHS
    op_shx, // OP_SHX
    op_shy, // OP_SHY
    op_las, // OP_LAS
    // Illegal opcodes - special operations
    op_xaa  // OP_XAA
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

static bus_state_t opcode_fetch(CPU6502* cpu, bus_state_t pins) {
    /* PHI2: Read opcode from PC and increment */
    pins = cpu_phi2_read(cpu, pins, REG_PC);
    if (CPU_GET_HALT(pins)) return pins;
    
    /* PHI1: Decode opcode and set up next handler */
    CPU_IR(cpu) = BUS_GET_DATA(pins);
    
    /* Cache the opcode entry (copy once, accessed many times) */
    opcode_info_t opcode_entry = opcode_table[CPU_IR(cpu)];
    cpu->opcode_entry = opcode_entry;
    cpu->cycle_index = 0;
    
    /* Transition based on cached entry */
    int am_index = opcode_entry.am_index;
    if (am_index > AM_NON) {
        /* Has addressing mode cycles */
        cpu->current_handler = addr_mode_table[am_index];
    } else {
        /* No addressing mode, go straight to operation */
        cpu->current_handler = op_handlers[opcode_entry.op_index];
    }
    
    return pins;
}

static void transition_to_operation(CPU6502* cpu) {
    cpu->cycle_index = 0;
    cpu->current_handler = op_handlers[cpu->opcode_entry.op_index];
}

static void transition_to_fetch(CPU6502* cpu) {
    cpu->cycle_index = 0;
    cpu->current_handler = opcode_fetch;
}

/* ============================================================================
 * CPU TICK FUNCTION
 * ============================================================================
 * This is the main entry point, called once per system cycle.
 * It executes PHI2, performs memory access, and calls PHI1.
 *
 * RDY checking is centralized here: writes ignore RDY, reads respect it.
 */

bus_state_t cpu_tick(CPU6502* cpu, bus_state_t pins) {
    /* ========================================================================
     * PHI1 PHASE - Call current handler (PHI2 is now called within handlers)
     * ========================================================================
     */
    
    /* Call current handler - PHI2 calls are now embedded within each handler */
    pins = cpu->current_handler(cpu, pins);
    cpu->cycles++;
    
    return pins;
}

/* ============================================================================
 * CPU INITIALIZATION
 * ============================================================================
 */

void cpu_init(CPU6502* cpu) {
    memset(cpu, 0, sizeof(CPU6502));
    
    /* Initialize register layout:
     * ZP high byte (REG_ZPH) = 0x00 (always zero for zero page)
     * SP high byte (REG_SPH) = 0x01 (stack is always in page 1)
     * S register (stack pointer low) = 0xFF (stack starts at top)
     */
    cpu->reg8[REG_ZPH] = 0x00;  /* Zero page high byte */
    cpu->reg8[REG_SPH] = 0x01;  /* Stack pointer high byte */
    CPU_S(cpu) = 0xFF;          /* Stack pointer low byte */
    
    /* Set processor status unused flag to 1 */
    CPU_P(cpu) = FLAG_U;
    
    /* Start at opcode fetch */
    cpu->current_handler = opcode_fetch;
    cpu->cycle_index = 0;
}

/* ============================================================================
 * SYSTEM TICK (MULTI-CHIP)
 * ============================================================================
 * Example of how to integrate CPU with other chips like VIC-II.
 * The glue logic function would translate VIC signals to CPU RDY.
 */

#if 0  /* Example code, not compiled */

bus_state_t glue_logic(bus_state_t pins) {
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

bus_state_t system_tick(CPU6502* cpu, VIC* vic, bus_state_t pins) {
    /* 1. VIC determines if it needs the bus */
    pins = vic_tick(vic, pins);
    
    /* 2. Glue logic translates signals */
    pins = glue_logic(pins);
    
    /* 3. CPU tick */
    pins = cpu_tick(cpu, pins);
    
    return pins;
}

#endif