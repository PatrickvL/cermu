#include "cpu6510.h"
#include "bus.h"
#include "c64.h"
#include <string.h>

// Include all instruction implementation files
#include "cpu6510_arithmetic.c"
#include "cpu6510_loads_stores.c"
#include "cpu6510_shifts.c"
#include "cpu6510_branches.c"
#include "cpu6510_flags.c"
#include "cpu6510_stack.c"
#include "cpu6510_system.c"
#include "cpu6510_unofficial.c"

// ============================================================================
// MOS 6510 CPU STATE
// ============================================================================
cpu6510_state_t cpu;

// Universal instruction table using function pointers
// Global instruction table
instruction_func_t instruction_table[256];

// Forward declarations
void* handle_interrupt = NULL;
void* fetch_opcode = NULL;

// ============================================================================
// CPU OPERATION FUNCTIONS (inline for performance)
// ============================================================================

// Initialize CPU
void cpu6510_init(void) {
    cpu.a = 0;
    cpu.x = 0;
    cpu.y = 0;
    cpu.sp = 0xFD;
    cpu.p = FLAG_U | FLAG_I;  // Unused flag always set, interrupt disable
    cpu.pc = 0;
    
    // 6510-specific I/O port (addresses $0000/$0001) initialization
    ram[0x0000] = 0x2F; // Default Data Direction Register (DDR at $0000)
    ram[0x0001] = 0x37;  // Default I/O Port Data (at $0001)
    switch_cpu_mode(0x07); // All RAM/ROM enabled
 
    cpu.total_cycles = 0;
    
    // Setup instruction table
    cpu6510_setup_opcode_table();
}

// Reset CPU
void cpu6510_reset(void) {
    // Read reset vector from $FFFC/$FFFD
    cpu_read_cycle(0xFFFC);
    uint8_t pcl = bus_state.data;
    cpu_read_cycle(0xFFFD);
    uint8_t pch = bus_state.data;
    
    cpu.pc = (pch << 8) | pcl;
    cpu.sp = 0xFF; // or 0FD?
    cpu.p |= FLAG_I;  // Set interrupt disable
    cpu.total_cycles = 0;
}

bool cpu6510_step(void) {
    cpu.total_cycles++;
    bus_cycle();
    // Return true when instruction completes (for testing)
    return true;
}

void cpu6510_irq(void) {
    // Push PC and status to stack, set interrupt disable, jump to IRQ vector
    cpu_push((cpu.pc >> 8) & 0xFF);
    cpu_push(cpu.pc & 0xFF);
    cpu_push(cpu.p & ~FLAG_B);  // Clear B flag for IRQ
    cpu.p |= FLAG_I;
    cpu_read_cycle(0xFFFE);
    cpu.pc = bus_state.data;
    cpu_read_cycle(0xFFFF);
    cpu.pc |= (bus_state.data << 8);
}

void cpu6510_nmi(void) {
    // Push PC and status to stack, set interrupt disable, jump to NMI vector
    cpu_push((cpu.pc >> 8) & 0xFF);
    cpu_push(cpu.pc & 0xFF);
    cpu_push(cpu.p & ~FLAG_B);  // Clear B flag for NMI
    cpu.p |= FLAG_I;
    cpu_read_cycle(0xFFFA);
    cpu.pc = bus_state.data;
    cpu_read_cycle(0xFFFB);
    cpu.pc |= (bus_state.data << 8);
}

// ============================================================================
// INSTRUCTION OPERATIONS - Now defined as inline functions in cpu6510.h
// ============================================================================

// ============================================================================
// INSTRUCTION FUNCTIONS (Universal approach using function pointers)
// ============================================================================

// Basic instruction functions that are not in separate files
void nop_instruction_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, nop_wait);  // Dummy read
    NEXT_INSTRUCTION(nop_fetch_wait);
}

void brk_instruction_func(void) {
    cpu.pc++;  // Skip BRK signature byte
    // Push PC high byte
    cpu_push((cpu.pc >> 8) & 0xFF);
    // Push PC low byte
    cpu_push(cpu.pc & 0xFF);
    // Push status register with B flag set
    cpu_push(cpu.p | FLAG_B);
    // Set interrupt disable
    cpu.p |= FLAG_I;
    // Read IRQ vector
    cpu_read_cycle(0xFFFE);
    cpu.pc = bus_state.data;
    cpu_read_cycle(0xFFFF);
    cpu.pc |= (bus_state.data << 8);
    NEXT_INSTRUCTION(brk_fetch_wait);
}

void illegal_instruction_func(void) {
    // Handle illegal opcodes - just NOP for now
    NEXT_INSTRUCTION(illegal_fetch_wait);
}

void handle_interrupt_func(void) {
    // Interrupt handling logic
    if (bus_state.control_lines & NMI_LINE) {
        // Handle NMI - non-maskable
        cpu6510_nmi();
    } else if ((bus_state.control_lines & IRQ_LINE) && !cpu_get_flag(FLAG_I)) {
        // Handle IRQ - only if interrupt disable is clear
        cpu6510_irq();
    }
    NEXT_INSTRUCTION(interrupt_fetch_wait);
}


// ============================================================================
// CPU EXECUTION LOOP WITH FUNCTION POINTERS (Universal)
// ============================================================================
void cpu6510_execute(void) {  
    // Start execution - fetch first instruction
    if (unlikely(bus_state.control_lines & (IRQ_LINE | NMI_LINE))) {
        handle_interrupt_func();
        return;
    }
    WAIT_READY_THEN_READ(cpu.pc++, main_fetch_start);
    cpu.opcode = bus_state.data;
    instruction_table[cpu.opcode]();
}

// ============================================================================
// INSTRUCTION TABLE SETUP
// ============================================================================
void cpu6510_setup_opcode_table(void) {
    instruction_table[0x00] = brk_instruction_func;    // BRK
    instruction_table[0x01] = ora_indirect_x_func;     // ORA ($nn,X)
    instruction_table[0x02] = illegal_instruction_func;
    instruction_table[0x03] = illegal_instruction_func;
    instruction_table[0x04] = illegal_instruction_func;
    instruction_table[0x05] = ora_zero_page_func;      // ORA $nn
    instruction_table[0x06] = asl_zero_page_func;      // ASL $nn
    instruction_table[0x07] = illegal_instruction_func;
    instruction_table[0x08] = illegal_instruction_func;
    instruction_table[0x09] = ora_immediate_func;      // ORA #$nn
    instruction_table[0x0A] = asl_accumulator_func;    // ASL A
    instruction_table[0x0B] = illegal_instruction_func;
    instruction_table[0x0C] = illegal_instruction_func;
    instruction_table[0x0D] = ora_absolute_func;       // ORA $nnnn
    instruction_table[0x0E] = asl_absolute_func;       // ASL $nnnn
    instruction_table[0x0F] = illegal_instruction_func;

    instruction_table[0x10] = bpl_func;                // BPL rel
    instruction_table[0x11] = ora_indirect_y_func;     // ORA ($nn),Y
    instruction_table[0x12] = illegal_instruction_func;
    instruction_table[0x13] = illegal_instruction_func;
    instruction_table[0x14] = illegal_instruction_func;
    instruction_table[0x15] = ora_zero_page_x_func;    // ORA $nn,X
    instruction_table[0x16] = asl_zero_page_x_func;    // ASL $nn,X
    instruction_table[0x17] = illegal_instruction_func;
    instruction_table[0x18] = illegal_instruction_func;
    instruction_table[0x19] = ora_absolute_y_func;     // ORA $nnnn,Y
    instruction_table[0x1A] = illegal_instruction_func;
    instruction_table[0x1B] = illegal_instruction_func;
    instruction_table[0x1C] = illegal_instruction_func;
    instruction_table[0x1D] = ora_absolute_x_func;     // ORA $nnnn,X
    instruction_table[0x1E] = asl_absolute_x_func;     // ASL $nnnn,X
    instruction_table[0x1F] = illegal_instruction_func;

    instruction_table[0x20] = jsr_func;                // JSR $nnnn
    instruction_table[0x21] = and_indirect_x_func;     // AND ($nn,X)
    instruction_table[0x22] = illegal_instruction_func;
    instruction_table[0x23] = illegal_instruction_func;
    instruction_table[0x24] = illegal_instruction_func;
    instruction_table[0x25] = and_zero_page_func;      // AND $nn
    instruction_table[0x26] = rol_zero_page_func;      // ROL $nn
    instruction_table[0x27] = illegal_instruction_func;
    instruction_table[0x28] = illegal_instruction_func;
    instruction_table[0x29] = and_immediate_func;      // AND #$nn
    instruction_table[0x2A] = rol_accumulator_func;    // ROL A
    instruction_table[0x2B] = illegal_instruction_func;
    instruction_table[0x2C] = illegal_instruction_func;
    instruction_table[0x2D] = and_absolute_func;       // AND $nnnn
    instruction_table[0x2E] = rol_absolute_func;       // ROL $nnnn
    instruction_table[0x2F] = illegal_instruction_func;

    instruction_table[0x30] = bmi_func;                // BMI rel
    instruction_table[0x31] = and_indirect_y_func;     // AND ($nn),Y
    instruction_table[0x32] = illegal_instruction_func;
    instruction_table[0x33] = illegal_instruction_func;
    instruction_table[0x34] = illegal_instruction_func;
    instruction_table[0x35] = and_zero_page_x_func;    // AND $nn,X
    instruction_table[0x36] = rol_zero_page_x_func;    // ROL $nn,X
    instruction_table[0x37] = illegal_instruction_func;
    instruction_table[0x38] = illegal_instruction_func;
    instruction_table[0x39] = and_absolute_y_func;     // AND $nnnn,Y
    instruction_table[0x3A] = illegal_instruction_func;
    instruction_table[0x3B] = illegal_instruction_func;
    instruction_table[0x3C] = illegal_instruction_func;
    instruction_table[0x3D] = and_absolute_x_func;     // AND $nnnn,X
    instruction_table[0x3E] = rol_absolute_x_func;     // ROL $nnnn,X
    instruction_table[0x3F] = illegal_instruction_func;

    instruction_table[0x40] = illegal_instruction_func;
    instruction_table[0x41] = eor_indirect_x_func;     // EOR ($nn,X)
    instruction_table[0x42] = illegal_instruction_func;
    instruction_table[0x43] = illegal_instruction_func;
    instruction_table[0x44] = illegal_instruction_func;
    instruction_table[0x45] = eor_zero_page_func;      // EOR $nn
    instruction_table[0x46] = lsr_zero_page_func;      // LSR $nn
    instruction_table[0x47] = illegal_instruction_func;
    instruction_table[0x48] = illegal_instruction_func;
    instruction_table[0x49] = eor_immediate_func;      // EOR #$nn
    instruction_table[0x4A] = lsr_accumulator_func;    // LSR A
    instruction_table[0x4B] = illegal_instruction_func;
    instruction_table[0x4C] = jmp_absolute_func;       // JMP $nnnn
    instruction_table[0x4D] = eor_absolute_func;       // EOR $nnnn
    instruction_table[0x4E] = lsr_absolute_func;       // LSR $nnnn
    instruction_table[0x4F] = illegal_instruction_func;

    instruction_table[0x50] = bvc_func;                // BVC rel
    instruction_table[0x51] = eor_indirect_y_func;     // EOR ($nn),Y
    instruction_table[0x52] = illegal_instruction_func;
    instruction_table[0x53] = illegal_instruction_func;
    instruction_table[0x54] = illegal_instruction_func;
    instruction_table[0x55] = eor_zero_page_x_func;    // EOR $nn,X
    instruction_table[0x56] = lsr_zero_page_x_func;    // LSR $nn,X
    instruction_table[0x57] = illegal_instruction_func;
    instruction_table[0x58] = illegal_instruction_func;
    instruction_table[0x59] = eor_absolute_y_func;     // EOR $nnnn,Y
    instruction_table[0x5A] = illegal_instruction_func;
    instruction_table[0x5B] = illegal_instruction_func;
    instruction_table[0x5C] = illegal_instruction_func;
    instruction_table[0x5D] = eor_absolute_x_func;     // EOR $nnnn,X
    instruction_table[0x5E] = lsr_absolute_x_func;     // LSR $nnnn,X
    instruction_table[0x5F] = illegal_instruction_func;

    instruction_table[0x60] = illegal_instruction_func;
    instruction_table[0x61] = adc_indirect_x_func;     // ADC ($nn,X)
    instruction_table[0x62] = illegal_instruction_func;
    instruction_table[0x63] = illegal_instruction_func;
    instruction_table[0x64] = illegal_instruction_func;
    instruction_table[0x65] = adc_zero_page_func;      // ADC $nn
    instruction_table[0x66] = ror_zero_page_func;      // ROR $nn
    instruction_table[0x67] = illegal_instruction_func;
    instruction_table[0x68] = illegal_instruction_func;
    instruction_table[0x69] = adc_immediate_func;      // ADC #$nn
    instruction_table[0x6A] = ror_accumulator_func;    // ROR A
    instruction_table[0x6B] = illegal_instruction_func;
    instruction_table[0x6C] = jmp_indirect_func;       // JMP ($nnnn)
    instruction_table[0x6D] = adc_absolute_func;       // ADC $nnnn
    instruction_table[0x6E] = ror_absolute_func;       // ROR $nnnn
    instruction_table[0x6F] = illegal_instruction_func;

    instruction_table[0x70] = bvs_func;                // BVS rel
    instruction_table[0x71] = adc_indirect_y_func;     // ADC ($nn),Y
    instruction_table[0x72] = illegal_instruction_func;
    instruction_table[0x73] = illegal_instruction_func;
    instruction_table[0x74] = illegal_instruction_func;
    instruction_table[0x75] = adc_zero_page_x_func;    // ADC $nn,X
    instruction_table[0x76] = ror_zero_page_x_func;    // ROR $nn,X
    instruction_table[0x77] = illegal_instruction_func;
    instruction_table[0x78] = illegal_instruction_func;
    instruction_table[0x79] = adc_absolute_y_func;     // ADC $nnnn,Y
    instruction_table[0x7A] = illegal_instruction_func;
    instruction_table[0x7B] = illegal_instruction_func;
    instruction_table[0x7C] = illegal_instruction_func;
    instruction_table[0x7D] = adc_absolute_x_func;     // ADC $nnnn,X
    instruction_table[0x7E] = ror_absolute_x_func;     // ROR $nnnn,X
    instruction_table[0x7F] = illegal_instruction_func;

    instruction_table[0x80] = illegal_instruction_func;
    instruction_table[0x81] = sta_indirect_x_func;     // STA ($nn,X)
    instruction_table[0x82] = illegal_instruction_func;
    instruction_table[0x83] = illegal_instruction_func;
    instruction_table[0x84] = sty_zero_page_func;      // STY $nn
    instruction_table[0x85] = sta_zero_page_func;      // STA $nn
    instruction_table[0x86] = stx_zero_page_func;      // STX $nn
    instruction_table[0x87] = illegal_instruction_func;
    instruction_table[0x88] = illegal_instruction_func;
    instruction_table[0x89] = illegal_instruction_func;
    instruction_table[0x8A] = illegal_instruction_func;
    instruction_table[0x8B] = illegal_instruction_func;
    instruction_table[0x8C] = sty_absolute_func;       // STY $nnnn
    instruction_table[0x8D] = sta_absolute_func;       // STA $nnnn
    instruction_table[0x8E] = stx_absolute_func;       // STX $nnnn
    instruction_table[0x8F] = illegal_instruction_func;

    instruction_table[0x90] = bcc_func;                // BCC rel
    instruction_table[0x91] = sta_indirect_y_func;     // STA ($nn),Y
    instruction_table[0x92] = illegal_instruction_func;
    instruction_table[0x93] = illegal_instruction_func;
    instruction_table[0x94] = sty_zero_page_x_func;    // STY $nn,X
    instruction_table[0x95] = sta_zero_page_x_func;    // STA $nn,X
    instruction_table[0x96] = stx_zero_page_y_func;    // STX $nn,Y
    instruction_table[0x97] = illegal_instruction_func;
    instruction_table[0x98] = illegal_instruction_func;
    instruction_table[0x99] = sta_absolute_y_func;     // STA $nnnn,Y
    instruction_table[0x9A] = illegal_instruction_func;
    instruction_table[0x9B] = illegal_instruction_func;
    instruction_table[0x9C] = illegal_instruction_func;
    instruction_table[0x9D] = sta_absolute_x_func;     // STA $nnnn,X
    instruction_table[0x9E] = illegal_instruction_func;
    instruction_table[0x9F] = illegal_instruction_func;

    instruction_table[0xA0] = ldy_immediate_func;      // LDY #$nn
    instruction_table[0xA1] = lda_indirect_x_func;     // LDA ($nn,X)
    instruction_table[0xA2] = ldx_immediate_func;      // LDX #$nn
    instruction_table[0xA3] = illegal_instruction_func;
    instruction_table[0xA4] = ldy_zero_page_func;      // LDY $nn
    instruction_table[0xA5] = lda_zero_page_func;      // LDA $nn
    instruction_table[0xA6] = ldx_zero_page_func;      // LDX $nn
    instruction_table[0xA7] = illegal_instruction_func;
    instruction_table[0xA8] = illegal_instruction_func;
    instruction_table[0xA9] = lda_immediate_func;      // LDA #$nn
    instruction_table[0xAA] = illegal_instruction_func;
    instruction_table[0xAB] = illegal_instruction_func;
    instruction_table[0xAC] = ldy_absolute_func;       // LDY $nnnn
    instruction_table[0xAD] = lda_absolute_func;       // LDA $nnnn
    instruction_table[0xAE] = ldx_absolute_func;       // LDX $nnnn
    instruction_table[0xAF] = illegal_instruction_func;

    instruction_table[0xB0] = bcs_func;                // BCS rel
    instruction_table[0xB1] = lda_indirect_y_func;     // LDA ($nn),Y
    instruction_table[0xB2] = illegal_instruction_func;
    instruction_table[0xB3] = illegal_instruction_func;
    instruction_table[0xB4] = ldy_zero_page_x_func;    // LDY $nn,X
    instruction_table[0xB5] = lda_zero_page_x_func;    // LDA $nn,X
    instruction_table[0xB6] = ldx_zero_page_y_func;    // LDX $nn,Y
    instruction_table[0xB7] = illegal_instruction_func;
    instruction_table[0xB8] = illegal_instruction_func;
    instruction_table[0xB9] = lda_absolute_y_func;     // LDA $nnnn,Y
    instruction_table[0xBA] = illegal_instruction_func;
    instruction_table[0xBB] = illegal_instruction_func;
    instruction_table[0xBC] = ldy_absolute_x_func;     // LDY $nnnn,X
    instruction_table[0xBD] = lda_absolute_x_func;     // LDA $nnnn,X
    instruction_table[0xBE] = ldx_absolute_y_func;     // LDX $nnnn,Y
    instruction_table[0xBF] = illegal_instruction_func;

    instruction_table[0xC0] = cpy_immediate_func;      // CPY #$nn
    instruction_table[0xC1] = cmp_indirect_x_func;     // CMP ($nn,X)
    instruction_table[0xC2] = illegal_instruction_func;
    instruction_table[0xC3] = illegal_instruction_func;
    instruction_table[0xC4] = cpy_zero_page_func;      // CPY $nn
    instruction_table[0xC5] = cmp_zero_page_func;      // CMP $nn
    instruction_table[0xC6] = illegal_instruction_func;
    instruction_table[0xC7] = illegal_instruction_func;
    instruction_table[0xC8] = illegal_instruction_func;
    instruction_table[0xC9] = cmp_immediate_func;      // CMP #$nn
    instruction_table[0xCA] = illegal_instruction_func;
    instruction_table[0xCB] = illegal_instruction_func;
    instruction_table[0xCC] = cpy_absolute_func;       // CPY $nnnn
    instruction_table[0xCD] = cmp_absolute_func;       // CMP $nnnn
    instruction_table[0xCE] = illegal_instruction_func;
    instruction_table[0xCF] = illegal_instruction_func;

    instruction_table[0xD0] = bne_func;                // BNE rel
    instruction_table[0xD1] = cmp_indirect_y_func;     // CMP ($nn),Y
    instruction_table[0xD2] = illegal_instruction_func;
    instruction_table[0xD3] = illegal_instruction_func;
    instruction_table[0xD4] = illegal_instruction_func;
    instruction_table[0xD5] = cmp_zero_page_x_func;    // CMP $nn,X
    instruction_table[0xD6] = illegal_instruction_func;
    instruction_table[0xD7] = illegal_instruction_func;
    instruction_table[0xD8] = illegal_instruction_func;
    instruction_table[0xD9] = cmp_absolute_y_func;     // CMP $nnnn,Y
    instruction_table[0xDA] = illegal_instruction_func;
    instruction_table[0xDB] = illegal_instruction_func;
    instruction_table[0xDC] = illegal_instruction_func;
    instruction_table[0xDD] = cmp_absolute_x_func;     // CMP $nnnn,X
    instruction_table[0xDE] = illegal_instruction_func;
    instruction_table[0xDF] = illegal_instruction_func;

    instruction_table[0xE0] = cpx_immediate_func;      // CPX #$nn
    instruction_table[0xE1] = sbc_indirect_x_func;     // SBC ($nn,X)
    instruction_table[0xE2] = illegal_instruction_func;
    instruction_table[0xE3] = illegal_instruction_func;
    instruction_table[0xE4] = cpx_zero_page_func;      // CPX $nn
    instruction_table[0xE5] = sbc_zero_page_func;      // SBC $nn
    instruction_table[0xE6] = illegal_instruction_func;
    instruction_table[0xE7] = illegal_instruction_func;
    instruction_table[0xE8] = illegal_instruction_func;
    instruction_table[0xE9] = sbc_immediate_func;      // SBC #$nn
    instruction_table[0xEA] = nop_instruction_func;    // NOP
    instruction_table[0xEB] = illegal_instruction_func;
    instruction_table[0xEC] = cpx_absolute_func;       // CPX $nnnn
    instruction_table[0xED] = sbc_absolute_func;       // SBC $nnnn
    instruction_table[0xEE] = illegal_instruction_func;
    instruction_table[0xEF] = illegal_instruction_func;

    instruction_table[0xF0] = beq_func;                // BEQ rel
    instruction_table[0xF1] = sbc_indirect_y_func;     // SBC ($nn),Y
    instruction_table[0xF2] = illegal_instruction_func;
    instruction_table[0xF3] = illegal_instruction_func;
    instruction_table[0xF4] = illegal_instruction_func;
    instruction_table[0xF5] = sbc_zero_page_x_func;    // SBC $nn,X
    instruction_table[0xF6] = illegal_instruction_func;
    instruction_table[0xF7] = illegal_instruction_func;
    instruction_table[0xF8] = illegal_instruction_func;
    instruction_table[0xF9] = sbc_absolute_y_func;     // SBC $nnnn,Y
    instruction_table[0xFA] = illegal_instruction_func;
    instruction_table[0xFB] = illegal_instruction_func;
    instruction_table[0xFC] = illegal_instruction_func;
    instruction_table[0xFD] = sbc_absolute_x_func;     // SBC $nnnn,X
    instruction_table[0xFE] = illegal_instruction_func;
    instruction_table[0xFF] = illegal_instruction_func;
    
    // ========================================================================
    // MISSING INSTRUCTIONS - Need to be implemented
    // ========================================================================
    // Flag Instructions (need implementation in cpu6510_flags.c):
    // 0x18 = CLC, 0x38 = SEC, 0x58 = CLI, 0x78 = SEI
    // 0xD8 = CLD, 0xF8 = SED, 0xB8 = CLV
    
    // Stack Instructions (need implementation in cpu6510_stack.c):
    // 0x48 = PHA, 0x68 = PLA, 0x08 = PHP, 0x28 = PLP
    // 0x40 = RTI, 0x60 = RTS
    
    // Transfer Instructions (need implementation in cpu6510_system.c):
    // 0xAA = TAX, 0x8A = TXA, 0xA8 = TAY, 0x98 = TYA
    // 0xBA = TSX, 0x9A = TXS
    
    // Increment/Decrement Instructions (need implementation):
    // 0xE6 = INC $nn, 0xF6 = INC $nn,X, 0xEE = INC $nnnn, 0xFE = INC $nnnn,X
    // 0xC6 = DEC $nn, 0xD6 = DEC $nn,X, 0xCE = DEC $nnnn, 0xDE = DEC $nnnn,X
    // 0xE8 = INX, 0xCA = DEX, 0xC8 = INY, 0x88 = DEY
    
    // BIT Instruction (need implementation):
    // 0x24 = BIT $nn, 0x2C = BIT $nnnn
}
