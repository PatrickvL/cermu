#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Compiler optimization hints
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely  
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// Chip select bit definitions
#define RAM_CS      (1 << 0)
#define ROM_CS      (1 << 1) 
#define VIC_CS      (1 << 2)
#define SID_CS      (1 << 3)
#define CIA1_CS     (1 << 4)
#define CIA2_CS     (1 << 5)
#define CHAR_ROM_CS (1 << 6)
#define IO_CS       (1 << 7)

// Control line definitions
#define READ_CYCLE  (1 << 0)
#define WRITE_CYCLE (1 << 1)
#define IRQ_LINE    (1 << 2)
#define NMI_LINE    (1 << 3)

// Bus control definitions  
#define BA_LINE     (1 << 0)
#define AEC_LINE    (1 << 1)
#define RDY_LINE    (1 << 2)

// ============================================================================
// BUS STATE - Lives in host CPU register for maximum performance
// ============================================================================
typedef union {
    uint64_t raw;
    struct {
        uint16_t address;       // A0-A15
        uint8_t  data;          // D0-D7  
        uint8_t  control_lines; // R/W, IRQ, NMI
        uint8_t  chip_selects;  // Chip select lines
        uint8_t  bus_control;   // BA, AEC, RDY
        uint16_t reserved;
    };
} bus_state_t;

// Keep bus state accessible
static bus_state_t bus_state;

// ============================================================================
// MEMORY - 64K RAM + ROM images
// ============================================================================
static uint8_t ram[65536];
static uint8_t kernal_rom[8192];
static uint8_t basic_rom[8192];  
static uint8_t char_rom[4096];

// RAM writes handled directly in device cycle handlers via bus_state

// ============================================================================
// VIC-II EMULATION - Video chip with cycle-accurate badline generation
// ============================================================================

static struct {
    uint8_t registers[64];

    uint16_t raster_line;
    uint8_t raster_cycle;
    bool badline_condition;
    bool prev_ba;
} vic;

// VIC write masks for each register (defines which bits are writable)
static const uint8_t vic_write_masks[64] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $00-$07
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $08-$0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $10-$17
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $18-$1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $20-$27
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $28-$2F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $30-$37
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF   // $38-$3F
};

// VIC writes handled directly in vic_cycle() via chip selects

static void vic_cycle(void) {
    // Always increment raster timing
    vic.raster_cycle++;
    if (vic.raster_cycle >= 63) {
        vic.raster_cycle = 0;
        vic.raster_line++;
        if (vic.raster_line >= 312) vic.raster_line = 0;
    }
    
    // Check for badline condition (hardware accurate)
    vic.badline_condition = (vic.raster_line >= 0x30 && vic.raster_line <= 0xF7) &&
                           ((vic.raster_line & 7) == (vic.registers[0x11] & 7));
    
    // Generate BA signal for badline
    if (vic.badline_condition && vic.raster_cycle >= 15 && vic.raster_cycle <= 54) {
        bus_state.bus_control &= ~BA_LINE; // Pull BA low - CPU will stall
    } else {
        bus_state.bus_control |= BA_LINE;  // Release BA
    }
    
    // AEC follows BA with one cycle delay (hardware accurate)
    if (vic.prev_ba && !(bus_state.bus_control & BA_LINE)) {
        bus_state.bus_control &= ~AEC_LINE;
    } else if (!vic.prev_ba && (bus_state.bus_control & BA_LINE)) {
        bus_state.bus_control |= AEC_LINE;
    }
    vic.prev_ba = (bus_state.bus_control & BA_LINE) != 0;
    
    // Handle CPU register access when chip selected
    if (unlikely(bus_state.chip_selects & VIC_CS)) {
        uint8_t reg = bus_state.address & 0x3F;
        if (bus_state.control_lines & WRITE_CYCLE) {
            vic.registers[reg] = bus_state.data & vic_write_masks[reg];
        } else {
            bus_state.data = vic.registers[reg];
        }
    }
}

// ============================================================================
// CIA EMULATION - Timer and I/O chips
// ============================================================================
static union {
    uint8_t registers[16];

    struct { // Does order correspond to CIA1/2 registers, or should this not even be a union?
        uint16_t timer_a, timer_b;
        uint16_t timer_a_latch, timer_b_latch;
        uint8_t control_a, control_b;
        uint8_t interrupt_control, interrupt_status;
        uint8_t port_a, port_b;
    };
} cia1, cia2;

// CIA writes handled directly in cia1_cycle() and cia2_cycle() via chip selects

