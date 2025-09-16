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
    std::cout << "=== Manual Analysis of Failing Test Case ===\n";
    
    // Test case "69 8f b3" from ProcessorTests
    // Initial state: a=227, p=111
    // Expected P=0x6d, got P=0xad (difference = 0x40, V and N flags swapped)
    
    uint8_t initial_a = 227;  // 0xE3
    uint8_t initial_p = 111;  // 0x6F
    uint8_t operand = 143;    // 0x8F
    uint8_t expected_p = 0x6d;
    uint8_t actual_p = 0xad;
    
    print_binary(initial_a, "Initial A");
    print_binary(initial_p, "Initial P");
    print_binary(operand, "Operand");
    print_binary(expected_p, "Expected P");
    print_binary(actual_p, "Actual P");
    
    std::cout << "\nFlag analysis:\n";
    std::cout << "Decimal mode: " << ((initial_p & 0x08) ? "YES" : "NO") << std::endl;
    std::cout << "Carry in: " << ((initial_p & 0x01) ? "YES" : "NO") << std::endl;
    
    // Expected flags: V=1, N=0 (0x6d = 0110 1101)
    // Actual flags:   V=0, N=1 (0xad = 1010 1101)
    
    std::cout << "\nExpected: V=" << ((expected_p & 0x40) ? 1 : 0) 
              << ", N=" << ((expected_p & 0x80) ? 1 : 0) << std::endl;
    std::cout << "Actual:   V=" << ((actual_p & 0x40) ? 1 : 0) 
              << ", N=" << ((actual_p & 0x80) ? 1 : 0) << std::endl;
    
    // Manual BCD calculation (since decimal mode is set)
    uint8_t a = initial_a;
    uint8_t data = operand;
    uint8_t carry_in = (initial_p & 0x01) ? 1 : 0;
    
    std::cout << "\nBCD Addition: " << (int)a << " + " << (int)data << " + " << (int)carry_in << std::endl;
    std::cout << "Hex: 0x" << std::hex << (int)a << " + 0x" << (int)data << " + " << (int)carry_in << std::dec << std::endl;
    
    // BCD calculation
    uint16_t lo_nibble = (a & 0x0F) + (data & 0x0F) + carry_in;
    uint16_t hi_nibble = (a >> 4) + (data >> 4);
    
    std::cout << "Lo nibble: " << (a & 0x0F) << " + " << (data & 0x0F) << " + " << (int)carry_in << " = " << lo_nibble;
    if (lo_nibble > 9) {
        lo_nibble += 6;
        hi_nibble += 1;
        std::cout << " -> " << lo_nibble << " (carry to hi)";
    }
    std::cout << std::endl;
    
    std::cout << "Hi nibble: " << (a >> 4) << " + " << (data >> 4) << " = " << hi_nibble;
    if (hi_nibble > 9) {
        hi_nibble += 6;
        std::cout << " -> " << hi_nibble;
    }
    std::cout << std::endl;
    
    uint8_t result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
    print_binary(result, "BCD Result");
    
    // Check overflow flag calculation using our current formula
    bool overflow_current = ((~(a ^ data) & (a ^ result) & 0x80) != 0);
    
    // Check the previous (wrong) formula to confirm this is the issue
    bool overflow_wrong = ((a ^ result) & (data ^ result) & 0x80) != 0;
    
    std::cout << "\nOverflow calculations:\n";
    std::cout << "Current formula: " << (overflow_current ? "V=1" : "V=0") << std::endl;
    std::cout << "Wrong formula:   " << (overflow_wrong ? "V=1" : "V=0") << std::endl;
    std::cout << "Expected:        " << ((expected_p & 0x40) ? "V=1" : "V=0") << std::endl;
    
    // Check if result sign matches expectation
    bool result_negative = (result & 0x80) != 0;
    bool expected_negative = (expected_p & 0x80) != 0;
    
    std::cout << "\nSign checks:\n";
    std::cout << "Result sign: " << (result_negative ? "N=1" : "N=0") << std::endl;
    std::cout << "Expected:    " << (expected_negative ? "N=1" : "N=0") << std::endl;
    
    return 0;
}