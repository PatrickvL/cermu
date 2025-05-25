#include "cpu.h"
#include "bus.h"
#include "memory.h"

// ============================================================================
// CPU STATE - 6502 registers and state
// ============================================================================
uint16_t cpu_pc;    // Program counter
uint8_t cpu_a;      // Accumulator
uint8_t cpu_sp, cpu_p; // Other registers (cpu_x, cpu_y reserved for future use)
uint8_t opcode;
uint16_t addr_temp;

// ============================================================================
// CPU INSTRUCTION HANDLERS - Direct threading dispatch for maximum performance
// ============================================================================
static const void* instruction_table[256];

// CPU execution loop with direct threading
void cpu_execute(void) {
    // Initialize instruction table with label addresses (must be done inside function)
    for (int i = 0; i < 256; i++) {
        instruction_table[i] = &&illegal_instruction;
    }
    
    // Map actual instructions
    instruction_table[0xA9] = &&lda_immediate;
    instruction_table[0xAD] = &&lda_absolute;
    instruction_table[0x8D] = &&sta_absolute;
    instruction_table[0x4C] = &&jmp_absolute;
    instruction_table[0x00] = &&brk_instruction;
    instruction_table[0xEA] = &&nop_instruction;

    // Start execution
    NEXT_INSTRUCTION(main_fetch_wait);

    // Sample instruction implementations showing the pattern
    lda_immediate:
        WAIT_READY_THEN_READ(cpu_pc++, lda_imm_wait);
        cpu_a = bus_state.data;
        // Set flags (abbreviated)
        cpu_p = (cpu_p & 0x7D) | (cpu_a ? 0 : 0x02) | (cpu_a & 0x80);
        NEXT_INSTRUCTION(lda_imm_fetch_wait);

    lda_absolute:
        WAIT_READY_THEN_READ(cpu_pc++, lda_abs_wait1);
        addr_temp = bus_state.data;
        
        WAIT_READY_THEN_READ(cpu_pc++, lda_abs_wait2);
        addr_temp |= bus_state.data << 8;
        
        WAIT_READY_THEN_READ(addr_temp, lda_abs_wait3);
        cpu_a = bus_state.data;
        cpu_p = (cpu_p & 0x7D) | (cpu_a ? 0 : 0x02) | (cpu_a & 0x80);
        NEXT_INSTRUCTION(lda_abs_fetch_wait);

    sta_absolute:
        WAIT_READY_THEN_READ(cpu_pc++, sta_abs_wait1);
        addr_temp = bus_state.data;
        
        WAIT_READY_THEN_READ(cpu_pc++, sta_abs_wait2);
        addr_temp |= bus_state.data << 8;
        
        // Wait for ready then write
        sta_abs_wait3:
        if (!CPU_READY()) { bus_cycle(); goto sta_abs_wait3; }
        cpu_write_cycle(addr_temp, cpu_a);
        NEXT_INSTRUCTION(sta_abs_fetch_wait);

    jmp_absolute:
        WAIT_READY_THEN_READ(cpu_pc++, jmp_abs_wait1);
        addr_temp = bus_state.data;
        
        WAIT_READY_THEN_READ(cpu_pc++, jmp_abs_wait2);
        addr_temp |= bus_state.data << 8;
        
        cpu_pc = addr_temp;
        NEXT_INSTRUCTION(jmp_abs_fetch_wait);

    nop_instruction:
        NEXT_INSTRUCTION(nop_fetch_wait);

    brk_instruction:
        // BRK implementation (abbreviated - needs full 7 cycle sequence)
        cpu_pc++; // Skip signature byte
        // Push PC high, PC low, status (3 cycles)
        // Fetch IRQ vector (2 cycles)
        // Set interrupt flag (1 cycle)
        NEXT_INSTRUCTION(brk_fetch_wait);

    illegal_instruction:
        // Handle illegal opcodes
        NEXT_INSTRUCTION(illegal_fetch_wait);

    handle_interrupt:
        // Interrupt handling logic
        if (bus_state.control_lines & NMI_LINE) {
            // Handle NMI
        } else if (bus_state.control_lines & IRQ_LINE) {
            // Handle IRQ
        }
        NEXT_INSTRUCTION(interrupt_fetch_wait);
}