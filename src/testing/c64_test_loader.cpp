#include "c64_test_loader.h"
#include "../chip/memory/memory_chip.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>

// Helper function to read 16-bit little-endian value
static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

bool c64_test_load_prg_file(const char* filename, MemoryChip* ram, 
                            uint16_t* out_load_address, uint16_t* out_sys_address) {
    if (!filename || !ram || !ram->data()) {
        printf("ERROR: Invalid parameters for PRG loading\n");
        return false;
    }

    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("ERROR: Cannot open PRG file: %s\n", filename);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 2) {
        printf("ERROR: PRG file too small (needs at least 2-byte load address header)\n");
        fclose(file);
        return false;
    }

    // Read 2-byte load address (little-endian)
    uint8_t load_addr_bytes[2];
    if (fread(load_addr_bytes, 1, 2, file) != 2) {
        printf("ERROR: Failed to read PRG load address\n");
        fclose(file);
        return false;
    }

    uint16_t load_address = read_le16(load_addr_bytes);
    size_t data_size = file_size - 2;

    if (load_address + data_size > 0x10000) {
        printf("ERROR: PRG file too large for memory (load_addr=$%04X, size=%zu)\n",
               load_address, data_size);
        fclose(file);
        return false;
    }

    // Read file data into RAM
    if (fread(&ram->data()[load_address], 1, data_size, file) != data_size) {
        printf("ERROR: Failed to read PRG file data\n");
        fclose(file);
        return false;
    }

    fclose(file);

    printf("Loaded PRG file: %s\n", filename);
    printf("  Load address: $%04X\n", load_address);
    printf("  Data size: %zu bytes\n", data_size);
    printf("  End address: $%04X\n", (unsigned int)(load_address + data_size - 1));

    if (out_load_address) {
        *out_load_address = load_address;
    }

    // Try to parse SYS address from BASIC program
    if (out_sys_address) {
        *out_sys_address = c64_test_parse_sys_address(ram, load_address, load_address);
        if (*out_sys_address != 0) {
            printf("  Found SYS address: $%04X\n", *out_sys_address);
        }
    }

    return true;
}

bool c64_test_load_bin_file(const char* filename, MemoryChip* ram, uint16_t load_address) {
    if (!filename || !ram || !ram->data()) {
        printf("ERROR: Invalid parameters for BIN loading\n");
        return false;
    }

    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("ERROR: Cannot open BIN file: %s\n", filename);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (load_address + file_size > 0x10000) {
        printf("ERROR: BIN file too large for memory (load_addr=$%04X, size=%ld)\n",
               load_address, file_size);
        fclose(file);
        return false;
    }

    // Read file data directly into RAM at specified address
    if (fread(&ram->data()[load_address], 1, file_size, file) != (size_t)file_size) {
        printf("ERROR: Failed to read BIN file data\n");
        fclose(file);
        return false;
    }

    fclose(file);

    printf("Loaded BIN file: %s\n", filename);
    printf("  Load address: $%04X\n", load_address);
    printf("  File size: %ld bytes\n", file_size);
    printf("  End address: $%04X\n", (unsigned int)(load_address + file_size - 1));

    return true;
}

