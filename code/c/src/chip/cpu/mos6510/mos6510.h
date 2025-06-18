#ifndef MOS6510_H
#define MOS6510_H

#include "../../../core/aiemuc.h"
#include "../../../core/chip.h"
#include "../../../core/bus_cycle_interface.h"
#include "../../../core/control_lines_interface.h"
#include "../../../core/system_lines.h"
#include "../../../core/system.h"
#include "../mos6502_family/mos6502_family_core.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// MOS6510 I/O port interface
typedef struct {
    void* context;
    uint8_t (*read_external_pins)(void* context, uint8_t port_value, uint8_t ddr);
    void (*output_pins_changed)(void* context, uint8_t port_value, uint8_t ddr);
} mos6510_io_port_interface_t;

// MOS6510 specific constants
#define MOS6510_MASK_IRQ    SYS_MASK_IRQ
#define MOS6510_MASK_NMI    SYS_MASK_NMI
#define MOS6510_MASK_RDY    SYS_MASK_RDY

// Forward declaration to resolve circular dependencies
typedef struct mos6510_s mos6510_t;

// MOS6510 opcode handler type (compatible with family handlers)
typedef void (*mos6510_opcode_handler_t)(mos6510_t* cpu);

// ============================================================================
// MOS 6510 CPU EMULATION - Extends 6502 family with I/O ports
// ============================================================================

// MOS6510 CPU state structure - extends the family structure
struct mos6510_s {
    // === BASE 6502 FAMILY STRUCTURE (MUST BE FIRST) ===
    // This allows safe casting between mos6510_t* and mos6502_family_t*
    mos6502_family_t base;
    
    // === MOS6510-SPECIFIC EXTENSIONS ===
    // I/O port interface (stored by value for optimal performance)
    mos6510_io_port_interface_t io_interface;
      // Direct RAM access (for zero bank $0002-$0FFF to avoid circular dependency)
    access_callback_t ram_access;  // Consolidated RAM access interface
    
    // I/O Ports (MOS6510-specific)
    uint8_t io_port[2]; // 0:DDR, 1:Port
};

// Global instruction table
extern mos6510_opcode_handler_t mos6510_opcode_handlers[256];


// --- Interception support: replace handlers with stubs until next opcode ---
/**
 * Begin intercepting the next opcode fetch for the specified CPU.  All 256 handlers will be
 * replaced with an internal stub that restores the original table on its
 * first invocation.
 */
void mos6510_start_intercept(mos6510_t* cpu);

/**
 * Cancel interception and restore the original handler table immediately for the specified CPU.
 */
void mos6510_stop_intercept(mos6510_t* cpu);

/**
 * Check if interception is currently active for the specified CPU.
 */
bool mos6510_is_intercepting(mos6510_t* cpu);

// MOS6510 Status Register Flags
#define FLAG_C  0x01    // Carry
#define FLAG_Z  0x02    // Zero
#define FLAG_I  0x04    // Interrupt Disable
#define FLAG_D  0x08    // Decimal Mode
#define FLAG_B  0x10    // Break Command
#define FLAG_U  0x20    // Unused (always 1)
#define FLAG_V  0x40    // Overflow
#define FLAG_N  0x80    // Negative

// MOS6510 Control Line Masks (mapped to system line positions)
#define MOS6510_MASK_IRQ    SYS_MASK_IRQ    // IRQ line mask
#define MOS6510_MASK_NMI    SYS_MASK_NMI    // NMI line mask  
#define MOS6510_MASK_RDY    SYS_MASK_RDY    // RDY line mask

// ============================================================================
// I/O PORT EMULATION - Direct handling in CPU read/write cycles
// ============================================================================

// ============================================================================
// MOS6510 ZERO BANK I/O PORT ACCESSORS
// ============================================================================

// Compute effective port output: outputs from Data when DDR=1, else external data
static inline uint8_t mos6510_io_mask(mos6510_t* cpu, uint8_t value)
{
    uint8_t ddr = cpu->io_port[0];
    uint8_t data = cpu->io_port[1];
    return (data & ddr) | (value & ~ddr);
}

