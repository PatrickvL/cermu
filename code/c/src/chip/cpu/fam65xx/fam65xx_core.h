#ifndef FAM65XX_CORE_H
#define FAM65XX_CORE_H

#include "../../../core/aiemuc.h"
#include "../../../core/chip.h"
#include "../../../core/bus_cycle_interface.h"
#include "../../../core/control_lines_interface.h"
#include "../../../core/system_lines.h"
#include "../../../core/system.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// MOS 6502 FAMILY CORE DEFINITIONS
// ============================================================================

// 6502 Status Register Flags (shared by all family members)
#define FLAG_C  0x01    // Carry
#define FLAG_Z  0x02    // Zero
#define FLAG_I  0x04    // Interrupt Disable
#define FLAG_D  0x08    // Decimal Mode
#define FLAG_B  0x10    // Break Command
#define FLAG_U  0x20    // Unused (always 1)
#define FLAG_V  0x40    // Overflow
#define FLAG_N  0x80    // Negative

// Universal instruction dispatch using function pointers
typedef struct fam65xx_s fam65xx_t;
typedef void (*fam65xx_opcode_handler_t)(fam65xx_t* cpu);

// ============================================================================
// SHARED MOS 6502 FAMILY CPU STATE STRUCTURE
// ============================================================================

struct fam65xx_s {
    chip_descriptor_t* desc; // Pointer to chip descriptor (must be first)
    
    // CPU Registers (standard 6502 family)
    uint16_t pc;        // Program Counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X Index Register
    uint8_t y;          // Y Index Register
    uint8_t sp;         // Stack Pointer
    uint8_t p;          // Processor Status Register
    
    // === CPU INTERNAL STATE (shared by all 6502 family) ===
    uint16_t address;   // Address for current instruction

    // === PERFORMANCE-OPTIMIZED INTERFACE STORAGE ===
    // Bus interface (stored by value for optimal performance)
    bus_cycle_ops_t bus_interface;
    
    // Control lines interface (stored by value for optimal performance) 
    control_lines_interface_t control_interface;
    
    // === SHARED STATE POINTERS ===
    // These CANNOT be copied - must remain as pointers to shared system state
    system_lines_t* system_lines;  // Shared system-wide line state
    
    fam65xx_opcode_handler_t opcode_handlers[256]; // Per-CPU handler table

    // Intercept mechanism for single-step execution
    fam65xx_opcode_handler_t saved_opcode_handlers[256]; // Saved handlers during intercept
    // === DIRECT RAM ACCESS (for zero page optimization) ===
    // Direct RAM accessors to avoid circular dependency with bus interface
    // TODO : Move to mos6510 (the sole user for now)
    access_callback_t ram_access;  // Consolidated RAM access interface
};

// ============================================================================
// SHARED MACROS FOR PERFORMANCE-CRITICAL CODE
// ============================================================================

// ============================================================================
// SHARED MOS 6502 FAMILY MACROS (used by all family members)
// ============================================================================

#define FAM65XX_MASK_IRQ  SYS_MASK_IRQ
#define FAM65XX_MASK_NMI  SYS_MASK_NMI
#define FAM65XX_MASK_RDY  SYS_MASK_RDY

// Control line access and testing (shared)
#define FAM65XX_CONTROL_LINES(cpu) \
    ((cpu)->control_interface.get_lines((cpu)->control_interface.context))

#define FAM65XX_TEST_IRQ(cpu) (FAM65XX_CONTROL_LINES(cpu) & FAM65XX_MASK_IRQ)
#define FAM65XX_TEST_NMI(cpu) (FAM65XX_CONTROL_LINES(cpu) & FAM65XX_MASK_NMI)
#define FAM65XX_TEST_RDY(cpu) (FAM65XX_CONTROL_LINES(cpu) & FAM65XX_MASK_RDY)

