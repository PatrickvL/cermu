#include "cpu6510.h"
#include <string.h>

// Include all instruction implementation files
#include "cpu6510_arithmetic.c"
#include "cpu6510_branches.c"
#include "cpu6510_flags.c"
#include "cpu6510_increment.c"
#include "cpu6510_loads_stores.c"
#include "cpu6510_shifts.c"
#include "cpu6510_stack.c"
#include "cpu6510_system.c"
#include "cpu6510_unofficial.c"

// Universal instruction table using function pointers
// Global instruction table
instruction_func_t instruction_table[256];

// ============================================================================
// CPU OPERATION FUNCTIONS (inline for performance)
// ============================================================================

// Compute effective port output: outputs from Data when DDR=1, else external bus data
static inline uint8_t cpu_io_mask(cpu6510_state_t* cpu_dev, uint8_t value)
{
    uint8_t ddr = cpu_dev->io_port[0];
    uint8_t data = cpu_dev->io_port[1];
    return (data & ddr) | (value & ~ddr);
}

// Current active map, updated by switch_cpu_mode()
device_callbacks_t* chip_select_map;

// Optimized read cycle implementation with embedded I/O port handling
void cpu_read_cycle(cpu6510_state_t* cpu_dev, uint16_t addr) {
    // Handle I/O ports directly in CPU - addresses $0000 and $0001
    if (addr <= 1) {
        if (addr == 0) {
            // Return Data Direction Register
            cpu_dev->data = cpu_dev->io_port[0];
        } else {
            // Return port: outputs defined by DDR bits, inputs from bus data
            cpu_dev->data = cpu_io_mask(cpu_dev, cpu_dev->bus->data);
        }
        // Note : MOS6510 I/O port accesses do not update bus address and data lines!
        c64_non_cpu_cycles();
        return;
    }
    
    // For all other addresses, use chip select map
    cpu_dev->bus->address = addr;
    c64_non_cpu_cycles();
    //cpu_dev->data = bus_read_cycle(cpu_dev->bus, addr);
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    uint8_t data = cb->read(cb->read_device, addr);
    cpu_dev->bus->data = data;
    cpu_dev->data = data;
}

// Optimized write cycle implementation with embedded I/O port handling
void cpu_write_cycle(cpu6510_state_t* cpu_dev, uint16_t addr, uint8_t value) {
    // Handle I/O ports directly in CPU - addresses $0000 and $0001
    if (addr <= 1) {
        // Update Data Direction / Data register
        cpu_dev->io_port[addr] = value;        
        // If writing to port data register, update CPU mode
        if (addr == 1) {
            uint8_t port_out = cpu_io_mask(cpu_dev, value);
            switch_cpu_mode(port_out);
        }        
        // Note : MOS6510 I/O port accesses do not update bus address and data lines!
        c64_non_cpu_cycles();
        return;
    }
    
    // For all other addresses, use chip select map
    //bus_write_cycle(cpu_dev->bus, addr, value);    
    cpu_dev->bus->address = addr;
    cpu_dev->bus->data = value;
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    cb->write(cb->write_device, addr, value);
    c64_non_cpu_cycles();
}

// CPU lifecycle wrapper functions
static void cpu_init(struct device_s* dev) {
    cpu6510_state_t* cpu_dev = (cpu6510_state_t*)dev;
    // Initialize CPU registers and state
    cpu_dev->a = 0;
    cpu_dev->x = 0;
    cpu_dev->y = 0;
    cpu_dev->sp = 0xFF;
    cpu_dev->p = FLAG_U | FLAG_I;  // Unused flag always set, interrupt disable
    cpu_dev->pc = 0;
    
    // 6510-specific I/O port (addresses $0000/$0001) initialization
    cpu_dev->io_port[0x0000] = 0x2F; // Default Data Direction Register (DDR at $0000)
    cpu_dev->io_port[0x0001] = 0x37;  // Default I/O Port Data (at $0001)
 
    // Setup instruction table
    cpu6510_setup_opcode_table();
}