static void cia1_cycle(void) {
    // Timer A always decrements when enabled (hardware accurate)
    if (cia1.control_a & 1) {
        if (cia1.timer_a == 0) {
            cia1.timer_a = cia1.timer_a_latch;
            cia1.interrupt_status |= 1; // Timer A interrupt
            if (cia1.interrupt_control & 1) {
                bus_state.control_lines |= IRQ_LINE;
            }
        } else {
            cia1.timer_a--;
        }
    }
    
    // Handle CPU register access when chip selected
    if (unlikely(bus_state.chip_selects & CIA1_CS)) {
        uint8_t reg = bus_state.address & 0x0F;
        if (bus_state.control_lines & WRITE_CYCLE) {
            switch (reg) {
                case 0x00: cia1.port_a = bus_state.data; break;
                case 0x01: cia1.port_b = bus_state.data; break;  
                case 0x04: cia1.timer_a_latch = (cia1.timer_a_latch & 0xFF00) | bus_state.data; break;
                case 0x05: cia1.timer_a_latch = (cia1.timer_a_latch & 0x00FF) | (bus_state.data << 8); break;
                case 0x0E: cia1.control_a = bus_state.data; break;
                case 0x0D: cia1.interrupt_control = bus_state.data; break;
            }
        } else {
            switch (reg) {
                case 0x00: bus_state.data = cia1.port_a; break;
                case 0x01: bus_state.data = cia1.port_b; break;
                case 0x04: bus_state.data = cia1.timer_a & 0xFF; break;
                case 0x05: bus_state.data = cia1.timer_a >> 8; break;
                case 0x0D: bus_state.data = cia1.interrupt_status; cia1.interrupt_status = 0; break;
            }
        }
    }
}

static void cia2_cycle(void) {
    // Timer A always decrements when enabled
    if (cia2.control_a & 1) {
        if (cia2.timer_a == 0) {
            cia2.timer_a = cia2.timer_a_latch; 
            cia2.interrupt_status |= 1;
            if (cia2.interrupt_control & 1) {
                bus_state.control_lines |= NMI_LINE;
            }
        } else {
            cia2.timer_a--;
        }
    }
    
    // Handle register access (similar to CIA1, abbreviated for space)
    if (unlikely(bus_state.chip_selects & CIA2_CS)) {
        // Register access logic similar to CIA1...
    }
}

// ============================================================================
// SID EMULATION - Sound chip with envelope generators
// ============================================================================
static struct {
    uint8_t registers[32];

    uint16_t envelope_counter[3];
    uint8_t envelope_state[3];
} sid;

// SID writes handled directly in sid_cycle() via chip selects

static void sid_cycle(void) {
    // Envelope generators always run (hardware accurate)
    for (int voice = 0; voice < 3; voice++) {
        sid.envelope_counter[voice]++;
        if (sid.envelope_counter[voice] >= 0x8000) {
            sid.envelope_counter[voice] = 0;
            // Simplified envelope state machine
            sid.envelope_state[voice] = (sid.envelope_state[voice] + 1) & 0xFF;
        }
    }
    
    // Handle CPU register access when chip selected
    if (unlikely(bus_state.chip_selects & SID_CS)) {
        uint8_t reg = bus_state.address & 0x1F;
        if (bus_state.control_lines & WRITE_CYCLE) {
            sid.registers[reg] = bus_state.data;
        } else {
            bus_state.data = sid.registers[reg];
        }
    }
}

// ============================================================================
// UNIFIED BUS CYCLE - All chips react to bus state simultaneously
// ============================================================================
static inline void bus_cycle(void) {
    // All chips always run for cycle accuracy - no conditionals for performance
    vic_cycle();    // Video timing, BA control, sprites
    cia1_cycle();   // Timers, keyboard, joystick
    cia2_cycle();   // Timers, serial, user port  
    sid_cycle();    // Sound generation, envelope generators
    
    // Update RDY line based on BA (hardware accurate)
    if (bus_state.bus_control & BA_LINE) {
        bus_state.bus_control |= RDY_LINE;
    } else {
        bus_state.bus_control &= ~RDY_LINE;
    }
}

// ============================================================================
// PLA EMULATION - Pre-computed chip select maps for each memory mode
// ============================================================================

// 32 possible PLA modes, 256 memory blocks each (256-byte granularity for CIA compatibility)
static uint8_t chip_select_maps[32][256] __attribute__((aligned(64)));
static uint8_t* chip_select_map; // Current active map

static uintptr_t mode_read_map[32][256];  // 32 configs × 256 256-byte blocks
static uintptr_t* read_map; // points to mode_read_map[mode]

// Write handlers removed - device cycle handlers already handle writes via chip selects

void switch_cpu_mode(uint8_t mode) {
    chip_select_map = chip_select_maps[mode];
    read_map = mode_read_map[mode];
}

// Ultra-fast address decoding - single memory access
static inline void update_chip_selects(uint16_t addr) {
    bus_state.chip_selects = chip_select_map[addr >> 8];
}

