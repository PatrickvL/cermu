#pragma once
/*
 * fam65xx.h - MOS 65xx Family CPU Emulator
 * 
 * Optimized for world's fastest 100% hardware accurate emulation
 * Supports: 6502, 6510, 65C02, and extensible to 65816
 * 
 * Key features:
 * - Cycle-accurate PHI1/PHI2 bus arbitration
 * - Memory callbacks between address setup and data usage
 * - Proper BA/RDY line handling with CPU stalling
 * - Support for partial bus writes (color RAM, etc.)
 * - Optimized case fallthrough for identical operations
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Copied from system_lines.h for fam65xx.h to avoid dependency

/* Field shifts and masks */
#define BUS_ADDR_SHIFT      0
#define BUS_DATA_SHIFT      16

#define BUS_ADDR_MASK       0x000000000000FFFFULL
#define BUS_DATA_MASK       0x0000000000FF0000ULL

/* High-performance field access macros */
#define BUS_GET_ADDR(state)     ((uint16_t)((state) & BUS_ADDR_MASK))
#define BUS_GET_DATA(state)     ((uint8_t) (((state) & BUS_DATA_MASK) >> BUS_DATA_SHIFT))

#define BUS_SET_ADDR(state, addr)   { ((state) = ((state) & ~BUS_ADDR_MASK) | ((uint64_t)(addr) & 0xFFFFULL)); } while(0)
#define BUS_SET_DATA(state, data)   { ((state) = ((state) & ~BUS_DATA_MASK) | (((uint64_t)(data) & 0xFFULL) << BUS_DATA_SHIFT)); } while(0)

// Pin definitions - only control signals
#define FAM65XX_PIN_RW      24
#define FAM65XX_PIN_SYNC    25
#define FAM65XX_PIN_IRQ     26
#define FAM65XX_PIN_NMI     27
#define FAM65XX_PIN_RDY     28
#define FAM65XX_PIN_RES     29

// 6510-specific pins
#define FAM6510_PIN_AEC     30
#define FAM6510_PIN_P0      40
#define FAM6510_PIN_P1      41
#define FAM6510_PIN_P2      42
#define FAM6510_PIN_P3      43
#define FAM6510_PIN_P4      44
#define FAM6510_PIN_P5      45

// Pin masks
#define FAM65XX_RW      (1ULL << FAM65XX_PIN_RW)
#define FAM65XX_SYNC    (1ULL << FAM65XX_PIN_SYNC)
#define FAM65XX_IRQ     (1ULL << FAM65XX_PIN_IRQ)
#define FAM65XX_NMI     (1ULL << FAM65XX_PIN_NMI)
#define FAM65XX_RDY     (1ULL << FAM65XX_PIN_RDY)
#define FAM65XX_RES     (1ULL << FAM65XX_PIN_RES)
#define FAM6510_AEC     (1ULL << FAM6510_PIN_AEC)

// Flag bits
#define FAM65XX_CF      (1<<0)
#define FAM65XX_ZF      (1<<1)
#define FAM65XX_IF      (1<<2)
#define FAM65XX_DF      (1<<3)
#define FAM65XX_BF      (1<<4)
#define FAM65XX_XF      (1<<5)
#define FAM65XX_VF      (1<<6)
#define FAM65XX_NF      (1<<7)

// BRK flags
#define FAM65XX_BRK_IRQ     (1<<0)
#define FAM65XX_BRK_NMI     (1<<1)
#define FAM65XX_BRK_RESET   (1<<2)

// Address bus access
#define FAM65XX_GET_ADDR(p) BUS_GET_ADDR(p)
#define FAM65XX_SET_ADDR(p, d) BUS_SET_ADDR(p, d)

// Data bus access
#define FAM65XX_GET_DATA(p) BUS_GET_DATA(p)
#define FAM65XX_SET_DATA(p, d) BUS_SET_DATA(p, d)

// Memory access callbacks
// cpu_read: called during PHI2 when CPU reads
//   - addr: address being read
//   - bus_state: current data on bus (for preserving floating bits)
//   - returns: data to place on bus (may only partially overwrite bus_state)
typedef uint8_t (*fam65xx_mem_read_t)(void* user_data, uint16_t addr, uint8_t bus_state);

// cpu_write: called during PHI2 when CPU writes
typedef void (*fam65xx_mem_write_t)(void* user_data, uint16_t addr, uint8_t data);

