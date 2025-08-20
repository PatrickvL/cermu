#include "src/chip/cpu/mos6510_cycle/pla_lookup.h"
#include <stdio.h>

int main() {
    printf("Debug: Illegal opcode pattern recognition\n");
    
    // Initialize table first
    pla_complete_instruction_table();
    
    // Test specific opcodes
    uint8_t test_opcodes[] = {0x02, 0x0B, 0x04, 0xA9};
    const char* names[] = {"JAM", "ANC", "NOP", "LDA"};
    int expected[] = {7, 1, 8, 0}; // ILLEGAL_CATEGORY_JAM=7, ALU_IMM=1, NOP_VARIANTS=8, NONE=0
    
    for (int i = 0; i < 4; i++) {
        uint8_t opcode = test_opcodes[i];
        illegal_opcode_category_t category = pla_get_illegal_category(opcode);
        bool is_legal = pla_is_legal_opcode(opcode);
        
        printf("%s (0x%02X): category=%d (expected %d), is_legal=%s\n", 
               names[i], opcode, category, expected[i], is_legal ? "true" : "false");
        
        // Check if it matches JAM pattern manually
        if (opcode == 0x02) {
            printf("  Manual JAM check: opcode==0x02? %s\n", (opcode == 0x02) ? "YES" : "NO");
        }
    }
    
    return 0;
}