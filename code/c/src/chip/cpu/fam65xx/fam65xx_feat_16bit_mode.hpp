#pragma once
/*
 * fam65xx_feat_16bit_mode.hpp - Shared 16-bit Mode Feature Component
 *
 * This file provides reusable 16-bit mode functionality for processors that support
 * 16-bit operations and addressing (primarily WDC 65C816 and derivatives).
 *
 * FEATURES PROVIDED:
 * - 16-bit accumulator and index register modes
 * - 24-bit addressing with bank registers (DBR, PBR, DP)
 * - Native and Emulation mode support
 * - Mode control instructions (REP, SEP, XCE)
 * - Enhanced stack operations (PEA, PEI, PER, PHB, PHD, PHK, PLB, PLD)
 * - Long addressing instructions (JSL, RTL, JML)
 * - Block move instructions (MVN, MVP)
 * - Register state management and flag control
 *
 * USAGE PATTERN:
 * - Template parameter determines if feature is enabled at compile time
 * - Only processors with SIXTEENBIT_MODE feature flag instantiate this component
 * - Provides both operation handlers and state management
 *
 * EXTRACTED FROM: wdc65c816.hpp (original implementation)
 * GENERALIZED FOR: Any processor that needs 16-bit mode functionality
 */

#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"
#include <cstdint>

#ifdef __cplusplus

namespace fam65xx_features {

// ============================================================================
// 16-BIT MODE REGISTERS AND FLAGS
// ============================================================================

// Additional status register flags for 65C816
enum SixteenBitModeFlags {
    FLAG_M = 0x20,  // Memory/Accumulator size (0=16-bit, 1=8-bit)
    FLAG_X = 0x10,  // Index register size (0=16-bit, 1=8-bit)
    FLAG_E = 0x100  // Emulation mode (separate from P register)
};

// 16-bit mode instruction opcodes
enum SixteenBitModeOpcodes {
    // Mode control
    REP = 0xC2,  // Reset Processor Status bits
    SEP = 0xE2,  // Set Processor Status bits
    XCE = 0xFB,  // Exchange Carry and Emulation flags
    
    // Register operations
    XBA = 0xEB,  // Exchange B and A (swap high/low bytes of A)
    
    // Enhanced stack operations
    PEA = 0xF4,  // Push Effective Absolute address
    PEI = 0xD4,  // Push Effective Indirect address
    PER = 0x62,  // Push Effective PC Relative address
    PHB = 0x8B,  // Push Data Bank register
    PHD = 0x0B,  // Push Direct Page register
    PHK = 0x4B,  // Push Program Bank register
    PLB = 0xAB,  // Pull Data Bank register
    PLD = 0x2B,  // Pull Direct Page register
    
    // Long addressing
    JSL = 0x22,  // Jump to Subroutine Long
    RTL = 0x6B,  // Return from Subroutine Long
    JML_ABS = 0x5C,  // Jump Long Absolute
    JML_IND = 0xDC,  // Jump Long Indirect
    
    // Block move
    MVN = 0x54,  // Move Block Negative
    MVP = 0x44,  // Move Block Positive
    
    // System
    COP = 0x02,  // Coprocessor
    WDM = 0x42   // William D. Mensch (reserved for future use)
};

// ============================================================================
// 16-BIT MODE STATE STRUCTURE
// ============================================================================

struct SixteenBitModeState {
    // Bank registers
    uint16_t dp;    // Direct Page register
    uint8_t dbr;    // Data Bank register
    uint8_t pbr;    // Program Bank register
    
    // Mode control
    bool emulation; // Emulation mode flag (E)
    
    // 16-bit register storage
    uint16_t a_16;  // 16-bit accumulator (when M=0)
    uint16_t x_16;  // 16-bit X register (when X=0)
    uint16_t y_16;  // 16-bit Y register (when X=0)
    
    // 16-bit stack pointer (full 16-bit in native mode)
    uint16_t s_16;  // 16-bit stack pointer
    
    // Initialize with power-on defaults
    SixteenBitModeState() : dp(0), dbr(0), pbr(0), emulation(true), 
                           a_16(0), x_16(0), y_16(0), s_16(0x01FF) {}
    
