#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdint.h>
#include <stdbool.h>

// Maximum array sizes for processor tests
#define MAX_RAM_ENTRIES 16
#define MAX_RAM_BYTES 8

// JSON value types
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

// CPU state structure matching ProcessorTests format
typedef struct {
    uint16_t pc;
    uint8_t s;     // stack pointer
    uint8_t a;     // accumulator
    uint8_t x;     // X register
    uint8_t y;     // Y register
    uint8_t p;     // processor status
    
    // RAM entries: [address, [bytes...]]
    struct {
        uint16_t address;
        uint8_t bytes[MAX_RAM_BYTES];
        uint8_t byte_count;
    } ram[MAX_RAM_ENTRIES];
    uint8_t ram_count;
    
    // Cycles (only in final state)
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
int json_parse_number(const char* json, const char* key);
bool json_parse_string(const char* json, const char* key, char* output, size_t max_len);

// Utility functions
const char* json_find_key(const char* json, const char* key);
const char* json_skip_whitespace(const char* str);
const char* json_find_object_end(const char* json);
const char* json_find_array_end(const char* json);

#endif // JSON_PARSER_H