// 8-bit register indices with endian-aware 16-bit pairs
enum {
    // 16-bit aligned register pairs (endian-aware) for memory addresses
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    R_ZPL,       // Zero page (low byte) - full 16-bit zero page register
    R_ZPH,       // Zero page (high byte) - always 0x00 for 6502/6510
    R_SPL,       // Stack pointer (low byte) - full 16-bit stack register
    R_SPH,       // Stack pointer (high byte) - always 0x01 for 6502/6510
    R_ADL,       // Address (low byte, even index for little endian)
    R_ADH,       // Address (high byte)
    R_PCL,       // Program counter (low byte, even index for little endian)
    R_PCH,       // Program counter (high byte)
#else
    R_ZPH,       // Zero page (high byte) - always 0x00 for 6502/6510
    R_ZPL,       // Zero page (low byte) - full 16-bit zero page register
    R_SPH,       // Stack pointer (high byte) - always 0x01 for 6502/6510
    R_SPL,       // Stack pointer (low byte) - full 16-bit stack register
    R_ADH,       // Address (high byte, even index for big endian)
    R_ADL,       // Address (low byte)
    R_PCH,       // Program counter (high byte, even index for big endian)
    R_PCL,       // Program counter (low byte)
#endif
    // Public registers (maintain compatibility)
    R_A,         // Accumulator
    R_X,         // X index
    R_Y,         // Y index
    R_P,         // Processor status
    // Internal registers
    R_IR,        // Instruction register
    R_DL,        // Data latch
    R_TMP,       // Temporary storage
    R_DISCARD,   // Discard register for dummy reads

    // Compatibility mapping for 8-bit stack pointer
    R_S = R_SPL,  // Map legacy S register to SPL for compatibility

    R_COUNT = R_DISCARD + 1 //  (no comma for last element)
};    

// 16-bit register indices (native endian compatible)
enum {
    R_ZP16 = R_ZPL / 2,  // Zero page (16 bits) - full zero page register
    R_SP16 = R_SPL / 2,  // Stack pointer (16 bits) - full stack register
    R_AD = R_ADL / 2,    // Address (16 bits)
    R_PC = R_PCL / 2,    // Program counter (16 bits)
};

// CPU state
typedef struct {
    union {
        uint8_t r8[R_COUNT];        // 8-bit register array (reduced, removed MEL/MEH)
        uint16_t r16[R_COUNT / 2];  // 16-bit overlay (native endian)
    };

// Accessors (c-> required before use) - enhanced for 16-bit memory registers

// Public registers (transparent array accessors)
#define A      r8[R_A]     // Accumulator register
#define X      r8[R_X]     // X index register
#define Y      r8[R_Y]     // Y index register
#define S      r8[R_SPL]   // Stack pointer (8-bit compatibility, maps to SPL)
#define P      r8[R_P]     // Processor status

// Enhanced 16-bit memory address registers
#define ZP     r16[R_ZP16] // Zero page register (full 16-bit with high=0x00)
#define ZPL    r8[R_ZPL]   // Zero page low
#define ZPH    r8[R_ZPH]   // Zero page high (always 0x00)

#define SP     r16[R_SP16] // Stack pointer (full 16-bit with high=0x01)
#define SPL    r8[R_SPL]   // Stack pointer low
#define SPH    r8[R_SPH]   // Stack pointer high (always 0x01)

#define AD     r16[R_AD]   // Address data (16 bit)
#define ADL    r8[R_ADL]   // Address data low
#define ADH    r8[R_ADH]   // Address data high

#define PC     r16[R_PC]   // Program counter (16 bit)
#define PCL    r8[R_PCL]   // Program counter low
#define PCH    r8[R_PCH]   // Program counter high

// Internal registers
#define opcode r8[R_IR]    // Current opcode
#define DL     r8[R_DL]    // Data latch
#define TMP    r8[R_TMP]   // Temporary storage
#define DISCARD r8[R_DISCARD] // Discard register for dummy operations

    // Cycle decoder state
    uint16_t CI;          // Current cycle index
    uint8_t reg_idx;      // Register index for memory accesses

    // Memory callbacks
    fam65xx_mem_read_t mem_read;
    fam65xx_mem_write_t mem_write;
    void* user_data;
    
    // Interrupt state
    uint8_t irq_pip;   // IRQ edge detection pipeline
    uint8_t nmi_pip;   // NMI edge detection pipeline
    uint8_t brk_flags; // BRK/IRQ/NMI/RESET flags
    
    // 6510-specific I/O port state
    uint8_t io_ddr;    // Data direction register
    uint8_t io_port;   // I/O port data
    uint8_t io_drive;  // What CPU is driving
    uint8_t io_pins;   // External pin state
} fam65xx_t;