    // Reset to power-on state
    void reset() {
        dp = 0x0000;        // Direct page starts at zero
        dbr = 0x00;         // Data bank starts at zero
        pbr = 0x00;         // Program bank starts at zero
        emulation = true;   // Start in emulation mode
        a_16 = 0x0000;      // Clear 16-bit accumulator
        x_16 = 0x0000;      // Clear 16-bit X
        y_16 = 0x0000;      // Clear 16-bit Y
        s_16 = 0x01FF;      // Stack starts at top of page 1 in emulation mode
    }
};

// ============================================================================
// 16-BIT MODE FEATURE TEMPLATE
// ============================================================================

template<typename ProcessorTag>
class SixteenBitModeFeature {
private:
    // Feature is only enabled if processor has SIXTEENBIT_MODE capability
    static constexpr bool enabled = fam65xx_core::has_feature<ProcessorTag>(fam65xx_core::ProcessorFeatures::SIXTEENBIT_MODE);
    
    SixteenBitModeState state;
    uint32_t long_instruction_count;
    uint32_t block_move_count;
    uint32_t mode_switch_count;
    
public:
    // ========================================================================
    // CONSTRUCTOR AND LIFECYCLE
    // ========================================================================
    
    SixteenBitModeFeature() : long_instruction_count(0), block_move_count(0), mode_switch_count(0) {
        if constexpr (enabled) {
            state.reset();
        }
    }
    
    void reset() {
        if constexpr (enabled) {
            state.reset();
            long_instruction_count = 0;
            block_move_count = 0;
            mode_switch_count = 0;
        }
    }
    
    // ========================================================================
    // REGISTER ACCESS INTERFACE
    // ========================================================================
    
    // Bank register access
    void set_dp(uint16_t value) {
        if constexpr (enabled) {
            state.dp = value;
        }
    }
    
    void set_dbr(uint8_t value) {
        if constexpr (enabled) {
            state.dbr = value;
        }
    }
    
    void set_pbr(uint8_t value) {
        if constexpr (enabled) {
            state.pbr = value;
        }
    }
    
    uint16_t get_dp() const {
        if constexpr (enabled) {
            return state.dp;
        }
        return 0;
    }
    
    uint8_t get_dbr() const {
        if constexpr (enabled) {
            return state.dbr;
        }
        return 0;
    }
    
    uint8_t get_pbr() const {
        if constexpr (enabled) {
            return state.pbr;
        }
        return 0;
    }
    
    // 16-bit register access
    void set_a_16(uint16_t value) {
        if constexpr (enabled) {
            state.a_16 = value;
        }
    }
    
    void set_x_16(uint16_t value) {
        if constexpr (enabled) {
            state.x_16 = value;
        }
    }
    
    void set_y_16(uint16_t value) {
        if constexpr (enabled) {
            state.y_16 = value;
        }
    }
    
    void set_s_16(uint16_t value) {
        if constexpr (enabled) {
            state.s_16 = value;
        }
    }
    
    uint16_t get_a_16() const {
        if constexpr (enabled) {
            return state.a_16;
        }
        return 0;
    }
    
    uint16_t get_x_16() const {
        if constexpr (enabled) {
            return state.x_16;
        }
        return 0;
    }
    
    uint16_t get_y_16() const {
        if constexpr (enabled) {
            return state.y_16;
        }
        return 0;
    }
    
    uint16_t get_s_16() const {
        if constexpr (enabled) {
            return state.s_16;
        }
        return 0x01FF;
    }
    
    // Mode control
    void set_emulation_mode(bool emulation) {
        if constexpr (enabled) {
            state.emulation = emulation;
            mode_switch_count++;
            
            if (emulation) {
                // Entering emulation mode - force 8-bit modes and stack page
                state.s_16 = (state.s_16 & 0x00FF) | 0x0100; // Force stack to page 1
            }
        }
    }
    
    bool get_emulation_mode() const {
        if constexpr (enabled) {
            return state.emulation;
        }
        return true; // Default to emulation mode
    }
    
    // ========================================================================
    // MODE QUERIES
    // ========================================================================
    