// System lines access macros for direct system state operations
#define FAM65XX_SYSTEM_LINES_TEST(cpu, mask) SYS_LINES_TEST((cpu)->system_lines, mask)
#define FAM65XX_SYSTEM_LINES_SET(cpu, mask) SYS_LINES_SET((cpu)->system_lines, mask)
#define FAM65XX_SYSTEM_LINES_CLEAR(cpu, mask) SYS_LINES_CLEAR((cpu)->system_lines, mask)

// Macro utilities for generating unique labels
#define FAM65XX_CONCAT_IMPL(a, b) a ## b
#define FAM65XX_CONCAT(a, b) FAM65XX_CONCAT_IMPL(a, b)
#define FAM65XX_UNIQUE_LABEL(prefix) FAM65XX_CONCAT(prefix, __LINE__)

// Cycle timing macros (shared by all family members)
#define FAM65XX_INTRA_CYCLE(cpu) do { \
    (cpu)->bus_interface.cycle_tick((cpu)->bus_interface.context); \
} while(0)

// Ready check and wait with automatic stall handling
#define FAM65XX_WAIT_READY(cpu) do { \
    FAM65XX_UNIQUE_LABEL(cpu_ready_stall): \
    if (unlikely(!FAM65XX_TEST_RDY(cpu))) { \
        FAM65XX_INTRA_CYCLE(cpu); \
        goto FAM65XX_UNIQUE_LABEL(cpu_ready_stall); \
    } \
} while(0)

// Core memory and cycle functions (shared)
static inline uint8_t fam65xx_read_cycle(fam65xx_t* cpu, uint16_t address) {
    cpu->bus_interface.cycle_tick(cpu->bus_interface.context);  // Bus cycle
    return cpu->bus_interface.bus_read(cpu->bus_interface.context, address);
}

static inline void fam65xx_write_cycle(fam65xx_t* cpu, uint16_t address, uint8_t value) {
    cpu->bus_interface.cycle_tick(cpu->bus_interface.context);  // Bus cycle
    cpu->bus_interface.bus_write(cpu->bus_interface.context, address, value);
}

// Instruction dispatch macros (shared - but implementation-specific functions)
#define FAM65XX_NEXT_INSTRUCTION_DISPATCH(cpu) do { \
    uint8_t opcode = fam65xx_read_cycle(cpu, (cpu)->pc++); \
    (cpu)->opcode_handlers[opcode](cpu); \
} while(0)

// Forward declaration for macros
void fam65xx_interrupt_handler(fam65xx_t* cpu);

#define FAM65XX_NEXT_INSTRUCTION(cpu) do { \
    if (unlikely(FAM65XX_TEST_IRQ(cpu) || FAM65XX_TEST_NMI(cpu))) { \
        fam65xx_interrupt_handler(cpu); \
    } else { \
        FAM65XX_WAIT_READY(cpu); \
    } \
    FAM65XX_NEXT_INSTRUCTION_DISPATCH(cpu); \
} while(0)

// Family-specific versions of shared macros
#define FAM65XX_OPCODE_FOOTER(cpu) \
    FAM65XX_NEXT_INSTRUCTION(cpu)

// Universal instruction dispatch using function pointers
// ============================================================================
// SHARED FUNCTION DECLARATIONS
// ============================================================================

// Arithmetic helper function types (shared by all family members)
typedef uint8_t (*fam65xx_addr_func_t)(fam65xx_t* cpu);
typedef void (*fam65xx_op_func_t)(fam65xx_t* cpu, uint8_t value);

