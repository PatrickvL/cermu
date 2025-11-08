#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Maximum array sizes for processor tests
#define MAX_RAM_ENTRIES 16
#define MAX_RAM_BYTES 8
#define MAX_BUS_CYCLES 32

// JSON value types
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

// Bus cycle trace entry (supports both legacy and 65816 formats)
typedef struct {
    uint32_t address;   // 24-bit address for 65816, 16-bit for legacy
    uint8_t data;
    union {
        bool is_write;  // legacy format: true = write, false = read
        struct {        // 65816 format: 8-character output flags
            bool vda;   // VDA (Valid Data Address)
            bool vpa;   // VPA (Valid Program Address) 
            bool vpb;   // VPB (Vector Pull)
            bool rwb;   // RWB (Read/Write: true=read, false=write)
            bool e;     // E (Emulation mode)
            bool m;     // M (Memory/Accumulator 8-bit)
            bool x;     // X (Index register 8-bit)
            bool mlb;   // MLB (Memory Lock)
        } flags_65816;
    };
    bool has_65816_flags; // true if flags_65816 is valid, false if is_write is valid
} bus_cycle_t;

// CPU state structure matching ProcessorTests format
typedef struct {
    uint16_t pc;
    uint8_t s;     // stack pointer
    uint8_t a;     // accumulator
    uint8_t x;     // X register
    uint8_t y;     // Y register
    uint8_t p;     // processor status
    
    // 65816-specific registers (optional)
    uint8_t e;     // emulation mode flag (65816)
    uint8_t dbr;   // data bank register (65816)  
    uint16_t d;    // direct page register (65816)
    uint8_t pbr;   // program bank register (65816)
    bool has_65816_state; // flag indicating if 65816 fields are valid
    
    // RAM entries: [address, [bytes...]] (24-bit addresses for 65816)
    struct {
        uint32_t address;   // 24-bit address for 65816, 16-bit for legacy
        uint8_t bytes[MAX_RAM_BYTES];
        uint8_t byte_count;
    } ram[MAX_RAM_ENTRIES];
    uint8_t ram_count;
    
    // Bus cycle trace (detailed memory access sequence)
    bus_cycle_t bus_cycles[MAX_BUS_CYCLES];
    uint8_t bus_cycle_count;
    bool has_bus_cycles;
    
    // Cycles count (only in final state for compatibility)
    uint32_t cycles;
    bool has_cycles;
} cpu_state_t;

// Single processor test case
typedef struct {
    char name[128];
    cpu_state_t initial;
    cpu_state_t final;
} processor_test_t;

// JSON parsing functions
bool json_parse_processor_test(const char* json_content, processor_test_t* test);
bool json_parse_cpu_state(const char* json, const char* state_name, cpu_state_t* state);
bool json_parse_ram_array(const char* json, cpu_state_t* state);
bool json_parse_cycles_array(const char* json, cpu_state_t* state);
int json_parse_number(const char* json, const char* key);
bool json_parse_string(const char* json, const char* key, char* output, size_t max_len);

// Utility functions
const char* json_find_key(const char* json, const char* key);
const char* json_skip_whitespace(const char* str);
const char* json_find_object_end(const char* json);
const char* json_find_array_end(const char* json);