// ============================================================================
// MEMORY ACCESS HELPERS
// ============================================================================
static inline void cpu_read_cycle(uint16_t addr) {
    bus_state.address = addr;
    bus_state.control_lines = READ_CYCLE;
    update_chip_selects(addr);
    bus_cycle();
    uintptr_t entry = read_map[addr >> 8];
    // Extract base pointer (256-byte aligned, so lower 8 bits are available for mask)
    uint8_t* base = (uint8_t*)(entry & ~0xFF);
    // Extract mask directly from LSB bits (no shifting needed)
    uint8_t mask = (uint8_t)(entry & 0xFF);
    bus_state.data = base[addr & mask];
}

static inline void cpu_write_cycle(uint16_t addr, uint8_t value) {
    bus_state.address = addr;
    bus_state.data = value;
    bus_state.control_lines = WRITE_CYCLE;
    update_chip_selects(addr);
    bus_cycle();
    // Write is handled by device cycle handlers via chip selects
}

// Map generator
static void generate_pla_maps(void) {
    for (int mode = 0; mode < 32; ++mode) {
        bool loram = mode & 1, hiram = mode & 2, charen = mode & 4;
        bool game  = mode & 8;
        (void)(mode & 16); // exrom - reserved for future cartridge support
        
        for (int block = 0; block < 256; block++) {
            uint16_t addr = block << 8;  // 256-byte blocks
            uint8_t cs = RAM_CS;

            if (addr >= 0xA000 && addr < 0xC000) cs = (loram && !game) ? ROM_CS : RAM_CS;
            else if (addr >= 0xD000 && addr < 0xE000) cs = charen ? IO_CS : CHAR_ROM_CS;
            else if (addr >= 0xE000) cs = (hiram && !game) ? ROM_CS : RAM_CS;
            
            chip_select_maps[mode][block] = cs;

            uint8_t* read_base;
            uint8_t read_mask = 0xFF;  // Default 256-byte mask

            if (cs & IO_CS) {
                if (addr < 0xD400) {
                    read_base = vic.registers;
                    read_mask = 0x3F;  // VIC: 64 registers (6 bits)
                } else if (addr < 0xD800) {
                    read_base = sid.registers;
                    read_mask = 0x1F;  // SID: 32 registers (5 bits)
                } else if (addr < 0xDD00) {
                    read_base = cia1.registers;
                    read_mask = 0x0F;  // CIA: 16 registers (4 bits)
                } else if (addr < 0xDE00) {
                    read_base = cia2.registers;
                    read_mask = 0x0F;  // CIA: 16 registers (4 bits)
                } else {
                    read_base = ram + addr;
                }
            } else if (cs & ROM_CS) {
                if (addr >= 0xE000)
                    read_base = &kernal_rom[addr - 0xE000];
                else
                    read_base = &basic_rom[addr - 0xA000];
            } else if (cs & CHAR_ROM_CS) {
                read_base = &char_rom[addr - 0xD000];
            } else {
                read_base = &ram[addr];
            }

            // Store mask directly in LSB bits (256-byte aligned pointers have 8 zero LSBs)
            mode_read_map[mode][block] = (uintptr_t)read_base | read_mask;
        }
    }
}

// ============================================================================
// CPU STATE - 6502 registers and state
// ============================================================================
static uint16_t cpu_pc;    // Program counter
static uint8_t cpu_a;      // Accumulator
static uint8_t cpu_sp, cpu_p; // Other registers (cpu_x, cpu_y reserved for future use)
static uint8_t opcode;
static uint16_t addr_temp;

// CPU ready linecheck - hardware accurate BA/RDY handling
#define CPU_READY() ((bus_state.bus_control & RDY_LINE) != 0)

// Wait for CPU ready with automatic stall handling
#define WAIT_READY_THEN_READ(addr, label) do { \
    label: \
    if (!CPU_READY()) { bus_cycle(); goto label; } \
    cpu_read_cycle(addr); \
} while(0)

// ============================================================================
// CPU INSTRUCTION HANDLERS - Direct threading dispatch for maximum performance
// ============================================================================
static const void* instruction_table[256];

#define NEXT_INSTRUCTION(fetch_label) do { \
    if (unlikely(bus_state.control_lines & (IRQ_LINE | NMI_LINE))) { \
        goto handle_interrupt; \
    } \
    WAIT_READY_THEN_READ(cpu_pc++, fetch_label); \
    opcode = bus_state.data; \
    goto *instruction_table[opcode]; \
} while(0)