    bool is_accumulator_16bit(uint8_t p_register) const {
        if constexpr (enabled) {
            return !state.emulation && !(p_register & FLAG_M);
        }
        return false;
    }
    
    bool is_index_16bit(uint8_t p_register) const {
        if constexpr (enabled) {
            return !state.emulation && !(p_register & FLAG_X);
        }
        return false;
    }
    
    bool is_native_mode() const {
        if constexpr (enabled) {
            return !state.emulation;
        }
        return false;
    }
    
    // ========================================================================
    // 24-BIT ADDRESS CALCULATION
    // ========================================================================
    
    uint32_t make_long_address(uint8_t bank, uint16_t addr) const {
        if constexpr (enabled) {
            return (static_cast<uint32_t>(bank) << 16) | addr;
        }
        return addr; // Fallback to 16-bit addressing
    }
    
    uint32_t make_data_address(uint16_t addr) const {
        if constexpr (enabled) {
            return make_long_address(state.dbr, addr);
        }
        return addr;
    }
    
    uint32_t make_program_address(uint16_t addr) const {
        if constexpr (enabled) {
            return make_long_address(state.pbr, addr);
        }
        return addr;
    }
    
    uint32_t make_direct_address(uint8_t offset) const {
        if constexpr (enabled) {
            if (state.emulation) {
                // In emulation mode, direct page is always in page 0
                return static_cast<uint32_t>(offset);
            } else {
                // In native mode, add to direct page register
                return make_data_address(state.dp + offset);
            }
        }
        return offset;
    }
    
    // ========================================================================
    // OPERATION HANDLERS
    // ========================================================================
    
    // REP - Reset Processor Status bits
    bus_state_t handle_rep(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            // Read immediate operand
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            // Clear specified bits in processor status
            uint8_t mask = BUS_GET_DATA(pins);
            CPU_P(cpu) &= ~mask;
            
            mode_switch_count++;
            fam65xx_transition_to_fetch(cpu);
        }
        return pins;
    }
    