// Arithmetic helper function implementation (static inline for performance)
static inline void fam65xx_addr_op_helper(fam65xx_t* cpu, fam65xx_addr_func_t addr_func, fam65xx_op_func_t op_func) {
    uint8_t value = addr_func(cpu);
    op_func(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// Flag operations (shared)
static inline bool fam65xx_get_flag(fam65xx_t* cpu, uint8_t flag) {
    return (cpu->p & flag) != 0;
}

static inline void fam65xx_set_flag(fam65xx_t* cpu, uint8_t flag, bool value) {
    if (value) {
        cpu->p |= flag;
    } else {
        cpu->p &= ~flag;
    }
}

static inline void fam65xx_set_nz_flags(fam65xx_t* cpu, uint8_t value) {
    fam65xx_set_flag(cpu, FLAG_Z, value == 0);
    fam65xx_set_flag(cpu, FLAG_N, (value & 0x80) != 0);
}

// Stack operations (shared)
static inline void fam65xx_push(fam65xx_t* cpu, uint8_t value) {
    fam65xx_write_cycle(cpu, 0x0100 | cpu->sp, value);  // Bus write cycle
    cpu->sp--;
}
static inline uint8_t fam65xx_pull(fam65xx_t* cpu) {
    (void)fam65xx_read_cycle(cpu, 0x0100 | cpu->sp);  // T1: Dummy read
    cpu->sp++;
    return fam65xx_read_cycle(cpu, 0x0100 | cpu->sp);  // T2: Stack read
}

// Interrupt handling (shared)
// For IRQ and NMI, the B flag is cleared in the pushed status. For BRK, it is set.
static void fam65xx_interrupt_sequence(fam65xx_t* cpu, uint8_t status_flags, uint16_t vector_addr) {
    // Push program counter (high byte first)
    fam65xx_push(cpu, (cpu->pc >> 8) & 0xFF);
    fam65xx_push(cpu, cpu->pc & 0xFF);
    // Push status register with specified flags (caller must set/clear B flag as appropriate)
    fam65xx_push(cpu, status_flags | FLAG_U); // B flag set for BRK, clear for IRQ/NMI
    fam65xx_set_flag(cpu, FLAG_I, true);
    // Load interrupt vector
    uint8_t addr_lo = fam65xx_read_cycle(cpu, vector_addr);  // Bus read cycle
    uint8_t addr_hi = fam65xx_read_cycle(cpu, vector_addr + 1);  // Bus read cycle
    cpu->pc = (addr_hi << 8) | addr_lo;
}

static void fam65xx_nmi(fam65xx_t* cpu) {
    fam65xx_interrupt_sequence(cpu, cpu->p & ~FLAG_B, 0xFFFA); // NMI vector, B flag cleared
}

static void fam65xx_irq(fam65xx_t* cpu) {
    fam65xx_interrupt_sequence(cpu, cpu->p & ~FLAG_B, 0xFFFE); // IRQ vector, B flag cleared
}

// Interception support (shared)
void fam65xx_start_intercept(fam65xx_t* cpu);
void fam65xx_stop_intercept(fam65xx_t* cpu);
bool fam65xx_is_intercepting(fam65xx_t* cpu);

// Single step execution (shared)
bool fam65xx_step(fam65xx_t* cpu);

// ============================================================================
// SHARED OPCODE HANDLER TABLE INITIALIZATION
// ============================================================================

// Initialize opcode handler table with CPU-specific features (shared by all family members)
void fam65xx_init_opcode_table(fam65xx_t* cpu, uint32_t cpu_features);

// Override specific opcodes for CPU variants (manual override if needed)
void fam65xx_override_opcode(fam65xx_t* cpu, uint8_t opcode, fam65xx_opcode_handler_t handler);

// CPU feature flags for automatic opcode table configuration
#define FAM65XX_FEATURE_DECIMAL_MODE    (1U << 0)   // CPU supports decimal mode ADC/SBC
#define FAM65XX_FEATURE_ILLEGAL_OPCODES (1U << 1)   // CPU supports illegal opcodes
#define FAM65XX_FEATURE_BCD_FLAG        (1U << 2)   // CPU sets BCD flag even without decimal mode
#define FAM65XX_FEATURE_ROR_BUG         (1U << 3)   // CPU has ROR absolute,X page boundary bug

// Default opcode handler table (shared base)
extern fam65xx_opcode_handler_t fam65xx_op_default_handlers[256];

// ============================================================================
// SHARED OPCODE OPERATION IMPLEMENTATIONS  
// ============================================================================
// ============================================================================
// ADDRESSING MODE HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Immediate addressing - returns the immediate value
static inline uint8_t fam65xx_addr_imm(fam65xx_t* cpu) {
    return fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Immediate value fetch
}

// Zero page addressing - sets address and returns fetched value
static inline uint8_t fam65xx_addr_zp(fam65xx_t* cpu) {
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Address fetch
    return fam65xx_read_cycle(cpu, cpu->address);       // T2: Data fetch
}

// Zero page,X addressing - sets address and returns fetched value
static inline uint8_t fam65xx_addr_zpx(fam65xx_t* cpu) {
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);        // T1: Address fetch
    (void)fam65xx_read_cycle(cpu, base);                      // T2: Dummy read of base address
    cpu->address = (base + cpu->x) & 0xFF;
    return fam65xx_read_cycle(cpu, cpu->address);             // T3: Data fetch
}

// Zero page,Y addressing - sets address and returns fetched value
static inline uint8_t fam65xx_addr_zpy(fam65xx_t* cpu) {
    uint8_t base = fam65xx_read_cycle(cpu, cpu->pc++);        // T1: Address fetch
    (void)fam65xx_read_cycle(cpu, base);                      // T2: Dummy read of base address
    cpu->address = (base + cpu->y) & 0xFF;
    return fam65xx_read_cycle(cpu, cpu->address);             // T3: Data fetch
}

// Absolute addressing - sets address and returns fetched value
static inline uint8_t fam65xx_addr_abs(fam65xx_t* cpu) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);     // T1: Low byte fetch
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);     // T2: High byte fetch
    cpu->address = (addr_hi << 8) | addr_lo;
    return fam65xx_read_cycle(cpu, cpu->address);             // T3: Data fetch
}

