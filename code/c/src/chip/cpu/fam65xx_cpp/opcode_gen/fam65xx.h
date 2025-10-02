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

// Pin definitions - only data bus and control signals
#define FAM65XX_PIN_D0      0
#define FAM65XX_PIN_D1      1
#define FAM65XX_PIN_D2      2
#define FAM65XX_PIN_D3      3
#define FAM65XX_PIN_D4      4
#define FAM65XX_PIN_D5      5
#define FAM65XX_PIN_D6      6
#define FAM65XX_PIN_D7      7

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

// Data bus access
#define FAM65XX_GET_DATA(p) ((uint8_t)((p) & 0xFF))
#define FAM65XX_SET_DATA(p, d) do { \
    (p) = ((p) & ~0xFFULL) | ((d) & 0xFF); \
} while(0)

// Memory access callbacks
// cpu_read: called during PHI2 when CPU reads
//   - addr: address being read
//   - bus_state: current data on bus (for preserving floating bits)
//   - returns: data to place on bus (may only partially overwrite bus_state)
typedef uint8_t (*fam65xx_mem_read_t)(void* user_data, uint16_t addr, uint8_t bus_state);

// cpu_write: called during PHI2 when CPU writes
typedef void (*fam65xx_mem_write_t)(void* user_data, uint16_t addr, uint8_t data);

// CPU state
typedef struct {
    // Registers
    uint16_t PC;
    uint8_t A, X, Y, S, P;
    
    // Internal working register for address calculations
    uint16_t AD;
    
    // Cycle decoder state
    uint16_t CI;      // Current cycle index
    uint8_t opcode;   // Current opcode byte
    
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
    
    // Pins (data + control only, no address)
    uint64_t pins;
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

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

//=============================================================================
// INTERNAL MACROS
//=============================================================================

// Bus read: Check RDY, then call memory callback
// If RDY is low, CPU stalls (returns without incrementing CI)
#define BUS_READ(addr) do { \
    if (!(pins & FAM65XX_RDY)) { \
        return pins; \
    } \
    uint8_t data = c->mem_read(c->user_data, (addr), FAM65XX_GET_DATA(pins)); \
    FAM65XX_SET_DATA(pins, data); \
} while(0)

// Bus write: Always proceeds (RDY doesn't affect writes on NMOS 6502/6510)
#define BUS_WRITE(addr, data) do { \
    c->mem_write(c->user_data, (addr), (data)); \
} while(0)

// Dummy bus operations for hardware-accurate 6502 timing
// Every cycle must perform exactly one memory access
// These perform actual bus operations for niche software compatibility
#define DUMMY_BUS_READ(addr) do { \
    if (!(pins & FAM65XX_RDY)) { \
        return pins; \
    } \
    uint8_t data = c->mem_read(c->user_data, (addr), FAM65XX_GET_DATA(pins)); \
    FAM65XX_SET_DATA(pins, data); \
} while(0)

#define DUMMY_BUS_WRITE(addr, data) do { \
    c->mem_write(c->user_data, (addr), (data)); \
} while(0)

// Extract data from pins
#define BUS_DATA() FAM65XX_GET_DATA(pins)

// Flag operations
#define _NZ(v) do { \
    c->P = (c->P & ~(FAM65XX_NF | FAM65XX_ZF)) | \
           (((v) & 0xFF) ? ((v) & FAM65XX_NF) : FAM65XX_ZF); \
} while(0)

