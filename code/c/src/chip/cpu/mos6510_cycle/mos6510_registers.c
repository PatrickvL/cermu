#include "mos6510_registers.h"
#include "mos6510_state.h"
#include "register_access.h"
#include "opcode_mapping.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

// Register properties table
// Defines characteristics of each register in the 16-register array
const register_properties_t register_properties[MOS6510_REGISTER_COUNT] = {
    // Architectural Registers (0-6)
    [REG_A] = {
        .name = "A",
        .category = REG_CATEGORY_ARCHITECTURAL,
        .is_architectural = true,
        .is_internal_bus = false,
        .is_address_related = false,
        .is_alu_related = true,
        .reset_value = 0x00
    },
    [REG_X] = {
        .name = "X",
        .category = REG_CATEGORY_ARCHITECTURAL,
        .is_architectural = true,
        .is_internal_bus = false,
        .is_address_related = false,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_Y] = {
        .name = "Y", 
        .category = REG_CATEGORY_ARCHITECTURAL,
        .is_architectural = true,
        .is_internal_bus = false,
        .is_address_related = false,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_P] = {
        .name = "P",
        .category = REG_CATEGORY_ARCHITECTURAL,
        .is_architectural = true,
        .is_internal_bus = false,
        .is_address_related = false,
        .is_alu_related = true,
        .reset_value = FLAG_UNUSED | FLAG_I  // Unused=1, I=1 after reset
    },
    [REG_SP] = {
        .name = "SP",
        .category = REG_CATEGORY_ARCHITECTURAL,
        .is_architectural = true,
        .is_internal_bus = false,
        .is_address_related = true,
        .is_alu_related = false,
        .reset_value = 0xFF  // Stack pointer starts at top
    },
    [REG_PCL] = {
        .name = "PCL",
        .category = REG_CATEGORY_ARCHITECTURAL,
        .is_architectural = true,
        .is_internal_bus = false,
        .is_address_related = true,
        .is_alu_related = false,
        .reset_value = 0x00  // Will be loaded from reset vector
    },
    [REG_PCH] = {
        .name = "PCH",
        .category = REG_CATEGORY_ARCHITECTURAL,
        .is_architectural = true,
        .is_internal_bus = false,
        .is_address_related = true,
        .is_alu_related = false,
        .reset_value = 0x00  // Will be loaded from reset vector
    },
    
    // Visual6502 Internal Registers (7-15)
    [REG_DL] = {
        .name = "DL",
        .category = REG_CATEGORY_INTERNAL_DATA,
        .is_architectural = false,
        .is_internal_bus = true,
        .is_address_related = false,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_DOR] = {
        .name = "DOR",
        .category = REG_CATEGORY_INTERNAL_DATA,
        .is_architectural = false,
        .is_internal_bus = true,
        .is_address_related = false,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_SB] = {
        .name = "SB",
        .category = REG_CATEGORY_INTERNAL_DATA,
        .is_architectural = false,
        .is_internal_bus = true,
        .is_address_related = false,
        .is_alu_related = true,
        .reset_value = 0x00
    },
    [REG_ADL] = {
        .name = "ADL",
        .category = REG_CATEGORY_INTERNAL_ADDR,
        .is_architectural = false,
        .is_internal_bus = true,
        .is_address_related = true,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_ADH] = {
        .name = "ADH",
        .category = REG_CATEGORY_INTERNAL_ADDR,
        .is_architectural = false,
        .is_internal_bus = true,
        .is_address_related = true,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_ABL] = {
        .name = "ABL",
        .category = REG_CATEGORY_INTERNAL_ADDR,
        .is_architectural = false,
        .is_internal_bus = true,
        .is_address_related = true,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_ABH] = {
        .name = "ABH",
        .category = REG_CATEGORY_INTERNAL_ADDR,
        .is_architectural = false,
        .is_internal_bus = true,
        .is_address_related = true,
        .is_alu_related = false,
        .reset_value = 0x00
    },
    [REG_AC] = {
        .name = "AC",
        .category = REG_CATEGORY_INTERNAL_ALU,
        .is_architectural = false,
        .is_internal_bus = false,
        .is_address_related = false,
        .is_alu_related = true,
        .reset_value = 0x00
    },
    [REG_ADD] = {
        .name = "ADD",
        .category = REG_CATEGORY_INTERNAL_ALU,
        .is_architectural = false,
        .is_internal_bus = false,
        .is_address_related = false,
        .is_alu_related = true,
        .reset_value = 0x00
    }
};

// ===== SHARED OPERATION FUNCTION IMPLEMENTATIONS =====

// Generic load operation: target_reg = value, set N/Z flags
void shared_load_operation(mos6510_state_t *cpu, uint8_t target_reg, uint8_t value) {
    if (!cpu) return;
    VALIDATE_REG(target_reg);
    
    SET_CPU_REG(cpu, target_reg, value);
    set_cpu_nz_flags(cpu, value);
}