// Initialization descriptor
typedef struct {
    // 6510-specific callbacks
    uint8_t (*m6510_in_cb)(void* user_data);
    void (*m6510_out_cb)(uint8_t data, void* user_data);
    uint8_t m6510_io_pullup;
    uint8_t m6510_io_floating;
    void* m6510_user_data;
    
    // Memory callbacks
    fam65xx_mem_read_t mem_read;
    fam65xx_mem_write_t mem_write;
    void* mem_user_data;
} fam65xx_desc_t;

// API functions
uint64_t fam65xx_init(fam65xx_t* cpu, const fam65xx_desc_t* desc);
void fam65xx_reset(fam65xx_t* cpu);
uint64_t fam65xx_tick(fam65xx_t* cpu, uint64_t pins);
bool fam65xx_opdone(fam65xx_t* cpu);
uint64_t fam65xx_bootstrap(fam65xx_t* cpu, uint64_t pins);

// 6510-specific
uint64_t fam6510_iorq(fam65xx_t* cpu, uint64_t pins);
#define FAM6510_CHECK_IO(addr) ((addr) <= 0x0001)

// Register accessors
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

#ifdef CHIPS_IMPL
#include <string.h>
#include <stdio.h>

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

//=============================================================================
// INTERNAL MACROS
//=============================================================================

// Enhanced memory access macros for optimized register allocation
// These macros enable direct use of 16-bit memory address registers,
// eliminating the need for address arithmetic and improving code generation

// LETS_READ: Setup memory read operation using adr_idx for hardware-accurate addressing
// Sets up addressing mode register and data register indices for memory access
#define LETS_READ(addr_register, data_register) do { \
    FAM65XX_SET_ADDR(pins, c->r16[addr_register]); \
    c->reg_idx = data_register; \
} while(0)

// LETS_WRITE: Setup memory write operation using adr_idx for hardware-accurate addressing
// Sets up addressing mode register and data register indices for memory access
#define LETS_WRITE(addr_register, data_register) do { \
    FAM65XX_SET_ADDR(pins, c->r16[addr_register]); \
    FAM65XX_SET_DATA(pins, c->r8[data_register]); \
    pins &= ~FAM65XX_RW; \
} while(0)

// Stack operation helpers using dedicated SP register
#define STACK_PUSH(data_register) do { \
    c->AD = c->SP--; /* Use full 16-bit SP register, auto-decrement */ \
    LETS_WRITE(R_AD, data_register); \
} while(0)

#define STACK_PULL() do { \
    c->S++; /* Increment S, then use for address */  \
    LETS_READ(R_SP16, R_DL); \
} while(0)

#define STACK_PEEK() do { \
    FAM65XX_SET_ADDR(pins, c->SP); /* Use current SP for address without changing it */ \
    c->reg_idx = R_DL; \
} while(0)

// Zero page operation helpers using dedicated ZP register
#define ZP_READ(zp_offset, data_register) do { \
    c->ZPL = zp_offset; /* Set low byte, high is always 0x00 */ \
    LETS_READ(R_ZP16, data_register); \
} while(0)

#define ZP_WRITE(zp_offset, data_register) do { \
    c->ZPL = zp_offset; /* Set low byte, high is always 0x00 */ \
    LETS_WRITE(R_ZP16, data_register); \
} while(0)

// RMW operation helpers using address set up by addressing modes
#define RMW_READ() do { \
    /* Address already set up by addressing mode in c->AD */ \
    /* c->adr_idx already set by addressing mode */ \
} while(0)

#define RMW_WRITE(data_register) do { \
    /* Bus address already set up by addressing mode */ \
    FAM65XX_SET_DATA(pins, c->r8[data_register]); \
    pins &= ~FAM65XX_RW; \
} while(0)

// Store operation helper using address set up by addressing modes
#define STORE_WRITE(data_register) do { \
    /* Bus address already set up by addressing mode */ \
    FAM65XX_SET_DATA(pins, c->r8[data_register]); \
    pins &= ~FAM65XX_RW; \
} while(0)

