#include <iostream>
#include <cstdint>
#include <array>
#include <type_traits>

// ============================================================================
// OPTIONAL FEATURE MIXINS
// ============================================================================

// Feature 1: I/O Port (6510 in C64)
struct io_port_mixin_t {
    struct {
        uint8_t direction;  // Data Direction Register (DDR)
        uint8_t data;       // Port data register
        uint8_t input;      // External input pins
    } io_port;
    
    void init_io_port() {
        io_port.direction = 0x00;
        io_port.data = 0x00;
        io_port.input = 0x17;  // Default C64 input state
    }
    
    void write_io_ddr(uint8_t value) {
        io_port.direction = value;
    }
    
    void write_io_data(uint8_t value) {
        io_port.data = value;
    }
    
    uint8_t read_io_port() const {
        // Combine output and input based on direction
        return (io_port.data & io_port.direction) | 
               (io_port.input & ~io_port.direction);
    }
};

// Feature 2: BCD Mode Support
struct bcd_mixin_t {
    bool bcd_enabled = true;
    
    uint8_t adc_bcd(uint8_t a, uint8_t b, bool carry_in, bool& carry_out, bool& overflow) {
        uint16_t al = (a & 0x0F) + (b & 0x0F) + (carry_in ? 1 : 0);
        if (al > 0x09) al += 0x06;
        
        uint16_t ah = (a >> 4) + (b >> 4) + (al > 0x0F ? 1 : 0);
        if (ah > 0x09) ah += 0x06;
        
        carry_out = (ah > 0x0F);
        overflow = false;  // V flag undefined in BCD mode
        
        return ((ah & 0x0F) << 4) | (al & 0x0F);
    }
    
    uint8_t sbc_bcd(uint8_t a, uint8_t b, bool borrow_in, bool& carry_out, bool& overflow) {
        uint16_t al = (a & 0x0F) - (b & 0x0F) - (borrow_in ? 0 : 1);
        if (al & 0x10) al -= 0x06;
        
        uint16_t ah = (a >> 4) - (b >> 4) - ((al & 0x10) ? 1 : 0);
        if (ah & 0x10) ah -= 0x06;
        
        carry_out = !(ah & 0x10);
        overflow = false;  // V flag undefined in BCD mode
        
        return ((ah & 0x0F) << 4) | (al & 0x0F);
    }
};

// Feature 3: Illegal/Undocumented Opcodes
struct illegal_opcodes_mixin_t {
    // SAX: Store A AND X
    uint8_t execute_sax(uint8_t a, uint8_t x) {
        return a & x;
    }
    
    // LAX: Load A and X
    void execute_lax(uint8_t& a, uint8_t& x, uint8_t value) {
        a = x = value;
    }
    
    // DCP: Decrement memory and compare with A
    void execute_dcp(uint8_t& memory, uint8_t a, uint8_t& p) {
        memory--;
        uint8_t result = a - memory;
        p = (p & 0x7C) | (result == 0 ? 0x02 : 0) | (result & 0x80) | (a >= memory ? 0x01 : 0);
    }
    
    // ISC: Increment memory and subtract from A
    uint8_t execute_isc(uint8_t& memory, uint8_t a, bool carry, bool& carry_out, bool& overflow) {
        memory++;
        uint16_t result = a - memory - (carry ? 0 : 1);
        carry_out = (result < 0x100);
        overflow = ((a ^ memory) & (a ^ result) & 0x80) != 0;
        return result & 0xFF;
    }
    
    // SLO: Shift left and OR with A
    uint8_t execute_slo(uint8_t& memory, uint8_t a, bool& carry) {
        carry = (memory & 0x80) != 0;
        memory <<= 1;
        return a | memory;
    }
    
    // RLA: Rotate left and AND with A
    uint8_t execute_rla(uint8_t& memory, uint8_t a, bool carry_in, bool& carry_out) {
        carry_out = (memory & 0x80) != 0;
        memory = (memory << 1) | (carry_in ? 1 : 0);
        return a & memory;
    }
};