// MOS6510 zero bank I/O port read (addresses $0000/$0001)
static inline uint8_t mos6510_ioport_read(mos6510_t* cpu, uint16_t addr) {
    if (addr == 0) {
        // Return Data Direction Register
        return cpu->io_port[0];
    } else {        // Return port: outputs defined by DDR bits, inputs from external pins
        uint8_t external = cpu->io_interface.read_external_pins(cpu->io_interface.context, cpu->io_port[1], cpu->io_port[0]);
        return mos6510_io_mask(cpu, external);
    }
}

// MOS6510 zero bank I/O port write (addresses $0000/$0001)
static inline void mos6510_ioport_write(mos6510_t* cpu, uint16_t addr, uint8_t value) {
    // Update Data Direction / Data register
    cpu->io_port[addr] = value;
    
    // Notify system of output pin changes
    uint8_t ddr = cpu->io_port[0];
    uint8_t port_data = cpu->io_port[1];
    cpu->io_interface.output_pins_changed(cpu->io_interface.context, port_data, ddr);
}

// Memory access functions using optimized direct callbacks
static inline uint8_t mos6510_read_cycle(mos6510_t* cpu, uint16_t addr) {
    // All addresses go through bus interface - banking system routes zero bank to chip functions
    uint8_t result = cpu->base.bus_interface.bus_read(cpu->base.bus_interface.context, addr);
    
    // Execute one cycle on other non-CPU chips after the bus operation
    cpu->base.bus_interface.cycle_tick(cpu->base.bus_interface.context);
    
    return result;
}

static inline void mos6510_write_cycle(mos6510_t* cpu, uint16_t addr, uint8_t value) {
    // All addresses go through bus interface - banking system routes zero bank to chip functions
    cpu->base.bus_interface.bus_write(cpu->base.bus_interface.context, addr, value);
    
    // Execute one cycle on other non-CPU chips after the bus operation
    cpu->base.bus_interface.cycle_tick(cpu->base.bus_interface.context);
}

// Forward declaration for functions used in macros
void mos6510_interrupt_handler(mos6510_t* cpu);
void c64_non_cpu_cycle(void* c64);  // c64_t* - forward declaration with opaque pointer

// CPU opcode dispatch function
static inline void mos6510_opcode_dispatch(mos6510_t* cpu, uint8_t opcode) {
    mos6510_opcode_handler_t handler = (mos6510_opcode_handler_t)cpu->base.opcode_handlers[opcode];
    handler(cpu);
}

// MOS6510-specific versions of shared macros - using wrapper functions to handle type conversion
// Since base is the first member of mos6510_t, we can safely cast directly
static inline void mos6510_interrupt_handler_wrapper(mos6502_family_t* base_cpu) {
    mos6510_t* cpu = (mos6510_t*)base_cpu;
    mos6510_interrupt_handler(cpu);
}

static inline uint8_t mos6510_read_cycle_wrapper(mos6502_family_t* base_cpu, uint16_t addr) {
    mos6510_t* cpu = (mos6510_t*)base_cpu;
    return mos6510_read_cycle(cpu, addr);
}

static inline void mos6510_opcode_dispatch_wrapper(mos6502_family_t* base_cpu, uint8_t opcode) {
    mos6510_t* cpu = (mos6510_t*)base_cpu;
    mos6510_opcode_dispatch(cpu, opcode);
}

#define MOS6510_OPCODE_FOOTER(cpu) \
    M6502_NEXT_INSTRUCTION(&((cpu)->base), mos6510_interrupt_handler_wrapper, mos6510_read_cycle_wrapper, mos6510_opcode_dispatch_wrapper)

// ============================================================================
// PERFORMANCE-OPTIMIZED MACROS FOR CODE DEDUPLICATION
// ============================================================================

// Cycle timing macro for intra-instruction cycles
#define MOS6510_INTRA_CYCLE(cpu) do { \
    (cpu)->base.bus_interface.cycle_tick((cpu)->base.bus_interface.context); \
} while(0)

// MOS6510_CONTROL_LINES - Get control lines with zero-indirection access
#define MOS6510_CONTROL_LINES(cpu) \
    ((cpu)->control_interface.get_lines((cpu)->control_interface.context))

// Test specific control lines using lightweight macros
#define MOS6510_TEST_IRQ(cpu) (MOS6510_CONTROL_LINES(cpu) & MOS6510_MASK_IRQ)
#define MOS6510_TEST_NMI(cpu) (MOS6510_CONTROL_LINES(cpu) & MOS6510_MASK_NMI)
#define MOS6510_TEST_RDY(cpu) (MOS6510_CONTROL_LINES(cpu) & MOS6510_MASK_RDY)

