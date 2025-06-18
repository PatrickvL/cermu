#ifndef MOS6502_FAMILY_CORE_H
#define MOS6502_FAMILY_CORE_H

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

// CPU features configuration (overridden by specific CPU types)
#ifndef CPU_HAS_DECIMAL_MODE
#define CPU_HAS_DECIMAL_MODE 1  // Default: support decimal mode
#endif

#ifndef CPU_HAS_IO_PORTS
#define CPU_HAS_IO_PORTS 0      // Default: no I/O ports
#endif

#ifndef CPU_HAS_ILLEGAL_OPCODES
#define CPU_HAS_ILLEGAL_OPCODES 1  // Default: support illegal opcodes
#endif

// Universal instruction dispatch using function pointers
typedef struct mos6502_family_s mos6502_family_t;
typedef void (*mos6502_family_opcode_handler_t)(mos6502_family_t* cpu);

// ============================================================================
// SHARED MOS 6502 FAMILY CPU STATE STRUCTURE
// ============================================================================

struct mos6502_family_s {
    chip_descriptor_t* desc; // Pointer to chip descriptor (must be first)
    
    // === PERFORMANCE-OPTIMIZED INTERFACE STORAGE ===
    // Bus interface (stored by value for optimal performance)
    bus_cycle_ops_t bus_interface;
    
    // Control lines interface (stored by value for optimal performance) 
    control_lines_interface_t control_interface;
    
    // === SHARED STATE POINTERS ===
    // These CANNOT be copied - must remain as pointers to shared system state
    system_lines_t* system_lines;  // Shared system-wide line state
    
    // === DIRECT RAM ACCESS (for zero page optimization) ===
    // Direct RAM accessors to avoid circular dependency with bus interface
    access_callback_t ram_access;  // Consolidated RAM access interface
    
    // === CPU INTERNAL STATE (shared by all 6502 family) ===
    uint16_t address;   // Address for current instruction
    
    // Interception support state
    bool intercepting;                           // True if interception is active
    mos6502_family_opcode_handler_t opcode_handlers[256]; // Per-CPU handler table
    
    // CPU Registers (standard 6502 family)
    uint16_t pc;        // Program Counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X Index Register
    uint8_t y;          // Y Index Register
    uint8_t sp;         // Stack Pointer
    uint8_t p;          // Processor Status Register
    
    // === CPU-SPECIFIC EXTENSIONS ===
    // Different family members can add their own fields here
    // For example, 6510 will add I/O ports, 65C02 might add new registers
    void* cpu_specific_data;  // Pointer to CPU-specific extensions
};

// ============================================================================
// SHARED MACROS FOR PERFORMANCE-CRITICAL CODE
// ============================================================================

// ============================================================================
// SHARED MOS 6502 FAMILY MACROS (used by all family members)
// ============================================================================

// Macro utilities for generating unique labels
#define M6502_CONCAT_IMPL(a, b) a ## b
#define M6502_CONCAT(a, b) M6502_CONCAT_IMPL(a, b)
#define M6502_UNIQUE_LABEL(prefix) M6502_CONCAT(prefix, __LINE__)

// Cycle timing macros (shared by all family members)
#define M6502_INTRA_CYCLE(cpu) do { \
    (cpu)->bus_interface.cycle_tick((cpu)->bus_interface.context); \
} while(0)

// Family-specific version for backward compatibility
#define MOS6502_FAMILY_INTRA_CYCLE(cpu) M6502_INTRA_CYCLE(cpu)

// Ready check and wait with automatic stall handling
#define M6502_READY(cpu) M6502_TEST_RDY(cpu)
#define M6502_WAIT_READY(cpu) do { \
    M6502_UNIQUE_LABEL(cpu_ready_stall): \
    if (unlikely(M6502_READY(cpu))) { \
        M6502_INTRA_CYCLE(cpu); \
        goto M6502_UNIQUE_LABEL(cpu_ready_stall); \
    } \
} while(0)

// Control line access and testing (shared)
#define M6502_CONTROL_LINES(cpu) \
    ((cpu)->control_interface.get_lines((cpu)->control_interface.context))

#define M6502_TEST_IRQ(cpu) (M6502_CONTROL_LINES(cpu) & SYS_MASK_IRQ)
#define M6502_TEST_NMI(cpu) (M6502_CONTROL_LINES(cpu) & SYS_MASK_NMI)
#define M6502_TEST_RDY(cpu) (M6502_CONTROL_LINES(cpu) & SYS_MASK_RDY)

