#pragma once
/*
 * fam65xx_feat_bit_ops.hpp - Shared Bit Manipulation Feature Component
 *
 * This file provides reusable bit manipulation functionality for processors that support
 * bit manipulation instructions (primarily Rockwell R65C02 and derivatives).
 *
 * FEATURES PROVIDED:
 * - RMB0-RMB7: Reset Memory Bit (clear specific bit in zero page)
 * - SMB0-SMB7: Set Memory Bit (set specific bit in zero page)
 * - BBR0-BBR7: Branch on Bit Reset (branch if specific bit clear)
 * - BBS0-BBS7: Branch on Bit Set (branch if specific bit set)
 * - Zero page relative addressing mode for bit branch instructions
 * - Bit manipulation helper functions and utilities
 * - Instruction tracking and statistics
 *
 * USAGE PATTERN:
 * - Template parameter determines if feature is enabled at compile time
 * - Only processors with BIT_MANIPULATION feature flag instantiate this component
 * - Provides both operation handlers and utility functions
 *
 * EXTRACTED FROM: rockwell65c02.hpp (original implementation)
 * GENERALIZED FOR: Any processor that needs bit manipulation functionality
 */

#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"
#include <cstdint>

#ifdef __cplusplus

namespace fam65xx_features {

// ============================================================================
// BIT MANIPULATION INSTRUCTION OPCODES
// ============================================================================

enum BitManipulationOpcodes {
    // Reset Memory Bit (RMB) - Clear specific bit in zero page
    RMB0 = 0x07, RMB1 = 0x17, RMB2 = 0x27, RMB3 = 0x37,
    RMB4 = 0x47, RMB5 = 0x57, RMB6 = 0x67, RMB7 = 0x77,
    
    // Set Memory Bit (SMB) - Set specific bit in zero page
    SMB0 = 0x87, SMB1 = 0x97, SMB2 = 0xA7, SMB3 = 0xB7,
    SMB4 = 0xC7, SMB5 = 0xD7, SMB6 = 0xE7, SMB7 = 0xF7,
    
    // Branch on Bit Reset (BBR) - Branch if specific bit clear
    BBR0 = 0x0F, BBR1 = 0x1F, BBR2 = 0x2F, BBR3 = 0x3F,
    BBR4 = 0x4F, BBR5 = 0x5F, BBR6 = 0x6F, BBR7 = 0x7F,
    
    // Branch on Bit Set (BBS) - Branch if specific bit set
    BBS0 = 0x8F, BBS1 = 0x9F, BBS2 = 0xAF, BBS3 = 0xBF,
    BBS4 = 0xCF, BBS5 = 0xDF, BBS6 = 0xEF, BBS7 = 0xFF
};

// ============================================================================
// BIT MANIPULATION UTILITIES
// ============================================================================

class BitManipulationUtils {
public:
    // Extract bit number from RMB/SMB opcode
    static constexpr uint8_t get_rmb_smb_bit(uint8_t opcode) {
        return (opcode >> 4) & 0x07;
    }
    
    // Extract bit number from BBR/BBS opcode
    static constexpr uint8_t get_bbr_bbs_bit(uint8_t opcode) {
        return (opcode >> 4) & 0x07;
    }
    
    // Check if opcode is a bit manipulation instruction
    static constexpr bool is_bit_instruction(uint8_t opcode) {
        // RMB/SMB: opcodes ending in 0x07 (0x_7)
        // BBR/BBS: opcodes ending in 0x0F (0x_F)
        return (opcode & 0x0F) == 0x07 || (opcode & 0x0F) == 0x0F;
    }
    
    // Check if opcode is RMB/SMB (memory bit set/reset)
    static constexpr bool is_memory_bit_instruction(uint8_t opcode) {
        return (opcode & 0x0F) == 0x07;
    }
    
    // Check if opcode is BBR/BBS (bit branch)
    static constexpr bool is_bit_branch_instruction(uint8_t opcode) {
        return (opcode & 0x0F) == 0x0F;
    }
    
    // Check if opcode is RMB (reset memory bit)
    static constexpr bool is_rmb_instruction(uint8_t opcode) {
        return is_memory_bit_instruction(opcode) && (opcode & 0x80) == 0;
    }
    