// System lines access macros for direct system state operations
#define MOS6510_SYSTEM_LINES_TEST(cpu, mask) SYS_LINES_TEST((cpu)->system_lines, mask)
#define MOS6510_SYSTEM_LINES_SET(cpu, mask) SYS_LINES_SET((cpu)->system_lines, mask)
#define MOS6510_SYSTEM_LINES_CLEAR(cpu, mask) SYS_LINES_CLEAR((cpu)->system_lines, mask)

// Bus cycle operations - call bus operation then cycle tick
#define MOS6510_BUS_CYCLE(cpu) \
    ((cpu)->bus_interface.cycle_tick((cpu)->bus_interface.context))



// Flag operations (using family functions)
static inline void mos6510_set_flag(mos6510_t* cpu, uint8_t flag, bool condition) {
    mos6502_family_set_flag(&cpu->base, flag, condition);
}

static inline bool mos6510_get_flag(mos6510_t* cpu, uint8_t flag) {
    return mos6502_family_get_flag(&cpu->base, flag);
}

static inline void mos6510_set_nz_flags(mos6510_t* cpu, uint8_t value) {
    mos6502_family_set_nz_flags(&cpu->base, value);
}

// Stack operations (using family functions)
static inline void mos6510_push(mos6510_t* cpu, uint8_t data) {
    mos6502_family_push(&cpu->base, data);
}

static inline uint8_t mos6510_pop(mos6510_t* cpu) {
    return mos6502_family_pull(&cpu->base);
}

// BRK/IRQ common sequence - handles the interrupt setup portion
static inline void mos6510_interrupt_sequence(mos6510_t* cpu, uint8_t status_flags, uint16_t vector_addr) {
    // Push PC and status unconditionally (skip RDY checks)
    mos6502_family_push(&cpu->base, (cpu->base.pc >> 8) & 0xFF);
    mos6502_family_push(&cpu->base, cpu->base.pc & 0xFF);
    mos6502_family_push(&cpu->base, status_flags);
    // Set interrupt disable
    cpu->base.p |= FLAG_I;
    // Read vector low and high without RDY checks
    uint8_t pc_lo = mos6510_read_cycle(cpu, vector_addr);
    uint8_t pc_hi = mos6510_read_cycle(cpu, vector_addr + 1);
    cpu->base.pc = (pc_hi << 8) | pc_lo;
    // Note : callers will dispatch the next instruction
}


// ============================================================================
// ADDRESSING MODE HELPER FUNCTIONS (delegating to family functions)
// ============================================================================

// ============================================================================
// MOS6510 ADDRESSING MODE FUNCTIONS
// ============================================================================
// Note: Use mos6502_family_addr_* functions directly instead of wrappers
// for better performance. Examples:
//   mos6502_family_addr_imm(&cpu->base)  // Immediate addressing
//   mos6502_family_addr_zp(&cpu->base)   // Zero page addressing
//   mos6502_family_addr_abs(&cpu->base)  // Absolute addressing
// etc.

// ============================================================================
// MOS6510 ADVANCED ADDRESSING MODE FUNCTIONS  
// ============================================================================
// Note: These use family functions directly for optimal performance

// (Zero page,X) - Indexed Indirect addressing
static inline uint8_t mos6510_addr_zpx_ind(mos6510_t* cpu) {
    return mos6502_family_addr_indx(&cpu->base);
}

// (Zero page),Y - Indirect Indexed addressing
static inline uint8_t mos6510_addr_zp_ind_y(mos6510_t* cpu) {
    return mos6502_family_addr_indy(&cpu->base);
}

// ============================================================================
// STORE ADDRESS HELPER FUNCTIONS (inline for performance)  
// ============================================================================

// Zero page addressing for stores - sets address only
static inline void addr_zp_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    cpu->base.address = mos6510_read_cycle(cpu, cpu->base.pc++);
}

// Zero page,X addressing for stores - sets address only  
static inline void addr_zpx_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->base.pc++);
    MOS6510_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->base.address = (base + cpu->base.x) & 0xFF;
}