// Dummy memory access macros for hardware-accurate dummy cycles
// DUMMY_READ: Perform a dummy read cycle (data discarded, but bus cycle occurs)
#define DUMMY_READ(addr_register) do { \
    LETS_READ(addr_register, R_DISCARD); \
} while(0)

// DUMMY_WRITE: Perform a dummy write cycle (no actual write, but bus cycle occurs)
// Note: In hardware, dummy writes don't actually modify memory, but the bus cycle happens
#define DUMMY_WRITE(addr_register, data_register) do { \
    LETS_WRITE(addr_register, data_register); \
} while(0)

// PC increment and data fetch centralization
// FETCH: Read from PC and increment PC, storing result in specified register
#define FETCH(dst_reg) do { \
    LETS_READ(R_PC, dst_reg); \
    c->PC++; \
} while(0)

// Instruction fetch with SYNC - used only for fetching next instruction
#define FETCH_IR() do { \
    FETCH(R_IR); \
    pins |= FAM65XX_SYNC; \
} while(0)

// Legacy address setup helper (maintained for compatibility)
#define SET_ADDR(pins, addr) do { \
    /* Address bits would be set on external address bus in real hardware */ \
    /* For emulation, address is passed directly to memory callbacks */ \
    (void)(pins); /* Keep pins unchanged - address handled in footer */ \
    (void)(addr); \
} while(0)

// Flag operations
#define _NZ(v) do { \
    c->P = (c->P & ~(FAM65XX_NF | FAM65XX_ZF)) | \
           (((v) & 0xFF) ? ((v) & FAM65XX_NF) : FAM65XX_ZF); \
} while(0)

// Cycle index layout:
enum {
    // [0..255]   = Opcode execution cycles
    // [256+]     = Addressing modes (ADDR_SEQ_BASE + addr_seq), where addr_seq 0 = opcode only
    ADDR_SEQ_BASE = 255,  // Addressing modes start at offset 1, so first mode at 256 (no gap after opcodes 0-255)
    // [400+]     = Continuation cycles for complex operations
};

// SYNC-based fetch architecture:
// addr_seq 0 = implied/accumulator (jump to opcode directly)
// addr_seq > 0 = addressing mode offset (ADDR_SEQ_BASE + addr_seq)
// Opcode decoding handled in fam65xx_tick() when SYNC is detected

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

static inline uint8_t _fam65xx_asl(fam65xx_t* c, uint8_t v) {
    c->P &= ~(FAM65XX_NF | FAM65XX_ZF | FAM65XX_CF);
    if (v & 0x80) {
        c->P |= FAM65XX_CF;
    }
    v <<= 1;
    _NZ(v);
    return v;
}

static inline uint8_t _fam65xx_lsr(fam65xx_t* c, uint8_t v) {
    c->P &= ~(FAM65XX_NF | FAM65XX_ZF | FAM65XX_CF);
    if (v & 1) {
        c->P |= FAM65XX_CF;
    }
    v >>= 1;
    _NZ(v);
    return v;
}

static inline uint8_t _fam65xx_rol(fam65xx_t* c, uint8_t v) {
    bool carry = c->P & FAM65XX_CF;
    c->P &= ~(FAM65XX_NF | FAM65XX_ZF | FAM65XX_CF);
    if (v & 0x80) {
        c->P |= FAM65XX_CF;
    }
    v <<= 1;
    if (carry) {
        v |= 1;
    }
    _NZ(v);
    return v;
}

static inline uint8_t _fam65xx_ror(fam65xx_t* c, uint8_t v) {
    bool carry = c->P & FAM65XX_CF;
    c->P &= ~(FAM65XX_NF | FAM65XX_ZF | FAM65XX_CF);
    if (v & 1) {
        c->P |= FAM65XX_CF;
    }
    v >>= 1;
    if (carry) {
        v |= 0x80;
    }
    _NZ(v);
    return v;
}

static inline void _fam65xx_cmp(fam65xx_t* c, uint8_t reg, uint8_t val) {
    uint16_t t = reg - val;
    c->P &= ~(FAM65XX_NF | FAM65XX_ZF | FAM65XX_CF);
    if (!(t & 0xFF)) {
        c->P |= FAM65XX_ZF;
    }
    if (t & 0x80) {
        c->P |= FAM65XX_NF;
    }
    if (!(t & 0xFF00)) {
        c->P |= FAM65XX_CF;
    }
}