    // Check if opcode is SMB (set memory bit)
    static constexpr bool is_smb_instruction(uint8_t opcode) {
        return is_memory_bit_instruction(opcode) && (opcode & 0x80) != 0;
    }
    
    // Check if opcode is BBR (branch on bit reset)
    static constexpr bool is_bbr_instruction(uint8_t opcode) {
        return is_bit_branch_instruction(opcode) && (opcode & 0x80) == 0;
    }
    
    // Check if opcode is BBS (branch on bit set)
    static constexpr bool is_bbs_instruction(uint8_t opcode) {
        return is_bit_branch_instruction(opcode) && (opcode & 0x80) != 0;
    }
    
    // Bit manipulation helper functions
    static constexpr uint8_t set_bit(uint8_t value, uint8_t bit) {
        return value | (1 << bit);
    }
    
    static constexpr uint8_t clear_bit(uint8_t value, uint8_t bit) {
        return value & ~(1 << bit);
    }
    
    static constexpr bool test_bit(uint8_t value, uint8_t bit) {
        return (value & (1 << bit)) != 0;
    }
    
    static constexpr uint8_t toggle_bit(uint8_t value, uint8_t bit) {
        return value ^ (1 << bit);
    }
};

// ============================================================================
// BIT MANIPULATION FEATURE TEMPLATE
// ============================================================================

template<typename ProcessorTag>
class BitManipulationFeature {
private:
    // Feature is only enabled if processor has BIT_MANIPULATION capability
    static constexpr bool enabled = fam65xx_core::has_feature<ProcessorTag>(fam65xx_core::ProcessorFeatures::BIT_MANIPULATION);
    
    uint32_t bit_instruction_count;
    uint32_t memory_bit_operations;
    uint32_t bit_branch_operations;
    
public:
    // ========================================================================
    // CONSTRUCTOR AND LIFECYCLE
    // ========================================================================
    
    BitManipulationFeature() : bit_instruction_count(0), memory_bit_operations(0), bit_branch_operations(0) {}
    
    void reset() {
        if constexpr (enabled) {
            bit_instruction_count = 0;
            memory_bit_operations = 0;
            bit_branch_operations = 0;
        }
    }
    
    // ========================================================================
    // OPERATION HANDLERS
    // ========================================================================
    
    // Generic RMB operation handler (Reset Memory Bit)
    template<uint8_t bit_number>
    bus_state_t handle_rmb(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            static_assert(bit_number < 8, "Bit number must be 0-7");
            
            switch (cpu->cycle_index++) {
                case 0: {
                    // PHI2: Read data from zero page address
                    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
                    if (!FAM65XX_GET_RDY(pins)) return pins;
                    
                    // PHI1: Reset bit and prepare write data
                    const uint8_t data = BUS_GET_DATA(pins);
                    CPU_DL(cpu) = BitManipulationUtils::clear_bit(data, bit_number);
                    memory_bit_operations++;
                    break;
                }
                    
                case 1:
                    // PHI2: Write modified data back
                    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
                    if (!FAM65XX_GET_RDY(pins)) return pins;
                    
                    // PHI1: Complete operation
                    fam65xx_transition_to_fetch(cpu);
                    bit_instruction_count++;
                    break;
            }
        }
        return pins;
    }
    