// Generic store operation: return source_reg value
uint8_t shared_store_operation(mos6510_state_t *cpu, uint8_t source_reg) {
    if (!cpu) return 0;
    VALIDATE_REG(source_reg);
    
    return CPU_REG(cpu, source_reg);
}

// Generic increment operation: target_reg++, set N/Z flags
void shared_increment_operation(mos6510_state_t *cpu, uint8_t target_reg) {
    if (!cpu) return;
    VALIDATE_REG(target_reg);
    
    uint8_t value = CPU_REG(cpu, target_reg) + 1;
    SET_CPU_REG(cpu, target_reg, value);
    set_cpu_nz_flags(cpu, value);
}

// Generic decrement operation: target_reg--, set N/Z flags
void shared_decrement_operation(mos6510_state_t *cpu, uint8_t target_reg) {
    if (!cpu) return;
    VALIDATE_REG(target_reg);
    
    uint8_t value = CPU_REG(cpu, target_reg) - 1;
    SET_CPU_REG(cpu, target_reg, value);
    set_cpu_nz_flags(cpu, value);
}

// Generic compare operation: compare target_reg with value, set N/Z/C flags
void shared_compare_operation(mos6510_state_t *cpu, uint8_t target_reg, uint8_t value) {
    if (!cpu) return;
    VALIDATE_REG(target_reg);
    
    uint16_t result = CPU_REG(cpu, target_reg) - value;
    set_cpu_nzc_flags_cmp(cpu, result);
}

// ===== REGISTER STATE DEBUGGING =====

// Generate human-readable register dump
void mos6510_registers_dump(const mos6510_state_t *cpu, char *buffer, size_t buffer_size) {
    if (!cpu || !buffer || buffer_size == 0) return;
    
    size_t pos = 0;
    
    // Architectural registers
    pos += snprintf(buffer + pos, buffer_size - pos, 
                   "A=%02X X=%02X Y=%02X P=%02X SP=%02X PC=%04X\n",
                   CPU_A(cpu), CPU_X(cpu), CPU_Y(cpu), CPU_P(cpu), 
                   CPU_SP(cpu), CPU_PC(cpu));
    
    if (pos >= buffer_size) return;
    
    // Processor flags
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "Flags: N=%d V=%d - B=%d D=%d I=%d Z=%d C=%d\n",
                   (CPU_P(cpu) & FLAG_N) ? 1 : 0,
                   (CPU_P(cpu) & FLAG_V) ? 1 : 0,
                   (CPU_P(cpu) & FLAG_B) ? 1 : 0,
                   (CPU_P(cpu) & FLAG_D) ? 1 : 0,
                   (CPU_P(cpu) & FLAG_I) ? 1 : 0,
                   (CPU_P(cpu) & FLAG_Z) ? 1 : 0,
                   (CPU_P(cpu) & FLAG_C) ? 1 : 0);
    
    if (pos >= buffer_size) return;
    
    // Internal registers
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "Internal: DL=%02X DOR=%02X SB=%02X\n",
                   CPU_DL(cpu), CPU_DOR(cpu), CPU_SB(cpu));
    
    if (pos >= buffer_size) return;
    
    // Address registers  
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "Address: ADL=%02X ADH=%02X ABL=%02X ABH=%02X\n",
                   CPU_ADL(cpu), CPU_ADH(cpu), CPU_ABL(cpu), CPU_ABH(cpu));
    
    if (pos >= buffer_size) return;
    
    // ALU registers
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "ALU: AC=%02X ADD=%02X\n",
                   CPU_AC(cpu), CPU_ADD(cpu));
}

// Validate register array state
bool mos6510_registers_validate(const mos6510_state_t *cpu) {
    if (!cpu) return false;
    
    // Check processor status register has unused bit set
    if (!(CPU_P(cpu) & FLAG_UNUSED)) {
        return false;  // Unused flag should always be 1
    }
    
    // Check stack pointer is in valid range (0x00-0xFF, but 0x100-0x1FF on actual stack)
    // No validation needed as it's uint8_t
    
    // Internal registers can have any value, so no specific validation
    
    return true;
}

// Initialize register array to reset values
void mos6510_registers_reset(mos6510_state_t *cpu) {
    if (!cpu) return;
    
    // Set all registers to their reset values
    for (int i = 0; i < MOS6510_REGISTER_COUNT; i++) {
        cpu->registers[i] = register_properties[i].reset_value;
    }
}

// Get register category for debugging/inspection
register_category_t mos6510_register_get_category(uint8_t reg) {
    if (!IS_VALID_REG(reg)) return REG_CATEGORY_ARCHITECTURAL;
    return register_properties[reg].category;
}