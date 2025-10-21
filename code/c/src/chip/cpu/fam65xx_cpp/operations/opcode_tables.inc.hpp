/*
 * opcode_tables.inc - Processor-Specific Opcode Table Specializations
 *
 * This file contains template specializations of the opcode table generation
 * function for each supported processor type. Each processor gets its own
 * compile-time generated opcode table based on its feature set.
 */

// ============================================================================
// OPCODE TABLE GENERATION SPECIALIZATIONS
// ============================================================================

// MOS 6502 (Original NMOS)
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<fam65xx_cpp::MOS6502Tag>() {
    std::array<opcode_info_t, 256> table{};
    
    // Initialize all entries to invalid/NOP
    for (int i = 0; i < 256; i++) {
        table[i] = {
            .op_index = 0,           // NOP operation
            .am_index = 0,           // No addressing mode
            .illegal_store = 0,
            .can_skip_page_cross = 0,
            .rmw = 0,
            ._reserved = 0
        };
    }
    
    // Basic 6502 opcodes (simplified for demonstration)
    
    // ADC
    table[0x69] = {.op_index = 1, .am_index = 1, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // ADC #imm
    table[0x65] = {.op_index = 1, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // ADC zp
    table[0x75] = {.op_index = 1, .am_index = 3, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // ADC zp,X
    table[0x6D] = {.op_index = 1, .am_index = 5, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // ADC abs
    table[0x7D] = {.op_index = 1, .am_index = 6, .illegal_store = 0, .can_skip_page_cross = 1, .rmw = 0, ._reserved = 0}; // ADC abs,X
    table[0x79] = {.op_index = 1, .am_index = 7, .illegal_store = 0, .can_skip_page_cross = 1, .rmw = 0, ._reserved = 0}; // ADC abs,Y
    table[0x61] = {.op_index = 1, .am_index = 9, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // ADC (zp,X)
    table[0x71] = {.op_index = 1, .am_index = 10, .illegal_store = 0, .can_skip_page_cross = 1, .rmw = 0, ._reserved = 0}; // ADC (zp),Y
    
    // LDA
    table[0xA9] = {.op_index = 2, .am_index = 1, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // LDA #imm
    table[0xA5] = {.op_index = 2, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // LDA zp
    table[0xB5] = {.op_index = 2, .am_index = 3, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // LDA zp,X
    table[0xAD] = {.op_index = 2, .am_index = 5, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // LDA abs
    table[0xBD] = {.op_index = 2, .am_index = 6, .illegal_store = 0, .can_skip_page_cross = 1, .rmw = 0, ._reserved = 0}; // LDA abs,X
    table[0xB9] = {.op_index = 2, .am_index = 7, .illegal_store = 0, .can_skip_page_cross = 1, .rmw = 0, ._reserved = 0}; // LDA abs,Y
    table[0xA1] = {.op_index = 2, .am_index = 9, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // LDA (zp,X)
    table[0xB1] = {.op_index = 2, .am_index = 10, .illegal_store = 0, .can_skip_page_cross = 1, .rmw = 0, ._reserved = 0}; // LDA (zp),Y
    
    // STA
    table[0x85] = {.op_index = 3, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STA zp
    table[0x95] = {.op_index = 3, .am_index = 3, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STA zp,X
    table[0x8D] = {.op_index = 3, .am_index = 5, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STA abs
    table[0x9D] = {.op_index = 3, .am_index = 6, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STA abs,X
    table[0x99] = {.op_index = 3, .am_index = 7, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STA abs,Y
    table[0x81] = {.op_index = 3, .am_index = 9, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STA (zp,X)
    table[0x91] = {.op_index = 3, .am_index = 10, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STA (zp),Y
    
    // NOP
    table[0xEA] = {.op_index = 0, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // NOP
    
    // Illegal opcodes for NMOS 6502
    table[0xA7] = {.op_index = 10, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // LAX zp
    table[0x87] = {.op_index = 11, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // SAX zp
    
    return table;
}

// MOS 6510 (C64/C128 variant)
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<fam65xx_cpp::MOS6510Tag>() {
    // 6510 uses same instruction set as 6502
    return generate_opcode_table<fam65xx_cpp::MOS6502Tag>();
}

// WDC 65C02 (CMOS variant)
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<fam65xx_cpp::WDC65C02Tag>() {
    // Start with basic 6502 table but without illegal opcodes
    auto table = generate_opcode_table<fam65xx_cpp::MOS6502Tag>();
    
    // Remove illegal opcodes (replace with NOP)
    table[0xA7] = {.op_index = 0, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // NOP instead of LAX
    table[0x87] = {.op_index = 0, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // NOP instead of SAX
    
    // Add 65C02 enhancements
    table[0x80] = {.op_index = 20, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // BRA rel
    table[0x64] = {.op_index = 21, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STZ zp
    table[0x9C] = {.op_index = 21, .am_index = 5, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STZ abs
    table[0x14] = {.op_index = 22, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // TRB zp
    table[0x04] = {.op_index = 23, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // TSB zp
    table[0xCB] = {.op_index = 24, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // WAI
    table[0xDB] = {.op_index = 25, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // STP
    table[0xDA] = {.op_index = 26, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PHX
    table[0x5A] = {.op_index = 27, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PHY
    table[0xFA] = {.op_index = 28, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PLX
    table[0x7A] = {.op_index = 29, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PLY
    
    return table;
}

// NES 6502 (no BCD, no illegal opcodes)
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<fam65xx_cpp::NES6502Tag>() {
    // Start with basic 6502 table but remove illegal opcodes
    auto table = generate_opcode_table<fam65xx_cpp::MOS6502Tag>();
    
    // Remove illegal opcodes
    table[0xA7] = {.op_index = 0, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0};
    table[0x87] = {.op_index = 0, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0};
    
    return table;
}

// WDC 65C816 (16-bit processor)
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<fam65xx_cpp::WDC65C816Tag>() {
    // Start with 65C02 table
    auto table = generate_opcode_table<fam65xx_cpp::WDC65C02Tag>();
    
    // Add 65C816 specific opcodes
    table[0xC2] = {.op_index = 40, .am_index = 1, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // REP #imm
    table[0xE2] = {.op_index = 41, .am_index = 1, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // SEP #imm
    table[0xFB] = {.op_index = 42, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // XCE
    table[0xF4] = {.op_index = 43, .am_index = 5, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PEA abs
    table[0x8B] = {.op_index = 44, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PHB
    table[0x0B] = {.op_index = 45, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PHD
    table[0x4B] = {.op_index = 46, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PHK
    table[0xAB] = {.op_index = 47, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PLB
    table[0x2B] = {.op_index = 48, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // PLD
    table[0x22] = {.op_index = 49, .am_index = 11, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // JSL long
    table[0x6B] = {.op_index = 50, .am_index = 0, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 0, ._reserved = 0}; // RTL
    
    return table;
}

// Rockwell 65C02 (CMOS with bit manipulation)
template<>
constexpr std::array<opcode_info_t, 256> generate_opcode_table<fam65xx_cpp::Rockwell65C02Tag>() {
    // Start with WDC 65C02 table
    auto table = generate_opcode_table<fam65xx_cpp::WDC65C02Tag>();
    
    // Add Rockwell bit manipulation opcodes
    // RMB0-RMB7 (Reset Memory Bit)
    table[0x07] = {.op_index = 60, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB0 zp
    table[0x17] = {.op_index = 61, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB1 zp
    table[0x27] = {.op_index = 62, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB2 zp
    table[0x37] = {.op_index = 63, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB3 zp
    table[0x47] = {.op_index = 64, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB4 zp
    table[0x57] = {.op_index = 65, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB5 zp
    table[0x67] = {.op_index = 66, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB6 zp
    table[0x77] = {.op_index = 67, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // RMB7 zp
    
    // SMB0-SMB7 (Set Memory Bit)
    table[0x87] = {.op_index = 68, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB0 zp
    table[0x97] = {.op_index = 69, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB1 zp
    table[0xA7] = {.op_index = 70, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB2 zp
    table[0xB7] = {.op_index = 71, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB3 zp
    table[0xC7] = {.op_index = 72, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB4 zp
    table[0xD7] = {.op_index = 73, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB5 zp
    table[0xE7] = {.op_index = 74, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB6 zp
    table[0xF7] = {.op_index = 75, .am_index = 2, .illegal_store = 0, .can_skip_page_cross = 0, .rmw = 1, ._reserved = 0}; // SMB7 zp
    
    return table;
}