// System lines access macros for direct system state operations
#define M6502_SYSTEM_LINES_TEST(cpu, mask) SYS_LINES_TEST((cpu)->system_lines, mask)
#define M6502_SYSTEM_LINES_SET(cpu, mask) SYS_LINES_SET((cpu)->system_lines, mask)
#define M6502_SYSTEM_LINES_CLEAR(cpu, mask) SYS_LINES_CLEAR((cpu)->system_lines, mask)

// Instruction dispatch macros (shared - but implementation-specific functions)
#define M6502_NEXT_INSTRUCTION_DISPATCH(cpu, read_func, dispatch_func) do { \
    uint8_t opcode = read_func(cpu, (cpu)->pc++); \
    dispatch_func(cpu, opcode); \
} while(0)

#define M6502_NEXT_INSTRUCTION(cpu, interrupt_func, read_func, dispatch_func) do { \
    if (unlikely(M6502_TEST_IRQ(cpu) || M6502_TEST_NMI(cpu))) { \
        interrupt_func(cpu); \
        return; \
    } \
    M6502_WAIT_READY(cpu); \
    M6502_NEXT_INSTRUCTION_DISPATCH(cpu, read_func, dispatch_func); \
    return; \
} while(0)

// Universal instruction dispatch using function pointers
// ============================================================================
// SHARED FUNCTION DECLARATIONS
// ============================================================================

// Core memory and cycle functions (shared by all family members)
uint8_t mos6502_family_read_cycle(mos6502_family_t* cpu, uint16_t address);
void mos6502_family_write_cycle(mos6502_family_t* cpu, uint16_t address, uint8_t value);
void mos6502_family_opcode_dispatch(mos6502_family_t* cpu, uint8_t opcode);
void mos6502_family_interrupt_handler(mos6502_family_t* cpu);

// Family-specific versions of shared macros
#define MOS6502_FAMILY_OPCODE_FOOTER(cpu) \
    M6502_NEXT_INSTRUCTION(cpu, mos6502_family_interrupt_handler, mos6502_family_read_cycle, mos6502_family_opcode_dispatch)

// Stack operations (shared)
void mos6502_family_push(mos6502_family_t* cpu, uint8_t value);
uint8_t mos6502_family_pull(mos6502_family_t* cpu);

// Flag operations (shared)
static inline bool mos6502_family_get_flag(mos6502_family_t* cpu, uint8_t flag) {
    return (cpu->p & flag) != 0;
}

static inline void mos6502_family_set_flag(mos6502_family_t* cpu, uint8_t flag, bool value) {
    if (value) {
        cpu->p |= flag;
    } else {
        cpu->p &= ~flag;
    }
}

static inline void mos6502_family_set_nz_flags(mos6502_family_t* cpu, uint8_t value) {
    mos6502_family_set_flag(cpu, FLAG_Z, value == 0);
    mos6502_family_set_flag(cpu, FLAG_N, (value & 0x80) != 0);
}

// Interrupt handling (shared)
void mos6502_family_interrupt_sequence(mos6502_family_t* cpu, uint8_t status_flags, uint16_t vector_addr);
void mos6502_family_interrupt_handler(mos6502_family_t* cpu);

// Interception support (shared)
void mos6502_family_start_intercept(mos6502_family_t* cpu);
void mos6502_family_stop_intercept(mos6502_family_t* cpu);
bool mos6502_family_is_intercepting(mos6502_family_t* cpu);

// Single step execution (shared)
bool mos6502_family_step(mos6502_family_t* cpu);

// ============================================================================
// SHARED OPCODE HANDLER TABLE INITIALIZATION
// ============================================================================

// Initialize default opcode handler table (shared by all family members)
void mos6502_family_init_opcode_table(mos6502_family_t* cpu);

// Override specific opcodes for CPU variants (e.g., disable decimal mode)
void mos6502_family_override_opcode(mos6502_family_t* cpu, uint8_t opcode, mos6502_family_opcode_handler_t handler);

// Default opcode handler table (shared base)
extern mos6502_family_opcode_handler_t mos6502_family_default_handlers[256];

// ============================================================================
// SHARED OPCODE OPERATION IMPLEMENTATIONS  
// ============================================================================