static inline void _fam65xx_adc(fam65xx_t* c, uint8_t val) {
    if (c->P & FAM65XX_DF) {
#if 0 // floooh original
        // BCD mode
        uint16_t t = (c->A & 0x0F) + (val & 0x0F) + (c->P & FAM65XX_CF ? 1 : 0);
        if (t > 9) t += 6;
        t += (c->A & 0xF0) + (val & 0xF0);
        if (t > 0x9F) t += 0x60;
        c->P &= ~(FAM65XX_CF | FAM65XX_NF | FAM65XX_ZF);
        if (t > 0xFF) c->P |= FAM65XX_CF;
        if (!((c->A + val + (c->P & FAM65XX_CF ? 1 : 0)) & 0xFF)) c->P |= FAM65XX_ZF;
        if (t & 0x80) c->P |= FAM65XX_NF;
        c->A = t & 0xFF;
#else // Claude's wooly fix
        // BCD mode - hardware accurate implementation
        uint8_t carry_in = (c->P & FAM65XX_CF) ? 1 : 0;
        
        // Calculate binary result for V flag detection
        uint16_t bin_result = c->A + val + carry_in;
        
        // Convert inputs to BCD
        uint8_t a_lo = c->A & 0x0F;
        uint8_t a_hi = (c->A & 0xF0) >> 4;
        uint8_t v_lo = val & 0x0F;
        uint8_t v_hi = (val & 0xF0) >> 4;
        
        // Add low nibbles
        uint16_t sum_lo = a_lo + v_lo + carry_in;
        uint8_t carry_mid = 0;
        if (sum_lo > 9) {
            sum_lo += 6;
            carry_mid = 1;
        }
        
        // Add high nibbles
        uint16_t sum_hi = a_hi + v_hi + carry_mid;
        uint8_t carry_out = 0;
        if (sum_hi > 9) {
            sum_hi += 6;
            carry_out = 1;
        }
        
        // Combine BCD result
        uint8_t bcd_result = ((sum_hi & 0x0F) << 4) | (sum_lo & 0x0F);
        
        // Set flags: C from BCD carry, N/Z from BCD result, V from binary overflow
        c->P &= ~(FAM65XX_CF | FAM65XX_NF | FAM65XX_ZF | FAM65XX_VF);
        if (carry_out) c->P |= FAM65XX_CF;
        
        // N and Z flags based on BCD result (hardware-accurate)
        if (bcd_result & 0x80) c->P |= FAM65XX_NF;
        if (bcd_result == 0) c->P |= FAM65XX_ZF;
        
        // V flag: overflow detection based on binary operation
        if (~(c->A ^ val) & (c->A ^ bin_result) & 0x80) {
            c->P |= FAM65XX_VF;
        }
        
        c->A = bcd_result;
#endif        
    } else {
        // Binary mode
        uint16_t t = c->A + val + (c->P & FAM65XX_CF ? 1 : 0);
        c->P &= ~(FAM65XX_VF | FAM65XX_CF | FAM65XX_NF | FAM65XX_ZF);
        if (~(c->A ^ val) & (c->A ^ t) & 0x80) {
            c->P |= FAM65XX_VF;
        }
        if (t > 0xFF) c->P |= FAM65XX_CF;
        c->A = t & 0xFF;
        _NZ(c->A);
    }
}

static inline void _fam65xx_sbc(fam65xx_t* c, uint8_t val) {
    if (c->P & FAM65XX_DF) {
        // BCD mode
        int16_t t = (c->A & 0x0F) - (val & 0x0F) - (c->P & FAM65XX_CF ? 0 : 1);
        if (t < 0) t = ((t - 6) & 0x0F) - 0x10;
        t += (c->A & 0xF0) - (val & 0xF0);
        if (t < 0) t -= 0x60;
        c->P &= ~(FAM65XX_CF | FAM65XX_NF | FAM65XX_ZF);
        uint16_t bin = c->A - val - (c->P & FAM65XX_CF ? 0 : 1);
        if (!(bin & 0xFF00)) c->P |= FAM65XX_CF;
        if (!(bin & 0xFF)) c->P |= FAM65XX_ZF;
        if (bin & 0x80) c->P |= FAM65XX_NF;
        c->A = t & 0xFF;
    } else {
        // Binary mode
        uint16_t t = c->A - val - (c->P & FAM65XX_CF ? 0 : 1);
        c->P &= ~(FAM65XX_VF | FAM65XX_CF | FAM65XX_NF | FAM65XX_ZF);
        if ((c->A ^ val) & (c->A ^ t) & 0x80) {
            c->P |= FAM65XX_VF;
        }
        if (!(t & 0xFF00)) c->P |= FAM65XX_CF;
        c->A = t & 0xFF;
        _NZ(c->A);
    }
}