// Absolute,X addressing - sets address and returns fetched value
static inline uint8_t fam65xx_addr_absx(fam65xx_t* cpu) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);     // T1: Low byte fetch
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);     // T2: High byte fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (cpu->address & 0xFF00)) {
        (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // T3: Dummy read (page cross)
    }
    return fam65xx_read_cycle(cpu, cpu->address);             // T3/T4: Data fetch
}

// Absolute,Y addressing - sets address and returns fetched value
static inline uint8_t fam65xx_addr_absy(fam65xx_t* cpu) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);     // T1: Low byte fetch
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);     // T2: High byte fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (cpu->address & 0xFF00)) {
        (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // T3: Dummy read (page cross)
    }
    return fam65xx_read_cycle(cpu, cpu->address);             // T3/T4: Data fetch
}

// Indexed indirect (zp,X) addressing
static inline uint8_t fam65xx_addr_indx(fam65xx_t* cpu) {
    uint8_t zp_addr = fam65xx_read_cycle(cpu, cpu->pc++);     // T1: ZP base address fetch
    (void)fam65xx_read_cycle(cpu, zp_addr);                   // T2: Dummy read of ZP base
    uint8_t effective_addr = (zp_addr + cpu->x) & 0xFF;
    
    uint8_t addr_lo = fam65xx_read_cycle(cpu, effective_addr);         // T3: Low byte of target
    uint8_t addr_hi = fam65xx_read_cycle(cpu, (effective_addr + 1) & 0xFF); // T4: High byte of target
    cpu->address = (addr_hi << 8) | addr_lo;
    
    return fam65xx_read_cycle(cpu, cpu->address);             // T5: Data fetch
}

// Indirect indexed (zp),Y addressing
static inline uint8_t fam65xx_addr_indy(fam65xx_t* cpu) {
    uint8_t zp_addr = fam65xx_read_cycle(cpu, cpu->pc++);     // T1: ZP address fetch
    
    uint8_t addr_lo = fam65xx_read_cycle(cpu, zp_addr);       // T2: Low byte of base address
    uint8_t addr_hi = fam65xx_read_cycle(cpu, (zp_addr + 1) & 0xFF); // T3: High byte of base address
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (cpu->address & 0xFF00)) {
        (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // T4: Dummy read (page cross)
    }
    return fam65xx_read_cycle(cpu, cpu->address);             // T4/T5: Data fetch
}

#endif // FAM65XX_CORE_H