// Feature 4: CMOS-specific opcodes (65C02)
struct cmos_opcodes_mixin_t {
    // BRA: Branch always (65C02)
    void execute_bra(int8_t offset, uint16_t& pc, uint8_t& cycles) {
        pc += offset;
        cycles++;  // Branch taken adds a cycle
    }
    
    // STZ: Store zero
    uint8_t execute_stz() {
        return 0x00;
    }
    
    // TRB: Test and reset bits
    void execute_trb(uint8_t& memory, uint8_t a, uint8_t& p) {
        p = (p & 0xFD) | ((memory & a) == 0 ? 0x02 : 0);  // Set Z flag
        memory &= ~a;
    }
    
    // TSB: Test and set bits
    void execute_tsb(uint8_t& memory, uint8_t a, uint8_t& p) {
        p = (p & 0xFD) | ((memory & a) == 0 ? 0x02 : 0);  // Set Z flag
        memory |= a;
    }
};

// Empty mixins for features not present (EBO makes these zero-size)
struct no_io_port_mixin_t {};
struct no_bcd_mixin_t {};
struct no_illegal_opcodes_mixin_t {};
struct no_cmos_opcodes_mixin_t {};

// ============================================================================
// TYPE TRAIT DETECTION
// ============================================================================

template<typename T, typename = void>
struct has_io_port : std::false_type {};

template<typename T>
struct has_io_port<T, std::void_t<decltype(std::declval<T>().write_io_data(0))>> 
    : std::true_type {};

template<typename T, typename = void>
struct has_bcd : std::false_type {};

template<typename T>
struct has_bcd<T, std::void_t<decltype(std::declval<T>().bcd_enabled)>> 
    : std::true_type {};

template<typename T, typename = void>
struct has_illegal_opcodes : std::false_type {};

template<typename T>
struct has_illegal_opcodes<T, std::void_t<decltype(std::declval<T>().execute_sax(0, 0))>> 
    : std::true_type {};

template<typename T, typename = void>
struct has_cmos_opcodes : std::false_type {};

template<typename T>
struct has_cmos_opcodes<T, std::void_t<decltype(std::declval<T>().execute_stz())>> 
    : std::true_type {};

// ============================================================================
// BASE CPU IMPLEMENTATION
// ============================================================================

template<typename Derived>
struct fam65xx_base_t {
    // Standard 6502 registers
    uint8_t A;      // Accumulator
    uint8_t X;      // X index register
    uint8_t Y;      // Y index register
    uint8_t S;      // Stack pointer
    uint8_t P;      // Processor status
    uint16_t PC;    // Program counter
    
    // Emulation state
    uint64_t total_cycles;
    bool irq_pending;
    bool nmi_pending;
    
    void reset() {
        A = 0x00;
        X = 0x00;
        Y = 0x00;
        S = 0xFF;
        P = 0x34;  // IRQ disabled, unused bit set
        PC = 0xFFFC;  // Will be loaded from reset vector
        total_cycles = 0;
        irq_pending = false;
        nmi_pending = false;
    }
    
    // Flag manipulation helpers
    void set_nz_flags(uint8_t value) {
        P = (P & 0x7D) | (value & 0x80) | (value == 0 ? 0x02 : 0);
    }
    
    void set_carry(bool c) {
        P = (P & 0xFE) | (c ? 0x01 : 0);
    }
    
    bool get_carry() const {
        return (P & 0x01) != 0;
    }
    
    bool get_decimal() const {
        return (P & 0x08) != 0;
    }
    
    bool get_overflow() const {
        return (P & 0x40) != 0;
    }
    
    void set_overflow(bool v) {
        P = (P & 0xBF) | (v ? 0x40 : 0);
    }
};

// ============================================================================
// MAIN CPU WITH CONDITIONAL FEATURES
// ============================================================================

