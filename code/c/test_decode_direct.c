#include "src/systems/c64/c64.h"
#include "src/systems/c64/c64_bus.h"
#include "src/chip/logic/pla.h"
#include <stdio.h>
#include <stdlib.h>

// Test the decode function directly
void test_decode(uint8_t encoded) {
    uint8_t read_acid, write_acid;
    
    // Extract read and write codes manually
    uint8_t read_code = encoded & 0x0F;
    uint8_t write_code = (encoded >> 5) & 0x07;
    
    printf("Encoded: 0x%02X\n", encoded);
    printf("  Read code: %d, Write code: %d\n", read_code, write_code);
    
    // Decode using our function
    if (read_code == 0) {
        read_acid = 0;  // I/O
    } else {
        read_acid = read_code + 15;  // ACID_IO2_DF = 15
    }
    
    if (write_code == 0) {
        write_acid = 0;  // I/O
    } else {
        write_acid = write_code + 15;
    }
    
    printf("  Decoded: read_acid=%d, write_acid=%d\n", read_acid, write_acid);
    printf("  Read chip: %s, Write chip: %s\n", 
           read_acid == 21 ? "BASIC-ROM" : read_acid == 23 ? "KERNAL-ROM" : read_acid == 0 ? "I/O" : "OTHER",
           write_acid == 0 ? "I/O" : "OTHER");
    printf("\n");
}

int main() {
    printf("Testing decode function directly:\n\n");
    
    test_decode(0x00);  // All I/O
    test_decode(0x06);  // Read BASIC, Write I/O
    test_decode(0x08);  // Read KERNAL, Write I/O
    
    return 0;
}