#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <set>
#include <algorithm>

// Simple test result structure
struct InstructionResult {
    uint8_t opcode;
    std::string name;
    bool is_known_working;
};

int main() {
    std::cout << "=== Identifying Remaining Failing Instructions ===" << std::endl;
    
    // Based on the comprehensive report, these are the known working instruction categories
    std::vector<uint8_t> known_working_opcodes = {
        // Core Control Flow (13 instructions)
        0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0,  // Branches
        0x20, 0x40, 0x4C, 0x60, 0x6C,  // JSR, RTI, JMP abs, RTS, JMP (abs)
        
        // Register Operations (19 instructions) - Load/Store/Inc/Dec
        0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1,  // LDA all modes
        0xA2, 0xA6, 0xB6, 0xAE, 0xBE,  // LDX all modes  
        0xA0, 0xA4, 0xB4, 0xAC, 0xBC,  // LDY all modes
        0x85, 0x95, 0x8D, 0x9D, 0x99, 0x81, 0x91,  // STA all modes
        0x86, 0x96, 0x8E,  // STX all modes
        0x84, 0x94, 0x8C,  // STY all modes
        0xE8, 0xCA, 0xC8, 0x88,  // INX, DEX, INY, DEY
        
        // Arithmetic & Logic (16 instructions)
        0x09, 0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11,  // ORA all modes
        0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31,  // AND all modes  
        0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51,  // EOR all modes
        0x06, 0x16, 0x0E, 0x1E,  // ASL zp, zp,X, abs, abs,X
        0x46, 0x56, 0x4E, 0x5E,  // LSR zp, zp,X, abs, abs,X
        0xC9, 0xC5, 0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1,  // CMP all modes
        0xE0, 0xE4, 0xEC,  // CPX imm, zp, abs
        0xC0, 0xC4, 0xCC,  // CPY imm, zp, abs
        0x24, 0x2C,  // BIT zp, abs
        
        // Stack Operations (2 instructions)  
        0x48, 0x68,  // PHA, PLA
        
        // System Instructions (20 NOP variants - just marking main ones)
        0xEA,  // NOP
        
        // My recent fixes
        0x00, 0x08, 0x28  // BRK, PHP, PLP
    };
    
    // Map opcode to instruction name
    std::map<uint8_t, std::string> opcode_names = {
        {0x00, "BRK"}, {0x01, "ORA (zp,X)"}, {0x02, "JAM"}, {0x03, "SLO (zp,X)"},
        {0x04, "NOP zp"}, {0x05, "ORA zp"}, {0x06, "ASL zp"}, {0x07, "SLO zp"},
        {0x08, "PHP"}, {0x09, "ORA #"}, {0x0A, "ASL A"}, {0x0B, "ANC #"},
        {0x0C, "NOP abs"}, {0x0D, "ORA abs"}, {0x0E, "ASL abs"}, {0x0F, "SLO abs"},
        {0x10, "BPL"}, {0x11, "ORA (zp),Y"}, {0x12, "JAM"}, {0x13, "SLO (zp),Y"},
        {0x14, "NOP zp,X"}, {0x15, "ORA zp,X"}, {0x16, "ASL zp,X"}, {0x17, "SLO zp,X"},
        {0x18, "CLC"}, {0x19, "ORA abs,Y"}, {0x1A, "NOP"}, {0x1B, "SLO abs,Y"},
        {0x1C, "NOP abs,X"}, {0x1D, "ORA abs,X"}, {0x1E, "ASL abs,X"}, {0x1F, "SLO abs,X"},
        {0x20, "JSR"}, {0x21, "AND (zp,X)"}, {0x22, "JAM"}, {0x23, "RLA (zp,X)"},
        {0x24, "BIT zp"}, {0x25, "AND zp"}, {0x26, "ROL zp"}, {0x27, "RLA zp"},
        {0x28, "PLP"}, {0x29, "AND #"}, {0x2A, "ROL A"}, {0x2B, "ANC #"},
        {0x2C, "BIT abs"}, {0x2D, "AND abs"}, {0x2E, "ROL abs"}, {0x2F, "RLA abs"},
        {0x30, "BMI"}, {0x31, "AND (zp),Y"}, {0x32, "JAM"}, {0x33, "RLA (zp),Y"},
        {0x34, "NOP zp,X"}, {0x35, "AND zp,X"}, {0x36, "ROL zp,X"}, {0x37, "RLA zp,X"},
        {0x38, "SEC"}, {0x39, "AND abs,Y"}, {0x3A, "NOP"}, {0x3B, "RLA abs,Y"},
        {0x3C, "NOP abs,X"}, {0x3D, "AND abs,X"}, {0x3E, "ROL abs,X"}, {0x3F, "RLA abs,X"},
        {0x40, "RTI"}, {0x41, "EOR (zp,X)"}, {0x42, "JAM"}, {0x43, "SRE (zp,X)"},
        {0x44, "NOP zp"}, {0x45, "EOR zp"}, {0x46, "LSR zp"}, {0x47, "SRE zp"},
        {0x48, "PHA"}, {0x49, "EOR #"}, {0x4A, "LSR A"}, {0x4B, "ALR #"},
        {0x4C, "JMP abs"}, {0x4D, "EOR abs"}, {0x4E, "LSR abs"}, {0x4F, "SRE abs"},
        {0x50, "BVC"}, {0x51, "EOR (zp),Y"}, {0x52, "JAM"}, {0x53, "SRE (zp),Y"},
        {0x54, "NOP zp,X"}, {0x55, "EOR zp,X"}, {0x56, "LSR zp,X"}, {0x57, "SRE zp,X"},
        {0x58, "CLI"}, {0x59, "EOR abs,Y"}, {0x5A, "NOP"}, {0x5B, "SRE abs,Y"},
        {0x5C, "NOP abs,X"}, {0x5D, "EOR abs,X"}, {0x5E, "LSR abs,X"}, {0x5F, "SRE abs,X"},
        {0x60, "RTS"}, {0x61, "ADC (zp,X)"}, {0x62, "JAM"}, {0x63, "RRA (zp,X)"},
        {0x64, "NOP zp"}, {0x65, "ADC zp"}, {0x66, "ROR zp"}, {0x67, "RRA zp"},
        {0x68, "PLA"}, {0x69, "ADC #"}, {0x6A, "ROR A"}, {0x6B, "ARR #"},
        {0x6C, "JMP (abs)"}, {0x6D, "ADC abs"}, {0x6E, "ROR abs"}, {0x6F, "RRA abs"},
        {0x70, "BVS"}, {0x71, "ADC (zp),Y"}, {0x72, "JAM"}, {0x73, "RRA (zp),Y"},
        {0x74, "NOP zp,X"}, {0x75, "ADC zp,X"}, {0x76, "ROR zp,X"}, {0x77, "RRA zp,X"},
        {0x78, "SEI"}, {0x79, "ADC abs,Y"}, {0x7A, "NOP"}, {0x7B, "RRA abs,Y"},
        {0x7C, "NOP abs,X"}, {0x7D, "ADC abs,X"}, {0x7E, "ROR abs,X"}, {0x7F, "RRA abs,X"},
        {0x80, "NOP #"}, {0x81, "STA (zp,X)"}, {0x82, "NOP #"}, {0x83, "SAX (zp,X)"},
        {0x84, "STY zp"}, {0x85, "STA zp"}, {0x86, "STX zp"}, {0x87, "SAX zp"},
        {0x88, "DEY"}, {0x89, "NOP #"}, {0x8A, "TXA"}, {0x8B, "XAA #"},
        {0x8C, "STY abs"}, {0x8D, "STA abs"}, {0x8E, "STX abs"}, {0x8F, "SAX abs"},
        {0x90, "BCC"}, {0x91, "STA (zp),Y"}, {0x92, "JAM"}, {0x93, "AHX (zp),Y"},
        {0x94, "STY zp,X"}, {0x95, "STA zp,X"}, {0x96, "STX zp,Y"}, {0x97, "SAX zp,Y"},
        {0x98, "TYA"}, {0x99, "STA abs,Y"}, {0x9A, "TXS"}, {0x9B, "TAS abs,Y"},
        {0x9C, "SHY abs,X"}, {0x9D, "STA abs,X"}, {0x9E, "SHX abs,Y"}, {0x9F, "AHX abs,Y"},
        {0xA0, "LDY #"}, {0xA1, "LDA (zp,X)"}, {0xA2, "LDX #"}, {0xA3, "LAX (zp,X)"},
        {0xA4, "LDY zp"}, {0xA5, "LDA zp"}, {0xA6, "LDX zp"}, {0xA7, "LAX zp"},
        {0xA8, "TAY"}, {0xA9, "LDA #"}, {0xAA, "TAX"}, {0xAB, "LAX #"},
        {0xAC, "LDY abs"}, {0xAD, "LDA abs"}, {0xAE, "LDX abs"}, {0xAF, "LAX abs"},
        {0xB0, "BCS"}, {0xB1, "LDA (zp),Y"}, {0xB2, "JAM"}, {0xB3, "LAX (zp),Y"},
        {0xB4, "LDY zp,X"}, {0xB5, "LDA zp,X"}, {0xB6, "LDX zp,Y"}, {0xB7, "LAX zp,Y"},
        {0xB8, "CLV"}, {0xB9, "LDA abs,Y"}, {0xBA, "TSX"}, {0xBB, "LAS abs,Y"},
        {0xBC, "LDY abs,X"}, {0xBD, "LDA abs,X"}, {0xBE, "LDX abs,Y"}, {0xBF, "LAX abs,Y"},
        {0xC0, "CPY #"}, {0xC1, "CMP (zp,X)"}, {0xC2, "NOP #"}, {0xC3, "DCP (zp,X)"},
        {0xC4, "CPY zp"}, {0xC5, "CMP zp"}, {0xC6, "DEC zp"}, {0xC7, "DCP zp"},
        {0xC8, "INY"}, {0xC9, "CMP #"}, {0xCA, "DEX"}, {0xCB, "AXS #"},
        {0xCC, "CPY abs"}, {0xCD, "CMP abs"}, {0xCE, "DEC abs"}, {0xCF, "DCP abs"},
        {0xD0, "BNE"}, {0xD1, "CMP (zp),Y"}, {0xD2, "JAM"}, {0xD3, "DCP (zp),Y"},
        {0xD4, "NOP zp,X"}, {0xD5, "CMP zp,X"}, {0xD6, "DEC zp,X"}, {0xD7, "DCP zp,X"},
        {0xD8, "CLD"}, {0xD9, "CMP abs,Y"}, {0xDA, "NOP"}, {0xDB, "DCP abs,Y"},
        {0xDC, "NOP abs,X"}, {0xDD, "CMP abs,X"}, {0xDE, "DEC abs,X"}, {0xDF, "DCP abs,X"},
        {0xE0, "CPX #"}, {0xE1, "SBC (zp,X)"}, {0xE2, "NOP #"}, {0xE3, "ISC (zp,X)"},
        {0xE4, "CPX zp"}, {0xE5, "SBC zp"}, {0xE6, "INC zp"}, {0xE7, "ISC zp"},
        {0xE8, "INX"}, {0xE9, "SBC #"}, {0xEA, "NOP"}, {0xEB, "SBC #"},
        {0xEC, "CPX abs"}, {0xED, "SBC abs"}, {0xEE, "INC abs"}, {0xEF, "ISC abs"},
        {0xF0, "BEQ"}, {0xF1, "SBC (zp),Y"}, {0xF2, "JAM"}, {0xF3, "ISC (zp),Y"},
        {0xF4, "NOP zp,X"}, {0xF5, "SBC zp,X"}, {0xF6, "INC zp,X"}, {0xF7, "ISC zp,X"},
        {0xF8, "SED"}, {0xF9, "SBC abs,Y"}, {0xFA, "NOP"}, {0xFB, "ISC abs,Y"},
        {0xFC, "NOP abs,X"}, {0xFD, "SBC abs,X"}, {0xFE, "INC abs,X"}, {0xFF, "ISC abs,X"}
    };
    
    // Convert known working to set for fast lookup
    std::set<uint8_t> working_set(known_working_opcodes.begin(), known_working_opcodes.end());
    
    std::vector<InstructionResult> failing_instructions;
    std::vector<InstructionResult> working_instructions;
    
    // Categorize all 256 opcodes
    for (int opcode = 0; opcode <= 255; opcode++) {
        InstructionResult result;
        result.opcode = (uint8_t)opcode;
        result.name = opcode_names[(uint8_t)opcode];
        result.is_known_working = working_set.count((uint8_t)opcode) > 0;
        
        if (result.is_known_working) {
            working_instructions.push_back(result);
        } else {
            failing_instructions.push_back(result);
        }
    }
    
    // Analyze instruction categories
    std::cout << "\n=== INSTRUCTION ANALYSIS ===" << std::endl;
    std::cout << "Total instructions: 256" << std::endl;
    std::cout << "Known working: " << working_instructions.size() << std::endl;
    std::cout << "Likely failing: " << failing_instructions.size() << std::endl;
    
    // Categorize failing instructions by type
    std::map<std::string, std::vector<uint8_t>> failing_categories;
    
    for (const auto& instr : failing_instructions) {
        std::string category = "Other";
        
        // ADC/SBC operations
        if (instr.name.find("ADC") == 0 || instr.name.find("SBC") == 0) {
            category = "ADC/SBC Operations";
        }
        // Transfer operations
        else if (instr.name == "TXA" || instr.name == "TYA" || instr.name == "TAX" || 
                 instr.name == "TAY" || instr.name == "TSX" || instr.name == "TXS") {
            category = "Transfer Operations";
        }
        // Flag operations
        else if (instr.name == "CLC" || instr.name == "SEC" || instr.name == "CLI" || 
                 instr.name == "SEI" || instr.name == "CLD" || instr.name == "SED" || 
                 instr.name == "CLV") {
            category = "Flag Operations";
        }
        // Memory shift/rotate
        else if (instr.name.find("ROL") == 0 || instr.name.find("ROR") == 0) {
            category = "Memory Shift/Rotate";
        }
        // Memory increment/decrement
        else if (instr.name.find("INC") == 0 || instr.name.find("DEC") == 0) {
            category = "Memory Inc/Dec";
        }
        // Accumulator operations
        else if (instr.name.find(" A") != std::string::npos) {
            category = "Accumulator Operations";
        }
        // Illegal/undocumented opcodes
        else if (instr.name.find("JAM") == 0 || instr.name.find("SLO") == 0 || 
                 instr.name.find("RLA") == 0 || instr.name.find("SRE") == 0 ||
                 instr.name.find("RRA") == 0 || instr.name.find("SAX") == 0 ||
                 instr.name.find("LAX") == 0 || instr.name.find("DCP") == 0 ||
                 instr.name.find("ISC") == 0 || instr.name.find("ANC") == 0 ||
                 instr.name.find("ALR") == 0 || instr.name.find("ARR") == 0 ||
                 instr.name.find("XAA") == 0 || instr.name.find("AHX") == 0 ||
                 instr.name.find("TAS") == 0 || instr.name.find("SHY") == 0 ||
                 instr.name.find("SHX") == 0 || instr.name.find("LAS") == 0 ||
                 instr.name.find("AXS") == 0) {
            category = "Illegal/Undocumented";
        }
        // Various NOP variants
        else if (instr.name.find("NOP") == 0) {
            category = "NOP Variants";
        }
        
        failing_categories[category].push_back(instr.opcode);
    }
    
    std::cout << "\n=== FAILING INSTRUCTION CATEGORIES ===" << std::endl;
    for (const auto& [category, opcodes] : failing_categories) {
        std::cout << "\n" << category << " (" << opcodes.size() << " instructions):" << std::endl;
        for (uint8_t opcode : opcodes) {
            std::cout << "  0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) 
                      << (int)opcode << " " << opcode_names[opcode] << std::endl;
        }
    }
    
    // Prioritize categories for fixing
    std::cout << "\n=== RECOMMENDED FIX PRIORITY ===" << std::endl;
    std::cout << "1. Transfer Operations (6 instructions) - Core functionality" << std::endl;
    std::cout << "2. Flag Operations (7 instructions) - Core functionality" << std::endl;  
    std::cout << "3. ADC/SBC Operations (~16 instructions) - Arithmetic core" << std::endl;
    std::cout << "4. Accumulator Operations (~4 instructions) - Common operations" << std::endl;
    std::cout << "5. Memory Shift/Rotate (~8 instructions) - Memory operations" << std::endl;
    std::cout << "6. Memory Inc/Dec (~8 instructions) - Memory operations" << std::endl;
    std::cout << "7. NOP Variants (~dozen instructions) - System compatibility" << std::endl;
    std::cout << "8. Illegal/Undocumented (~dozens) - Advanced compatibility" << std::endl;
    
    return 0;
}