static void cpu_cycle(struct device_s* dev) {
    cpu6510_state_t* cpu_dev = (cpu6510_state_t*)dev;
    cpu6510_step(cpu_dev);
}

// Forward declaration of descriptor
static const device_t cpu_device_descriptor;

// Initialize CPU
void cpu6510_init(cpu6510_state_t* cpu_dev) {
    // Set up device callbacks from descriptor pointer
    cpu_dev->device = &cpu_device_descriptor;
}

// Attach bus to CPU
void cpu_attach_bus(cpu6510_state_t* cpu_dev, bus_state_t* bus_state) {
    cpu_dev->bus = bus_state;
}


// Reset CPU
void cpu6510_reset(cpu6510_state_t* cpu_dev) {
    // Read reset vector from $FFFC/$FFFD
    cpu_read_cycle(cpu_dev, 0xFFFC);
    uint8_t pcl = cpu_dev->data;
    cpu_read_cycle(cpu_dev, 0xFFFD);
    uint8_t pch = cpu_dev->data;
    
    cpu_dev->pc = (pch << 8) | pcl;
    cpu_dev->sp = 0xFF; // or 0FD?
    cpu_dev->p |= FLAG_I;  // Set interrupt disable
}

bool cpu6510_step(cpu6510_state_t* cpu_dev) {
    (void)cpu_dev; // Currently unused
    c64_non_cpu_cycles();
    // Return true when instruction completes (for testing)
    return true;
}

void cpu6510_irq(cpu6510_state_t* cpu_dev, uint8_t status) {
    // Push PC and status to stack, set interrupt disable, jump to IRQ vector
    // Push PC high byte
    cpu_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF);
    // Push PC low byte
    cpu_push(cpu_dev, cpu_dev->pc & 0xFF);
    // Push status argument byte
    cpu_push(cpu_dev, status); 
    // Set interrupt disable
    cpu_dev->p |= FLAG_I;
    // Read IRQ vector
    cpu_read_cycle(cpu_dev, 0xFFFE);
    cpu_dev->pc = cpu_dev->data;
    cpu_read_cycle(cpu_dev, 0xFFFF);
    cpu_dev->pc |= (cpu_dev->data << 8);
}

void cpu6510_nmi(cpu6510_state_t* cpu_dev) {
    // Push PC and status to stack, set interrupt disable, jump to NMI vector
    cpu_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF);
    cpu_push(cpu_dev, cpu_dev->pc & 0xFF);
    cpu_push(cpu_dev, cpu_dev->p & ~FLAG_B);  // Clear B flag for NMI
    cpu_dev->p |= FLAG_I;
    cpu_read_cycle(cpu_dev, 0xFFFA);
    cpu_dev->pc = cpu_dev->data;
    cpu_read_cycle(cpu_dev, 0xFFFB);
    cpu_dev->pc |= (cpu_dev->data << 8);
}

// ============================================================================
// INSTRUCTION FUNCTIONS (Universal approach using function pointers)
// ============================================================================

// Basic instruction functions that are not in separate files
void nop_instruction_func(cpu6510_state_t* cpu_dev) {
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc, nop_wait);  // Dummy read
    NEXT_INSTRUCTION(cpu_dev, nop_fetch_wait);
}

void brk_instruction_func(cpu6510_state_t* cpu_dev) {
    cpu_dev->pc++;  // Skip BRK signature byte
    cpu6510_irq(cpu_dev, cpu_dev->p | FLAG_B); // Call IRQ handler
    NEXT_INSTRUCTION(cpu_dev, brk_fetch_wait);
}