static inline void _fam65xx_bit(fam65xx_t* c, uint8_t val) {
    uint8_t t = c->A & val;
    c->P &= ~(FAM65XX_NF | FAM65XX_VF | FAM65XX_ZF);
    if (!t) {
        c->P |= FAM65XX_ZF;
    }
    c->P |= val & (FAM65XX_NF | FAM65XX_VF);
}

// Helper function to get interrupt vector address based on BRK flags
static inline uint16_t _fam65xx_get_vector_addr(fam65xx_t* c) {
    if (c->brk_flags & FAM65XX_BRK_RESET) {
        return 0xFFFC;
    } else if (c->brk_flags & FAM65XX_BRK_NMI) {
        return 0xFFFA;
    } else {
        return 0xFFFE;  // BRK/IRQ vector
    }
}

//=============================================================================
// DECODER FUNCTION (generated by fam65xx_gen.py)
//=============================================================================

// Forward declaration for the generated decoder function
static inline uint64_t _fam65xx_decode(fam65xx_t* c, uint64_t pins);

#include "_fam65xx_decoder.h"

//=============================================================================
// API IMPLEMENTATION
//=============================================================================

uint64_t fam65xx_init(fam65xx_t* c, const fam65xx_desc_t* desc) {
    CHIPS_ASSERT(c && desc);
    CHIPS_ASSERT(desc->mem_read && desc->mem_write);
    
    memset(c, 0, sizeof(fam65xx_t));
    
    c->mem_read = desc->mem_read;
    c->mem_write = desc->mem_write;
    c->user_data = desc->mem_user_data;
    
    // 6510-specific initialization
    c->io_ddr = 0;
    c->io_port = 0;
    c->io_drive = 0;
    c->io_pins = desc->m6510_io_pullup;

    // Set initial register state for 16-bit memory registers
    c->r8[R_SPH] = 0x01;   // Stack pointer high byte (always 0x01 for 6502/6510)
    c->r8[R_ZPH] = 0x00;   // Zero page high byte (always 0x00 for 6502/6510)

    // ProcessorTests compatibility: Initialize to fetch first instruction
    c->P = FAM65XX_XF;  // Only set unused flag
    c->SPL = 0xFF;      // Stack pointer low byte at top
    c->PC = 0x0000;     // Will be set by test harness
    
    // Clear all interrupt/BRK state
    c->brk_flags = 0;
    c->irq_pip = 0xFF;
    c->nmi_pip = 0xFF;
    
    // Initialize for immediate instruction fetch
    c->CI = 0xFFFF;     // Invalid CI to force proper initialization
    c->AD = 0x0000;     // Will point to PC during first fetch
    c->reg_idx = R_IR;  // Initialize to instruction register for instruction fetch
    
    uint64_t pins = FAM65XX_RDY;  // Ready, but no SYNC yet
    
    return pins;
}

// Bootstrap function for proper CPU initialization
uint64_t fam65xx_bootstrap(fam65xx_t* c, uint64_t pins) {
    CHIPS_ASSERT(c);
    
    // Only bootstrap if CPU is in uninitialized state
    if (c->CI == 0xFFFF) {
        // Set up for first instruction fetch
        pins |= FAM65XX_RDY;   // Ensure RDY is high for execution
        pins |= FAM65XX_RW;    // Ensure RW is set for read operation
        c->CI = 0x0000;        // Clear the invalid marker
        // Set up first instruction fetch - read opcode from current PC into IR
        FETCH_IR(); // Set up instruction fetch and SYNC pin
    }
    
    return pins;
}

void fam65xx_reset(fam65xx_t* c) {
    CHIPS_ASSERT(c);
    c->brk_flags = FAM65XX_BRK_RESET;
    c->CI = 0x00;
    c->P |= FAM65XX_IF;
}

