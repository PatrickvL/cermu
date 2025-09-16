#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    std::cout << "=== Debugging Specific Failing Case ===\n";
    
    // Test case: "69 8f b3" - expected P=0x6d, got P=0xad
    // This means we're getting 0xad when we should get 0x6d
    // Difference: 0xad - 0x6d = 0x40 = V flag is incorrectly set
    
    // Let's manually trace what should happen
    std::cout << "Expected P=0x6d (0110 1101): N=0, V=1, D=1, I=1, Z=0, C=1\n";
    std::cout << "Actual P=0xad   (1010 1101): N=1, V=0, D=1, I=1, Z=0, C=1\n";
    std::cout << "Problem: N flag and V flag are swapped!\n\n";
    
    // Let's examine what 0x6d vs 0xad means:
    // 0x6d = 0110 1101 = bits 6,5,3,2,0 set = V,D,I,C set, N=0, Z=0
    // 0xad = 1010 1101 = bits 7,5,3,2,0 set = N,D,I,C set, V=0, Z=0
    
    std::cout << "The V flag and N flag are being confused!\n";
    std::cout << "Expected: V=1, N=0\n";
    std::cout << "Actual:   V=0, N=1\n\n";
    
    // Let's verify this pattern with the other failing case
    std::cout << "Other failing case: '69 93 e9' - expected P=0x69, got P=0xa9\n";
    std::cout << "Expected P=0x69 (0110 1001): N=0, V=1, D=1, I=0, Z=0, C=1\n";
    std::cout << "Actual P=0xa9   (1010 1001): N=1, V=0, D=1, I=0, Z=0, C=1\n";
    std::cout << "Same pattern: N and V flags are swapped!\n\n";
    
    // This suggests there's an issue with how we're handling either:
    // 1. The overflow flag calculation in general
    // 2. Some interaction between overflow and negative flags
    // 3. The way flags are being set/cleared
    
    std::cout << "Flag bit definitions:\n";
    std::cout << "N (bit 7) = 0x80 = " << std::hex << 0x80 << std::dec << "\n";
    std::cout << "V (bit 6) = 0x40 = " << std::hex << 0x40 << std::dec << "\n";
    std::cout << "D (bit 3) = 0x08 = " << std::hex << 0x08 << std::dec << "\n";
    std::cout << "I (bit 2) = 0x04 = " << std::hex << 0x04 << std::dec << "\n";
    std::cout << "Z (bit 1) = 0x02 = " << std::hex << 0x02 << std::dec << "\n";
    std::cout << "C (bit 0) = 0x01 = " << std::hex << 0x01 << std::dec << "\n";
    
    return 0;
}