// Interrupt handler - called when IRQ or NMI lines are active
void handle_interrupt_func(cpu6510_state_t* cpu_dev) {
    if (cpu_dev->bus->control_lines & NMI_LINE) {
        // Handle NMI - non-maskable
        cpu6510_nmi(cpu_dev);
    } else if ((cpu_dev->bus->control_lines & IRQ_LINE) && !cpu_get_flag(cpu_dev, FLAG_I)) {
        // Handle IRQ when interrupt disable is clear
        cpu6510_irq(cpu_dev, cpu_dev->p & ~FLAG_B); // Clear B flag for IRQ
    }
    NEXT_INSTRUCTION(cpu_dev, interrupt_fetch_wait);
}

// ============================================================================
// CPU EXECUTION LOOP WITH FUNCTION POINTERS (Universal)
// ============================================================================
void cpu6510_execute(cpu6510_state_t* cpu_dev) {
    // Start execution - fetch first instruction
    if (unlikely(cpu_dev->bus->control_lines & (IRQ_LINE | NMI_LINE))) {
        handle_interrupt_func(cpu_dev);
        return;
    }
    WAIT_READY_THEN_READ(cpu_dev, cpu_dev->pc++, main_fetch_start);
    instruction_table[cpu_dev->data](cpu_dev);
}