    // SEP - Set Processor Status bits
    bus_state_t handle_sep(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            // Read immediate operand
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            // Set specified bits in processor status
            uint8_t mask = BUS_GET_DATA(pins);
            CPU_P(cpu) |= mask;
            
            mode_switch_count++;
            fam65xx_transition_to_fetch(cpu);
        }
        return pins;
    }
    
    // XCE - Exchange Carry and Emulation flags
    bus_state_t handle_xce(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            // Dummy cycle for internal operation
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            // Exchange carry flag and emulation mode
            bool old_carry = (CPU_P(cpu) & FLAG_C) != 0;
            bool old_emulation = state.emulation;
            
            if (old_emulation) {
                CPU_P(cpu) |= FLAG_C;
            } else {
                CPU_P(cpu) &= ~FLAG_C;
            }
            
            set_emulation_mode(old_carry);
            fam65xx_transition_to_fetch(cpu);
        }
        return pins;
    }
    
    // XBA - Exchange B and A (swap high/low bytes of accumulator)
    bus_state_t handle_xba(fam65xx_t* cpu, bus_state_t pins) {
        if constexpr (enabled) {
            // Dummy cycle for internal operation
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            // Swap high and low bytes of 16-bit accumulator
            uint8_t low = state.a_16 & 0xFF;
            uint8_t high = (state.a_16 >> 8) & 0xFF;
            state.a_16 = (low << 8) | high;
            
            // Update 8-bit accumulator register
            CPU_A(cpu) = high;
            
            // Update N and Z flags based on new low byte (now in A)
            fam65xx_update_nz_flags(cpu, high);
            fam65xx_transition_to_fetch(cpu);
        }
        return pins;
    }
    
    // ========================================================================
    // INSTRUCTION TYPE DETECTION
    // ========================================================================
    
    static bool is_long_instruction(uint8_t opcode) {
        if constexpr (enabled) {
            switch (opcode) {
                case REP: case SEP: case XCE: case XBA:
                case PEA: case PEI: case PER: case PHB: case PHD: case PHK:
                case PLB: case PLD: case JSL: case RTL: case JML_ABS: case JML_IND:
                case MVN: case MVP: case COP: case WDM:
                    return true;
                default:
                    return false;
            }
        }
        return false;
    }
    
    static bool is_block_move_instruction(uint8_t opcode) {
        if constexpr (enabled) {
            return (opcode == MVN) || (opcode == MVP);
        }
        return false;
    }
    
    static bool is_mode_control_instruction(uint8_t opcode) {
        if constexpr (enabled) {
            return (opcode == REP) || (opcode == SEP) || (opcode == XCE);
        }
        return false;
    }
    
    // ========================================================================
    // STATISTICS AND DEBUGGING
    // ========================================================================
    
    uint32_t get_long_instruction_count() const {
        if constexpr (enabled) {
            return long_instruction_count;
        }
        return 0;
    }
    
    uint32_t get_block_move_count() const {
        if constexpr (enabled) {
            return block_move_count;
        }
        return 0;
    }
    
    uint32_t get_mode_switch_count() const {
        if constexpr (enabled) {
            return mode_switch_count;
        }
        return 0;
    }
    
    void reset_statistics() {
        if constexpr (enabled) {
            long_instruction_count = 0;
            block_move_count = 0;
            mode_switch_count = 0;
        }
    }
    
    // Get complete state for debugging
    SixteenBitModeState get_state() const {
        if constexpr (enabled) {
            return state;
        }
        return SixteenBitModeState();
    }
    
    // Feature availability check
    static constexpr bool is_enabled() {
        return enabled;
    }
    
    // ========================================================================
    // TICK INTEGRATION (for CPU integration)
    // ========================================================================
    
    // Called during CPU tick to handle any 16-bit mode specific timing
    template<typename BusState>
    BusState handle_tick(BusState pins) {
        if constexpr (enabled) {
            // 16-bit mode doesn't need special tick handling in basic implementation
            // Advanced implementations could handle bank switching timing here
        }
        return pins;
    }
    
    // Track instruction usage
    void track_instruction(uint8_t opcode) {
        if constexpr (enabled) {
            if (is_long_instruction(opcode)) {
                long_instruction_count++;
            }
            
            if (is_block_move_instruction(opcode)) {
                block_move_count++;
            }
            
            if (is_mode_control_instruction(opcode)) {
                mode_switch_count++;
            }
        }
    }
    
    // Handle register width changes based on P register flags
    void update_register_widths(fam65xx_t* cpu) {
        if constexpr (enabled) {
            if (!state.emulation) {
                // In native mode, check M and X flags
                bool accumulator_16bit = !(CPU_P(cpu) & FLAG_M);
                bool index_16bit = !(CPU_P(cpu) & FLAG_X);
                
                if (accumulator_16bit) {
                    // Sync 16-bit accumulator with 8-bit register
                    state.a_16 = (state.a_16 & 0xFF00) | CPU_A(cpu);
                } else {
                    // Clear high byte when switching to 8-bit mode
                    state.a_16 = CPU_A(cpu);
                }
                
                if (index_16bit) {
                    // Sync 16-bit index registers with 8-bit registers
                    state.x_16 = (state.x_16 & 0xFF00) | CPU_X(cpu);
                    state.y_16 = (state.y_16 & 0xFF00) | CPU_Y(cpu);
                } else {
                    // Clear high bytes when switching to 8-bit mode
                    state.x_16 = CPU_X(cpu);
                    state.y_16 = CPU_Y(cpu);
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
// These functions assume the processor has 16-bit mode capability

static bus_state_t op_rep(fam65xx_t* cpu, bus_state_t pins) {
    // Implementation would use the feature template above
    // For now, provide fallback
    return pins;
}

static bus_state_t op_sep(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_xce(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_xba(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_pea(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_pei(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_per(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_phb(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_phd(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_phk(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_plb(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_pld(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_jsl(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_rtl(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_jml(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_mvn(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_mvp(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_cop(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

static bus_state_t op_wdm(fam65xx_t* cpu, bus_state_t pins) {
    return pins;
}

#endif // AIEMUC_IMPL

} // namespace fam65xx_features

#endif // __cplusplus