// Helper to evaluate simple BASIC expression for SYS command
// Handles: PEEK(addr), numbers, +, *, and combinations
// Supports both tokenized BASIC ($C2 for PEEK) and text
static uint16_t evaluate_basic_expression(MemoryChip* ram, const char* expr, size_t len, uint16_t load_address) {
    // Parse SYS address from BASIC expression
    // Handles both simple numeric addresses and complex PEEK expressions
    
    size_t pos = 0;
    
    // Skip leading spaces
    while (pos < len && (expr[pos] == ' ' || expr[pos] == 0x20)) pos++;
    
    // Check if expression starts with a simple number (ASCII digits)
    if (pos < len && isdigit((unsigned char)expr[pos])) {
        // Simple numeric SYS address (e.g., "2061")
        uint16_t addr = 0;
        while (pos < len && isdigit((unsigned char)expr[pos])) {
            addr = addr * 10 + (expr[pos] - '0');
            pos++;
        }
        return addr;
    }
    
    // Check for tokenized BASIC expression starting with PEEK ($C2)
    if (pos < len && (unsigned char)expr[pos] == 0xC2) {
        // This is a complex expression like PEEK(43)*256+PEEK(44)*26
        // The standard formula for 64doc tests is: PEEK(43) + PEEK(44)*256 + offset
        // which calculates: low_byte + high_byte*256 + offset
        // where PEEK(43) and PEEK(44) point to the BASIC program start
        
        // In direct execution mode (no BASIC ROM boot), $2B-$2C are not initialized
        // So we use the load_address as the BASIC start (typically $0801)
        uint16_t basic_start = load_address;
        
        // Find the last number in the expression - it's likely the offset
        // We scan backwards to find the last sequence of digits
        uint16_t offset = 0;
        
        for (int i = len - 1; i >= 0; i--) {
            if (isdigit((unsigned char)expr[i])) {
                // Found end of a number, parse it backwards
                int j = i;
                uint16_t num = 0;
                uint16_t multiplier = 1;
                
                while (j >= 0 && isdigit((unsigned char)expr[j])) {
                    num += (expr[j] - '0') * multiplier;
                    multiplier *= 10;
                    j--;
                }
                
                // Check if this number is not 43, 44, or 256
                if (num != 43 && num != 44 && num != 256) {
                    offset = num;
                    break;
                }
                
                // Skip past this number
                i = j + 1;
            }
        }
        
        return basic_start + offset;
    }
    
    // Text "PEEK" or other expressions - try to parse similarly
    if (pos + 4 <= len && strncmp(&expr[pos], "PEEK", 4) == 0) {
        uint16_t basic_start = load_address;
        
        // Find the last number
        uint16_t offset = 0;
        for (int i = len - 1; i >= 0; i--) {
            if (isdigit((unsigned char)expr[i])) {
                int j = i;
                uint16_t num = 0;
                uint16_t multiplier = 1;
                
                while (j >= 0 && isdigit((unsigned char)expr[j])) {
                    num += (expr[j] - '0') * multiplier;
                    multiplier *= 10;
                    j--;
                }
                
                if (num != 43 && num != 44 && num != 256) {
                    offset = num;
                    break;
                }
                
                i = j + 1;
            }
        }
        
        return basic_start + offset;
    }
    
    return 0;  // Unable to parse
}

uint16_t c64_test_parse_sys_address(MemoryChip* ram, uint16_t start_address, uint16_t load_address) {
    if (!ram || !ram->data()) {
        return 0;
    }

    // Parse BASIC program structure to find SYS command
    // BASIC program format:
    //   [2 bytes: address of next line (little-endian)]
    //   [2 bytes: line number (little-endian)]
    //   [variable length: BASIC tokens and text]
    //   [1 byte: 0x00 (end of line)]

    uint16_t current_line = start_address;

    // Scan up to 10 BASIC lines
    for (int line_count = 0; line_count < 10; line_count++) {
        // Read next line address
        if (current_line + 2 >= 0x10000) break;
        uint16_t next_line = read_le16(&ram->data()[current_line]);

        // Check for end of BASIC program
        if (next_line == 0x0000) {
            break;
        }

        // Read line number (for debugging)
        if (current_line + 4 >= 0x10000) break;
        uint16_t line_number = read_le16(&ram->data()[current_line + 2]);

        // Start of line tokens/text
        uint16_t line_data = current_line + 4;

        // Look for SYS token (0x9E in BASIC V2)
        // Skip REM lines (0x8F token)
        bool is_rem_line = false;
        for (uint16_t check_pos = line_data; check_pos < next_line && !is_rem_line; check_pos++) {
            if (ram->data()[check_pos] == 0x8F) {  // REM token
                is_rem_line = true;
                break;
            }
            if (ram->data()[check_pos] == 0x00) break;  // End of line
        }
        
        if (is_rem_line) {
            // Skip this REM line
            current_line = next_line;
            continue;
        }
        
        for (uint16_t pos = line_data; pos < next_line; pos++) {
            if (ram->data()[pos] == 0x9E) {  // SYS token
                // Extract the expression after SYS
                pos++;
                
                // Skip leading spaces
                while (pos < next_line && ram->data()[pos] == ' ') {
                    pos++;
                }
                
                // Collect expression until end of line or colon
                char expr_buffer[256];
                size_t expr_len = 0;
                uint16_t expr_start = pos;
                
                while (pos < next_line && ram->data()[pos] != 0x00 &&
                       ram->data()[pos] != ':' && expr_len < sizeof(expr_buffer) - 1) {
                    expr_buffer[expr_len++] = ram->data()[pos];
                    pos++;
                }
                expr_buffer[expr_len] = '\0';
                
                // Try to evaluate the expression
                uint16_t sys_addr = evaluate_basic_expression(ram, expr_buffer, expr_len, start_address);
                
                // Debug: print what we're evaluating
                printf("  BASIC line %u: Evaluating SYS expression (len=%zu, result=%u/$%04X)\n",
                       line_number, expr_len, sys_addr, sys_addr);
                
                if (sys_addr != 0) {
                    printf("  Found BASIC line %u: SYS %u ($%04X)\n",
                           line_number, sys_addr, sys_addr);
                    return sys_addr;
                }
            }
        }

        // Move to next line
        current_line = next_line;
    }

    return 0;  // No SYS address found
}