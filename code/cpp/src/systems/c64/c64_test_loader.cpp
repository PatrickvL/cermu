#include "c64_test_loader.h"
#include "../../chip/memory/ram.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Helper function to read 16-bit little-endian value
static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

bool c64_test_load_prg_file(const char* filename, ram_t* ram, 
                            uint16_t* out_load_address, uint16_t* out_sys_address) {
    if (!filename || !ram || !ram->memory) {
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
    if (fread(&ram->memory[load_address], 1, data_size, file) != data_size) {
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
        *out_sys_address = c64_test_parse_sys_address(ram, load_address);
        if (*out_sys_address != 0) {
            printf("  Found SYS address: $%04X\n", *out_sys_address);
        }
    }

    return true;
}

bool c64_test_load_bin_file(const char* filename, ram_t* ram, uint16_t load_address) {
    if (!filename || !ram || !ram->memory) {
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
    if (fread(&ram->memory[load_address], 1, file_size, file) != (size_t)file_size) {
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

uint16_t c64_test_parse_sys_address(ram_t* ram, uint16_t start_address) {
    if (!ram || !ram->memory) {
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
        uint16_t next_line = read_le16(&ram->memory[current_line]);

        // Check for end of BASIC program
        if (next_line == 0x0000) {
            break;
        }

        // Read line number (for debugging)
        if (current_line + 4 >= 0x10000) break;
        uint16_t line_number = read_le16(&ram->memory[current_line + 2]);

        // Start of line tokens/text
        uint16_t line_data = current_line + 4;

        // Look for SYS token (0x9E in BASIC V2)
        for (uint16_t pos = line_data; pos < next_line; pos++) {
            if (ram->memory[pos] == 0x9E) {  // SYS token
                // Skip spaces and find digits
                pos++;
                while (pos < next_line && !isdigit(ram->memory[pos])) {
                    pos++;
                }

                // Parse decimal number
                if (pos < next_line && isdigit(ram->memory[pos])) {
                    uint16_t sys_addr = 0;
                    while (pos < next_line && isdigit(ram->memory[pos])) {
                        sys_addr = sys_addr * 10 + (ram->memory[pos] - '0');
                        pos++;
                    }

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