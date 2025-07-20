#ifndef C64_BUS_OPTIMIZED_H
#define C64_BUS_OPTIMIZED_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct c64_s c64_t;
typedef struct vic_state_s vic_state_t;
typedef struct sid_state_s sid_state_t;
typedef struct cia_state_s cia_state_t;

// Chip IDs ordered by memory size (largest first), then I/O by page number
// Must be consecutive for optimal jump table
#define CHIP_RAM       0   // 64KB RAM (largest)
#define CHIP_BASIC     1   // 8KB BASIC ROM
#define CHIP_KERNAL    2   // 8KB KERNAL ROM
#define CHIP_ROML      3   // 8KB ROM Low (cartridge)
#define CHIP_ROMH      4   // 8KB ROM High (cartridge)
#define CHIP_CHARROM   5   // 4KB Character ROM
#define CHIP_COLORRAM  6   // 1KB Color RAM (smallest memory)
#define CHIP_UNMAPPED  7   // Unmapped regions
// I/O pages in $D000-$DFFF range (16 pages of $100 bytes each)
#define CHIP_VIC_0     8   // $D000-$D0FF (I/O page 0) - VIC-II registers
#define CHIP_VIC_1     9   // $D100-$D1FF (I/O page 1) - VIC-II mirrors
#define CHIP_VIC_2    10   // $D200-$D2FF (I/O page 2) - VIC-II mirrors  
#define CHIP_VIC_3    11   // $D300-$D3FF (I/O page 3) - VIC-II mirrors
#define CHIP_SID_0    12   // $D400-$D4FF (I/O page 4) - SID registers
#define CHIP_SID_1    13   // $D500-$D5FF (I/O page 5) - SID mirrors
#define CHIP_SID_2    14   // $D600-$D6FF (I/O page 6) - SID mirrors
#define CHIP_SID_3    15   // $D700-$D7FF (I/O page 7) - SID mirrors
#define CHIP_COLORRAM_PAGE 16 // $D800-$D8FF (I/O page 8) - Color RAM via VIC
#define CHIP_IO_UNMAPPED_9 17 // $D900-$D9FF (I/O page 9) - Unmapped
#define CHIP_IO_UNMAPPED_A 18 // $DA00-$DAFF (I/O page 10) - Unmapped
#define CHIP_IO_UNMAPPED_B 19 // $DB00-$DBFF (I/O page 11) - Unmapped  
#define CHIP_CIA1     20   // $DC00-$DCFF (I/O page 12) - CIA1
#define CHIP_CIA2     21   // $DD00-$DDFF (I/O page 13) - CIA2
#define CHIP_IO1      22   // $DE00-$DEFF (I/O page 14) - Cartridge I/O 1
#define CHIP_IO2      23   // $DF00-$DFFF (I/O page 15) - Cartridge I/O 2
#define CHIP_MAX      24

// Convenience aliases for the primary I/O chips
#define CHIP_VIC      CHIP_VIC_0    // Primary VIC-II chip (first I/O chip)
#define CHIP_SID      CHIP_SID_0    // Primary SID chip

// System line masks for cartridge signals (moved out of control lines to separate field)
#define SYS_MASK_EXROM 8   // EXROM signal
#define SYS_MASK_GAME  16  // GAME signal

// Chip select encoding structure
typedef struct {
    uint8_t read_chip  : 4;  // 4 bits = 16 possible read chips (0-15)
    uint8_t write_chip : 4;  // 4 bits = 16 possible write chips (0-15)
} chip_select_t;

// ============================================================================
// GENERIC 16-BIT SYSTEM BUS STATE (REUSABLE ACROSS SYSTEMS)
// ============================================================================

// Generic bus state for 16-bit systems (32-bit register value)
typedef union {
    uint32_t raw;           // 32-bit register value
    struct {
        uint16_t addr;      // Bits 0-15: Address bus
        uint8_t data;       // Bits 16-23: Data bus
        uint8_t lines;      // Bits 24-31: Control lines including R/W
    };
} generic_bus_state_t;

// Generic control line masks (system-independent)
#define GENERIC_RW_LINE    0x01  // Read/Write line (1=read, 0=write)
#define GENERIC_IRQ_LINE   0x02  // Interrupt request line
#define GENERIC_NMI_LINE   0x04  // Non-maskable interrupt line
#define GENERIC_RDY_LINE   0x08  // Ready line
#define GENERIC_BA_LINE    0x10  // Bus available line
#define GENERIC_AEC_LINE   0x20  // Address enable control line

// C64-specific bus state (extends generic bus state)
typedef generic_bus_state_t c64_bus_state_t;

