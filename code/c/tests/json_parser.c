#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Skip whitespace and return pointer to next non-whitespace character
const char* json_skip_whitespace(const char* str) {
    while (str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) {
        str++;
    }
    return str;
}

// Find a key in JSON object and return pointer to its value
const char* json_find_key(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) return NULL;
    
    // Move past the key
    pos += strlen(search_key);
    pos = json_skip_whitespace(pos);
    
    // Expect colon
    if (*pos != ':') return NULL;
    pos++;
    
    return json_skip_whitespace(pos);
}

// Find the end of an object (matching closing brace)
const char* json_find_object_end(const char* json) {
    if (!json || *json != '{') return NULL;
    
    int brace_count = 1;
    json++; // Skip opening brace
    
    while (*json && brace_count > 0) {
        if (*json == '{') {
            brace_count++;
        } else if (*json == '}') {
            brace_count--;
        } else if (*json == '"') {
            // Skip string content
            json++;
            while (*json && *json != '"') {
                if (*json == '\\') json++; // Skip escaped character
                json++;
            }
        }
        json++;
    }
    
    return brace_count == 0 ? json - 1 : NULL;
}

// Find the end of an array (matching closing bracket)
const char* json_find_array_end(const char* json) {
    if (!json || *json != '[') return NULL;
    
    int bracket_count = 1;
    json++; // Skip opening bracket
    
    while (*json && bracket_count > 0) {
        if (*json == '[') {
            bracket_count++;
        } else if (*json == ']') {
            bracket_count--;
        } else if (*json == '"') {
            // Skip string content
            json++;
            while (*json && *json != '"') {
                if (*json == '\\') json++; // Skip escaped character
                json++;
            }
        }
        json++;
    }
    
    return bracket_count == 0 ? json - 1 : NULL;
}

// Parse a number value for a given key
int json_parse_number(const char* json, const char* key) {
    const char* value = json_find_key(json, key);
    if (!value) return -1;
    
    return (int)strtol(value, NULL, 10);
}

// Parse a string value for a given key
bool json_parse_string(const char* json, const char* key, char* output, size_t max_len) {
    const char* value = json_find_key(json, key);
    if (!value || *value != '"') return false;
    
    value++; // Skip opening quote
    size_t i = 0;
    
    while (*value && *value != '"' && i < max_len - 1) {
        if (*value == '\\') {
            value++; // Skip escape character
            if (*value) {
                output[i++] = *value++;
            }
        } else {
            output[i++] = *value++;
        }
    }
    
    output[i] = '\0';
    return *value == '"';
}

// Parse RAM array: supports both [[address, value], ...] and [{"address": N, "bytes": [...]}, ...]
bool json_parse_ram_array(const char* json, cpu_state_t* state) {
    const char* ram_start = json_find_key(json, "ram");
    if (!ram_start || *ram_start != '[') return true; // RAM is optional
    
    state->ram_count = 0;
    const char* pos = ram_start + 1; // Skip opening bracket
    
    while (*pos && *pos != ']' && state->ram_count < MAX_RAM_ENTRIES) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break;
        
        if (*pos == '[') {
            // ProcessorTests format: [address, value]
            pos++;
            
            // Parse address
            pos = json_skip_whitespace(pos);
            state->ram[state->ram_count].address = (uint16_t)strtol(pos, (char**)&pos, 10);
            
            // Expect comma
            pos = json_skip_whitespace(pos);
            if (*pos != ',') break;
            pos++;
            
            // Parse value or array of values
            pos = json_skip_whitespace(pos);
            if (*pos == '[') {
                // Array of bytes: [byte1, byte2, ...]
                pos++; // Skip opening bracket
                state->ram[state->ram_count].byte_count = 0;
                
                while (*pos && *pos != ']' && state->ram[state->ram_count].byte_count < MAX_RAM_BYTES) {
                    pos = json_skip_whitespace(pos);
                    if (*pos == ']') break;
                    
                    state->ram[state->ram_count].bytes[state->ram[state->ram_count].byte_count++] =
                        (uint8_t)strtol(pos, (char**)&pos, 10);
                    
                    pos = json_skip_whitespace(pos);
                    if (*pos == ',') pos++;
                }
                
                // Skip closing bracket
                pos = json_skip_whitespace(pos);
                if (*pos == ']') pos++;
            } else {
                // Single value
                state->ram[state->ram_count].bytes[0] = (uint8_t)strtol(pos, (char**)&pos, 10);
                state->ram[state->ram_count].byte_count = 1;
            }
            
            // Skip closing bracket for this pair
            pos = json_skip_whitespace(pos);
            if (*pos == ']') pos++;
            
        } else if (*pos == '{') {
            // Extended format: {"address": N, "bytes": [...]}
            const char* obj_end = json_find_object_end(pos);
            if (!obj_end) break;
            
            // Create temporary buffer for this object
            size_t obj_len = obj_end - pos + 1;
            char* obj_json = (char*)malloc(obj_len + 1);
            if (!obj_json) break;
            
            strncpy(obj_json, pos, obj_len);
            obj_json[obj_len] = '\0';
            
            // Parse address
            int addr = json_parse_number(obj_json, "address");
            if (addr < 0) {
                free(obj_json);
                break;
            }
            state->ram[state->ram_count].address = (uint16_t)addr;
            
            // Parse bytes array
            const char* bytes_start = json_find_key(obj_json, "bytes");
            if (bytes_start && *bytes_start == '[') {
                const char* bytes_pos = bytes_start + 1;
                state->ram[state->ram_count].byte_count = 0;
                
                while (*bytes_pos && *bytes_pos != ']' &&
                       state->ram[state->ram_count].byte_count < MAX_RAM_BYTES) {
                    bytes_pos = json_skip_whitespace(bytes_pos);
                    if (*bytes_pos == ']') break;
                    
                    int byte_val = (int)strtol(bytes_pos, (char**)&bytes_pos, 10);
                    state->ram[state->ram_count].bytes[state->ram[state->ram_count].byte_count++] =
                        (uint8_t)byte_val;
                    
                    bytes_pos = json_skip_whitespace(bytes_pos);
                    if (*bytes_pos == ',') bytes_pos++;
                }
            } else {
                // Fallback: single value
                state->ram[state->ram_count].bytes[0] = 0;
                state->ram[state->ram_count].byte_count = 1;
            }
            
            free(obj_json);
            pos = obj_end + 1;
        } else {
            break; // Unknown format
        }
        
        state->ram_count++;
        
        // Skip comma between entries
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;
    }
    
    return true;
}

