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

#include <stdio.h> // TMP for printf

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

// Core memory and cycle functions (shared)
static inline uint8_t fam65xx_read_cycle(fam65xx_t* cpu, uint16_t address) {
    uint8_t value = cpu->bus_interface.bus_read_cycle(cpu->bus_interface.context, address);
    if (address >= 0xFFFC && address <= 0xFFFF) {
        static int counter = 0;
        if (counter < 12) {
            counter++;
            printf("vector read at %04X: %02X\n", address, value);
        }
    }
    return value;
//    return cpu->bus_interface.bus_read_cycle(cpu->bus_interface.context, address);
}

static inline void fam65xx_write_cycle(fam65xx_t* cpu, uint16_t address, uint8_t value) {
    cpu->bus_interface.bus_write_cycle(cpu->bus_interface.context, address, value);
}

static const char* fam65xx_opcode_mnemonics[256] = {
    "BRK","ORA","JAM","SLO","NOP","ORA","ASL","SLO","PHP","ORA","ASL","JAM","NOP","ORA","ASL","SLO",
    "BPL","ORA","JAM","SLO","NOP","ORA","ASL","SLO","CLC","ORA","NOP","SLO","NOP","ORA","ASL","SLO",
    "JSR","AND","JAM","RLA","BIT","AND","ROL","RLA","PLP","AND","ROL","JAM","BIT","AND","ROL","RLA",
    "BMI","AND","JAM","RLA","NOP","AND","ROL","RLA","SEC","AND","NOP","RLA","NOP","AND","ROL","RLA",
    "RTI","EOR","JAM","SRE","NOP","EOR","LSR","SRE","PHA","EOR","LSR","JAM","JMP","EOR","LSR","SRE",
    "BVC","EOR","JAM","SRE","NOP","EOR","LSR","SRE","CLI","EOR","NOP","SRE","NOP","EOR","LSR","SRE",
    "RTS","ADC","JAM","RRA","NOP","ADC","ROR","RRA","PLA","ADC","ROR","JAM","JMP","ADC","ROR","RRA",
    "BVS","ADC","JAM","RRA","NOP","ADC","ROR","RRA","SEI","ADC","NOP","RRA","NOP","ADC","ROR","RRA",
    "NOP","STA","NOP","SAX","STY","STA","STX","SAX","DEY","NOP","TXA","XAA","STY","STA","STX","SAX",
    "BCC","STA","JAM","AHX","STY","STA","STX","SAX","TYA","STA","TXS","TAS","SHY","STA","SHX","AHX",
    "LDY","LDA","LDX","LAX","LDY","LDA","LDX","LAX","TAY","LDA","TAX","LAX","LDY","LDA","LDX","LAX",
    "BCS","LDA","JAM","LAX","LDY","LDA","LDX","LAX","CLV","LDA","TSX","LAS","LDY","LDA","LDX","LAX",
    "CPY","CMP","JAM","DCP","CPY","CMP","DEC","DCP","INY","CMP","DEX","AXS","CPY","CMP","DEC","DCP",
    "BNE","CMP","JAM","DCP","NOP","CMP","DEC","DCP","CLD","CMP","NOP","DCP","NOP","CMP","DEC","DCP",
    "CPX","SBC","JAM","ISC","CPX","SBC","INC","ISC","INX","SBC","NOP","SBC","CPX","SBC","INC","ISC",
    "BEQ","SBC","JAM","ISC","NOP","SBC","INC","ISC","SED","SBC","NOP","ISC","NOP","SBC","INC","ISC"
};

#ifdef _MSC_VER
#define SPRINTF_SAFE(buf, size, fmt, ...) sprintf_s(buf, size, fmt, __VA_ARGS__)
#else
#define SPRINTF_SAFE(buf, size, fmt, ...) sprintf(buf, fmt, __VA_ARGS__)
#endif