// Zero page,Y addressing for stores - sets address only
static inline void addr_zpy_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->base.pc++);
    MOS6510_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->base.address = (base + cpu->base.y) & 0xFF;
}

// Absolute addressing for stores - sets address only
static inline void addr_abs_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->base.pc++);
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->base.pc++);
    cpu->base.address = (addr_hi << 8) | addr_lo;
}

// Absolute,X addressing for stores - sets address only (with dummy read)
static inline void addr_absx_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->base.pc++);
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->base.pc++);
    cpu->base.address = (addr_hi << 8) | addr_lo;
    MOS6510_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->base.address + cpu->base.x); // Dummy read
    cpu->base.address += cpu->base.x;
}

// Absolute,Y addressing for stores - sets address only (with dummy read)
static inline void addr_absy_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->base.pc++);
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->base.pc++);
    cpu->base.address = (addr_hi << 8) | addr_lo;
    MOS6510_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->base.address + cpu->base.y); // Dummy read
    cpu->base.address += cpu->base.y;
}

// (Zero page,X) addressing for stores - sets address only
static inline void addr_zpx_ind_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->base.pc++);
    MOS6510_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    uint8_t zp_addr = (base + cpu->base.x) & 0xFF;
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    cpu->base.address = (addr_hi << 8) | addr_lo;
}

// (Zero page),Y addressing for stores - sets address only (with dummy read)
static inline void addr_zp_ind_y_store(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t zp_addr = mos6510_read_cycle(cpu, cpu->base.pc++);
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    MOS6510_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    cpu->base.address = (addr_hi << 8) | addr_lo;
    MOS6510_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->base.address + cpu->base.y); // Dummy read
    cpu->base.address += cpu->base.y;
}

// Stack push with timing control
static inline void mos6510_push_with_wait(mos6510_t* cpu, uint8_t data) {
    MOS6510_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, 0x0100 + cpu->base.sp, data);
    cpu->base.sp--;
}

// Stack pop with timing control  
static inline uint8_t mos6510_pop_with_wait(mos6510_t* cpu) {
    MOS6510_INTRA_CYCLE(cpu);
    cpu->base.sp++;
    return mos6510_read_cycle(cpu, 0x0100 + cpu->base.sp);
}

// ============================================================================
// MOS6510 OPCODE IMPLEMENTATIONS
// ============================================================================

// ============================================================================
// OPCODE FUNCTION DECLARATIONS
// ============================================================================

// ============================================================================
// ILLEGAL INSTRUCTION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Read-modify-write + register operation combo (SLO, RLA, RRA, SRE)
static inline void mos6510_illegal_rmw_combo(mos6510_t* cpu, uint8_t value, 
                                          uint8_t (*rmw_op)(mos6510_t*, uint8_t),
                                          void (*reg_op)(mos6510_t*, uint8_t)) {
    uint8_t result = rmw_op(cpu, value);
    MOS6510_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->base.address, result);
    reg_op(cpu, result);
    MOS6510_OPCODE_FOOTER(cpu);
}

// INC/DEC + register operation combo (DCP, ISC)
static inline void mos6510_illegal_inc_dec_combo(mos6510_t* cpu, uint8_t value, 
                                              int delta, void (*reg_op)(mos6510_t*, uint8_t)) {
    value += (uint8_t)delta;
    MOS6510_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->base.address, value);
    reg_op(cpu, value);
    MOS6510_OPCODE_FOOTER(cpu);
}

// Load both A and X (LAX variants)
static inline void mos6510_load_a_and_x(mos6510_t* cpu, uint8_t value) {
    cpu->base.a = value;
    cpu->base.x = value;
    mos6502_family_set_nz_flags(&cpu->base, value);
    MOS6510_OPCODE_FOOTER(cpu);
}

// Store A & X (SAX variants)
static inline void mos6510_store_a_and_x(mos6510_t* cpu, void (*addr_func)(mos6510_t*, uint8_t)) {
    addr_func(cpu, cpu->base.a & cpu->base.x);
    MOS6510_OPCODE_FOOTER(cpu);
}

// Complex store with high byte manipulation (AHX, SHX, SHY, TAS)
static inline void mos6510_complex_store(mos6510_t* cpu, uint8_t value) {
    value &= ((cpu->base.address >> 8) + 1);
    MOS6510_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->base.address, value);
    MOS6510_OPCODE_FOOTER(cpu);
}