    // Generic SMB operation handler (Set Memory Bit)
    template<uint8_t bit_number>
    bus_state_t handle_smb(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            static_assert(bit_number < 8, "Bit number must be 0-7");
            
            switch (cpu->cycle_index++) {
                case 0: {
                    // PHI2: Read data from zero page address
                    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
                    if (!FAM65XX_GET_RDY(pins)) return pins;
                    
                    // PHI1: Set bit and prepare write data
                    const uint8_t data = BUS_GET_DATA(pins);
                    CPU_DL(cpu) = BitManipulationUtils::set_bit(data, bit_number);
                    memory_bit_operations++;
                    break;
                }
                    
                case 1:
                    // PHI2: Write modified data back
                    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
                    if (!FAM65XX_GET_RDY(pins)) return pins;
                    
                    // PHI1: Complete operation
                    fam65xx_transition_to_fetch(cpu);
                    bit_instruction_count++;
                    break;
            }
        }
        return pins;
    }
    
    // Generic BBR operation handler (Branch on Bit Reset)
    template<uint8_t bit_number>
    bus_state_t handle_bbr(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            static_assert(bit_number < 8, "Bit number must be 0-7");
            
            // Test bit from data latch (set up by ZPR addressing mode)
            const uint8_t data = CPU_DL(cpu);
            if (!BitManipulationUtils::test_bit(data, bit_number)) {
                // Bit is reset, take branch
                int8_t offset = (int8_t)CPU_IR(cpu);  // Branch offset from addressing mode
                CPU_PC(cpu) += offset;
                bit_branch_operations++;
            }
            
            fam65xx_transition_to_fetch(cpu);
            bit_instruction_count++;
        }
        return pins;
    }
    
    // Generic BBS operation handler (Branch on Bit Set)
    template<uint8_t bit_number>
    bus_state_t handle_bbs(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            static_assert(bit_number < 8, "Bit number must be 0-7");
            
            // Test bit from data latch (set up by ZPR addressing mode)
            const uint8_t data = CPU_DL(cpu);
            if (BitManipulationUtils::test_bit(data, bit_number)) {
                // Bit is set, take branch
                int8_t offset = (int8_t)CPU_IR(cpu);  // Branch offset from addressing mode
                CPU_PC(cpu) += offset;
                bit_branch_operations++;
            }
            
            fam65xx_transition_to_fetch(cpu);
            bit_instruction_count++;
        }
        return pins;
    }
    
    // ========================================================================
    // OPERATION HANDLER ARRAYS (for opcode table integration)
    // ========================================================================
    
    // Array of RMB handlers (indexed by bit number)
    static constexpr auto get_rmb_handlers() {
        if constexpr (enabled) {
            return std::array<cycle_fn_t, 8>{
                [0] = &BitManipulationFeature::handle_rmb<0>,
                [1] = &BitManipulationFeature::handle_rmb<1>,
                [2] = &BitManipulationFeature::handle_rmb<2>,
                [3] = &BitManipulationFeature::handle_rmb<3>,
                [4] = &BitManipulationFeature::handle_rmb<4>,
                [5] = &BitManipulationFeature::handle_rmb<5>,
                [6] = &BitManipulationFeature::handle_rmb<6>,
                [7] = &BitManipulationFeature::handle_rmb<7>
            };
        }
    }
    
    // Array of SMB handlers (indexed by bit number)
    static constexpr auto get_smb_handlers() {
        if constexpr (enabled) {
            return std::array<cycle_fn_t, 8>{
                [0] = &BitManipulationFeature::handle_smb<0>,
                [1] = &BitManipulationFeature::handle_smb<1>,
                [2] = &BitManipulationFeature::handle_smb<2>,
                [3] = &BitManipulationFeature::handle_smb<3>,
                [4] = &BitManipulationFeature::handle_smb<4>,
                [5] = &BitManipulationFeature::handle_smb<5>,
                [6] = &BitManipulationFeature::handle_smb<6>,
                [7] = &BitManipulationFeature::handle_smb<7>
            };
        }
    }
    
    // Array of BBR handlers (indexed by bit number)
    static constexpr auto get_bbr_handlers() {
        if constexpr (enabled) {
            return std::array<cycle_fn_t, 8>{
                [0] = &BitManipulationFeature::handle_bbr<0>,
                [1] = &BitManipulationFeature::handle_bbr<1>,
                [2] = &BitManipulationFeature::handle_bbr<2>,
                [3] = &BitManipulationFeature::handle_bbr<3>,
                [4] = &BitManipulationFeature::handle_bbr<4>,
                [5] = &BitManipulationFeature::handle_bbr<5>,
                [6] = &BitManipulationFeature::handle_bbr<6>,
                [7] = &BitManipulationFeature::handle_bbr<7>
            };
        }
    }
    
    // Array of BBS handlers (indexed by bit number)
    static constexpr auto get_bbs_handlers() {
        if constexpr (enabled) {
            return std::array<cycle_fn_t, 8>{
                [0] = &BitManipulationFeature::handle_bbs<0>,
                [1] = &BitManipulationFeature::handle_bbs<1>,
                [2] = &BitManipulationFeature::handle_bbs<2>,
                [3] = &BitManipulationFeature::handle_bbs<3>,
                [4] = &BitManipulationFeature::handle_bbs<4>,
                [5] = &BitManipulationFeature::handle_bbs<5>,
                [6] = &BitManipulationFeature::handle_bbs<6>,
                [7] = &BitManipulationFeature::handle_bbs<7>
            };
        }
    }
    
    // ========================================================================
    // OPCODE TABLE INTEGRATION HELPERS
    // ========================================================================
    
    // Get handler for specific bit manipulation opcode
    cycle_fn_t get_bit_handler(uint8_t opcode) {
        if constexpr (enabled) {
            if (BitManipulationUtils::is_rmb_instruction(opcode)) {
                uint8_t bit = BitManipulationUtils::get_rmb_smb_bit(opcode);
                auto handlers = get_rmb_handlers();
                return handlers[bit];
            } else if (BitManipulationUtils::is_smb_instruction(opcode)) {
                uint8_t bit = BitManipulationUtils::get_rmb_smb_bit(opcode);
                auto handlers = get_smb_handlers();
                return handlers[bit];
            } else if (BitManipulationUtils::is_bbr_instruction(opcode)) {
                uint8_t bit = BitManipulationUtils::get_bbr_bbs_bit(opcode);
                auto handlers = get_bbr_handlers();
                return handlers[bit];
            } else if (BitManipulationUtils::is_bbs_instruction(opcode)) {
                uint8_t bit = BitManipulationUtils::get_bbr_bbs_bit(opcode);
                auto handlers = get_bbs_handlers();
                return handlers[bit];
            }
        }
        return nullptr;
    }
    
    // ========================================================================
    // STATISTICS AND DEBUGGING
    // ========================================================================
    
    uint32_t get_bit_instruction_count() const {
        if constexpr (enabled) {
            return bit_instruction_count;
        }
        return 0;
    }
    
    uint32_t get_memory_bit_operations() const {
        if constexpr (enabled) {
            return memory_bit_operations;
        }
        return 0;
    }
    
    uint32_t get_bit_branch_operations() const {
        if constexpr (enabled) {
            return bit_branch_operations;
        }
        return 0;
    }
    
    void reset_statistics() {
        if constexpr (enabled) {
            bit_instruction_count = 0;
            memory_bit_operations = 0;
            bit_branch_operations = 0;
        }
    }
    
    // Feature availability check
    static constexpr bool is_enabled() {
        return enabled;
    }
    
    // ========================================================================
    // TICK INTEGRATION (for CPU integration)
    // ========================================================================
    
    // Called during CPU tick to handle any bit manipulation specific timing
    template<typename BusState>
    BusState handle_tick(BusState pins) {
        if constexpr (enabled) {
            // Bit manipulation doesn't need special tick handling in basic implementation
            // Advanced implementations could handle timing-sensitive operations here
        }
        return pins;
    }
    
    // Track instruction usage
    void track_instruction(uint8_t opcode) {
        if constexpr (enabled) {
            if (BitManipulationUtils::is_bit_instruction(opcode)) {
                bit_instruction_count++;
                
                if (BitManipulationUtils::is_memory_bit_instruction(opcode)) {
                    memory_bit_operations++;
                } else if (BitManipulationUtils::is_bit_branch_instruction(opcode)) {
                    bit_branch_operations++;
                }
            }
        }
    }
};

// ============================================================================
// STATIC OPERATION HANDLERS (for legacy opcode table compatibility)
// ============================================================================

#ifdef AIEMUC_IMPL

// Define static operation handlers for use in opcode tables
// These functions assume the processor has bit manipulation capability

// RMB operation handlers
static bus_state_t op_rmb0(fam65xx_t* cpu, bus_state_t pins) {
    // Implementation would use the feature template above
    // For now, provide fallback
    return pins;
}

static bus_state_t op_rmb1(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_rmb2(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_rmb3(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_rmb4(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_rmb5(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_rmb6(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_rmb7(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

// SMB operation handlers
static bus_state_t op_smb0(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_smb1(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_smb2(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_smb3(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_smb4(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_smb5(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_smb6(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_smb7(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

// BBR operation handlers
static bus_state_t op_bbr0(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbr1(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbr2(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbr3(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbr4(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbr5(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbr6(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbr7(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

// BBS operation handlers
static bus_state_t op_bbs0(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbs1(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbs2(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbs3(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbs4(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbs5(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbs6(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_bbs7(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

#endif // AIEMUC_IMPL

} // namespace fam65xx_features

#endif // __cplusplus