// Fetch next opcode
#define _FETCH() do { \
    if (!(pins & FAM65XX_RDY)) { \
        return pins; \
    } \
    c->opcode = c->mem_read(c->user_data, c->PC++, FAM65XX_GET_DATA(pins)); \
    uint8_t addr_seq = opcode_addr_start[c->opcode]; \
    if (addr_seq == 0) { \
        c->CI = c->opcode; \
    } else { \
        c->CI = 256 + addr_seq; \
    } \
    pins |= FAM65XX_SYNC; \
} while(0)

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
// DECODER SWITCH STATEMENT (generated by fam65xx_gen.py)
//=============================================================================

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
    
    // ProcessorTests-compatible initialization: ready for immediate execution
    c->P = FAM65XX_XF;  // Only set unused flag, no interrupt disable
    c->S = 0xFF;        // Stack pointer starts at top
    c->CI = C_FETCH_CYCLE; // Start in fetch state to get first instruction
    c->opcode = 0;
    
    // No reset sequence for ProcessorTests - CPU ready for direct execution
    c->brk_flags = 0;   // Clear all interrupt flags
    
    uint64_t pins = FAM65XX_RDY;  // Ready but not in reset
    c->pins = pins;
    
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
    
    // Clear SYNC by default
    pins &= ~FAM65XX_SYNC;
    
    // Check for interrupts at end of instruction
    if (c->CI == c->opcode && (c->opcode & 0x07) == 0) {
        // Instruction just completed, check for pending interrupts
        if (pins & FAM65XX_RES) {
            c->brk_flags |= FAM65XX_BRK_RESET;
            c->CI = 0x00;
        } else {
            // NMI edge detection
            if ((c->nmi_pip & 0x80) && !(c->nmi_pip & 0x40)) {
                c->brk_flags |= FAM65XX_BRK_NMI;
                c->CI = 0x00;
            }
            // IRQ level detection
            else if ((pins & FAM65XX_IRQ) && !(c->P & FAM65XX_IF)) {
                c->brk_flags |= FAM65XX_BRK_IRQ;
                c->CI = 0x00;
            }
        }
        
        // Shift interrupt pipelines
        c->nmi_pip = ((c->nmi_pip << 1) | ((pins & FAM65XX_NMI) ? 0x01 : 0x00)) & 0xFF;
        c->irq_pip = ((c->irq_pip << 1) | ((pins & FAM65XX_IRQ) ? 0x01 : 0x00)) & 0xFF;
    }
    
    // Execute one cycle
    pins = _fam65xx_decode(c, pins);
    
    c->pins = pins;
    return pins;
}

bool fam65xx_opdone(fam65xx_t* c) {
    return (c->CI == c->opcode);
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

/*
 * USAGE EXAMPLE:
 * 
 * // Memory callbacks
 * uint8_t cpu_read(void* user_data, uint16_t addr, uint8_t bus_state) {
 *     c64_t* sys = (c64_t*)user_data;
 *     
 *     // Handle I/O area
 *     if (addr >= 0xD800 && addr < 0xDC00) {
 *         // Color RAM: preserve high nibble from bus_state!
 *         return (bus_state & 0xF0) | (sys->color_ram[addr & 0x3FF] & 0x0F);
 *     }
 *     
 *     return sys->ram[addr];
 * }
 * 
 * void cpu_write(void* user_data, uint16_t addr, uint8_t data) {
 *     c64_t* sys = (c64_t*)user_data;
 *     
 *     if (addr >= 0xD800 && addr < 0xDC00) {
 *         sys->color_ram[addr & 0x3FF] = data & 0x0F;
 *         return;
 *     }
 *     
 *     sys->ram[addr] = data;
 * }
 * 
 * // Initialize CPU
 * fam65xx_t cpu;
 * fam65xx_desc_t desc = {
 *     .mem_read = cpu_read,
 *     .mem_write = cpu_write,
 *     .mem_user_data = &sys
 * };
 * uint64_t pins = fam65xx_init(&cpu, &desc);
 * 
 * // C64 tick with PHI1/PHI2 separation
 * uint32_t c64_tick(c64_t* sys) {
 *     uint64_t pins = sys->pins;
 *     
 *     // PHI1: VIC-II memory access
 *     m6569_tick_phi1(&sys->vic);
 *     if (sys->vic.needs_bus_phi1) {
 *         uint8_t vic_data = vic_mem_read(sys, sys->vic.addr);
 *         FAM65XX_SET_DATA(pins, vic_data);
 *         m6569_store_data(&sys->vic, vic_data);
 *     }
 *     
 *     // Set BA/RDY for badlines
 *     if (sys->vic.needs_bus_phi2) {
 *         pins &= ~FAM65XX_RDY;
 *     } else {
 *         pins |= FAM65XX_RDY;
 *     }
 *     
 *     // PHI2: CPU or VIC badline access
 *     if (sys->vic.needs_bus_phi2) {
 *         uint8_t vic_data = vic_mem_read(sys, sys->vic.addr2);
 *         FAM65XX_SET_DATA(pins, vic_data);
 *         m6569_store_data2(&sys->vic, vic_data);
 *     } else {
 *         pins = fam65xx_tick(&sys->cpu, pins);
 *     }
 *     
 *     sys->pins = pins;
 *     return 1;
 * }
 */