// Immediate mode accumulator operations (ALR, ANC, ARR, AXS, XAA)
static inline void mos6510_immediate_accumulator_op(mos6510_t* cpu, void (*operation)(mos6510_t*, uint8_t)) {
    uint8_t value = mos6502_family_addr_imm(&cpu->base);
    operation(cpu, value);
    MOS6510_OPCODE_FOOTER(cpu);
}

// ============================================================================
// ILLEGAL INSTRUCTION SPECIFIC OPERATIONS (inline for performance)
// ============================================================================

// ALR operation: AND then LSR
static inline void mos6502_family_op_alr(mos6510_t* cpu, uint8_t value) {
    cpu->base.a &= value;
    mos6510_set_flag(cpu, FLAG_C, cpu->base.a & 0x01);
    cpu->base.a >>= 1;
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.a);
}

// ANC operation: AND then copy N to C
static inline void mos6502_family_op_anc(mos6510_t* cpu, uint8_t value) {
    cpu->base.a &= value;
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.a);
    mos6510_set_flag(cpu, FLAG_C, cpu->base.a & 0x80);
}

// ARR operation: AND then ROR with special V flag behavior
static inline void mos6502_family_op_arr(mos6510_t* cpu, uint8_t value) {
    cpu->base.a &= value;
    uint8_t old_carry = mos6502_family_get_flag(&cpu->base, FLAG_C) ? 1 : 0;
    mos6510_set_flag(cpu, FLAG_C, cpu->base.a & 0x01);
    cpu->base.a = (cpu->base.a >> 1) | (old_carry << 7);
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.a);
    // V flag behavior is complex for ARR
    mos6510_set_flag(cpu, FLAG_V, ((cpu->base.a >> 6) ^ (cpu->base.a >> 5)) & 1);
}

// AXS operation: (A & X) - immediate, store in X
static inline void mos6502_family_op_axs(mos6510_t* cpu, uint8_t value) {
    uint8_t temp = cpu->base.a & cpu->base.x;
    uint16_t result = temp - value;
    mos6510_set_flag(cpu, FLAG_C, result < 0x100);
    cpu->base.x = result & 0xFF;
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.x);
}

// XAA operation: Transfer X to A, then AND with immediate
static inline void mos6502_family_op_xaa(mos6510_t* cpu, uint8_t value) {
    cpu->base.a = cpu->base.x;
    cpu->base.a &= value;
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.a);
}

// SLO register operation: ORA with result
static inline void mos6502_family_op_slo_reg(mos6510_t* cpu, uint8_t value) {
    cpu->base.a |= value;
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.a);
}

// RLA register operation: AND with result
static inline void mos6502_family_op_rla_reg(mos6510_t* cpu, uint8_t value) {
    cpu->base.a &= value;
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.a);
}

// SRE register operation: EOR with result
static inline void mos6502_family_op_sre_reg(mos6510_t* cpu, uint8_t value) {
    cpu->base.a ^= value;
    mos6502_family_set_nz_flags(&cpu->base, cpu->base.a);
}

// CPU core functions
void mos6510_init(mos6510_t* cpu);
void mos6510_reset(mos6510_t* cpu);
bool mos6510_step(mos6510_t* cpu);
void mos6510_execute(mos6510_t* cpu);
bool mos6510_is_intercepting(mos6510_t* cpu);
void mos6510_nmi(mos6510_t* cpu);
void mos6510_irq(mos6510_t* cpu, uint8_t status);

// Chip descriptor
extern chip_descriptor_t mos6510_descriptor;

// Performance-optimized interface attachment functions
void mos6510_attach_bus_interface(mos6510_t* cpu, const bus_cycle_ops_t* bus_interface);
void mos6510_attach_control_lines_interface(mos6510_t* cpu, const control_lines_interface_t* control_interface);
void mos6510_attach_io_interface(mos6510_t* cpu, const mos6510_io_port_interface_t* io_interface);
void mos6510_attach_system_lines(mos6510_t* cpu, system_lines_t* system_lines);
void mos6510_attach_ram(mos6510_t* cpu, const access_callback_t* ram_access);

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
// GUI functions
void mos6510_render_debug_window(void* chip, bool* show_window);
void mos6510_render_settings_window(void* chip, bool* show_window);
#endif

#endif // MOS6510_H