// Core disassembler function - code[0] is the opcode at current PC
static int fam65xx_disasm(uint16_t pc, unsigned char *code, char *output, size_t output_size) {
    static const unsigned char modes[256] = {
        // 0x00-0x0F
        0, 10, 0, 10, 3, 3, 3, 3, 0, 2, 1, 2, 6, 6, 6, 6,
        // 0x10-0x1F  
        12, 11, 0, 11, 4, 4, 4, 4, 0, 8, 0, 8, 7, 7, 7, 7,
        // 0x20-0x2F
        6, 10, 0, 10, 3, 3, 3, 3, 0, 2, 1, 2, 6, 6, 6, 6,
        // 0x30-0x3F
        12, 11, 0, 11, 4, 4, 4, 4, 0, 8, 0, 8, 7, 7, 7, 7,
        // 0x40-0x4F
        0, 10, 0, 10, 3, 3, 3, 3, 0, 2, 1, 2, 6, 6, 6, 6,
        // 0x50-0x5F
        12, 11, 0, 11, 4, 4, 4, 4, 0, 8, 0, 8, 7, 7, 7, 7,
        // 0x60-0x6F
        0, 10, 0, 10, 3, 3, 3, 3, 0, 2, 1, 2, 9, 6, 6, 6,
        // 0x70-0x7F
        12, 11, 0, 11, 4, 4, 4, 4, 0, 8, 0, 8, 7, 7, 7, 7,
        // 0x80-0x8F: Fixed STX opcodes
        2, 10, 2, 10, 3, 3, 5, 3, 0, 2, 0, 2, 6, 6, 6, 6,
        // 0x90-0x9F: Fixed STA/STX opcodes  
        12, 11, 0, 11, 4, 4, 5, 4, 0, 8, 0, 8, 7, 7, 8, 7,
        // 0xA0-0xAF
        2, 10, 2, 10, 3, 3, 3, 3, 0, 2, 0, 2, 6, 6, 6, 6,
        // 0xB0-0xBF: Fixed LDX/LDA opcodes
        12, 11, 0, 11, 4, 4, 5, 4, 0, 8, 0, 8, 7, 7, 8, 7,
        // 0xC0-0xCF
        2, 10, 2, 10, 3, 3, 3, 3, 0, 2, 0, 2, 6, 6, 6, 6,
        // 0xD0-0xDF
        12, 11, 0, 11, 4, 4, 4, 4, 0, 8, 0, 8, 7, 7, 7, 7,
        // 0xE0-0xEF
        2, 10, 2, 10, 3, 3, 3, 3, 0, 2, 0, 2, 6, 6, 6, 6,
        // 0xF0-0xFF
        12, 11, 0, 11, 4, 4, 4, 4, 0, 8, 0, 8, 7, 7, 7, 7
    };

    unsigned char op = code[0];
    int m = modes[op];
    int bytes = "\0\0\1\1\1\1\2\2\2\2\1\1\1"[m];

    // If output is NULL, just return byte count
    if (!output) return bytes;

    int len = SPRINTF_SAFE(output, output_size, "%s ", fam65xx_opcode_mnemonics[op]);
    switch(m) {
        case 1: return len + SPRINTF_SAFE(output + len, output_size - len, "A");
        case 2: return len + SPRINTF_SAFE(output + len, output_size - len, "#$%02X", code[1]);
        case 3: return len + SPRINTF_SAFE(output + len, output_size - len, "$%02X", code[1]);
        case 4: return len + SPRINTF_SAFE(output + len, output_size - len, "$%02X,X", code[1]);
        case 5: return len + SPRINTF_SAFE(output + len, output_size - len, "$%02X,Y", code[1]);
        case 6: return len + SPRINTF_SAFE(output + len, output_size - len, "$%04X", code[1] | (code[2] << 8));
        case 7: return len + SPRINTF_SAFE(output + len, output_size - len, "$%04X,X", code[1] | (code[2] << 8));
        case 8: return len + SPRINTF_SAFE(output + len, output_size - len, "$%04X,Y", code[1] | (code[2] << 8));
        case 9: return len + SPRINTF_SAFE(output + len, output_size - len, "($%04X)", code[1] | (code[2] << 8));
        case 10: return len + SPRINTF_SAFE(output + len, output_size - len, "($%02X,X)", code[1]);
        case 11: return len + SPRINTF_SAFE(output + len, output_size - len, "($%02X),Y", code[1]);
        case 12: {
            // Relative branch: target = pc + 2 + (signed char)code[1]
            uint16_t target = (uint16_t)(pc + 2 + (int8_t)code[1]);
            return len + SPRINTF_SAFE(output + len, output_size - len, "$%04X", target);
        }
    }
    return len;
}

// Wrapper function to get instruction byte count only
static int fam65xx_opcode_extra_byte_count(uint8_t opcode) {
    return fam65xx_disasm(0, &opcode, NULL, 0);
}

