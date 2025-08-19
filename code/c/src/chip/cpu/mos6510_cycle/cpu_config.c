#include "cpu_config.h"
#include <string.h>

// Predefined configurations for standard 650x family variants

// MOS 6502 (Original) - No I/O ports, standard interrupt vectors
const cpu_config_t CPU_CONFIG_6502 = {
    .cpu_variant = CPU_6502,
    .has_io_port = false,
    .has_aec_pin = false,
    .address_lines = 16,
    .supports_illegal_ops = true,
    .enhanced_rdy = false,
    .base_frequency = 1000000,  // 1 MHz
    .variable_clock = false,
    .has_irq_pin = true,
    .has_nmi_pin = true,
    .has_reset_pin = true,
    .reset_vector = 0xFFFC,
    .irq_vector = 0xFFFE,
    .nmi_vector = 0xFFFA
};

// MOS 6507 (Atari 2600) - Reduced address space, no interrupt pins except reset
const cpu_config_t CPU_CONFIG_6507 = {
    .cpu_variant = CPU_6507,
    .has_io_port = false,
    .has_aec_pin = false,
    .address_lines = 13,        // A0-A12 only (8KB address space)
    .supports_illegal_ops = true,
    .enhanced_rdy = false,
    .base_frequency = 1193182,  // NTSC 2600 frequency
    .variable_clock = false,
    .has_irq_pin = false,       // No IRQ pin on 6507
    .has_nmi_pin = false,       // No NMI pin on 6507
    .has_reset_pin = true,
    .reset_vector = 0x1FFC,     // Vector in 13-bit address space
    .irq_vector = 0x1FFE,       // Not used but defined
    .nmi_vector = 0x1FFA        // Not used but defined
};

// MOS 6510 (Commodore 64) - 6-bit I/O port, AEC pin for VIC-II bus sharing
const cpu_config_t CPU_CONFIG_6510 = {
    .cpu_variant = CPU_6510,
    .has_io_port = true,        // 6-bit I/O port at $00/$01
    .has_aec_pin = true,        // AEC pin for bus sharing with VIC-II
    .address_lines = 16,
    .supports_illegal_ops = true,
    .enhanced_rdy = true,       // Enhanced RDY handling for VIC-II coordination
    .base_frequency = 985248,   // PAL C64 frequency (1.023 MHz NTSC)
    .variable_clock = false,
    .has_irq_pin = true,
    .has_nmi_pin = true,
    .has_reset_pin = true,
    .reset_vector = 0xFFFC,
    .irq_vector = 0xFFFE,
    .nmi_vector = 0xFFFA
};

// MOS 8502 (Commodore 128) - HMOS process, enhanced features, 2MHz support
const cpu_config_t CPU_CONFIG_8502 = {
    .cpu_variant = CPU_8502,
    .has_io_port = true,        // Enhanced I/O capabilities
    .has_aec_pin = true,        // AEC support maintained
    .address_lines = 16,
    .supports_illegal_ops = true,
    .enhanced_rdy = true,       // Enhanced RDY handling
    .base_frequency = 2000000,  // 2 MHz operation support
    .variable_clock = true,     // Support variable clock speeds
    .has_irq_pin = true,
    .has_nmi_pin = true,
    .has_reset_pin = true,
    .reset_vector = 0xFFFC,
    .irq_vector = 0xFFFE,
    .nmi_vector = 0xFFFA
};