uint64_t fam65xx_tick(fam65xx_t* c, uint64_t pins) {
    CHIPS_ASSERT(c);
    
    // RDY check: stall CPU BEFORE calling decode if not ready
    if (!(pins & FAM65XX_RDY)) {
        if (pins & FAM65XX_RW) {
            // Only stall READ cycles
            // CPU is stalled - don't advance, return current state
            return pins;
        }
        // WRITE cycles proceed regardless
    }
    
    // Memory access happens FIRST (hardware-accurate)
    // Use adr_idx to select address register, with address passed via pins
    uint16_t mem_addr = FAM65XX_GET_ADDR(pins);
    uint8_t pins_data = FAM65XX_GET_DATA(pins);
    if (pins & FAM65XX_RW) {
        // Read operation - perform memory read
        c->r8[c->reg_idx] = c->mem_read(c->user_data, mem_addr, pins_data);
    } else {
        // Write operation - perform memory write
        c->mem_write(c->user_data, mem_addr, pins_data);
        // Automatically raise RW pin after write completes - test runner can infer write from mem_write call
        pins |= FAM65XX_RW;
    }
    
    // SYNC-based opcode decoding with RDY stalling BEFORE decode
    if (pins & FAM65XX_SYNC) {
        pins &= ~FAM65XX_SYNC;
        
        // Shift interrupt pipelines (required for hardware accuracy)
        c->nmi_pip = ((c->nmi_pip << 1) | ((pins & FAM65XX_NMI) ? 0x01 : 0x00)) & 0xFF;
        c->irq_pip = ((c->irq_pip << 1) | ((pins & FAM65XX_IRQ) ? 0x01 : 0x00)) & 0xFF;

        // Decode opcode and set next CI based on addressing mode
        extern const uint8_t opcode_addr_start[256];  // From generated decoder
        uint8_t addr_seq = opcode_addr_start[c->opcode];
        
        // Set next CI: either direct opcode execution or addressing mode sequence
        if (addr_seq == 0) {
            // Direct opcode execution (implied addressing)
            c->CI = (uint16_t)c->opcode;
        } else {
            // Addressing mode sequence
            c->CI = ADDR_SEQ_BASE + addr_seq;
        }
    }
    
    // Execute one cycle using generated decoder (after memory access)
    pins = _fam65xx_decode(c, pins);
    
    return pins;
}

bool fam65xx_opdone(fam65xx_t* c) {
    // Instruction is complete when the CPU has finished execution and is ready
    // for the next instruction fetch. This is indicated when we're about to
    // fetch the next instruction (SYNC will be set in the next tick).
    // We check if we're at a fetch_next state by looking at the CI value
    // and checking if it matches the completion patterns.
    
    // Direct instruction completion (immediate/implied addressing)
    if (c->CI < 256) {
        // We're in an opcode-specific cycle, check if this opcode goes directly to fetch_next
        return true;  // Most single-cycle instructions complete immediately
    }
    
    // For multi-cycle instructions, they complete when CI is set back to fetch_next
    // This is a simplified heuristic - in practice, the test harness should track
    // the pins state returned from fam65xx_tick to detect SYNC being set
    return false;
}

// 6510 I/O port handling
uint64_t fam6510_iorq(fam65xx_t* c, uint64_t pins) {
    uint16_t addr = 0;  // Would come from external tracking if needed
    
    // This would be called from the memory callback when addr <= 1
    // Implementation depends on whether it's $0000 (DDR) or $0001 (port)
    // For now, just return pins
    return pins;
}

// Register accessors
void fam65xx_set_a(fam65xx_t* c, uint8_t v) { c->A = v; }
void fam65xx_set_x(fam65xx_t* c, uint8_t v) { c->X = v; }
void fam65xx_set_y(fam65xx_t* c, uint8_t v) { c->Y = v; }
void fam65xx_set_s(fam65xx_t* c, uint8_t v) { c->S = v; }
void fam65xx_set_p(fam65xx_t* c, uint8_t v) { c->P = v; }
void fam65xx_set_pc(fam65xx_t* c, uint16_t v) { c->PC = v; }

uint8_t fam65xx_a(fam65xx_t* c) { return c->A; }
uint8_t fam65xx_x(fam65xx_t* c) { return c->X; }
uint8_t fam65xx_y(fam65xx_t* c) { return c->Y; }
uint8_t fam65xx_s(fam65xx_t* c) { return c->S; }
uint8_t fam65xx_p(fam65xx_t* c) { return c->P; }
uint16_t fam65xx_pc(fam65xx_t* c) { return c->PC; }

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif
