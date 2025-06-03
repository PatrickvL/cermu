#include "device.h"
#include "mos6510.h"
#include <stdlib.h>
#include "c64_bus.h"
#include <stdlib.h>

// ============================================================================
// CPU OPERATION FUNCTIONS (inline for performance)
// ============================================================================

// Compute effective port output: outputs from Data when DDR=1, else external data
static inline uint8_t mos6510_io_mask(mos6510_t* cpu_dev, uint8_t value)
{
    uint8_t ddr = cpu_dev->io_port[0];
    uint8_t data = cpu_dev->io_port[1];
    return (data & ddr) | (value & ~ddr);
}

static inline uint8_t mos6510_ioport_read(mos6510_t* cpu_dev, uint16_t addr) {
    uint8_t value = cpu_dev->io_port[addr];
    if (addr == 1) {
        // Return port: outputs defined by DDR bits, inputs from bus data
        return mos6510_io_mask(cpu_dev, value); // TODO : Figure out if `value` is the correct argument, and if so, could we just return it directly?
    }
    return value;
}

static inline void mos6510_ioport_write(mos6510_t* cpu_dev, uint16_t addr, uint8_t value) {
    cpu_dev->io_port[addr] = value; // Update Data Direction (or Data) register
    // Mux Data Direction with Data register and the given value
    uint8_t port_out = mos6510_io_mask(cpu_dev, cpu_dev->io_port[1]);
    // Update the bus mode based on the port output
    c64_bus_mode_switch(cpu_dev->c64_bus, port_out);
}

// CPU lifecycle wrapper functions
void mos6510_init(mos6510_t* cpu_dev) {
    // Initialize CPU registers and state
    cpu_dev->a = 0;
    cpu_dev->x = 0;
    cpu_dev->y = 0;
    cpu_dev->sp = 0xFF;
    cpu_dev->p = FLAG_U | FLAG_I;  // Unused flag always set, interrupt disable
    cpu_dev->pc = 0;
    
    // 6510-specific I/O port (addresses $0000/$0001) initialization
    cpu_dev->io_port[0] = 0x2F;  // Default Data Direction Register (DDR at $0000)
    mos6510_ioport_write(cpu_dev, 1, 0x37);  // Default I/O Port Data (at $0001)
}

//

void mos6510_system_destroy(void* device) {
    free(device);
}

void* mos6510_system_create(device_descriptor_t* desc) {
    mos6510_t* cpu = (mos6510_t*)calloc(1, sizeof(mos6510_t));
    if (!cpu) return NULL;
    cpu->desc = desc;
    mos6510_init(cpu);
    return cpu;
}

// Attach bus to CPU
void mos6510_bus_attach(void* device, c64_bus_t* bus) {
    mos6510_t* cpu = (mos6510_t*)device;
    cpu->c64_bus = bus;
}

device_descriptor_t mos6510_descriptor = {
    .create = mos6510_system_create,
    .destroy = mos6510_system_destroy,
    .bus_attach = mos6510_bus_attach,
    .read = mos6510_ioport_read,
    .write = mos6510_ioport_write,
    .bank_change = NULL
};

//

// Optimized read cycle implementation with embedded I/O port handling
uint8_t mos6510_read_cycle(mos6510_t* cpu_dev, uint16_t addr) {
    // Handle I/O ports directly in CPU - addresses $0000 and $0001
    if (addr <= 1) {
        uint8_t value = mos6510_ioport_read(cpu_dev, addr);
        // Note : MOS6510 I/O port accesses do not update bus address and data lines!
        c64_non_cpu_cycle(cpu_dev->c64_bus->c64); // But all other devices must still run for this cycle
        return value;
    }
    
    return c64_bus_read_cycle(cpu_dev->c64_bus, addr);
}

// Optimized write cycle implementation with embedded I/O port handling
void mos6510_write_cycle(mos6510_t* cpu_dev, uint16_t addr, uint8_t value) {
    // Handle I/O ports directly in CPU - addresses $0000 and $0001
    if (addr <= 1) {
        mos6510_ioport_write(cpu_dev, addr, value);
        // Note : MOS6510 I/O port accesses do not update bus address and data lines!
        c64_non_cpu_cycle(cpu_dev->c64_bus->c64); // But all other devices must still run for this cycle
        return;
    }
    
    c64_bus_write_cycle(cpu_dev->c64_bus, addr, value);
}

// Interrupt handler - called when IRQ or NMI lines are active
void mos6510_interrupt_handler(mos6510_t* cpu_dev) {
    if (CPU_CONTROL_LINES(cpu_dev) & NMI_LINE) {
        // Handle NMI - non-maskable
        mos6510_nmi(cpu_dev);
    } else if ((CPU_CONTROL_LINES(cpu_dev) & IRQ_LINE) && !cpu_get_flag(cpu_dev, FLAG_I)) {
        // Handle IRQ when interrupt disable is clear
        mos6510_irq(cpu_dev, cpu_dev->p & ~FLAG_B); // Clear B flag for IRQ
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Reset CPU
void mos6510_reset(mos6510_t* cpu_dev) {
    // Read reset vector from $FFFC/$FFFD
    uint8_t pcl = mos6510_read_cycle(cpu_dev, 0xFFFC);
    uint8_t pch = mos6510_read_cycle(cpu_dev, 0xFFFD);
    
    cpu_dev->pc = (pch << 8) | pcl;
    cpu_dev->sp = 0xFF; // or 0FD?
    cpu_dev->p |= FLAG_I;  // Set interrupt disable
}

bool mos6510_step(mos6510_t* cpu_dev) { // _dispatch
    uint8_t opcode = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    mos6510_opcode_dispatch(cpu_dev, opcode);
    // Return true when instruction completes (for testing)
    return true;
}

void mos6510_irq(mos6510_t* cpu_dev, uint8_t status) {
    // Push PC and status to stack, set interrupt disable, jump to IRQ vector
    // Push PC high byte
    mos6510_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF);
    // Push PC low byte
    mos6510_push(cpu_dev, cpu_dev->pc & 0xFF);
    // Push status argument byte
    mos6510_push(cpu_dev, status); 
    // Set interrupt disable
    cpu_dev->p |= FLAG_I;
    // Read IRQ vector
    cpu_dev->pc = mos6510_read_cycle(cpu_dev, 0xFFFE);
    uint8_t cpu_data = mos6510_read_cycle(cpu_dev, 0xFFFF);
    cpu_dev->pc |= (cpu_data << 8);
}

void mos6510_nmi(mos6510_t* cpu_dev) {
    // Push PC and status to stack, set interrupt disable, jump to NMI vector
    mos6510_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF);
    mos6510_push(cpu_dev, cpu_dev->pc & 0xFF);
    mos6510_push(cpu_dev, cpu_dev->p & ~FLAG_B);  // Clear B flag for NMI
    cpu_dev->p |= FLAG_I;
    cpu_dev->pc = mos6510_read_cycle(cpu_dev, 0xFFFA);
    uint8_t cpu_data = mos6510_read_cycle(cpu_dev, 0xFFFB);
    cpu_dev->pc |= (cpu_data << 8);
}

// ============================================================================
// CPU EXECUTION LOOP WITH FUNCTION POINTERS (Universal)
// ============================================================================
void mos6510_execute(mos6510_t* cpu_dev) {
    // Start execution - fetch first instruction
    CPU_NEXT_INSTRUCTION(cpu_dev);
}

// ============================================================================
// INSTRUCTION TABLE SETUP
// ============================================================================

// TODO : I want the function signature of all 256 opcode handlers to be declared using a macro.

// Include all instruction implementation files in the new organization
#include "mos6510_arithmetic.c"   // ADC, SBC, AND, ORA, EOR, CMP, CPX, CPY, BIT
#include "mos6510_control.c"      // BRK, RTI, RTS, JSR, JMP, all branches
#include "mos6510_flags.c"        // CLC, SEC, CLI, SEI, CLD, SED, CLV
#include "mos6510_illegal.c"      // All unofficial instructions
#include "mos6510_memory.c"       // LDA, LDX, LDY, STA, STX, STY
#include "mos6510_misc.c"         // NOP variants
#include "mos6510_registers.c"    // TAX, TXA, TAY, TYA, TSX, TXS, INC, DEC, INX, DEX, INY, DEY
#include "mos6510_shifts.c"       // ASL, LSR, ROL, ROR
#include "mos6510_stack.c"        // PHA, PLA, PHP, PLP

// Global MOS6510 instruction table using function pointers
mos6510_opcode_handler_t mos6510_opcode_handlers[256] = {
    [0x00] = brk_instruction_func,   // BRK
    [0x01] = ora_indirect_x_func,    // ORA ($nn,X)
    [0x02] = jam_func,               // JAM (illegal)
    [0x03] = slo_indirect_x_func,    // SLO ($nn,X)
    [0x04] = nop_zero_page_func,     // NOP $nn (illegal)
    [0x05] = ora_zero_page_func,     // ORA $nn
    [0x06] = asl_zero_page_func,     // ASL $nn
    [0x07] = slo_zero_page_func,     // SLO $nn (illegal)
    [0x08] = php_func,               // PHP
    [0x09] = ora_immediate_func,     // ORA #$nn
    [0x0A] = asl_accumulator_func,   // ASL A
    [0x0B] = anc_immediate_func,     // ANC #$nn (illegal)
    [0x0C] = nop_absolute_func,      // NOP $nnnn (illegal)
    [0x0D] = ora_absolute_func,      // ORA $nnnn
    [0x0E] = asl_absolute_func,      // ASL $nnnn
    [0x0F] = slo_absolute_func,      // SLO $nnnn (illegal)

    [0x10] = bpl_func,               // BPL rel
    [0x11] = ora_indirect_y_func,    // ORA ($nn),Y
    [0x12] = jam_func,               // JAM (illegal)
    [0x13] = slo_indirect_y_func,    // SLO ($nn),Y (illegal)
    [0x14] = nop_zero_page_x_func,   // NOP $nn,X (illegal)
    [0x15] = ora_zero_page_x_func,   // ORA $nn,X
    [0x16] = asl_zero_page_x_func,   // ASL $nn,X
    [0x17] = slo_zero_page_x_func,   // SLO $nn,X (illegal)
    [0x18] = clc_func,               // CLC
    [0x19] = ora_absolute_y_func,    // ORA $nnnn,Y
    [0x1A] = nop_func,               // NOP (illegal)
    [0x1B] = slo_absolute_y_func,    // SLO $nnnn,Y (illegal)
    [0x1C] = nop_absolute_x_func,    // NOP $nnnn,X (illegal)
    [0x1D] = ora_absolute_x_func,    // ORA $nnnn,X
    [0x1E] = asl_absolute_x_func,    // ASL $nnnn,X
    [0x1F] = slo_absolute_x_func,    // SLO $nnnn,X (illegal)

    [0x20] = jsr_func,               // JSR $nnnn
    [0x21] = and_indirect_x_func,    // AND ($nn,X)
    [0x22] = jam_func,               // JAM (illegal)
    [0x23] = rla_indirect_x_func,    // RLA ($nn,X) (illegal)
    [0x24] = bit_zero_page_func,     // BIT $nn
    [0x25] = and_zero_page_func,     // AND $nn
    [0x26] = rol_zero_page_func,     // ROL $nn
    [0x27] = rla_zero_page_func,     // RLA $nn (illegal)
    [0x28] = plp_func,               // PLP
    [0x29] = and_immediate_func,     // AND #$nn
    [0x2A] = rol_accumulator_func,   // ROL A
    [0x2B] = anc_immediate_func,     // ANC #$nn (illegal)
    [0x2C] = bit_absolute_func,      // BIT $nnnn
    [0x2D] = and_absolute_func,      // AND $nnnn
    [0x2E] = rol_absolute_func,      // ROL $nnnn
    [0x2F] = rla_absolute_func,      // RLA $nnnn (illegal)

    [0x30] = bmi_func,               // BMI rel
    [0x31] = and_indirect_y_func,    // AND ($nn),Y
    [0x32] = jam_func,               // JAM (illegal)
    [0x33] = rla_indirect_y_func,    // RLA ($nn),Y (illegal)
    [0x34] = nop_zero_page_x_func,   // NOP $nn,X (illegal)
    [0x35] = and_zero_page_x_func,   // AND $nn,X
    [0x36] = rol_zero_page_x_func,   // ROL $nn,X
    [0x37] = rla_zero_page_x_func,   // RLA $nn,X (illegal)
    [0x38] = sec_func,               // SEC
    [0x39] = and_absolute_y_func,    // AND $nnnn,Y
    [0x3A] = nop_func,               // NOP (illegal)
    [0x3B] = rla_absolute_y_func,    // RLA $nnnn,Y (illegal)
    [0x3C] = nop_absolute_x_func,    // NOP $nnnn,X (illegal)
    [0x3D] = and_absolute_x_func,    // AND $nnnn,X
    [0x3E] = rol_absolute_x_func,    // ROL $nnnn,X
    [0x3F] = rla_absolute_x_func,    // RLA $nnnn,X (illegal)

    [0x40] = rti_func,               // RTI
    [0x41] = eor_indirect_x_func,    // EOR ($nn,X)
    [0x42] = jam_func,               // JAM (illegal)
    [0x43] = sre_indirect_x_func,    // SRE ($nn,X) (illegal)
    [0x44] = nop_zero_page_func,     // NOP $nn (illegal)
    [0x45] = eor_zero_page_func,     // EOR $nn
    [0x46] = lsr_zero_page_func,     // LSR $nn
    [0x47] = sre_zero_page_func,     // SRE $nn (illegal)
    [0x48] = pha_func,               // PHA
    [0x49] = eor_immediate_func,     // EOR #$nn
    [0x4A] = lsr_accumulator_func,   // LSR A
    [0x4B] = alr_immediate_func,     // ALR #$nn (illegal)
    [0x4C] = jmp_absolute_func,      // JMP $nnnn
    [0x4D] = eor_absolute_func,      // EOR $nnnn
    [0x4E] = lsr_absolute_func,      // LSR $nnnn
    [0x4F] = sre_absolute_func,      // SRE $nnnn (illegal)

    [0x50] = bvc_func,               // BVC rel
    [0x51] = eor_indirect_y_func,    // EOR ($nn),Y
    [0x52] = jam_func,               // JAM (illegal)
    [0x53] = sre_indirect_y_func,    // SRE ($nn),Y (illegal)
    [0x54] = nop_zero_page_x_func,   // NOP $nn,X (illegal)
    [0x55] = eor_zero_page_x_func,   // EOR $nn,X
    [0x56] = lsr_zero_page_x_func,   // LSR $nn,X
    [0x57] = sre_zero_page_x_func,   // SRE $nn,X (illegal)
    [0x58] = cli_func,               // CLI
    [0x59] = eor_absolute_y_func,    // EOR $nnnn,Y
    [0x5A] = nop_func,               // NOP (illegal)
    [0x5B] = sre_absolute_y_func,    // SRE $nnnn,Y (illegal)
    [0x5C] = nop_absolute_x_func,    // NOP $nnnn,X (illegal)
    [0x5D] = eor_absolute_x_func,    // EOR $nnnn,X
    [0x5E] = lsr_absolute_x_func,    // LSR $nnnn,X
    [0x5F] = sre_absolute_x_func,    // SRE $nnnn,X (illegal)

    [0x60] = rts_func,               // RTS
    [0x61] = adc_indirect_x_func,    // ADC ($nn,X)
    [0x62] = jam_func,               // JAM (illegal)
    [0x63] = rra_indirect_x_func,    // RRA ($nn,X) (illegal)
    [0x64] = nop_zero_page_func,     // NOP $nn (illegal)
    [0x65] = adc_zero_page_func,     // ADC $nn
    [0x66] = ror_zero_page_func,     // ROR $nn
    [0x67] = rra_zero_page_func,     // RRA $nn (illegal)
    [0x68] = pla_func,               // PLA
    [0x69] = adc_immediate_func,     // ADC #$nn
    [0x6A] = ror_accumulator_func,   // ROR A
    [0x6B] = arr_immediate_func,     // ARR #$nn (illegal)
    [0x6C] = jmp_indirect_func,      // JMP ($nnnn)
    [0x6D] = adc_absolute_func,      // ADC $nnnn
    [0x6E] = ror_absolute_func,      // ROR $nnnn
    [0x6F] = rra_absolute_func,      // RRA $nnnn (illegal)

    [0x70] = bvs_func,               // BVS rel
    [0x71] = adc_indirect_y_func,    // ADC ($nn),Y
    [0x72] = jam_func,               // JAM (illegal)
    [0x73] = rra_indirect_y_func,    // RRA ($nn),Y (illegal)
    [0x74] = nop_zero_page_x_func,   // NOP $nn,X (illegal)
    [0x75] = adc_zero_page_x_func,   // ADC $nn,X
    [0x76] = ror_zero_page_x_func,   // ROR $nn,X
    [0x77] = rra_zero_page_x_func,   // RRA $nn,X (illegal)
    [0x78] = sei_func,               // SEI
    [0x79] = adc_absolute_y_func,    // ADC $nnnn,Y
    [0x7A] = nop_func,               // NOP (illegal)
    [0x7B] = rra_absolute_y_func,    // RRA $nnnn,Y (illegal)
    [0x7C] = nop_absolute_x_func,    // NOP $nnnn,X (illegal)
    [0x7D] = adc_absolute_x_func,    // ADC $nnnn,X
    [0x7E] = ror_absolute_x_func,    // ROR $nnnn,X
    [0x7F] = rra_absolute_x_func,    // RRA $nnnn,X (illegal)

    [0x80] = nop_immediate_func,     // NOP #$nn (illegal)
    [0x81] = sta_indirect_x_func,    // STA ($nn,X)
    [0x82] = nop_immediate_func,     // NOP #$nn (illegal)
    [0x83] = sax_indirect_x_func,    // SAX ($nn,X) (illegal)
    [0x84] = sty_zero_page_func,     // STY $nn
    [0x85] = sta_zero_page_func,     // STA $nn
    [0x86] = stx_zero_page_func,     // STX $nn
    [0x87] = sax_zero_page_func,     // SAX $nn (illegal)
    [0x88] = dey_func,               // DEY
    [0x89] = nop_immediate_func,     // NOP #$nn (illegal)
    [0x8A] = txa_func,               // TXA
    [0x8B] = xaa_immediate_func,     // XAA #$nn (illegal)
    [0x8C] = sty_absolute_func,      // STY $nnnn
    [0x8D] = sta_absolute_func,      // STA $nnnn
    [0x8E] = stx_absolute_func,      // STX $nnnn
    [0x8F] = sax_absolute_func,      // SAX $nnnn (illegal)

    [0x90] = bcc_func,               // BCC rel
    [0x91] = sta_indirect_y_func,    // STA ($nn),Y
    [0x92] = jam_func,               // JAM (illegal)
    [0x93] = ahx_indirect_y_func,    // AHX ($nn),Y (illegal)
    [0x94] = sty_zero_page_x_func,   // STY $nn,X
    [0x95] = sta_zero_page_x_func,   // STA $nn,X
    [0x96] = stx_zero_page_y_func,   // STX $nn,Y
    [0x97] = sax_zero_page_y_func,   // SAX $nn,Y (illegal)
    [0x98] = tya_func,               // TYA
    [0x99] = sta_absolute_y_func,    // STA $nnnn,Y
    [0x9A] = txs_func,               // TXS
    [0x9B] = tas_absolute_y_func,    // TAS $nnnn,Y (illegal)
    [0x9C] = shy_absolute_x_func,    // SHY $nnnn,X (illegal)
    [0x9D] = sta_absolute_x_func,    // STA $nnnn,X
    [0x9E] = shx_absolute_y_func,    // SHX $nnnn,Y (illegal)
    [0x9F] = ahx_absolute_y_func,    // AHX $nnnn,Y (illegal)

    [0xA0] = ldy_immediate_func,     // LDY #$nn
    [0xA1] = lda_indirect_x_func,    // LDA ($nn,X)
    [0xA2] = ldx_immediate_func,     // LDX #$nn
    [0xA3] = lax_indirect_x_func,    // LAX ($nn,X) (illegal)
    [0xA4] = ldy_zero_page_func,     // LDY $nn
    [0xA5] = lda_zero_page_func,     // LDA $nn
    [0xA6] = ldx_zero_page_func,     // LDX $nn
    [0xA7] = lax_zero_page_func,     // LAX $nn (illegal)
    [0xA8] = tay_func,               // TAY
    [0xA9] = lda_immediate_func,     // LDA #$nn
    [0xAA] = tax_func,               // TAX
    [0xAB] = lax_immediate_func,     // LAX #$nn (illegal)
    [0xAC] = ldy_absolute_func,      // LDY $nnnn
    [0xAD] = lda_absolute_func,      // LDA $nnnn
    [0xAE] = ldx_absolute_func,      // LDX $nnnn
    [0xAF] = lax_absolute_func,      // LAX $nnnn (illegal)

    [0xB0] = bcs_func,               // BCS rel
    [0xB1] = lda_indirect_y_func,    // LDA ($nn),Y
    [0xB2] = jam_func,               // JAM (illegal)
    [0xB3] = lax_indirect_y_func,    // LAX ($nn),Y (illegal)
    [0xB4] = ldy_zero_page_x_func,   // LDY $nn,X
    [0xB5] = lda_zero_page_x_func,   // LDA $nn,X
    [0xB6] = ldx_zero_page_y_func,   // LDX $nn,Y
    [0xB7] = lax_zero_page_y_func,   // LAX $nn,Y (illegal)
    [0xB8] = clv_func,               // CLV
    [0xB9] = lda_absolute_y_func,    // LDA $nnnn,Y
    [0xBA] = tsx_func,               // TSX
    [0xBB] = las_absolute_y_func,    // LAS $nnnn,Y (illegal)
    [0xBC] = ldy_absolute_x_func,    // LDY $nnnn,X
    [0xBD] = lda_absolute_x_func,    // LDA $nnnn,X
    [0xBE] = ldx_absolute_y_func,    // LDX $nnnn,Y
    [0xBF] = lax_absolute_y_func,    // LAX $nnnn,Y (illegal)

    [0xC0] = cpy_immediate_func,     // CPY #$nn
    [0xC1] = cmp_indirect_x_func,    // CMP ($nn,X)
    [0xC2] = nop_immediate_func,     // NOP #$nn (illegal)
    [0xC3] = dcp_indirect_x_func,    // DCP ($nn,X) (illegal)
    [0xC4] = cpy_zero_page_func,     // CPY $nn
    [0xC5] = cmp_zero_page_func,     // CMP $nn
    [0xC6] = dec_zero_page_func,     // DEC $nn
    [0xC7] = dcp_zero_page_func,     // DCP $nn (illegal)
    [0xC8] = iny_func,               // INY
    [0xC9] = cmp_immediate_func,     // CMP #$nn
    [0xCA] = dex_func,               // DEX
    [0xCB] = axs_immediate_func,     // AXS #$nn (illegal)
    [0xCC] = cpy_absolute_func,      // CPY $nnnn
    [0xCD] = cmp_absolute_func,      // CMP $nnnn
    [0xCE] = dec_absolute_func,      // DEC $nnnn
    [0xCF] = dcp_absolute_func,      // DCP $nnnn (illegal)

    [0xD0] = bne_func,               // BNE rel
    [0xD1] = cmp_indirect_y_func,    // CMP ($nn),Y
    [0xD2] = jam_func,               // JAM (illegal)
    [0xD3] = dcp_indirect_y_func,    // DCP ($nn),Y (illegal)
    [0xD4] = nop_zero_page_x_func,   // NOP $nn,X (illegal)
    [0xD5] = cmp_zero_page_x_func,   // CMP $nn,X
    [0xD6] = dec_zero_page_x_func,   // DEC $nn,X
    [0xD7] = dcp_zero_page_x_func,   // DCP $nn,X (illegal)
    [0xD8] = cld_func,               // CLD
    [0xD9] = cmp_absolute_y_func,    // CMP $nnnn,Y
    [0xDA] = nop_func,               // NOP (illegal)
    [0xDB] = dcp_absolute_y_func,    // DCP $nnnn,Y (illegal)
    [0xDC] = nop_absolute_x_func,    // NOP $nnnn,X (illegal)
    [0xDD] = cmp_absolute_x_func,    // CMP $nnnn,X
    [0xDE] = dec_absolute_x_func,    // DEC $nnnn,X
    [0xDF] = dcp_absolute_x_func,    // DCP $nnnn,X (illegal)

    [0xE0] = cpx_immediate_func,     // CPX #$nn
    [0xE1] = sbc_indirect_x_func,    // SBC ($nn,X)
    [0xE2] = nop_immediate_func,     // NOP #$nn (illegal)
    [0xE3] = isc_indirect_x_func,    // ISC ($nn,X) (illegal)
    [0xE4] = cpx_zero_page_func,     // CPX $nn
    [0xE5] = sbc_zero_page_func,     // SBC $nn
    [0xE6] = inc_zero_page_func,     // INC $nn
    [0xE7] = isc_zero_page_func,     // ISC $nn (illegal)
    [0xE8] = inx_func,               // INX
    [0xE9] = sbc_immediate_func,     // SBC #$nn
    [0xEA] = nop_func,               // NOP
    [0xEB] = sbc_immediate_func,     // SBC #$nn (illegal)
    [0xEC] = cpx_absolute_func,      // CPX $nnnn
    [0xED] = sbc_absolute_func,      // SBC $nnnn
    [0xEE] = inc_absolute_func,      // INC $nnnn
    [0xEF] = isc_absolute_func,      // ISC $nnnn (illegal)

    [0xF0] = beq_func,               // BEQ rel
    [0xF1] = sbc_indirect_y_func,    // SBC ($nn),Y
    [0xF2] = jam_func,               // JAM (illegal)
    [0xF3] = isc_indirect_y_func,    // ISC ($nn),Y (illegal)
    [0xF4] = nop_zero_page_x_func,   // NOP $nn,X (illegal)
    [0xF5] = sbc_zero_page_x_func,   // SBC $nn,X
    [0xF6] = inc_zero_page_x_func,   // INC $nn,X
    [0xF7] = isc_zero_page_x_func,   // ISC $nn,X (illegal)
    [0xF8] = sed_func,               // SED
    [0xF9] = sbc_absolute_y_func,    // SBC $nnnn,Y
    [0xFA] = nop_func,               // NOP (illegal)
    [0xFB] = isc_absolute_y_func,    // ISC $nnnn,Y (illegal)
    [0xFC] = nop_absolute_x_func,    // NOP $nnnn,X (illegal)
    [0xFD] = sbc_absolute_x_func,    // SBC $nnnn,X
    [0xFE] = inc_absolute_x_func,    // INC $nnnn,X
    [0xFF] = isc_absolute_x_func,    // ISC $nnnn,X (illegal)
};
