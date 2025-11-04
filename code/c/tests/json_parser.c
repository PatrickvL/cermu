#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// JSON PARSER IMPLEMENTATION
// ============================================================================

// Skip whitespace characters
const char* json_skip_whitespace(const char* str) {
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) {
        str++;
    }
    return str;
}

// Find the value associated with a key in a JSON object
const char* json_find_key(const char* json, const char* key) {
    const char* pos = json;
    size_t key_len = strlen(key);
    
    pos = json_skip_whitespace(pos);
    if (*pos != '{') {
        return NULL; // Not a JSON object
    }
    pos++; // Skip opening brace
    
    while (*pos) {
        pos = json_skip_whitespace(pos);
        if (*pos == '}') break; // End of object
        
        // Expect a string key
        if (*pos != '"') return NULL;
        pos++; // Skip opening quote
        
        // Check if this is our key
        if (strncmp(pos, key, key_len) == 0 && pos[key_len] == '"') {
            pos += key_len + 1; // Skip key and closing quote
            pos = json_skip_whitespace(pos);
            if (*pos != ':') return NULL;
            pos++; // Skip colon
            return json_skip_whitespace(pos);
        }
        
        // Skip to end of this key
        while (*pos && *pos != '"') {
            if (*pos == '\\') pos++; // Skip escaped characters
            pos++;
        }
        if (*pos != '"') return NULL;
        pos++; // Skip closing quote
        
        pos = json_skip_whitespace(pos);
        if (*pos != ':') return NULL;
        pos++; // Skip colon
        
        // Skip the value
        pos = json_skip_whitespace(pos);
        if (*pos == '"') {
            // String value
            pos++;
            while (*pos && *pos != '"') {
                if (*pos == '\\') pos++; // Skip escaped characters
                pos++;
            }
            if (*pos == '"') pos++;
        } else if (*pos == '{') {
            // Object value - find matching closing brace
            int depth = 1;
            pos++;
            while (*pos && depth > 0) {
                if (*pos == '{') depth++;
                else if (*pos == '}') depth--;
                pos++;
            }
        } else if (*pos == '[') {
            // Array value - find matching closing bracket
            int depth = 1;
            pos++;
            while (*pos && depth > 0) {
                if (*pos == '[') depth++;
                else if (*pos == ']') depth--;
                pos++;
            }
        } else {
            // Number, boolean, or null
            while (*pos && *pos != ',' && *pos != '}' && !isspace(*pos)) {
                pos++;
            }
        }
        
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++; // Skip comma if present
    }
    
    return NULL; // Key not found
}

// Find the end of a JSON object
const char* json_find_object_end(const char* json) {
    const char* pos = json;
    if (*pos != '{') return NULL;
    
    int depth = 1;
    pos++;
    
    while (*pos && depth > 0) {
        if (*pos == '{') {
            depth++;
        } else if (*pos == '}') {
            depth--;
        } else if (*pos == '"') {
            // Skip string
            pos++;
            while (*pos && *pos != '"') {
                if (*pos == '\\') pos++; // Skip escaped characters
                pos++;
            }
            if (*pos == '"') pos++;
            continue;
        }
        pos++;
    }
    
    return (depth == 0) ? pos - 1 : NULL;
}

// Find the end of a JSON array
const char* json_find_array_end(const char* json) {
    const char* pos = json;
    if (*pos != '[') return NULL;
    
    int depth = 1;
    pos++;
    
    while (*pos && depth > 0) {
        if (*pos == '[') {
            depth++;
        } else if (*pos == ']') {
            depth--;
        } else if (*pos == '"') {
            // Skip string
            pos++;
            while (*pos && *pos != '"') {
                if (*pos == '\\') pos++; // Skip escaped characters
                pos++;
            }
            if (*pos == '"') pos++;
            continue;
        }
        pos++;
    }
    
    return (depth == 0) ? pos - 1 : NULL;
}

// Parse a number from JSON
int json_parse_number(const char* json, const char* key) {
    const char* value = json_find_key(json, key);
    if (!value) return 0;
    
    return (int)strtol(value, NULL, 0); // Support hex with 0x prefix
}

// Parse a string from JSON
bool json_parse_string(const char* json, const char* key, char* output, size_t max_len) {
    const char* value = json_find_key(json, key);
    if (!value || *value != '"') return false;
    
    value++; // Skip opening quote
    size_t i = 0;
    
    while (*value && *value != '"' && i < max_len - 1) {
        if (*value == '\\') {
            value++; // Skip backslash
            if (*value) {
                output[i++] = *value++;
            }
        } else {
            output[i++] = *value++;
        }
    }
    
    output[i] = '\0';
    return *value == '"'; // Should end with closing quote
}

// Parse RAM array from JSON - ProcessorTests format: [[address, byte], [address, byte], ...]
bool json_parse_ram_array(const char* json, cpu_state_t* state) {
    const char* ram_array = json_find_key(json, "ram");
    if (!ram_array || *ram_array != '[') {
        state->ram_count = 0;
        return true; // Empty RAM is valid
    }
    
    const char* pos = ram_array + 1; // Skip opening bracket
    state->ram_count = 0;
    
    while (*pos && state->ram_count < MAX_RAM_ENTRIES) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break; // End of array
        
        if (*pos != '[') return false; // Each RAM entry should be an array
        pos++; // Skip opening bracket of RAM entry
        
        // Parse address (first element)
        pos = json_skip_whitespace(pos);
        state->ram[state->ram_count].address = (uint16_t)strtol(pos, (char**)&pos, 0);
        
        pos = json_skip_whitespace(pos);
        if (*pos != ',') return false;
        pos++; // Skip comma
        
        // Parse single byte (second element) - ProcessorTests format
        pos = json_skip_whitespace(pos);
        state->ram[state->ram_count].bytes[0] = (uint8_t)strtol(pos, (char**)&pos, 0);
        state->ram[state->ram_count].byte_count = 1; // Always 1 byte per entry in ProcessorTests
        
        pos = json_skip_whitespace(pos);
        if (*pos != ']') return false; // Should end RAM entry array
        pos++; // Skip closing bracket of RAM entry
        
        state->ram_count++;
        
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++; // Skip comma if present
    }
    
    return true;
}