// Parse cycles array: [[address, data, "read/write"], ...]
bool json_parse_cycles_array(const char* json, cpu_state_t* state) {
    const char* cycles_start = json_find_key(json, "cycles");
    if (!cycles_start || *cycles_start != '[') {
        state->bus_cycle_count = 0;
        state->has_bus_cycles = false;
        return true; // Cycles array is optional
    }
    
    state->bus_cycle_count = 0;
    state->has_bus_cycles = true;
    const char* pos = cycles_start + 1; // Skip opening bracket
    
    while (*pos && *pos != ']' && state->bus_cycle_count < MAX_BUS_CYCLES) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break;
        
        if (*pos == '[') {
            // Parse cycle entry: [address, data, "read"/"write"]
            pos++;
            
            // Parse address
            pos = json_skip_whitespace(pos);
            state->bus_cycles[state->bus_cycle_count].address = (uint16_t)strtol(pos, (char**)&pos, 10);
            
            // Expect comma
            pos = json_skip_whitespace(pos);
            if (*pos != ',') break;
            pos++;
            
            // Parse data
            pos = json_skip_whitespace(pos);
            state->bus_cycles[state->bus_cycle_count].data = (uint8_t)strtol(pos, (char**)&pos, 10);
            
            // Expect comma
            pos = json_skip_whitespace(pos);
            if (*pos != ',') break;
            pos++;
            
            // Parse "read" or "write"
            pos = json_skip_whitespace(pos);
            if (*pos == '"') {
                pos++; // Skip opening quote
                if (strncmp(pos, "write", 5) == 0) {
                    state->bus_cycles[state->bus_cycle_count].is_write = true;
                    pos += 5;
                } else if (strncmp(pos, "read", 4) == 0) {
                    state->bus_cycles[state->bus_cycle_count].is_write = false;
                    pos += 4;
                } else {
                    break; // Invalid read/write type
                }
                
                // Skip closing quote
                if (*pos == '"') pos++;
            } else {
                break; // Expected quoted string
            }
            
            // Skip closing bracket for this cycle
            pos = json_skip_whitespace(pos);
            if (*pos == ']') pos++;
            
            state->bus_cycle_count++;
        } else {
            break; // Unknown format
        }
        
        // Skip comma between entries
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;
    }
    
    return true;
}

// Parse CPU state (initial or final)
bool json_parse_cpu_state(const char* json, const char* state_name, cpu_state_t* state) {
    const char* state_start = json_find_key(json, state_name);
    if (!state_start || *state_start != '{') return false;
    
    const char* state_end = json_find_object_end(state_start);
    if (!state_end) return false;
    
    // Create a temporary buffer for the state object
    size_t state_len = state_end - state_start + 1;
    char* state_json = (char*)malloc(state_len + 1);
    if (!state_json) return false;
    
    strncpy(state_json, state_start, state_len);
    state_json[state_len] = '\0';
    
    // Parse individual fields
    state->pc = (uint16_t)json_parse_number(state_json, "pc");
    state->s = (uint8_t)json_parse_number(state_json, "s");
    state->a = (uint8_t)json_parse_number(state_json, "a");
    state->x = (uint8_t)json_parse_number(state_json, "x");
    state->y = (uint8_t)json_parse_number(state_json, "y");
    state->p = (uint8_t)json_parse_number(state_json, "p");
    
    // Parse cycles (only present in final state)
    int cycles = json_parse_number(state_json, "cycles");
    if (cycles >= 0) {
        state->cycles = (uint32_t)cycles;
        state->has_cycles = true;
    } else {
        state->cycles = 0;
        state->has_cycles = false;
    }
    
    // Parse RAM array
    bool ram_ok = json_parse_ram_array(state_json, state);
    
    // Parse cycles array (bus trace)
    bool cycles_ok = json_parse_cycles_array(state_json, state);
    
    free(state_json);
    return ram_ok && cycles_ok;
}

// Parse a complete processor test from JSON
bool json_parse_processor_test(const char* json_content, processor_test_t* test) {
    if (!json_content || !test) return false;
    
    // Initialize test structure
    memset(test, 0, sizeof(processor_test_t));
    
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