// Shared operation functions (CPU-agnostic implementations)
void mos6502_family_op_adc(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_sbc(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_and(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_ora(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_eor(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_cmp(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_cpx(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_cpy(mos6502_family_t* cpu, uint8_t value);
void mos6502_family_op_bit(mos6502_family_t* cpu, uint8_t value);

// Note: mos6502_family_arithmetic_helper and mos6502_family_branch_helper 
// are now defined as static inline functions at the end of this header

// Opcode table initialization
void mos6502_family_init_opcode_table(mos6502_family_t* cpu);

// Default opcode implementations (used by all family members unless overridden)
void mos6502_family_adc_immediate(mos6502_family_t* cpu);
void mos6502_family_adc_zero_page(mos6502_family_t* cpu);
void mos6502_family_adc_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_adc_absolute(mos6502_family_t* cpu);
void mos6502_family_adc_absolute_x(mos6502_family_t* cpu);
void mos6502_family_adc_absolute_y(mos6502_family_t* cpu);
void mos6502_family_adc_indirect_x(mos6502_family_t* cpu);
void mos6502_family_adc_indirect_y(mos6502_family_t* cpu);

void mos6502_family_sbc_immediate(mos6502_family_t* cpu);
void mos6502_family_sbc_zero_page(mos6502_family_t* cpu);
void mos6502_family_sbc_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_sbc_absolute(mos6502_family_t* cpu);
void mos6502_family_sbc_absolute_x(mos6502_family_t* cpu);
void mos6502_family_sbc_absolute_y(mos6502_family_t* cpu);
void mos6502_family_sbc_indirect_x(mos6502_family_t* cpu);
void mos6502_family_sbc_indirect_y(mos6502_family_t* cpu);

void mos6502_family_and_immediate(mos6502_family_t* cpu);
void mos6502_family_and_zero_page(mos6502_family_t* cpu);
void mos6502_family_and_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_and_absolute(mos6502_family_t* cpu);
void mos6502_family_and_absolute_x(mos6502_family_t* cpu);
void mos6502_family_and_absolute_y(mos6502_family_t* cpu);
void mos6502_family_and_indirect_x(mos6502_family_t* cpu);
void mos6502_family_and_indirect_y(mos6502_family_t* cpu);

void mos6502_family_ora_immediate(mos6502_family_t* cpu);
void mos6502_family_ora_zero_page(mos6502_family_t* cpu);
void mos6502_family_ora_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_ora_absolute(mos6502_family_t* cpu);
void mos6502_family_ora_absolute_x(mos6502_family_t* cpu);
void mos6502_family_ora_absolute_y(mos6502_family_t* cpu);
void mos6502_family_ora_indirect_x(mos6502_family_t* cpu);
void mos6502_family_ora_indirect_y(mos6502_family_t* cpu);

void mos6502_family_eor_immediate(mos6502_family_t* cpu);
void mos6502_family_eor_zero_page(mos6502_family_t* cpu);
void mos6502_family_eor_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_eor_absolute(mos6502_family_t* cpu);
void mos6502_family_eor_absolute_x(mos6502_family_t* cpu);
void mos6502_family_eor_absolute_y(mos6502_family_t* cpu);
void mos6502_family_eor_indirect_x(mos6502_family_t* cpu);
void mos6502_family_eor_indirect_y(mos6502_family_t* cpu);

// Add similar declarations for all other operations...

// Control flow operations
void mos6502_family_bpl(mos6502_family_t* cpu);
void mos6502_family_bmi(mos6502_family_t* cpu);
void mos6502_family_bvc(mos6502_family_t* cpu);
void mos6502_family_bvs(mos6502_family_t* cpu);
void mos6502_family_bcc(mos6502_family_t* cpu);
void mos6502_family_bcs(mos6502_family_t* cpu);
void mos6502_family_bne(mos6502_family_t* cpu);
void mos6502_family_beq(mos6502_family_t* cpu);

void mos6502_family_jmp_absolute(mos6502_family_t* cpu);
void mos6502_family_jmp_indirect(mos6502_family_t* cpu);
void mos6502_family_jsr(mos6502_family_t* cpu);
void mos6502_family_rts(mos6502_family_t* cpu);

void mos6502_family_brk(mos6502_family_t* cpu);
void mos6502_family_rti(mos6502_family_t* cpu);

// Flag operations
void mos6502_family_clc(mos6502_family_t* cpu);
void mos6502_family_sec(mos6502_family_t* cpu);
void mos6502_family_cli(mos6502_family_t* cpu);
void mos6502_family_sei(mos6502_family_t* cpu);
void mos6502_family_clv(mos6502_family_t* cpu);
void mos6502_family_cld(mos6502_family_t* cpu);
void mos6502_family_sed(mos6502_family_t* cpu);

// Register operations
void mos6502_family_tax(mos6502_family_t* cpu);
void mos6502_family_tay(mos6502_family_t* cpu);
void mos6502_family_txa(mos6502_family_t* cpu);
void mos6502_family_tya(mos6502_family_t* cpu);
void mos6502_family_tsx(mos6502_family_t* cpu);
void mos6502_family_txs(mos6502_family_t* cpu);

void mos6502_family_pha(mos6502_family_t* cpu);
void mos6502_family_pla(mos6502_family_t* cpu);
void mos6502_family_php(mos6502_family_t* cpu);
void mos6502_family_plp(mos6502_family_t* cpu);

void mos6502_family_inx(mos6502_family_t* cpu);
void mos6502_family_iny(mos6502_family_t* cpu);
void mos6502_family_dex(mos6502_family_t* cpu);
void mos6502_family_dey(mos6502_family_t* cpu);

void mos6502_family_inc_zero_page(mos6502_family_t* cpu);
void mos6502_family_inc_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_inc_absolute(mos6502_family_t* cpu);
void mos6502_family_inc_absolute_x(mos6502_family_t* cpu);

void mos6502_family_dec_zero_page(mos6502_family_t* cpu);
void mos6502_family_dec_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_dec_absolute(mos6502_family_t* cpu);
void mos6502_family_dec_absolute_x(mos6502_family_t* cpu);

// Shift operations
void mos6502_family_asl_accumulator(mos6502_family_t* cpu);
void mos6502_family_asl_zero_page(mos6502_family_t* cpu);
void mos6502_family_asl_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_asl_absolute(mos6502_family_t* cpu);
void mos6502_family_asl_absolute_x(mos6502_family_t* cpu);

void mos6502_family_lsr_accumulator(mos6502_family_t* cpu);
void mos6502_family_lsr_zero_page(mos6502_family_t* cpu);
void mos6502_family_lsr_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_lsr_absolute(mos6502_family_t* cpu);
void mos6502_family_lsr_absolute_x(mos6502_family_t* cpu);

void mos6502_family_rol_accumulator(mos6502_family_t* cpu);
void mos6502_family_rol_zero_page(mos6502_family_t* cpu);
void mos6502_family_rol_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_rol_absolute(mos6502_family_t* cpu);
void mos6502_family_rol_absolute_x(mos6502_family_t* cpu);

void mos6502_family_ror_accumulator(mos6502_family_t* cpu);
void mos6502_family_ror_zero_page(mos6502_family_t* cpu);
void mos6502_family_ror_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_ror_absolute(mos6502_family_t* cpu);
void mos6502_family_ror_absolute_x(mos6502_family_t* cpu);

// Memory operations
void mos6502_family_lda_immediate(mos6502_family_t* cpu);
void mos6502_family_lda_zero_page(mos6502_family_t* cpu);
void mos6502_family_lda_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_lda_absolute(mos6502_family_t* cpu);
void mos6502_family_lda_absolute_x(mos6502_family_t* cpu);
void mos6502_family_lda_absolute_y(mos6502_family_t* cpu);
void mos6502_family_lda_indirect_x(mos6502_family_t* cpu);
void mos6502_family_lda_indirect_y(mos6502_family_t* cpu);

void mos6502_family_ldx_immediate(mos6502_family_t* cpu);
void mos6502_family_ldx_zero_page(mos6502_family_t* cpu);
void mos6502_family_ldx_zero_page_y(mos6502_family_t* cpu);
void mos6502_family_ldx_absolute(mos6502_family_t* cpu);
void mos6502_family_ldx_absolute_y(mos6502_family_t* cpu);

void mos6502_family_ldy_immediate(mos6502_family_t* cpu);
void mos6502_family_ldy_zero_page(mos6502_family_t* cpu);
void mos6502_family_ldy_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_ldy_absolute(mos6502_family_t* cpu);
void mos6502_family_ldy_absolute_x(mos6502_family_t* cpu);

void mos6502_family_sta_zero_page(mos6502_family_t* cpu);
void mos6502_family_sta_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_sta_absolute(mos6502_family_t* cpu);
void mos6502_family_sta_absolute_x(mos6502_family_t* cpu);
void mos6502_family_sta_absolute_y(mos6502_family_t* cpu);
void mos6502_family_sta_indirect_x(mos6502_family_t* cpu);
void mos6502_family_sta_indirect_y(mos6502_family_t* cpu);

void mos6502_family_stx_zero_page(mos6502_family_t* cpu);
void mos6502_family_stx_zero_page_y(mos6502_family_t* cpu);
void mos6502_family_stx_absolute(mos6502_family_t* cpu);

void mos6502_family_sty_zero_page(mos6502_family_t* cpu);
void mos6502_family_sty_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_sty_absolute(mos6502_family_t* cpu);

// Compare and bit operations  
void mos6502_family_cmp_immediate(mos6502_family_t* cpu);
void mos6502_family_cmp_zero_page(mos6502_family_t* cpu);
void mos6502_family_cmp_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_cmp_absolute(mos6502_family_t* cpu);
void mos6502_family_cmp_absolute_x(mos6502_family_t* cpu);
void mos6502_family_cmp_absolute_y(mos6502_family_t* cpu);
void mos6502_family_cmp_indirect_x(mos6502_family_t* cpu);
void mos6502_family_cmp_indirect_y(mos6502_family_t* cpu);

void mos6502_family_cpx_immediate(mos6502_family_t* cpu);
void mos6502_family_cpx_zero_page(mos6502_family_t* cpu);
void mos6502_family_cpx_absolute(mos6502_family_t* cpu);

void mos6502_family_cpy_immediate(mos6502_family_t* cpu);
void mos6502_family_cpy_zero_page(mos6502_family_t* cpu);
void mos6502_family_cpy_absolute(mos6502_family_t* cpu);

void mos6502_family_bit_zero_page(mos6502_family_t* cpu);
void mos6502_family_bit_absolute(mos6502_family_t* cpu);

// NOP and JAM operations
void mos6502_family_nop(mos6502_family_t* cpu);
void mos6502_family_nop_immediate(mos6502_family_t* cpu);
void mos6502_family_nop_zero_page(mos6502_family_t* cpu);
void mos6502_family_nop_zero_page_x(mos6502_family_t* cpu);
void mos6502_family_nop_absolute(mos6502_family_t* cpu);
void mos6502_family_nop_absolute_x(mos6502_family_t* cpu);

void mos6502_family_jam(mos6502_family_t* cpu);

// ============================================================================
// ADDRESSING MODE HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Immediate addressing - returns the immediate value
static inline uint8_t mos6502_family_addr_imm(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->pc++);
}

// Zero page addressing - sets address and returns fetched value
static inline uint8_t mos6502_family_addr_zp(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// Zero page,X addressing - sets address and returns fetched value
static inline uint8_t mos6502_family_addr_zpx(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// Zero page,Y addressing - sets address and returns fetched value
static inline uint8_t mos6502_family_addr_zpy(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t base = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->y) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// Absolute addressing - sets address and returns fetched value
static inline uint8_t mos6502_family_addr_abs(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// Absolute,X addressing - sets address and returns fetched value
static inline uint8_t mos6502_family_addr_absx(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (cpu->address & 0xFF00)) {
        MOS6502_FAMILY_INTRA_CYCLE(cpu);
        (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    }
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// Absolute,Y addressing - sets address and returns fetched value
static inline uint8_t mos6502_family_addr_absy(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (cpu->address & 0xFF00)) {
        MOS6502_FAMILY_INTRA_CYCLE(cpu);
        (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read
    }
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// Indexed indirect (zp,X) addressing
static inline uint8_t mos6502_family_addr_indx(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, zp_addr); // Dummy read
    uint8_t effective_addr = (zp_addr + cpu->x) & 0xFF;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, effective_addr);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (effective_addr + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// Indirect indexed (zp),Y addressing
static inline uint8_t mos6502_family_addr_indy(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc++);
    
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, zp_addr);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->y;
    
    // Check for page boundary crossing
    if ((base_addr & 0xFF00) != (cpu->address & 0xFF00)) {
        MOS6502_FAMILY_INTRA_CYCLE(cpu);
        (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->y) & 0xFF)); // Dummy read
    }
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, cpu->address);
}

// ============================================================================
// PERFORMANCE-CRITICAL INLINE HELPER FUNCTIONS
// ============================================================================

// Inline simple arithmetic operations for maximum performance
static inline void mos6502_family_op_and_inline(mos6502_family_t* cpu, uint8_t value) {
    cpu->a &= value;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

static inline void mos6502_family_op_ora_inline(mos6502_family_t* cpu, uint8_t value) {
    cpu->a |= value;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

static inline void mos6502_family_op_eor_inline(mos6502_family_t* cpu, uint8_t value) {
    cpu->a ^= value;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

// Inline load helper (replaces DEFINE_LOAD_OP macro)
static inline void mos6502_family_load_helper(mos6502_family_t* cpu, 
    uint8_t (*addr_func)(mos6502_family_t*), 
    void (*op_func)(mos6502_family_t*, uint8_t)) {
    uint8_t value = addr_func(cpu);
    op_func(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Inline store helper (replaces DEFINE_STORE_OP macro)
static inline void mos6502_family_store_helper(mos6502_family_t* cpu, 
    void (*addr_store_func)(mos6502_family_t*, uint8_t), 
    uint8_t reg_value) {
    addr_store_func(cpu, reg_value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Inline register transfer with flags (replaces DEFINE_REG_XFER macro)
static inline void mos6502_family_register_transfer_with_flags(mos6502_family_t* cpu, 
    uint8_t* dest, uint8_t src_value) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    *dest = src_value;
    mos6502_family_set_nz_flags(cpu, *dest);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Inline register transfer without flags (replaces DEFINE_REG_XFER_NOFLAG macro)
static inline void mos6502_family_register_transfer_no_flags(mos6502_family_t* cpu, 
    uint8_t* dest, uint8_t src_value) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    *dest = src_value;
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Inline register increment/decrement (replaces DEFINE_REG_INCDEC macro)
static inline void mos6502_family_register_inc_dec(mos6502_family_t* cpu, 
    uint8_t* reg, int8_t delta) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    *reg += delta;
    mos6502_family_set_nz_flags(cpu, *reg);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Inline flag operations (replaces DEFINE_FLAG_CLEAR/SET macros)
static inline void mos6502_family_flag_clear_helper(mos6502_family_t* cpu, uint8_t flag) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, flag, false);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

static inline void mos6502_family_flag_set_helper(mos6502_family_t* cpu, uint8_t flag) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    mos6502_family_set_flag(cpu, flag, true);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// READ-MODIFY-WRITE INLINE HELPERS (for shifts, INC/DEC operations)
// ============================================================================

// Accumulator read-modify-write operations (2 cycles)
static inline void mos6502_family_rmw_accumulator(mos6502_family_t* cpu, 
    uint8_t (*operation)(mos6502_family_t*, uint8_t)) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = operation(cpu, cpu->a);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Zero page read-modify-write operations
static inline void mos6502_family_rmw_zero_page(mos6502_family_t* cpu, 
    uint8_t (*operation)(mos6502_family_t*, uint8_t)) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Zero page,X read-modify-write operations
static inline void mos6502_family_rmw_zero_page_x(mos6502_family_t* cpu, 
    uint8_t (*operation)(mos6502_family_t*, uint8_t)) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    cpu->address = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, cpu->address); // Dummy read
    cpu->address = (cpu->address + cpu->x) & 0xFF;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Absolute read-modify-write operations
static inline void mos6502_family_rmw_absolute(mos6502_family_t* cpu, 
    uint8_t (*operation)(mos6502_family_t*, uint8_t)) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address = addr_lo;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    cpu->address |= (addr_hi << 8);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Absolute,X read-modify-write operations
static inline void mos6502_family_rmw_absolute_x(mos6502_family_t* cpu, 
    uint8_t (*operation)(mos6502_family_t*, uint8_t)) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc++);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc++);
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t value = mos6502_family_read_cycle(cpu, cpu->address);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, cpu->address, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Specialized inline arithmetic helpers for common simple operations
#define MOS6502_FAMILY_AND_HELPER(cpu, addr_func) do { \
    uint8_t value = addr_func(cpu); \
    mos6502_family_op_and_inline(cpu, value); \
    MOS6502_FAMILY_OPCODE_FOOTER(cpu); \
} while(0)

#define MOS6502_FAMILY_ORA_HELPER(cpu, addr_func) do { \
    uint8_t value = addr_func(cpu); \
    mos6502_family_op_ora_inline(cpu, value); \
    MOS6502_FAMILY_OPCODE_FOOTER(cpu); \
} while(0)

#define MOS6502_FAMILY_EOR_HELPER(cpu, addr_func) do { \
    uint8_t value = addr_func(cpu); \
    mos6502_family_op_eor_inline(cpu, value); \
    MOS6502_FAMILY_OPCODE_FOOTER(cpu); \
} while(0)

// Inline arithmetic helper for maximum performance
static inline void mos6502_family_arithmetic_helper(mos6502_family_t* cpu, 
    uint8_t (*addr_func)(mos6502_family_t*), 
    void (*op_func)(mos6502_family_t*, uint8_t)) {
    uint8_t value = addr_func(cpu);
    op_func(cpu, value);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Inline branch helper for maximum performance
static inline void mos6502_family_branch_helper(mos6502_family_t* cpu, bool condition) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    int8_t offset = (int8_t)mos6502_family_read_cycle(cpu, cpu->pc++);
    
    if (condition) {
        uint16_t old_pc = cpu->pc;
        cpu->pc += offset;
        
        // Extra cycle for taking the branch
        MOS6502_FAMILY_INTRA_CYCLE(cpu);
        (void)mos6502_family_read_cycle(cpu, old_pc); // Dummy read
        
        // Extra cycle if page boundary crossed
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00)) {
            MOS6502_FAMILY_INTRA_CYCLE(cpu);
            (void)mos6502_family_read_cycle(cpu, (old_pc & 0xFF00) | (cpu->pc & 0xFF)); // Dummy read
        }
    }    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// ============================================================================
// INLINE OPERATION FUNCTIONS (for RMW and load/store operations)
// ============================================================================

// Shift and rotate operations (inline for maximum performance)
static inline uint8_t mos6502_family_op_asl(mos6502_family_t* cpu, uint8_t value) {
    mos6502_family_set_flag(cpu, FLAG_C, value & 0x80);
    value <<= 1;
    mos6502_family_set_nz_flags(cpu, value);
    return value;
}

static inline uint8_t mos6502_family_op_lsr(mos6502_family_t* cpu, uint8_t value) {
    mos6502_family_set_flag(cpu, FLAG_C, value & 0x01);
    value >>= 1;
    mos6502_family_set_nz_flags(cpu, value);
    return value;
}

static inline uint8_t mos6502_family_op_rol(mos6502_family_t* cpu, uint8_t value) {
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, value & 0x80);
    value = (value << 1) | (old_carry ? 1 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    return value;
}

static inline uint8_t mos6502_family_op_ror(mos6502_family_t* cpu, uint8_t value) {
    bool old_carry = mos6502_family_get_flag(cpu, FLAG_C);
    mos6502_family_set_flag(cpu, FLAG_C, value & 0x01);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    mos6502_family_set_nz_flags(cpu, value);
    return value;
}

// Increment/decrement operations (inline for maximum performance)
static inline uint8_t mos6502_family_op_inc(mos6502_family_t* cpu, uint8_t value) {
    value++;
    mos6502_family_set_nz_flags(cpu, value);
    return value;
}

static inline uint8_t mos6502_family_op_dec(mos6502_family_t* cpu, uint8_t value) {
    value--;
    mos6502_family_set_nz_flags(cpu, value);
    return value;
}

// Load operations (inline for maximum performance)
static inline void mos6502_family_op_lda(mos6502_family_t* cpu, uint8_t value) {
    cpu->a = value;
    mos6502_family_set_nz_flags(cpu, cpu->a);
}

static inline void mos6502_family_op_ldx(mos6502_family_t* cpu, uint8_t value) {
    cpu->x = value;
    mos6502_family_set_nz_flags(cpu, cpu->x);
}

static inline void mos6502_family_op_ldy(mos6502_family_t* cpu, uint8_t value) {
    cpu->y = value;
    mos6502_family_set_nz_flags(cpu, cpu->y);
}

#endif // MOS6502_FAMILY_CORE_H
