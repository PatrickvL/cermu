#include <iostream>
#include <iomanip>

// Helper to print binary representation of a byte
void print_binary(uint8_t val, const std::string& label) {
    std::cout << label << " = 0x" << std::hex << (int)val << " (";
    for (int i = 7; i >= 0; i--) {
        std::cout << ((val >> i) & 1);
        if (i == 4) std::cout << " ";
    }
    std::cout << ")" << std::dec << std::endl;
}

int main() {
    std::cout << "=== Debugging Remaining ADC Failures ===\n";
    
    // Case 1: "69 d5 74" - Expected P=0xad, Got P=0x6d
    std::cout << "\nCase 1: Expected 0xad, Got 0x6d\n";
    print_binary(0xad, "Expected P");
    print_binary(0x6d, "Actual P");
    std::cout << "Expected: N=1, V=0, D=1, I=1, Z=0, C=1\n";
    std::cout << "Actual:   N=0, V=1, D=1, I=1, Z=0, C=1\n";
    std::cout << "Problem: We're setting V=1 when we should set N=1\n\n";
    
    // Case 2: "69 c5 43" - Expected P=0x29, Got P=0xa9  
    std::cout << "Case 2: Expected 0x29, Got 0xa9\n";
    print_binary(0x29, "Expected P");
    print_binary(0xa9, "Actual P");
    std::cout << "Expected: N=0, V=0, D=1, I=0, Z=0, C=1\n";
    std::cout << "Actual:   N=1, V=0, D=1, I=0, Z=0, C=1\n";
    std::cout << "Problem: We're setting N=1 when we should set N=0\n\n";
    
    // Case 3: "69 b7 27" - Expected P=0x2d, Got P=0xad
    std::cout << "Case 3: Expected 0x2d, Got 0xad\n";
    print_binary(0x2d, "Expected P");
    print_binary(0xad, "Actual P");
    std::cout << "Expected: N=0, V=0, D=1, I=1, Z=0, C=1\n";
    std::cout << "Actual:   N=1, V=0, D=1, I=1, Z=0, C=1\n";
    std::cout << "Problem: We're setting N=1 when we should set N=0\n\n";
    
    std::cout << "Pattern Analysis:\n";
    std::cout << "- Issue is with N flag calculation in decimal mode\n";
    std::cout << "- Sometimes we set N=1 when hardware sets N=0\n";
    std::cout << "- Sometimes we set V=1 when hardware sets N=1\n";
    std::cout << "- V flag calculation might also be wrong in some cases\n\n";
    
    std::cout << "Hypothesis: The issue is that I'm setting N/Z flags based on\n";
    std::cout << "binary_result in ALL decimal mode cases, but maybe there are\n";
    std::cout << "specific conditions where the hardware behaves differently.\n\n";
    
    std::cout << "Need to investigate: Are there edge cases in decimal mode\n";
    std::cout << "where the N/Z flags are NOT set based on binary result?\n";
    
    return 0;
}