// Configuration validation function
bool cpu_config_validate(const cpu_config_t *config) {
    if (!config) return false;
    
    // Validate CPU variant
    if (config->cpu_variant < CPU_6502 || config->cpu_variant > CPU_8502) {
        return false;
    }
    
    // Validate address lines
    if (config->address_lines < 13 || config->address_lines > 16) {
        return false;
    }
    
    // Validate frequency (must be > 0 and reasonable)
    if (config->base_frequency == 0 || config->base_frequency > 50000000) {
        return false;
    }
    
    // Variant-specific validation
    switch (config->cpu_variant) {
        case CPU_6502:
            // 6502 should not have I/O port or AEC pin
            if (config->has_io_port || config->has_aec_pin) return false;
            // Should have 16 address lines
            if (config->address_lines != 16) return false;
            // Should not support variable clock
            if (config->variable_clock) return false;
            break;
            
        case CPU_6507:
            // 6507 should not have I/O port, AEC pin, or interrupt pins
            if (config->has_io_port || config->has_aec_pin) return false;
            if (config->has_irq_pin || config->has_nmi_pin) return false;
            // Should have 13 address lines
            if (config->address_lines != 13) return false;
            // Should not support variable clock
            if (config->variable_clock) return false;
            break;
            
        case CPU_6510:
            // 6510 should have I/O port and AEC pin
            if (!config->has_io_port || !config->has_aec_pin) return false;
            // Should have 16 address lines
            if (config->address_lines != 16) return false;
            // Should have interrupt pins
            if (!config->has_irq_pin || !config->has_nmi_pin) return false;
            break;
            
        case CPU_8502:
            // 8502 should have I/O port and AEC pin
            if (!config->has_io_port || !config->has_aec_pin) return false;
            // Should have 16 address lines
            if (config->address_lines != 16) return false;
            // Should have interrupt pins
            if (!config->has_irq_pin || !config->has_nmi_pin) return false;
            break;
    }
    
    return true;
}

// Check if configuration is compatible with specified variant
bool cpu_config_is_compatible(const cpu_config_t *config, cpu_variant_t variant) {
    if (!config) return false;
    
    return config->cpu_variant == variant && cpu_config_validate(config);
}

// Apply default configuration for specified variant
void cpu_config_apply_variant_defaults(cpu_config_t *config, cpu_variant_t variant) {
    if (!config) return;
    
    switch (variant) {
        case CPU_6502:
            *config = CPU_CONFIG_6502;
            break;
        case CPU_6507:
            *config = CPU_CONFIG_6507;
            break;
        case CPU_6510:
            *config = CPU_CONFIG_6510;
            break;
        case CPU_8502:
            *config = CPU_CONFIG_8502;
            break;
        default:
            // Default to 6502 for unknown variants
            *config = CPU_CONFIG_6502;
            break;
    }
}

// Get human-readable variant name
const char* cpu_config_get_variant_name(cpu_variant_t variant) {
    switch (variant) {
        case CPU_6502: return "MOS 6502";
        case CPU_6507: return "MOS 6507";
        case CPU_6510: return "MOS 6510";
        case CPU_8502: return "MOS 8502";
        default: return "Unknown";
    }
}

// Get address mask for variant
uint32_t cpu_config_get_address_mask(const cpu_config_t *config) {
    if (!config) return 0xFFFF;  // Default to 16-bit
    
    switch (config->address_lines) {
        case 13: return 0x1FFF;   // 6507: 8KB address space
        case 16: return 0xFFFF;   // Standard: 64KB address space
        default: return (1U << config->address_lines) - 1;
    }
}

// Check if configuration supports specific pin
bool cpu_config_supports_pin(const cpu_config_t *config, const char* pin_name) {
    if (!config || !pin_name) return false;
    
    if (strcmp(pin_name, "IRQ") == 0) {
        return config->has_irq_pin;
    } else if (strcmp(pin_name, "NMI") == 0) {
        return config->has_nmi_pin;
    } else if (strcmp(pin_name, "RESET") == 0) {
        return config->has_reset_pin;
    } else if (strcmp(pin_name, "AEC") == 0) {
        return config->has_aec_pin;
    } else if (strcmp(pin_name, "RDY") == 0) {
        return true;  // All variants support RDY
    }
    
    return false;
}

// Runtime configuration switching (forward declaration implementation)
bool cpu_config_switch_runtime(mos6510_state_t *cpu, const cpu_config_t *new_config) {
    // This will be implemented when we have the CPU state structure
    // For now, just validate the configuration
    if (!cpu || !new_config) return false;
    
    return cpu_config_validate(new_config);
}