static void fam65xx_disasm_full(fam65xx_t* cpu, uint8_t opcode) {
    uint16_t pc = cpu->pc - 1;
    // Prefetch instruction bytes efficiently
    uint8_t inst_bytes[3] = {opcode, 0, 0};
    int byte_count = fam65xx_opcode_extra_byte_count(opcode);  // Get count from opcode alone

    // Only read the bytes we actually need
    for (int i = 1; i <= byte_count; ++i) {
        inst_bytes[i] = fam65xx_read_cycle(cpu, (uint16_t)(pc + i));
    }

    // Generate full disassembly
    char disasm_buffer[16];

    fam65xx_disasm(pc, inst_bytes, disasm_buffer, sizeof(disasm_buffer));

    uint8_t p = cpu->p;
    
    // Print instruction bytes (only the ones we fetched)
    printf(".,%04X", pc);
    for (int i = 0; i < 3; ++i) {
        if (i <= byte_count) {
            printf(" %02X", inst_bytes[i]);
        } else {
            printf("   ");  // Pad with spaces for unused bytes
        }
    }

    // Print disassembly and registers
    printf(" %-12s  A:%02X X:%02X Y:%02X P:%c%c%c%c%c%c%c%c\n",
        disasm_buffer,
        cpu->a, cpu->x, cpu->y,
        (p & 0x80) ? 'N' : '-', // Negative
        (p & 0x40) ? 'V' : '-', // Overflow
        (p & 0x20) ? 'U' : '-', // Unused
        (p & 0x10) ? 'B' : '-', // Break
        (p & 0x08) ? 'D' : '-', // Decimal
        (p & 0x04) ? 'I' : '-', // Interrupt Disable
        (p & 0x02) ? 'Z' : '-', // Zero
        (p & 0x01) ? 'C' : '-'  // Carry
    );
}

// Instruction dispatch (shared - but implementation-specific functions)
static inline void fam65xx_next_instruction_dispatch(fam65xx_t* cpu) {
    uint8_t opcode = fam65xx_read_cycle(cpu, cpu->pc++);
    static int dump_counter = 50;
    if (dump_counter > 0) {
        dump_counter--;
        fam65xx_disasm_full(cpu, opcode);
    }

    void (*next_handler)(fam65xx_t*) = cpu->opcode_handlers[opcode];
#if defined(_MSC_VER) && defined(_M_IX86)
    // MSVC x86 inline assembly
    __asm {
        mov eax, next_handler
        mov ecx, cpu
        jmp eax
    }
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    // GCC/Clang x86/x64 inline assembly
    asm volatile (
        "mov %[cpu], %%rdi\n\t"   // Pass cpu in first argument register (x86_64 System V ABI)
        "jmp *%[handler]\n\t"
        :
        : [handler] "r" (next_handler), [cpu] "r" (cpu)
        : "rdi"
    );
#else
    // Fallback: normal call (will grow stack)
    next_handler(cpu);
#endif
}

// Forward declaration for macros
void fam65xx_interrupt_handler(fam65xx_t* cpu);

#ifdef REDESIGN
    #define FAM65XX_NEXT_INSTRUCTION(cpu) do { \
        if (unlikely(FAM65XX_TEST_IRQ(cpu) || FAM65XX_TEST_NMI(cpu))) { \
            fam65xx_interrupt_handler(cpu); \
        } \
        fam65xx_next_instruction_dispatch(cpu); \
    } while(0)
#else
    #define FAM65XX_NEXT_INSTRUCTION(cpu) do { \
        if (unlikely(FAM65XX_TEST_IRQ(cpu) || FAM65XX_TEST_NMI(cpu))) { \
            fam65xx_interrupt_handler(cpu); \
        } \
        fam65xx_next_instruction_dispatch(cpu); \
    } while(0)
#endif

// Family-specific versions of shared macros
#ifdef REDESIGN
    #define FAM65XX_OPCODE_PROTO(name) \
        REGISTER_CALL void* name(fam65xx_t* cpu, bus_state_t* bus_state)
#else
    #define FAM65XX_OPCODE_PROTO(name) \
        void name(fam65xx_t* cpu)
#endif

#ifdef REDESIGN
    #define PROTO_RETURN  return
#else
    #define PROTO_RETURN
#endif

#define FAM65XX_OPCODE_FOOTER() \
    FAM65XX_NEXT_INSTRUCTION(cpu)

// Universal instruction dispatch using function pointers
// ============================================================================
// SHARED FUNCTION DECLARATIONS
// ============================================================================

// Arithmetic helper function types (shared by all family members)
typedef uint8_t (*fam65xx_addr_func_t)(fam65xx_t* cpu);
typedef void (*fam65xx_op_func_t)(fam65xx_t* cpu, uint8_t value);

// Arithmetic helper function implementation (static inline for performance)
static FORCE_INLINE void fam65xx_addr_op_helper(fam65xx_t* cpu, fam65xx_addr_func_t addr_func, fam65xx_op_func_t op_func) {
    uint8_t value = addr_func(cpu);
    op_func(cpu, value);
    FAM65XX_OPCODE_FOOTER();
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