// C64 bus controller structure
typedef struct c64_bus_s {
    // System state
    c64_t* c64;                           // Parent C64 system
    uint16_t address;                     // Current bus address
    uint8_t data;                         // Current bus data
    uint8_t control_lines;                // BA, AEC, RDY lines
    uint8_t system_lines;                 // EXROM, GAME signals
    
    // Optimized chip selection arrays (precalculated from PLA)
    uint8_t chip_select_per_bank[16];     // Current banking mode
    uint8_t chip_select_per_bank_per_mode[32][16]; // All PLA modes
    uint8_t pla_banking_mode;             // Current 5-bit PLA mode
} c64_bus_t;

// Memory subsystem structures
typedef struct {
    uint8_t memory[65536];
} ram_state_t;

typedef struct {
    uint8_t memory[1024];  // Color RAM is only 1KB
} colorram_state_t;

typedef struct {
    uint8_t* roml;         // 8KB ROM Low
    uint8_t* romh;         // 8KB ROM High
    void* context;
    REGISTER_CALL c64_bus_state_t (*io1_read)(void* context, generic_bus_state_t bus_state);
    REGISTER_CALL c64_bus_state_t (*io1_write)(void* context, generic_bus_state_t bus_state);
    REGISTER_CALL c64_bus_state_t (*io2_read)(void* context, generic_bus_state_t bus_state);
    REGISTER_CALL c64_bus_state_t (*io2_write)(void* context, generic_bus_state_t bus_state);
} cartridge_state_t;

// Main C64 system structure
typedef struct c64_s {
    c64_bus_t* bus;
    vic_state_t* vic;
    sid_state_t* sid;
    cia_state_t* cia1;
    cia_state_t* cia2;
    ram_state_t ram;
    colorram_state_t colorram;
    cartridge_state_t cartridge;
    
    // ROM data
    uint8_t basic_rom[8192];
    uint8_t kernal_rom[8192]; 
    uint8_t char_rom[4096];
} c64_t;

// Cross-platform calling convention for register arguments/returns
#ifdef _MSC_VER
    #define REGISTER_CALL __vectorcall  // Ensures register passing on MSVC
#elif defined(__GNUC__) || defined(__clang__)
    #define REGISTER_CALL __attribute__((regparm(3)))  // Register calling on GCC/Clang
#else
    #define REGISTER_CALL
#endif

// Cross-platform force inline macro
#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

// Simple ROM read macro for code deduplication
#define SIMPLE_ROM_READ(buffer, mask) \
    bus.data = (buffer)[bus.addr & (mask)]

// Function declarations
c64_bus_t* c64_bus_create(void);
void c64_bus_destroy(c64_bus_t* bus);
void c64_bus_attach_c64(c64_bus_t* bus, c64_t* c64);

// Memory access functions with bus in/out pattern and register calling convention
REGISTER_CALL c64_bus_state_t c64_bus_read_cycle(c64_bus_t* bus, c64_bus_state_t bus_state_in);
REGISTER_CALL c64_bus_state_t c64_bus_write_cycle(c64_bus_t* bus, c64_bus_state_t bus_state_in);

// PLA mode switching
void c64_bus_mode_switch(c64_bus_t* bus, uint8_t mode);
uint8_t c64_bus_generate_pla_mode(c64_bus_t* bus, uint8_t cpu_port_bits);

// Cartridge control
void c64_bus_set_cartridge_signals(c64_bus_t* bus, bool exrom_active, bool game_active);

// System tick functions (optimized read/write variants with bus state)
FORCE_INLINE REGISTER_CALL c64_bus_state_t c64_system_tick_read(c64_t* c64, c64_bus_state_t bus_state_in);
FORCE_INLINE REGISTER_CALL c64_bus_state_t c64_system_tick_write(c64_t* c64, c64_bus_state_t bus_state_in);

// Chip advance cycle functions - WITH GENERIC BUS STATE for system independence
FORCE_INLINE REGISTER_CALL generic_bus_state_t vic_advance_cycle(vic_state_t* vic, generic_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL generic_bus_state_t sid_advance_cycle(sid_state_t* sid, generic_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL generic_bus_state_t cia_advance_cycle(cia_state_t* cia, generic_bus_state_t bus_state);

// Chip I/O functions with generic bus state (called only when chip is selected)
FORCE_INLINE REGISTER_CALL generic_bus_state_t vic_read(vic_state_t* vic, generic_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL generic_bus_state_t vic_write(vic_state_t* vic, generic_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL generic_bus_state_t sid_read(sid_state_t* sid, generic_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL generic_bus_state_t sid_write(sid_state_t* sid, generic_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL generic_bus_state_t cia_read(cia_state_t* cia, generic_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL generic_bus_state_t cia_write(cia_state_t* cia, generic_bus_state_t bus_state);

// Non-CPU cycle function (ticks all chips except CPU)
REGISTER_CALL c64_bus_state_t c64_non_cpu_cycle(c64_t* c64, c64_bus_state_t bus_state);

#endif // C64_BUS_OPTIMIZED_H