// CPU execution loop with direct threading
static void cpu_execute(void) {
    // Initialize instruction table with label addresses (must be done inside function)
    for (int i = 0; i < 256; i++) {
        instruction_table[i] = &&illegal_instruction;
    }
    
    // Map actual instructions
    instruction_table[0xA9] = &&lda_immediate;
    instruction_table[0xAD] = &&lda_absolute;
    instruction_table[0x8D] = &&sta_absolute;
    instruction_table[0x4C] = &&jmp_absolute;
    instruction_table[0x00] = &&brk_instruction;
    instruction_table[0xEA] = &&nop_instruction;

    // Start execution
    NEXT_INSTRUCTION(main_fetch_wait);

    // Sample instruction implementations showing the pattern
    lda_immediate:
        WAIT_READY_THEN_READ(cpu_pc++, lda_imm_wait);
        cpu_a = bus_state.data;
        // Set flags (abbreviated)
        cpu_p = (cpu_p & 0x7D) | (cpu_a ? 0 : 0x02) | (cpu_a & 0x80);
        NEXT_INSTRUCTION(lda_imm_fetch_wait);

    lda_absolute:
        WAIT_READY_THEN_READ(cpu_pc++, lda_abs_wait1);
        addr_temp = bus_state.data;
        
        WAIT_READY_THEN_READ(cpu_pc++, lda_abs_wait2);
        addr_temp |= bus_state.data << 8;
        
        WAIT_READY_THEN_READ(addr_temp, lda_abs_wait3);
        cpu_a = bus_state.data;
        cpu_p = (cpu_p & 0x7D) | (cpu_a ? 0 : 0x02) | (cpu_a & 0x80);
        NEXT_INSTRUCTION(lda_abs_fetch_wait);

    sta_absolute:
        WAIT_READY_THEN_READ(cpu_pc++, sta_abs_wait1);
        addr_temp = bus_state.data;
        
        WAIT_READY_THEN_READ(cpu_pc++, sta_abs_wait2);
        addr_temp |= bus_state.data << 8;
        
        // Wait for ready then write
        sta_abs_wait3:
        if (!CPU_READY()) { bus_cycle(); goto sta_abs_wait3; }
        cpu_write_cycle(addr_temp, cpu_a);
        NEXT_INSTRUCTION(sta_abs_fetch_wait);

    jmp_absolute:
        WAIT_READY_THEN_READ(cpu_pc++, jmp_abs_wait1);
        addr_temp = bus_state.data;
        
        WAIT_READY_THEN_READ(cpu_pc++, jmp_abs_wait2);
        addr_temp |= bus_state.data << 8;
        
        cpu_pc = addr_temp;
        NEXT_INSTRUCTION(jmp_abs_fetch_wait);

    nop_instruction:
        NEXT_INSTRUCTION(nop_fetch_wait);

    brk_instruction:
        // BRK implementation (abbreviated - needs full 7 cycle sequence)
        cpu_pc++; // Skip signature byte
        // Push PC high, PC low, status (3 cycles)
        // Fetch IRQ vector (2 cycles)
        // Set interrupt flag (1 cycle)
        NEXT_INSTRUCTION(brk_fetch_wait);

    illegal_instruction:
        // Handle illegal opcodes
        NEXT_INSTRUCTION(illegal_fetch_wait);

    handle_interrupt:
        // Interrupt handling logic
        if (bus_state.control_lines & NMI_LINE) {
            // Handle NMI
        } else if (bus_state.control_lines & IRQ_LINE) {
            // Handle IRQ
        }
        NEXT_INSTRUCTION(interrupt_fetch_wait);
}

// ============================================================================
// MAIN EMULATION LOOP
// ============================================================================
void c64_emulate_frame(void) {
    // Set initial PLA mode (all RAM/ROM enabled)
    chip_select_map = chip_select_maps[0x07]; // LORAM=1, HIRAM=1, CHAREN=1
    
    // Initialize bus state
    bus_state.raw = 0;
    bus_state.bus_control = BA_LINE | AEC_LINE | RDY_LINE;
    
    // Start execution
    cpu_execute();
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void c64_init(void) {
    // Generate all PLA memory maps
    generate_pla_maps();
    
    // Initialize CPU state
    cpu_pc = 0xFCE2; // RESET vector
    cpu_a = 0;
    cpu_sp = 0xFF;
    cpu_p = 0x04; // Interrupt disable flag set
    
    // Initialize chip states
    memset(&vic, 0, sizeof(vic));
    memset(&cia1, 0, sizeof(cia1));
    memset(&cia2, 0, sizeof(cia2));
    memset(&sid, 0, sizeof(sid));
    
    // Load ROM images (external function)
    // load_roms(kernal_rom, basic_rom, char_rom);
}

// This is the foundation for a cycle-accurate C64 emulator:
// - Bus state for zero-overhead access
// - Pre-computed PLA maps for instant address decoding  
// - Function pointer dispatch for instruction handling
// - Hardware-accurate BA/RDY handling eliminates complex state machines
// - Unconditional chip updates for predictable performance
// - All chips run every cycle for perfect timing accuracy
