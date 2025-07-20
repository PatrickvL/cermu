#ifndef MOS6510_H
#define MOS6510_H

#include "../../../core/aiemuc.h"
#include "../../../core/chip.h"
#include "../../../core/bus_cycle_interface.h"
#include "../../../core/control_lines_interface.h"
#include "../../../core/system_lines.h"
#include "../../../core/system.h"
#include "../fam65xx/fam65xx_core.h"
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
    // This allows safe casting between mos6510_t* and fam65xx_t*
    fam65xx_t base;
    
    // === MOS6510-SPECIFIC EXTENSIONS ===
    // I/O port interface (stored by value for optimal performance)
    mos6510_io_port_interface_t io_interface;
      // Direct RAM access (for zero bank $0002-$0FFF to avoid circular dependency)
    access_callback_t ram_access;  // Consolidated RAM access interface
    
    // I/O Ports (MOS6510-specific)
    uint8_t io_port[2]; // 0:DDR, 1:Port
};

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
    printf("mos6510_ioport_write: addr=%04X value=%02X\n", addr, value);
    // Update Data Direction / Data register
    cpu->io_port[addr] = value;
    if (addr == 1) {
        // Notify system of output pin changes
        uint8_t ddr = cpu->io_port[0];
        uint8_t port_data = cpu->io_port[1];
        cpu->io_interface.output_pins_changed(cpu->io_interface.context, port_data, ddr);
    }
}

// Memory access functions using optimized direct callbacks
static inline uint8_t mos6510_read_cycle(mos6510_t* cpu, uint16_t addr) {
    // Forward to family implementation (identical logic)
    return fam65xx_read_cycle(&cpu->base, addr);
}

static inline void mos6510_write_cycle(mos6510_t* cpu, uint16_t addr, uint8_t value) {
    fam65xx_write_cycle(&cpu->base, addr, value);
}

// ============================================================================
// PERFORMANCE-OPTIMIZED MACROS FOR CODE DEDUPLICATION
// ============================================================================

// MOS6510-specific versions of shared macros

// MOS6510_CONTROL_LINES - Get control lines with zero-indirection access
#define MOS6510_CONTROL_LINES(cpu) \
    ((cpu)->control_interface.get_lines((cpu)->control_interface.context))

// Test specific control lines using lightweight macros
#define MOS6510_TEST_IRQ(cpu) (MOS6510_CONTROL_LINES(cpu) & MOS6510_MASK_IRQ)
#define MOS6510_TEST_NMI(cpu) (MOS6510_CONTROL_LINES(cpu) & MOS6510_MASK_NMI)
#define MOS6510_TEST_RDY(cpu) (MOS6510_CONTROL_LINES(cpu) & MOS6510_MASK_RDY)

// System lines access macros for direct system state operations
// Replace with direct bus_state_t access or equivalent logic
// Example: ((cpu)->bus_state->lines & mask)

#define MOS6510_OPCODE_FOOTER(cpu) \
    FAM65XX_OPCODE_FOOTER(&(cpu)->base)

// Flag operations (using family functions)
static inline void mos6510_set_flag(mos6510_t* cpu, uint8_t flag, bool condition) {
    fam65xx_set_flag(&cpu->base, flag, condition);
}

// BRK/IRQ common sequence - handles the interrupt setup portion
static inline void mos6510_interrupt_sequence(mos6510_t* cpu, uint8_t status_flags, uint16_t vector_addr) {
    // Push PC and status unconditionally (skip RDY checks)
    fam65xx_push(&cpu->base, (cpu->base.pc >> 8) & 0xFF);
    fam65xx_push(&cpu->base, cpu->base.pc & 0xFF);
    fam65xx_push(&cpu->base, status_flags);
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
// Note: Use fam65xx_addr_* functions directly instead of wrappers
// for better performance. Examples:
//   fam65xx_addr_imm(&cpu->base)  // Immediate addressing
//   fam65xx_addr_zp(&cpu->base)   // Zero page addressing
//   fam65xx_addr_abs(&cpu->base)  // Absolute addressing
// etc.

// ============================================================================
// MOS6510 ADVANCED ADDRESSING MODE FUNCTIONS  
// ============================================================================
// Note: These use family functions directly for optimal performance

// (Zero page,X) - Indexed Indirect addressing
static inline uint8_t mos6510_addr_zpx_ind(mos6510_t* cpu) {
    return fam65xx_addr_indx(&cpu->base);
}

// (Zero page),Y - Indirect Indexed addressing
static inline uint8_t mos6510_addr_zp_ind_y(mos6510_t* cpu) {
    return fam65xx_addr_indy(&cpu->base);
}

// ============================================================================
// STORE ADDRESS HELPER FUNCTIONS (inline for performance)  
// ============================================================================

// Zero page addressing for stores - sets address only
static inline void addr_zp_store(mos6510_t* cpu) {
    cpu->base.address = mos6510_read_cycle(cpu, cpu->base.pc++);
}

// Zero page,X addressing for stores - sets address only  
static inline void addr_zpx_store(mos6510_t* cpu) {
    uint8_t base = mos6510_read_cycle(cpu, cpu->base.pc++);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->base.address = (base + cpu->base.x) & 0xFF;
}

// Zero page,Y addressing for stores - sets address only
static inline void addr_zpy_store(mos6510_t* cpu) {
    uint8_t base = mos6510_read_cycle(cpu, cpu->base.pc++);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->base.address = (base + cpu->base.y) & 0xFF;
}

// Absolute addressing for stores - sets address only
static inline void addr_abs_store(mos6510_t* cpu) {
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->base.pc++);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->base.pc++);
    cpu->base.address = (addr_hi << 8) | addr_lo;
}

// Absolute,X addressing for stores - sets address only (with dummy read)
static inline void addr_absx_store(mos6510_t* cpu) {
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->base.pc++);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->base.pc++);
    cpu->base.address = (addr_hi << 8) | addr_lo;
    (void)mos6510_read_cycle(cpu, cpu->base.address + cpu->base.x); // Dummy read
    cpu->base.address += cpu->base.x;
}

// Absolute,Y addressing for stores - sets address only (with dummy read)
static inline void addr_absy_store(mos6510_t* cpu) {
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->base.pc++);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->base.pc++);
    cpu->base.address = (addr_hi << 8) | addr_lo;
    (void)mos6510_read_cycle(cpu, cpu->base.address + cpu->base.y); // Dummy read
    cpu->base.address += cpu->base.y;
}

// (Zero page,X) addressing for stores - sets address only
static inline void addr_zpx_ind_store(mos6510_t* cpu) {
    uint8_t base = mos6510_read_cycle(cpu, cpu->base.pc++);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    uint8_t zp_addr = (base + cpu->base.x) & 0xFF;
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    cpu->base.address = (addr_hi << 8) | addr_lo;
}

// (Zero page),Y addressing for stores - sets address only (with dummy read)
static inline void addr_zp_ind_y_store(mos6510_t* cpu) {
    uint8_t zp_addr = mos6510_read_cycle(cpu, cpu->base.pc++);
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    cpu->base.address = (addr_hi << 8) | addr_lo;
    (void)mos6510_read_cycle(cpu, cpu->base.address + cpu->base.y); // Dummy read
    cpu->base.address += cpu->base.y;
}

// ============================================================================
// ILLEGAL INSTRUCTION HELPER FUNCTIONS (inline for performance)
// ============================================================================

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
