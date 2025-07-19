#ifndef C64_BUS_OPTIMIZED_H
#define C64_BUS_OPTIMIZED_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct c64_s c64_t;
typedef struct vic_state_s vic_state_t;
typedef struct sid_state_s sid_state_t;
typedef struct cia_state_s cia_state_t;

// Chip IDs (must be consecutive for optimal jump table)
#define CHIP_VIC       0
#define CHIP_SID       1  
#define CHIP_CIA1      2
#define CHIP_CIA2      3
#define CHIP_RAM       4
#define CHIP_COLORRAM  5
#define CHIP_BASIC     6
#define CHIP_KERNAL    7
#define CHIP_CHARROM   8
#define CHIP_ROML      9
#define CHIP_ROMH      10
#define CHIP_IO1       11
#define CHIP_IO2       12
#define CHIP_UNMAPPED  13
#define CHIP_IO_REGION 14  // Special marker for I/O area (gets refined)
#define CHIP_MAX       15

// Control line masks
#define LINE_MASK_RW   4   // Bit 2 = R/W line (1=read, 0=write)
#define BA_LINE        1   // Bus Available
#define AEC_LINE       2   // Address Enable Control
#define RDY_LINE       4   // Ready

// System line masks for cartridge signals
#define SYS_MASK_EXROM 8   // EXROM signal
#define SYS_MASK_GAME  16  // GAME signal

// Chip select encoding structure
typedef struct {
    uint8_t read_chip  : 4;  // 4 bits = 16 possible read chips (0-15)
    uint8_t write_chip : 4;  // 4 bits = 16 possible write chips (0-15)
} chip_select_t;

// 32-bit bus state register (passed via register argument)
typedef union {
    uint32_t raw;           // 32-bit register value
    struct {
        uint16_t addr;      // Bits 0-15: Address bus
        uint8_t data;       // Bits 16-23: Data bus
        uint8_t lines;      // Bits 24-31: Control lines including R/W
    };
} c64_bus_state_t;

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
    REGISTER_CALL c64_bus_state_t (*io1_read)(void* context, c64_bus_state_t bus_state);
    REGISTER_CALL c64_bus_state_t (*io1_write)(void* context, c64_bus_state_t bus_state);
    REGISTER_CALL c64_bus_state_t (*io2_read)(void* context, c64_bus_state_t bus_state);
    REGISTER_CALL c64_bus_state_t (*io2_write)(void* context, c64_bus_state_t bus_state);
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

// Chip advance cycle functions - WITH BUS STATE for interrupt handling
FORCE_INLINE REGISTER_CALL c64_bus_state_t vic_advance_cycle(vic_state_t* vic, c64_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL c64_bus_state_t sid_advance_cycle(sid_state_t* sid, c64_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL c64_bus_state_t cia_advance_cycle(cia_state_t* cia, c64_bus_state_t bus_state);

// Chip I/O functions with register calling convention (called only when chip is selected)
FORCE_INLINE REGISTER_CALL c64_bus_state_t vic_read(vic_state_t* vic, c64_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL c64_bus_state_t vic_write(vic_state_t* vic, c64_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL c64_bus_state_t sid_read(sid_state_t* sid, c64_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL c64_bus_state_t sid_write(sid_state_t* sid, c64_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL c64_bus_state_t cia_read(cia_state_t* cia, c64_bus_state_t bus_state);
FORCE_INLINE REGISTER_CALL c64_bus_state_t cia_write(cia_state_t* cia, c64_bus_state_t bus_state);

// Non-CPU cycle function (ticks all chips except CPU)
REGISTER_CALL c64_bus_state_t c64_non_cpu_cycle(c64_t* c64, c64_bus_state_t bus_state);

#endif // C64_BUS_OPTIMIZED_H