template
    typename IOPortMixin = no_io_port_mixin_t,
    typename BCDMixin = no_bcd_mixin_t,
    typename IllegalOpcodesMixin = no_illegal_opcodes_mixin_t,
    typename CMOSOpcodesMixin = no_cmos_opcodes_mixin_t
>
struct fam65xx_t : 
    fam65xx_base_t<fam65xx_t<IOPortMixin, BCDMixin, IllegalOpcodesMixin, CMOSOpcodesMixin>>,
    IOPortMixin,
    BCDMixin,
    IllegalOpcodesMixin,
    CMOSOpcodesMixin
{
    using Self = fam65xx_t<IOPortMixin, BCDMixin, IllegalOpcodesMixin, CMOSOpcodesMixin>;
    using Handler = void (Self::*)();
    
    std::array<Handler, 256> opcode_table;
    
    // ========================================================================
    // STANDARD OPCODE HANDLERS
    // ========================================================================
    
    void op_adc_imm() {  // 0x69
        uint8_t operand = fetch_byte();
        
        if (this->get_decimal()) {
            if constexpr (has_bcd<Self>::value) {
                bool carry_out, overflow;
                this->A = this->adc_bcd(this->A, operand, this->get_carry(), carry_out, overflow);
                this->set_carry(carry_out);
                this->set_overflow(overflow);
            } else {
                // Fall back to binary mode if BCD not supported
                adc_binary(operand);
            }
        } else {
            adc_binary(operand);
        }
        
        this->set_nz_flags(this->A);
        this->total_cycles += 2;
    }
    
    void op_sbc_imm() {  // 0xE9
        uint8_t operand = fetch_byte();
        
        if (this->get_decimal()) {
            if constexpr (has_bcd<Self>::value) {
                bool carry_out, overflow;
                this->A = this->sbc_bcd(this->A, operand, !this->get_carry(), carry_out, overflow);
                this->set_carry(carry_out);
                this->set_overflow(overflow);
            } else {
                sbc_binary(operand);
            }
        } else {
            sbc_binary(operand);
        }
        
        this->set_nz_flags(this->A);
        this->total_cycles += 2;
    }
    
    void op_lda_imm() {  // 0xA9
        this->A = fetch_byte();
        this->set_nz_flags(this->A);
        this->total_cycles += 2;
    }
    
    void op_ldx_imm() {  // 0xA2
        this->X = fetch_byte();
        this->set_nz_flags(this->X);
        this->total_cycles += 2;
    }
    
    void op_ldy_imm() {  // 0xA0
        this->Y = fetch_byte();
        this->set_nz_flags(this->Y);
        this->total_cycles += 2;
    }
    
    void op_jmp_abs() {  // 0x4C
        uint16_t addr = fetch_word();
        this->PC = addr;
        this->total_cycles += 3;
    }
    
    void op_jsr_abs() {  // 0x20
        uint16_t addr = fetch_word();
        push_word(this->PC - 1);
        this->PC = addr;
        this->total_cycles += 6;
    }
    
    void op_rts() {  // 0x60
        this->PC = pop_word() + 1;
        this->total_cycles += 6;
    }
    
    void op_nop() {  // 0xEA
        this->total_cycles += 2;
    }
    
    // ========================================================================
    // ILLEGAL OPCODE HANDLERS (conditionally compiled)
    // ========================================================================
    
    void op_sax_zpg() {  // 0x87
        if constexpr (has_illegal_opcodes<Self>::value) {
            uint8_t addr = fetch_byte();
            uint8_t value = this->execute_sax(this->A, this->X);
            write_byte(addr, value);
            this->total_cycles += 3;
        } else {
            op_invalid();
        }
    }
    
    void op_lax_zpg() {  // 0xA7
        if constexpr (has_illegal_opcodes<Self>::value) {
            uint8_t addr = fetch_byte();
            uint8_t value = read_byte(addr);
            this->execute_lax(this->A, this->X, value);
            this->set_nz_flags(this->A);
            this->total_cycles += 3;
        } else {
            op_invalid();
        }
    }
    
    void op_dcp_zpg() {  // 0xC7
        if constexpr (has_illegal_opcodes<Self>::value) {
            uint8_t addr = fetch_byte();
            uint8_t value = read_byte(addr);
            this->execute_dcp(value, this->A, this->P);
            write_byte(addr, value);
            this->total_cycles += 5;
        } else {
            op_invalid();
        }
    }
    
    void op_isc_zpg() {  // 0xE7
        if constexpr (has_illegal_opcodes<Self>::value) {
            uint8_t addr = fetch_byte();
            uint8_t value = read_byte(addr);
            bool carry_out, overflow;
            this->A = this->execute_isc(value, this->A, this->get_carry(), carry_out, overflow);
            write_byte(addr, value);
            this->set_carry(carry_out);
            this->set_overflow(overflow);
            this->set_nz_flags(this->A);
            this->total_cycles += 5;
        } else {
            op_invalid();
        }
    }
    
    // ========================================================================
    // CMOS OPCODE HANDLERS (65C02)
    // ========================================================================
    
    void op_bra_rel() {  // 0x80
        if constexpr (has_cmos_opcodes<Self>::value) {
            int8_t offset = static_cast<int8_t>(fetch_byte());
            uint8_t cycles = 0;
            this->execute_bra(offset, this->PC, cycles);
            this->total_cycles += 2 + cycles;
        } else {
            op_invalid();
        }
    }
    
    void op_stz_zpg() {  // 0x64
        if constexpr (has_cmos_opcodes<Self>::value) {
            uint8_t addr = fetch_byte();
            uint8_t value = this->execute_stz();
            write_byte(addr, value);
            this->total_cycles += 3;
        } else {
            op_invalid();
        }
    }
    
    void op_trb_zpg() {  // 0x14
        if constexpr (has_cmos_opcodes<Self>::value) {
            uint8_t addr = fetch_byte();
            uint8_t value = read_byte(addr);
            this->execute_trb(value, this->A, this->P);
            write_byte(addr, value);
            this->total_cycles += 5;
        } else {
            op_invalid();
        }
    }
    
    // ========================================================================
    // INVALID OPCODE HANDLER
    // ========================================================================
    
    void op_invalid() {
        std::cout << "Invalid/Unsupported opcode at PC=0x" 
                  << std::hex << this->PC - 1 << "\n";
        this->total_cycles += 1;
    }
    
    // ========================================================================
    // OPCODE TABLE INITIALIZATION
    // ========================================================================
    
    void init_opcode_table() {
        // Fill with invalid handler
        opcode_table.fill(&Self::op_invalid);
        
        // Standard opcodes (always available)
        opcode_table[0x69] = &Self::op_adc_imm;
        opcode_table[0xE9] = &Self::op_sbc_imm;
        opcode_table[0xA9] = &Self::op_lda_imm;
        opcode_table[0xA2] = &Self::op_ldx_imm;
        opcode_table[0xA0] = &Self::op_ldy_imm;
        opcode_table[0x4C] = &Self::op_jmp_abs;
        opcode_table[0x20] = &Self::op_jsr_abs;
        opcode_table[0x60] = &Self::op_rts;
        opcode_table[0xEA] = &Self::op_nop;
        
        // Illegal opcodes (conditional)
        if constexpr (has_illegal_opcodes<Self>::value) {
            opcode_table[0x87] = &Self::op_sax_zpg;
            opcode_table[0xA7] = &Self::op_lax_zpg;
            opcode_table[0xC7] = &Self::op_dcp_zpg;
            opcode_table[0xE7] = &Self::op_isc_zpg;
        }
        
        // CMOS opcodes (conditional)
        if constexpr (has_cmos_opcodes<Self>::value) {
            opcode_table[0x80] = &Self::op_bra_rel;
            opcode_table[0x64] = &Self::op_stz_zpg;
            opcode_table[0x14] = &Self::op_trb_zpg;
        }
    }
    
    // ========================================================================
    // MEMORY ACCESS (stub implementations)
    // ========================================================================
    
    uint8_t memory[65536] = {0};
    
    uint8_t read_byte(uint16_t addr) {
        // Check for I/O port access on 6510
        if constexpr (has_io_port<Self>::value) {
            if (addr == 0x0000) {
                return this->read_io_port();
            } else if (addr == 0x0001) {
                return this->io_port.direction;
            }
        }
        return memory[addr];
    }
    
    void write_byte(uint16_t addr, uint8_t value) {
        // Check for I/O port access on 6510
        if constexpr (has_io_port<Self>::value) {
            if (addr == 0x0000) {
                this->write_io_ddr(value);
                return;
            } else if (addr == 0x0001) {
                this->write_io_data(value);
                return;
            }
        }
        memory[addr] = value;
    }
    
    uint8_t fetch_byte() {
        return read_byte(this->PC++);
    }
    
    uint16_t fetch_word() {
        uint8_t lo = fetch_byte();
        uint8_t hi = fetch_byte();
        return (hi << 8) | lo;
    }
    
    void push_byte(uint8_t value) {
        write_byte(0x0100 + this->S--, value);
    }
    
    void push_word(uint16_t value) {
        push_byte(value >> 8);
        push_byte(value & 0xFF);
    }
    
    uint8_t pop_byte() {
        return read_byte(0x0100 + ++this->S);
    }
    
    uint16_t pop_word() {
        uint8_t lo = pop_byte();
        uint8_t hi = pop_byte();
        return (hi << 8) | lo;
    }
    
    // ========================================================================
    // BINARY MODE ARITHMETIC HELPERS
    // ========================================================================
    
    void adc_binary(uint8_t operand) {
        uint16_t result = this->A + operand + (this->get_carry() ? 1 : 0);
        this->set_carry(result > 0xFF);
        this->set_overflow(((this->A ^ result) & (operand ^ result) & 0x80) != 0);
        this->A = result & 0xFF;
    }
    
    void sbc_binary(uint8_t operand) {
        uint16_t result = this->A - operand - (this->get_carry() ? 0 : 1);
        this->set_carry(result < 0x100);
        this->set_overflow(((this->A ^ operand) & (this->A ^ result) & 0x80) != 0);
        this->A = result & 0xFF;
    }
    
    // ========================================================================
    // MAIN EXECUTION
    // ========================================================================
    
    void execute() {
        uint8_t opcode = fetch_byte();
        Handler handler = opcode_table[opcode];
        (this->*handler)();
    }
    
    void step() {
        execute();
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    fam65xx_t() {
        this->reset();
        init_opcode_table();
        
        // Initialize I/O port if present
        if constexpr (has_io_port<Self>::value) {
            this->init_io_port();
        }
    }
};

// ============================================================================
// CPU VARIANT DEFINITIONS
// ============================================================================

// NMOS 6502: BCD, illegal opcodes, no I/O port, no CMOS opcodes
using mos6502_t = fam65xx_t
    no_io_port_mixin_t,
    bcd_mixin_t,
    illegal_opcodes_mixin_t,
    no_cmos_opcodes_mixin_t
>;

// 6510 (C64): I/O port, BCD, illegal opcodes, no CMOS opcodes
using mos6510_t = fam65xx_t
    io_port_mixin_t,
    bcd_mixin_t,
    illegal_opcodes_mixin_t,
    no_cmos_opcodes_mixin_t
>;

// 65C02: BCD, no illegal opcodes, no I/O port, CMOS opcodes
using mos65c02_t = fam65xx_t
    no_io_port_mixin_t,
    bcd_mixin_t,
    no_illegal_opcodes_mixin_t,
    cmos_opcodes_mixin_t
>;

// 65C02 strict (no BCD in native mode): no BCD, no illegal, no I/O, CMOS opcodes
using mos65c02_strict_t = fam65xx_t
    no_io_port_mixin_t,
    no_bcd_mixin_t,
    no_illegal_opcodes_mixin_t,
    cmos_opcodes_mixin_t
>;

// Minimal 6502 (strict emulation): BCD only, no extras
using mos6502_strict_t = fam65xx_t
    no_io_port_mixin_t,
    bcd_mixin_t,
    no_illegal_opcodes_mixin_t,
    no_cmos_opcodes_mixin_t
>;

// ============================================================================
// USAGE DEMONSTRATION
// ============================================================================

void print_cpu_info(const char* name, size_t size, bool has_io, bool has_bcd, 
                    bool has_illegal, bool has_cmos) {
    std::cout << name << ":\n";
    std::cout << "  Size: " << size << " bytes\n";
    std::cout << "  Features: ";
    if (has_io) std::cout << "[I/O Port] ";
    if (has_bcd) std::cout << "[BCD] ";
    if (has_illegal) std::cout << "[Illegal Opcodes] ";
    if (has_cmos) std::cout << "[CMOS Opcodes] ";
    std::cout << "\n\n";
}

int main() {
    std::cout << "=== FAM65XX Emulator - EBO Mixin Pattern Example ===\n\n";
    
    std::cout << "CPU Variant Information:\n";
    std::cout << "========================\n\n";
    
    print_cpu_info("MOS 6502 (NMOS)", sizeof(mos6502_t), 
                   false, true, true, false);
    
    print_cpu_info("MOS 6510 (C64)", sizeof(mos6510_t), 
                   true, true, true, false);
    
    print_cpu_info("MOS 65C02 (CMOS)", sizeof(mos65c02_t), 
                   false, true, false, true);
    
    print_cpu_info("MOS 65C02 Strict", sizeof(mos65c02_strict_t), 
                   false, false, false, true);
    
    print_cpu_info("MOS 6502 Strict", sizeof(mos6502_strict_t), 
                   false, true, false, false);
    
    std::cout << "\nRunning 6510 (C64) example:\n";
    std::cout << "===========================\n";
    mos6510_t c64_cpu;
    
    // Demonstrate I/O port access
    std::cout << "Writing to I/O port: 0x37 (C64 memory config)\n";
    c64_cpu.write_byte(0x0001, 0x37);
    uint8_t port_value = c64_cpu.read_byte(0x0001);
    std::cout << "Read back: 0x" << std::hex << (int)port_value << "\n\n";
    
    // Load a simple program
    c64_cpu.memory[0xFFFC] = 0x00;  // Reset vector low
    c64_cpu.memory[0xFFFD] = 0x10;  // Reset vector high
    c64_cpu.memory[0x1000] = 0xA9;  // LDA #$42
    c64_cpu.memory[0x1001] = 0x42;
    c64_cpu.memory[0x1002] = 0x69;  // ADC #$08
    c64_cpu.memory[0x1003] = 0x08;
    c64_cpu.memory[0x1004] = 0x87;  // SAX $00 (illegal)
    c64_cpu.memory[0x1005] = 0x00;
    c64_cpu.memory[0x1006] = 0xEA;  // NOP
    
    std::cout << "Executing test program:\n";
    c64_cpu.PC = 0x1000;
    for (int i = 0; i < 4; i++) {
        std::cout << "Step " << (i + 1) << ": PC=0x" << std::hex << c64_cpu.PC 
                  << " A=0x" << (int)c64_cpu.A << " X=0x" << (int)c64_cpu.X << "\n";
        c64_cpu.step();
    }
    
    std::cout << "\nFinal state: A=0x" << std::hex << (int)c64_cpu.A 
              << " Total cycles: " << std::dec << c64_cpu.total_cycles << "\n";
    
    return 0;
}