// ============================================================================
// INSTRUCTION TABLE SETUP
// ============================================================================
void cpu6510_setup_opcode_table(void) {
    instruction_table[0x00] = brk_instruction_func;    // BRK
    instruction_table[0x01] = ora_indirect_x_func;     // ORA ($nn,X)
    instruction_table[0x02] = jam_func;                // JAM (illegal)
    instruction_table[0x03] = slo_indirect_x_func;     // SLO ($nn,X)
    instruction_table[0x04] = nop_zero_page_func;      // NOP $nn (illegal)
    instruction_table[0x05] = ora_zero_page_func;      // ORA $nn
    instruction_table[0x06] = asl_zero_page_func;      // ASL $nn
    instruction_table[0x07] = slo_zero_page_func;      // SLO $nn (illegal)
    instruction_table[0x08] = php_func;                // PHP
    instruction_table[0x09] = ora_immediate_func;      // ORA #$nn
    instruction_table[0x0A] = asl_accumulator_func;    // ASL A
    instruction_table[0x0B] = anc_immediate_func;      // ANC #$nn (illegal)
    instruction_table[0x0C] = nop_absolute_func;       // NOP $nnnn (illegal)
    instruction_table[0x0D] = ora_absolute_func;       // ORA $nnnn
    instruction_table[0x0E] = asl_absolute_func;       // ASL $nnnn
    instruction_table[0x0F] = slo_absolute_func;       // SLO $nnnn (illegal)

    instruction_table[0x10] = bpl_func;                // BPL rel
    instruction_table[0x11] = ora_indirect_y_func;     // ORA ($nn),Y
    instruction_table[0x12] = jam_func;                // JAM (illegal)
    instruction_table[0x13] = slo_indirect_y_func;     // SLO ($nn),Y (illegal)
    instruction_table[0x14] = nop_zero_page_x_func;    // NOP $nn,X (illegal)
    instruction_table[0x15] = ora_zero_page_x_func;    // ORA $nn,X
    instruction_table[0x16] = asl_zero_page_x_func;    // ASL $nn,X
    instruction_table[0x17] = slo_zero_page_x_func;    // SLO $nn,X (illegal)
    instruction_table[0x18] = clc_func;                // CLC
    instruction_table[0x19] = ora_absolute_y_func;     // ORA $nnnn,Y
    instruction_table[0x1A] = nop_func;                // NOP (illegal)
    instruction_table[0x1B] = slo_absolute_y_func;     // SLO $nnnn,Y (illegal)
    instruction_table[0x1C] = nop_absolute_x_func;     // NOP $nnnn,X (illegal)
    instruction_table[0x1D] = ora_absolute_x_func;     // ORA $nnnn,X
    instruction_table[0x1E] = asl_absolute_x_func;     // ASL $nnnn,X
    instruction_table[0x1F] = slo_absolute_x_func;     // SLO $nnnn,X (illegal)

    instruction_table[0x20] = jsr_func;                // JSR $nnnn
    instruction_table[0x21] = and_indirect_x_func;     // AND ($nn,X)
    instruction_table[0x22] = jam_func;                // JAM (illegal)
    instruction_table[0x23] = rla_indirect_x_func;     // RLA ($nn,X) (illegal)
    instruction_table[0x24] = bit_zero_page_func;      // BIT $nn
    instruction_table[0x25] = and_zero_page_func;      // AND $nn
    instruction_table[0x26] = rol_zero_page_func;      // ROL $nn
    instruction_table[0x27] = rla_zero_page_func;      // RLA $nn (illegal)
    instruction_table[0x28] = plp_func;                // PLP
    instruction_table[0x29] = and_immediate_func;      // AND #$nn
    instruction_table[0x2A] = rol_accumulator_func;    // ROL A
    instruction_table[0x2B] = anc_immediate_func;      // ANC #$nn (illegal)
    instruction_table[0x2C] = bit_absolute_func;       // BIT $nnnn
    instruction_table[0x2D] = and_absolute_func;       // AND $nnnn
    instruction_table[0x2E] = rol_absolute_func;       // ROL $nnnn
    instruction_table[0x2F] = rla_absolute_func;       // RLA $nnnn (illegal)

    instruction_table[0x30] = bmi_func;                // BMI rel
    instruction_table[0x31] = and_indirect_y_func;     // AND ($nn),Y
    instruction_table[0x32] = jam_func;                // JAM (illegal)
    instruction_table[0x33] = rla_indirect_y_func;     // RLA ($nn),Y (illegal)
    instruction_table[0x34] = nop_zero_page_x_func;    // NOP $nn,X (illegal)
    instruction_table[0x35] = and_zero_page_x_func;    // AND $nn,X
    instruction_table[0x36] = rol_zero_page_x_func;    // ROL $nn,X
    instruction_table[0x37] = rla_zero_page_x_func;    // RLA $nn,X (illegal)
    instruction_table[0x38] = sec_func;                // SEC
    instruction_table[0x39] = and_absolute_y_func;     // AND $nnnn,Y
    instruction_table[0x3A] = nop_func;                // NOP (illegal)
    instruction_table[0x3B] = rla_absolute_y_func;     // RLA $nnnn,Y (illegal)
    instruction_table[0x3C] = nop_absolute_x_func;     // NOP $nnnn,X (illegal)
    instruction_table[0x3D] = and_absolute_x_func;     // AND $nnnn,X
    instruction_table[0x3E] = rol_absolute_x_func;     // ROL $nnnn,X
    instruction_table[0x3F] = rla_absolute_x_func;     // RLA $nnnn,X (illegal)

    instruction_table[0x40] = rti_func;                // RTI
    instruction_table[0x41] = eor_indirect_x_func;     // EOR ($nn,X)
    instruction_table[0x42] = jam_func;                // JAM (illegal)
    instruction_table[0x43] = sre_indirect_x_func;     // SRE ($nn,X) (illegal)
    instruction_table[0x44] = nop_zero_page_func;      // NOP $nn (illegal)
    instruction_table[0x45] = eor_zero_page_func;      // EOR $nn
    instruction_table[0x46] = lsr_zero_page_func;      // LSR $nn
    instruction_table[0x47] = sre_zero_page_func;      // SRE $nn (illegal)
    instruction_table[0x48] = pha_func;                // PHA
    instruction_table[0x49] = eor_immediate_func;      // EOR #$nn
    instruction_table[0x4A] = lsr_accumulator_func;    // LSR A
    instruction_table[0x4B] = alr_immediate_func;      // ALR #$nn (illegal)
    instruction_table[0x4C] = jmp_absolute_func;       // JMP $nnnn
    instruction_table[0x4D] = eor_absolute_func;       // EOR $nnnn
    instruction_table[0x4E] = lsr_absolute_func;       // LSR $nnnn
    instruction_table[0x4F] = sre_absolute_func;       // SRE $nnnn (illegal)

    instruction_table[0x50] = bvc_func;                // BVC rel
    instruction_table[0x51] = eor_indirect_y_func;     // EOR ($nn),Y
    instruction_table[0x52] = jam_func;                // JAM (illegal)
    instruction_table[0x53] = sre_indirect_y_func;     // SRE ($nn),Y (illegal)
    instruction_table[0x54] = nop_zero_page_x_func;    // NOP $nn,X (illegal)
    instruction_table[0x55] = eor_zero_page_x_func;    // EOR $nn,X
    instruction_table[0x56] = lsr_zero_page_x_func;    // LSR $nn,X
    instruction_table[0x57] = sre_zero_page_x_func;    // SRE $nn,X (illegal)
    instruction_table[0x58] = cli_func;                // CLI
    instruction_table[0x59] = eor_absolute_y_func;     // EOR $nnnn,Y
    instruction_table[0x5A] = nop_func;                // NOP (illegal)
    instruction_table[0x5B] = sre_absolute_y_func;     // SRE $nnnn,Y (illegal)
    instruction_table[0x5C] = nop_absolute_x_func;     // NOP $nnnn,X (illegal)
    instruction_table[0x5D] = eor_absolute_x_func;     // EOR $nnnn,X
    instruction_table[0x5E] = lsr_absolute_x_func;     // LSR $nnnn,X
    instruction_table[0x5F] = sre_absolute_x_func;     // SRE $nnnn,X (illegal)

    instruction_table[0x60] = rts_func;                // RTS
    instruction_table[0x61] = adc_indirect_x_func;     // ADC ($nn,X)
    instruction_table[0x62] = jam_func;                // JAM (illegal)
    instruction_table[0x63] = rra_indirect_x_func;     // RRA ($nn,X) (illegal)
    instruction_table[0x64] = nop_zero_page_func;      // NOP $nn (illegal)
    instruction_table[0x65] = adc_zero_page_func;      // ADC $nn
    instruction_table[0x66] = ror_zero_page_func;      // ROR $nn
    instruction_table[0x67] = rra_zero_page_func;      // RRA $nn (illegal)
    instruction_table[0x68] = pla_func;                // PLA
    instruction_table[0x69] = adc_immediate_func;      // ADC #$nn
    instruction_table[0x6A] = ror_accumulator_func;    // ROR A
    instruction_table[0x6B] = arr_immediate_func;      // ARR #$nn (illegal)
    instruction_table[0x6C] = jmp_indirect_func;       // JMP ($nnnn)
    instruction_table[0x6D] = adc_absolute_func;       // ADC $nnnn
    instruction_table[0x6E] = ror_absolute_func;       // ROR $nnnn
    instruction_table[0x6F] = rra_absolute_func;       // RRA $nnnn (illegal)

    instruction_table[0x70] = bvs_func;                // BVS rel
    instruction_table[0x71] = adc_indirect_y_func;     // ADC ($nn),Y
    instruction_table[0x72] = jam_func;                // JAM (illegal)
    instruction_table[0x73] = rra_indirect_y_func;     // RRA ($nn),Y (illegal)
    instruction_table[0x74] = nop_zero_page_x_func;    // NOP $nn,X (illegal)
    instruction_table[0x75] = adc_zero_page_x_func;    // ADC $nn,X
    instruction_table[0x76] = ror_zero_page_x_func;    // ROR $nn,X
    instruction_table[0x77] = rra_zero_page_x_func;    // RRA $nn,X (illegal)
    instruction_table[0x78] = sei_func;                // SEI
    instruction_table[0x79] = adc_absolute_y_func;     // ADC $nnnn,Y
    instruction_table[0x7A] = nop_func;                // NOP (illegal)
    instruction_table[0x7B] = rra_absolute_y_func;     // RRA $nnnn,Y (illegal)
    instruction_table[0x7C] = nop_absolute_x_func;     // NOP $nnnn,X (illegal)
    instruction_table[0x7D] = adc_absolute_x_func;     // ADC $nnnn,X
    instruction_table[0x7E] = ror_absolute_x_func;     // ROR $nnnn,X
    instruction_table[0x7F] = rra_absolute_x_func;     // RRA $nnnn,X (illegal)

    instruction_table[0x80] = nop_immediate_func;      // NOP #$nn (illegal)
    instruction_table[0x81] = sta_indirect_x_func;     // STA ($nn,X)
    instruction_table[0x82] = nop_immediate_func;      // NOP #$nn (illegal)
    instruction_table[0x83] = sax_indirect_x_func;     // SAX ($nn,X) (illegal)
    instruction_table[0x84] = sty_zero_page_func;      // STY $nn
    instruction_table[0x85] = sta_zero_page_func;      // STA $nn
    instruction_table[0x86] = stx_zero_page_func;      // STX $nn
    instruction_table[0x87] = sax_zero_page_func;      // SAX $nn (illegal)
    instruction_table[0x88] = dey_func;                // DEY
    instruction_table[0x89] = nop_immediate_func;      // NOP #$nn (illegal)
    instruction_table[0x8A] = txa_func;                // TXA
    instruction_table[0x8B] = xaa_immediate_func;      // XAA #$nn (illegal)
    instruction_table[0x8C] = sty_absolute_func;       // STY $nnnn
    instruction_table[0x8D] = sta_absolute_func;       // STA $nnnn
    instruction_table[0x8E] = stx_absolute_func;       // STX $nnnn
    instruction_table[0x8F] = sax_absolute_func;       // SAX $nnnn (illegal)

    instruction_table[0x90] = bcc_func;                // BCC rel
    instruction_table[0x91] = sta_indirect_y_func;     // STA ($nn),Y
    instruction_table[0x92] = jam_func;                // JAM (illegal)
    instruction_table[0x93] = ahx_indirect_y_func;     // AHX ($nn),Y (illegal)
    instruction_table[0x94] = sty_zero_page_x_func;    // STY $nn,X
    instruction_table[0x95] = sta_zero_page_x_func;    // STA $nn,X
    instruction_table[0x96] = stx_zero_page_y_func;    // STX $nn,Y
    instruction_table[0x97] = sax_zero_page_y_func;    // SAX $nn,Y (illegal)
    instruction_table[0x98] = tya_func;                // TYA
    instruction_table[0x99] = sta_absolute_y_func;     // STA $nnnn,Y
    instruction_table[0x9A] = txs_func;                // TXS
    instruction_table[0x9B] = tas_absolute_y_func;     // TAS $nnnn,Y (illegal)
    instruction_table[0x9C] = shy_absolute_x_func;     // SHY $nnnn,X (illegal)
    instruction_table[0x9D] = sta_absolute_x_func;     // STA $nnnn,X
    instruction_table[0x9E] = shx_absolute_y_func;     // SHX $nnnn,Y (illegal)
    instruction_table[0x9F] = ahx_absolute_y_func;     // AHX $nnnn,Y (illegal)

    instruction_table[0xA0] = ldy_immediate_func;      // LDY #$nn
    instruction_table[0xA1] = lda_indirect_x_func;     // LDA ($nn,X)
    instruction_table[0xA2] = ldx_immediate_func;      // LDX #$nn
    instruction_table[0xA3] = lax_indirect_x_func;     // LAX ($nn,X) (illegal)
    instruction_table[0xA4] = ldy_zero_page_func;      // LDY $nn
    instruction_table[0xA5] = lda_zero_page_func;      // LDA $nn
    instruction_table[0xA6] = ldx_zero_page_func;      // LDX $nn
    instruction_table[0xA7] = lax_zero_page_func;      // LAX $nn (illegal)
    instruction_table[0xA8] = tay_func;                // TAY
    instruction_table[0xA9] = lda_immediate_func;      // LDA #$nn
    instruction_table[0xAA] = tax_func;                // TAX
    instruction_table[0xAB] = lax_immediate_func;      // LAX #$nn (illegal)
    instruction_table[0xAC] = ldy_absolute_func;       // LDY $nnnn
    instruction_table[0xAD] = lda_absolute_func;       // LDA $nnnn
    instruction_table[0xAE] = ldx_absolute_func;       // LDX $nnnn
    instruction_table[0xAF] = lax_absolute_func;       // LAX $nnnn (illegal)

    instruction_table[0xB0] = bcs_func;                // BCS rel
    instruction_table[0xB1] = lda_indirect_y_func;     // LDA ($nn),Y
    instruction_table[0xB2] = jam_func;                // JAM (illegal)
    instruction_table[0xB3] = lax_indirect_y_func;     // LAX ($nn),Y (illegal)
    instruction_table[0xB4] = ldy_zero_page_x_func;    // LDY $nn,X
    instruction_table[0xB5] = lda_zero_page_x_func;    // LDA $nn,X
    instruction_table[0xB6] = ldx_zero_page_y_func;    // LDX $nn,Y
    instruction_table[0xB7] = lax_zero_page_y_func;    // LAX $nn,Y (illegal)
    instruction_table[0xB8] = clv_func;                // CLV
    instruction_table[0xB9] = lda_absolute_y_func;     // LDA $nnnn,Y
    instruction_table[0xBA] = tsx_func;                // TSX
    instruction_table[0xBB] = las_absolute_y_func;     // LAS $nnnn,Y (illegal)
    instruction_table[0xBC] = ldy_absolute_x_func;     // LDY $nnnn,X
    instruction_table[0xBD] = lda_absolute_x_func;     // LDA $nnnn,X
    instruction_table[0xBE] = ldx_absolute_y_func;     // LDX $nnnn,Y
    instruction_table[0xBF] = lax_absolute_y_func;     // LAX $nnnn,Y (illegal)

    instruction_table[0xC0] = cpy_immediate_func;      // CPY #$nn
    instruction_table[0xC1] = cmp_indirect_x_func;     // CMP ($nn,X)
    instruction_table[0xC2] = nop_immediate_func;      // NOP #$nn (illegal)
    instruction_table[0xC3] = dcp_indirect_x_func;     // DCP ($nn,X) (illegal)
    instruction_table[0xC4] = cpy_zero_page_func;      // CPY $nn
    instruction_table[0xC5] = cmp_zero_page_func;      // CMP $nn
    instruction_table[0xC6] = dec_zero_page_func;      // DEC $nn
    instruction_table[0xC7] = dcp_zero_page_func;      // DCP $nn (illegal)
    instruction_table[0xC8] = iny_func;                // INY
    instruction_table[0xC9] = cmp_immediate_func;      // CMP #$nn
    instruction_table[0xCA] = dex_func;                // DEX
    instruction_table[0xCB] = axs_immediate_func;      // AXS #$nn (illegal)
    instruction_table[0xCC] = cpy_absolute_func;       // CPY $nnnn
    instruction_table[0xCD] = cmp_absolute_func;       // CMP $nnnn
    instruction_table[0xCE] = dec_absolute_func;       // DEC $nnnn
    instruction_table[0xCF] = dcp_absolute_func;       // DCP $nnnn (illegal)

    instruction_table[0xD0] = bne_func;                // BNE rel
    instruction_table[0xD1] = cmp_indirect_y_func;     // CMP ($nn),Y
    instruction_table[0xD2] = jam_func;                // JAM (illegal)
    instruction_table[0xD3] = dcp_indirect_y_func;     // DCP ($nn),Y (illegal)
    instruction_table[0xD4] = nop_zero_page_x_func;    // NOP $nn,X (illegal)
    instruction_table[0xD5] = cmp_zero_page_x_func;    // CMP $nn,X
    instruction_table[0xD6] = dec_zero_page_x_func;    // DEC $nn,X
    instruction_table[0xD7] = dcp_zero_page_x_func;    // DCP $nn,X (illegal)
    instruction_table[0xD8] = cld_func;                // CLD
    instruction_table[0xD9] = cmp_absolute_y_func;     // CMP $nnnn,Y
    instruction_table[0xDA] = nop_func;                // NOP (illegal)
    instruction_table[0xDB] = dcp_absolute_y_func;     // DCP $nnnn,Y (illegal)
    instruction_table[0xDC] = nop_absolute_x_func;     // NOP $nnnn,X (illegal)
    instruction_table[0xDD] = cmp_absolute_x_func;     // CMP $nnnn,X
    instruction_table[0xDE] = dec_absolute_x_func;     // DEC $nnnn,X
    instruction_table[0xDF] = dcp_absolute_x_func;     // DCP $nnnn,X (illegal)

    instruction_table[0xE0] = cpx_immediate_func;      // CPX #$nn
    instruction_table[0xE1] = sbc_indirect_x_func;     // SBC ($nn,X)
    instruction_table[0xE2] = nop_immediate_func;      // NOP #$nn (illegal)
    instruction_table[0xE3] = isc_indirect_x_func;     // ISC ($nn,X) (illegal)
    instruction_table[0xE4] = cpx_zero_page_func;      // CPX $nn
    instruction_table[0xE5] = sbc_zero_page_func;      // SBC $nn
    instruction_table[0xE6] = inc_zero_page_func;      // INC $nn
    instruction_table[0xE7] = isc_zero_page_func;      // ISC $nn (illegal)
    instruction_table[0xE8] = inx_func;                // INX
    instruction_table[0xE9] = sbc_immediate_func;      // SBC #$nn
    instruction_table[0xEA] = nop_instruction_func;    // NOP
    instruction_table[0xEB] = sbc_immediate_func;      // SBC #$nn (illegal)
    instruction_table[0xEC] = cpx_absolute_func;       // CPX $nnnn
    instruction_table[0xED] = sbc_absolute_func;       // SBC $nnnn
    instruction_table[0xEE] = inc_absolute_func;       // INC $nnnn
    instruction_table[0xEF] = isc_absolute_func;       // ISC $nnnn (illegal)

    instruction_table[0xF0] = beq_func;                // BEQ rel
    instruction_table[0xF1] = sbc_indirect_y_func;     // SBC ($nn),Y
    instruction_table[0xF2] = jam_func;                // JAM (illegal)
    instruction_table[0xF3] = isc_indirect_y_func;     // ISC ($nn),Y (illegal)
    instruction_table[0xF4] = nop_zero_page_x_func;    // NOP $nn,X (illegal)
    instruction_table[0xF5] = sbc_zero_page_x_func;    // SBC $nn,X
    instruction_table[0xF6] = inc_zero_page_x_func;    // INC $nn,X
    instruction_table[0xF7] = isc_zero_page_x_func;    // ISC $nn,X (illegal)
    instruction_table[0xF8] = sed_func;                // SED
    instruction_table[0xF9] = sbc_absolute_y_func;     // SBC $nnnn,Y
    instruction_table[0xFA] = nop_func;                // NOP (illegal)
    instruction_table[0xFB] = isc_absolute_y_func;     // ISC $nnnn,Y (illegal)
    instruction_table[0xFC] = nop_absolute_x_func;     // NOP $nnnn,X (illegal)
    instruction_table[0xFD] = sbc_absolute_x_func;     // SBC $nnnn,X
    instruction_table[0xFE] = inc_absolute_x_func;     // INC $nnnn,X
    instruction_table[0xFF] = isc_absolute_x_func;     // ISC $nnnn,X (illegal)
}

// Static device descriptor for CPU6510
static const device_t cpu_device_descriptor = {
    .r8 = NULL,  // I/O ports handled directly in cpu_read_cycle
    .w8 = NULL,  // I/O ports handled directly in cpu_write_cycle
    .init = cpu_init,
    .cycle = cpu_cycle,
    .cleanup = NULL // CPU has no cleanup needed
};