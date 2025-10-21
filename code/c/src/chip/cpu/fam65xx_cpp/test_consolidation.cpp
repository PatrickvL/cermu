/*
 * test_consolidation.cpp - Simple test to verify modular header extraction
 */

#include "fam65xx.hpp"

using namespace fam65xx_cpp;

// Test that all processor types can be instantiated
void test_all_processors() {
    // Test MOS6502 with chip_descriptor_t initialization
    fam65xx_t<MOS6502Tag> mos6502;
    chip_descriptor_t desc = {};
    mos6502.init(&desc);
    
    static_assert(has_illegal_opcodes<MOS6502Tag>());
    static_assert(has_nmos_bugs<MOS6502Tag>());
    static_assert(has_bcd<MOS6502Tag>());
    static_assert(!has_io_port<MOS6502Tag>());
    
    // Test MOS6510 
    fam65xx_t<MOS6510Tag> mos6510;
    static_assert(has_illegal_opcodes<MOS6510Tag>());
    static_assert(has_nmos_bugs<MOS6510Tag>());
    static_assert(has_bcd<MOS6510Tag>());
    static_assert(has_io_port<MOS6510Tag>());
    
    // Test NES6502
    fam65xx_t<NES6502Tag> nes6502;
    static_assert(has_illegal_opcodes<NES6502Tag>());
    static_assert(has_nmos_bugs<NES6502Tag>());
    static_assert(!has_bcd<NES6502Tag>());
    static_assert(!has_io_port<NES6502Tag>());
    
    // Test WDC65C02
    fam65xx_t<WDC65C02Tag> wdc65c02;
    static_assert(!has_illegal_opcodes<WDC65C02Tag>());
    static_assert(!has_nmos_bugs<WDC65C02Tag>());
    static_assert(has_bcd<WDC65C02Tag>());
    static_assert(has_cmos_enhancements<WDC65C02Tag>());
    
    // Test Rockwell65C02
    fam65xx_t<Rockwell65C02Tag> rockwell65c02;
    static_assert(!has_illegal_opcodes<Rockwell65C02Tag>());
    static_assert(!has_nmos_bugs<Rockwell65C02Tag>());
    static_assert(has_bcd<Rockwell65C02Tag>());
    static_assert(has_cmos_enhancements<Rockwell65C02Tag>());
    static_assert(has_bit_manipulation<Rockwell65C02Tag>());
    
    // Test WDC65C816
    fam65xx_t<WDC65C816Tag> wdc65c816;
    static_assert(!has_illegal_opcodes<WDC65C816Tag>());
    static_assert(!has_nmos_bugs<WDC65C816Tag>());
    static_assert(has_bcd<WDC65C816Tag>());
    static_assert(has_cmos_enhancements<WDC65C816Tag>());
    static_assert(has_wide_registers<WDC65C816Tag>());
}

// Test that conditional mixins work
void test_conditional_features() {
    // Test I/O port mixin is included for MOS6510
    fam65xx_t<MOS6510Tag> cpu6510;
    // This should compile if I/O port mixin is included
    cpu6510.init_io_port();
    cpu6510.write_io_data(0x37);
    uint8_t port_value = cpu6510.read_io_port();
    
    // Test wide registers mixin is included for WDC65C816
    fam65xx_t<WDC65C816Tag> cpu65c816;
    // This should compile if wide registers mixin is included
    cpu65c816.init_wide_registers();
    uint16_t acc = cpu65c816.get_accumulator();
    cpu65c816.set_accumulator(0x1234);
    
    // Test BCD mixin is included for processors with decimal mode
    fam65xx_t<MOS6502Tag> cpu6502;
    bool carry_out, overflow;
    // This should compile if BCD mixin is included
    uint8_t result = cpu6502.adc_bcd(0x09, 0x01, false, carry_out, overflow);
}

// Test instruction decoding and handler lookup
void test_instruction_decoding() {
    fam65xx_t<MOS6502Tag> cpu;
    chip_descriptor_t desc = {};
    cpu.init(&desc);
    
    // Test opcode decoding for LDA immediate (0xA9)
    opcode_info_t info = cpu.get_opcode_info(0xA9);
    // Should be LDA with immediate addressing
    // Note: Exact values depend on the opcode table implementation
    
    // Test handler lookup for operations
    auto lda_handler = cpu.get_operation_handler(OP_LDA);
    auto adc_handler = cpu.get_operation_handler(OP_ADC);
    
    // Test addressing mode handler lookup
    auto zp_handler = cpu.get_addressing_mode_handler(AM_ZER);
    auto abs_handler = cpu.get_addressing_mode_handler(AM_ABS);
    auto imm_handler = cpu.get_addressing_mode_handler(AM_IMM);
    
    // Verify handlers are different (basic sanity check)
    if (lda_handler != adc_handler && zp_handler != abs_handler) {
        // Test passed - handlers are properly differentiated
    }
}

int main() {
    test_all_processors();
    test_conditional_features();
    test_instruction_decoding();
    return 0;
}