// Parse cycles array from JSON (bus cycles)
bool json_parse_cycles_array(const char* json, cpu_state_t* state) {
    const char* cycles_array = json_find_key(json, "cycles");
    if (!cycles_array || *cycles_array != '[') {
        state->has_bus_cycles = false;
        state->bus_cycle_count = 0;
        return true; // No cycles is valid
    }
    
    const char* pos = cycles_array + 1; // Skip opening bracket
    state->bus_cycle_count = 0;
    state->has_bus_cycles = true;
    
    while (*pos && state->bus_cycle_count < MAX_BUS_CYCLES) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break; // End of array
        
        if (*pos != '[') return false; // Each cycle should be an array
        pos++; // Skip opening bracket
        
        // Parse address
        pos = json_skip_whitespace(pos);
        state->bus_cycles[state->bus_cycle_count].address = (uint16_t)strtol(pos, (char**)&pos, 0);
        
        pos = json_skip_whitespace(pos);
        if (*pos != ',') return false;
        pos++; // Skip comma
        
        // Parse data
        pos = json_skip_whitespace(pos);
        state->bus_cycles[state->bus_cycle_count].data = (uint8_t)strtol(pos, (char**)&pos, 0);
        
        pos = json_skip_whitespace(pos);
        if (*pos != ',') return false;
        pos++; // Skip comma
        
        // Parse read/write flag
        pos = json_skip_whitespace(pos);
        if (*pos == '"') {
            pos++; // Skip opening quote
            state->bus_cycles[state->bus_cycle_count].is_write = (*pos == 'w' || *pos == 'W');
            while (*pos && *pos != '"') pos++; // Skip to closing quote
            if (*pos == '"') pos++;
        } else {
            // Numeric format: assume 0 = read, 1 = write
            int rw = (int)strtol(pos, (char**)&pos, 0);
            state->bus_cycles[state->bus_cycle_count].is_write = (rw != 0);
        }
        
        pos = json_skip_whitespace(pos);
        if (*pos != ']') return false; // Should end cycle array
        pos++; // Skip closing bracket
        
        state->bus_cycle_count++;
        
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++; // Skip comma if present
    }
    
    return true;
}

// Parse CPU state from JSON
bool json_parse_cpu_state(const char* json, const char* state_name, cpu_state_t* state) {
    const char* state_obj = json_find_key(json, state_name);
    if (!state_obj || *state_obj != '{') {
        return false;
    }
    
    // Parse registers
    state->pc = (uint16_t)json_parse_number(state_obj, "pc");
    state->s = (uint8_t)json_parse_number(state_obj, "s");
    state->a = (uint8_t)json_parse_number(state_obj, "a");
    state->x = (uint8_t)json_parse_number(state_obj, "x");
    state->y = (uint8_t)json_parse_number(state_obj, "y");
    state->p = (uint8_t)json_parse_number(state_obj, "p");
    
    // Parse 65816-specific registers (optional)
    const char* e_value = json_find_key(state_obj, "e");
    const char* dbr_value = json_find_key(state_obj, "dbr");
    const char* d_value = json_find_key(state_obj, "d");
    const char* pbr_value = json_find_key(state_obj, "pbr");
    
    if (e_value || dbr_value || d_value || pbr_value) {
        state->has_65816_state = true;
        state->e = e_value ? (uint8_t)json_parse_number(state_obj, "e") : 0;
        state->dbr = dbr_value ? (uint8_t)json_parse_number(state_obj, "dbr") : 0;
        state->d = d_value ? (uint16_t)json_parse_number(state_obj, "d") : 0;
        state->pbr = pbr_value ? (uint8_t)json_parse_number(state_obj, "pbr") : 0;
    } else {
        state->has_65816_state = false;
        state->e = 0;
        state->dbr = 0;
        state->d = 0;
        state->pbr = 0;
    }
    
    // Parse RAM
    if (!json_parse_ram_array(state_obj, state)) {
        return false;
    }
    
    // Parse cycles (optional)
    const char* cycles_value = json_find_key(state_obj, "cycles");
    if (cycles_value && *cycles_value >= '0' && *cycles_value <= '9') {
        state->cycles = (uint32_t)strtoul(cycles_value, NULL, 10);
        state->has_cycles = true;
    } else {
        state->has_cycles = false;
        state->cycles = 0;
    }
    
    // Parse bus cycles (optional)
    json_parse_cycles_array(state_obj, state);
    
    return true;
}

// Parse a complete processor test from JSON
bool json_parse_processor_test(const char* json_content, processor_test_t* test) {
    if (!json_content || !test) {
        return false;
    }
    
    // Parse test name
    if (!json_parse_string(json_content, "name", test->name, sizeof(test->name))) {
        strcpy(test->name, "unnamed_test");
    }
    
    // Parse initial state
    if (!json_parse_cpu_state(json_content, "initial", &test->initial)) {
        return false;
    }
    
    // Parse final state
    if (!json_parse_cpu_state(json_content, "final", &test->final)) {
        return false;
    }
    
    return true;
}