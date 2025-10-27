#pragma once
/*
 * fam65xx.hpp - MOS 65xx Family CPU Emulator (Main Template Implementation)
 *
 * This header contains the main CPU template class implementation that builds upon
 * modular components for maintainability. It includes C types, processor traits,
 * and conditional mixins from separate headers to keep file sizes manageable.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * 1. Zero overhead abstractions - templates eliminate runtime costs
 * 2. Conditional features - only include data/code for enabled features  
 * 3. Pin-accurate bus interface - maintains bus_state_t pins design
 * 4. Modular components - separate headers for logical groupings
 * 5. Single include point - just include this header for full functionality
 *
 * SUPPORTED PROCESSOR VARIANTS:
 * =============================
 * - MOS6502Tag:     Original NMOS 6502 with illegal opcodes
 * - MOS6510Tag:     C64/C128 variant with I/O port  
 * - NES6502Tag:     NES variant (no BCD, no illegal opcodes)
 * - WDC65C02Tag:    CMOS 65C02 with enhanced instructions
 * - Rockwell65C02Tag: CMOS with bit manipulation instructions
 * - WDC65C816Tag:   16-bit extended processor
 *
 * USAGE:
 * ======
 * ```cpp
 * #include "fam65xx.hpp"
 * 
 * // Create a C64 CPU instance
 * fam65xx_cpp::fam65xx_t<MOS6510Tag> c64_cpu;
 * 
 * // Initialize and use with pin-based interface
 * bus_state_t pins = c64_cpu.init(&desc);
 * pins = c64_cpu.reset(pins);
 * pins = c64_cpu.tick(pins);
 * ```
 */

#include <cstdint>
#include <type_traits>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdarg>

#include "fam65xx_types.h"
#include "fam65xx_processor_traits.hpp"
#include "fam65xx_mixins.hpp"

// ============================================================================
// C++ NAMESPACE - MAIN CPU TEMPLATE IMPLEMENTATION
// ============================================================================

namespace fam65xx_cpp {

// ============================================================================
// FORWARD DECLARATIONS FOR OPCODE TABLE GENERATION
// ============================================================================

// Forward declaration - will be specialized for each processor
template<typename ProcessorTag>
constexpr std::array<opcode_info_t, 256> generate_opcode_table();

// Include processor-specific opcode table specializations BEFORE class definition
#include "operations/opcode_tables.inc.hpp"

// ============================================================================
// INTERRUPT SHIFT REGISTER CONSTANTS (use definitions from fam65xx_types.h)
// ============================================================================
// Note: INT_* constants are defined in fam65xx_types.h and used here via C header inclusion

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template<typename ProcessorTag>
class fam65xx_t : 
    public io_port_base_t<ProcessorTag>        // Conditional I/O port only
{
public:
    // Type aliases for cleaner code
    using ProcessorTraits = fam65xx_cpp::ProcessorTraits<ProcessorTag>;
    using Features = fam65xx_cpp::ProcessorFeatures;
    
    // ========================================================================
    // CPU STATE (merged from fam65xx_cpu_state_t)
    // ========================================================================
    
    /* Register array - union allows both 8-bit and 16-bit access */
    union {
        uint8_t reg8[REG_COUNT];        /* 8-bit register access */
        uint16_t reg16[REG_COUNT / 2];  /* 16-bit pair access (little-endian) */
    };
    
    /* Current execution state */
    opcode_info_t opcode_entry;         /* Cached opcode entry (copied once) */
    bus_state_t (fam65xx_t::*current_handler)(bus_state_t);  /* Current instruction handler */
    uint8_t cycle_index;                /* Current cycle within instruction */
    
    /* Interrupt state - hardware-accurate shift register system */
    uint8_t brk_flags;                  /* BRK/IRQ/NMI/RESET flags */
    uint8_t nmi_prev;                   /* Previous NMI line state for edge detection */
    uint32_t interrupt_shift_register;  /* Combined shift register for all interrupt types */
    
    /* Memory callbacks (for test runner compatibility) */
    fam65xx_mem_read_t mem_read;        /* Memory read callback */
    fam65xx_mem_write_t mem_write;      /* Memory write callback */
    void* mem_user_data;                /* User data for memory callbacks */
    
    /* 65C02 extended state (merged from cmos_state_mixin_t) */
    bool wait_for_interrupt;            /* WAI instruction state */
    bool stopped;                       /* STP instruction state */
    
    /* Debug tracing state */
    static constexpr bool ENABLE_TRACING = false;  /* Compile-time tracing flag */
    mutable int trace_indent;           /* Current tracing indentation level */
    
    // ========================================================================
    // CONSTRUCTOR AND INITIALIZATION
    // ========================================================================
    
    fam65xx_t() {
        // Initialize CPU state to zero
        memset(&reg8, 0, sizeof(reg8));
        opcode_entry = {};
        current_handler = nullptr;
        cycle_index = 0;
        brk_flags = 0;
        nmi_prev = 0;
        interrupt_shift_register = 0;
        mem_read = nullptr;
        mem_write = nullptr;
        mem_user_data = nullptr;
        wait_for_interrupt = false;
        stopped = false;
        trace_indent = 0;
        
        // Initialize conditional features
        this->init_conditional_features();
    }
    
    // ========================================================================
    // DEBUG TRACING HELPERS
    // ========================================================================
    
    void trace(const char* format, ...) const {
        if constexpr (ENABLE_TRACING) {
            // Print indentation
            for (int i = 0; i < trace_indent; i++) {
                printf("  ");
            }
            
            // Print formatted message
            va_list args;
            va_start(args, format);
            vprintf(format, args);
            va_end(args);
            printf("\n");
        }
    }
    
    void trace_enter(const char* function_name) const {
        if constexpr (ENABLE_TRACING) {
            trace("→ %s", function_name);
            trace_indent++;
        }
    }
    
    void trace_exit(const char* function_name) const {
        if constexpr (ENABLE_TRACING) {
            trace_indent--;
            trace("← %s", function_name);
        }
    }
    
    void trace_registers(const char* context = "") const {
        if constexpr (ENABLE_TRACING) {
            trace("REGS %s: PC=%04X A=%02X X=%02X Y=%02X P=%02X S=%02X", 
                  context,
                  CPU_PC(this), CPU_A(this), CPU_X(this), CPU_Y(this), 
                  CPU_P(this), CPU_S(this));
        }
    }
    
    void trace_memory_access(const char* op, uint16_t addr, uint8_t data) const {
        if constexpr (ENABLE_TRACING) {
            trace("MEM %s: [%04X] = %02X", op, addr, data);
        }
    }
    
    void trace_instruction(uint8_t opcode, const char* mnemonic) const {
        if constexpr (ENABLE_TRACING) {
            trace("EXEC: %02X %s (cycle %d)", opcode, mnemonic, cycle_index);
        }
    }
    
    // ========================================================================
    // PROCESSOR-SPECIFIC INITIALIZATION  
    // ========================================================================
    
    bus_state_t init(const chip_descriptor_t* desc) {
        // Note: Memory callbacks will be set through separate API calls
        // This matches the old implementation's approach
        this->mem_read = nullptr;
        this->mem_write = nullptr;
        this->mem_user_data = nullptr;
        
        // Initialize processor-specific features
        this->init_conditional_features();
        
        // Initialize opcode table for this processor type
        this->init_opcode_table();
        
        /* Initialize register layout:
        * ZP high byte (REG_ZPH) = 0x00 (always zero for zero page)
        * SP = 0x01FF (stack starts at top of page 1)
        */
        CPU_ZPH(this) = 0x00;  /* Zero page high byte */
        CPU_SP(this) = 0x01FF; /* Stack pointer (page 1, starts at 0xFF) */

        // Return initial pin state
        bus_state_t pins = 0;
        return pins;
    }
    
    // Add memory callback setup function (matching old implementation)
    void set_memory_callbacks(fam65xx_mem_read_t read_fn, fam65xx_mem_write_t write_fn, void* user_data) {
        this->mem_read = read_fn;
        this->mem_write = write_fn;
        this->mem_user_data = user_data;
    }
    
    bus_state_t bootstrap(bus_state_t pins) {
        // Bootstrap CPU for immediate execution (test runner compatibility)
        // Ported from original fam65xx implementation
        
        /* Set up for immediate instruction execution without RESET sequence */
        pins |= FAM65XX_RDY;   /* Ensure RDY is high for execution */
        pins |= FAM65XX_RW;    /* Ensure RW is set as default state */
        pins |= FAM65XX_IRQ;   /* IRQ line high (inactive) */
        pins |= FAM65XX_NMI;   /* NMI line high (inactive) */
        pins |= FAM65XX_RES;   /* RESET line high (inactive) */
        
        /* Clear any interrupt flags that might have been set */
        this->brk_flags = 0;
        
        /* CRITICAL: Reset interrupt shift register to prevent false triggers */
        this->interrupt_shift_register = 0x00000000;  /* No interrupt activity detected yet */
        this->nmi_prev = 1;  /* NMI line starts high (inactive) for edge detection */
        
        /* Set up for instruction fetch - CPU ready to execute next instruction */
        this->transition_to_fetch();
        
        return pins;
    }
    
    bus_state_t reset(bus_state_t pins) {
        // Reset CPU state
        CPU_A(this) = 0x00;
        CPU_X(this) = 0x00;
        CPU_Y(this) = 0x00;
        CPU_S(this) = 0xFF;
        CPU_P(this) = FLAG_U | FLAG_I; // Unused bit set, interrupts disabled
        
        // Reset interrupt state
        this->brk_flags = 0;
        this->nmi_prev = 0;
        this->interrupt_shift_register = 0;
        
        // Reset execution state
        this->cycle_index = 0;
        this->current_handler = nullptr;
        
        // Reset 65C02 extended state
        this->wait_for_interrupt = false;
        this->stopped = false;
        
        // Reset processor-specific features
        this->init_conditional_features();
        
        // Load reset vector
        CPU_AB(this) = 0xFFFC;
        pins = this->phi2_read(pins, REG_AB, REG_PCL);
        CPU_AB(this) = 0xFFFD;
        pins = this->phi2_read(pins, REG_AB, REG_PCH);
        
        // Start fetch cycle
        this->transition_to_fetch();
        
        return pins;
    }
    
    bus_state_t tick(bus_state_t pins) {
        trace_enter("tick");
        trace_registers("before");
        
        // SYNC pin management - asserted during opcode fetch cycles (matching old implementation)
        if (this->current_handler == &fam65xx_t::fetch_opcode && this->cycle_index == 0) {
            pins |= FAM65XX_SYNC;
            trace("SYNC asserted (opcode fetch)");
        } else {
            pins &= ~FAM65XX_SYNC;
        }
        
        // Hardware-accurate interrupt detection every cycle (matching old implementation)
        if (this->process_interrupt_detection(pins)) {
            // Interrupt detected - check if we should hijack current instruction
            if (this->brk_flags & FAM65XX_BRK_RESET) {
                // RESET has highest priority - immediately start RESET sequence
                return reset(pins);
            } else if (this->current_handler == &fam65xx_t::fetch_opcode && this->cycle_index == 0) {
                // At instruction boundary - start interrupt sequence (matching old implementation)
                // Use op_brk as unified interrupt handler like old implementation
                this->current_handler = &fam65xx_t::op_brk;
                this->cycle_index = 0;
                // Continue with op_brk handler execution this cycle
                pins = (this->*this->current_handler)(pins);
                return pins;
            }
        }
        
        // Execute current instruction cycle
        if (this->current_handler != nullptr) {
            trace("Executing handler (cycle %d)", this->cycle_index);
            pins = (this->*this->current_handler)(pins);
        } else {
            // Start new instruction fetch - should not happen with proper initialization
            trace("No handler - starting fetch_opcode");
            pins = this->fetch_opcode(pins);
        }
        
        trace_registers("after");
        trace_exit("tick");
        return pins;
    }
    
    bool opdone() const {
        return this->current_handler == &fam65xx_t::fetch_opcode;
    }
    
    // ========================================================================
    // PHI2 UNIFIED MEMORY ACCESS WITH PROCESSOR-VARIANT RDY HANDLING
    // ========================================================================
    
    /*
     * PHI2 Write with Hardware-Accurate RDY Behavior
     *
     * The RDY (Ready) pin allows external hardware to stretch CPU cycles by pulling
     * RDY low, causing the CPU to wait until RDY goes high before continuing.
     * This is essential for interfacing with slower memory or I/O devices.
     *
     * PROCESSOR-VARIANT BEHAVIOR:
     * ===========================
     *
     * NMOS processors (6502, 6510, NES 6502):
     *   - HARDWARE BUG: Write cycles ignore RDY and always complete
     *   - Read cycles respect RDY and will stretch when RDY is low
     *   - This asymmetry is a well-documented NMOS design flaw
     *
     * CMOS processors (65C02, 65C816):
     *   - BUG FIXED: Both read AND write cycles respect RDY consistently
     *   - When RDY is low, the cycle stretches and NO bus operation occurs
     *   - This allows proper interfacing with slow devices on both reads and writes
     *
     * Returns true if the operation completed and caller should advance to next cycle.
     * Returns false if RDY is low and the cycle should be repeated next tick.
     */
    // Check if write cycle should complete based on RDY state and processor variant
    inline bool should_complete_write_cycle(bus_state_t pins) {
        // NMOS processors (6502/6510/NES6502): Ignore RDY during write cycles (hardware bug)
        // CMOS processors (65C02/65C816): Respect RDY during write cycles (bug fixed)
        if constexpr (has_nmos_bugs<ProcessorTag>()) {
            // NMOS: Always complete write regardless of RDY state (matches hardware bug)
            return true;
        } else {
            // CMOS: Only complete write when RDY is high (proper behavior)
            return FAM65XX_GET_RDY(pins);
        }
    }

    // Unified memory write function with optional I/O port handling
    bus_state_t phi2_write(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) {
        // Get address and data for both checks and bus operations
        uint16_t addr = this->reg16[addr_reg];
        uint8_t data = this->reg8[data_reg];
        
        // Standard bus write for all other addresses
        pins = BUS_SET_ADDR(pins, addr);
        pins = BUS_SET_DATA(pins, data);
        pins &= ~FAM65XX_RW; // Set WRITE mode
        
        // Handle 6510 I/O port access (compile-time conditional)
        if constexpr (has_io_port<ProcessorTag>()) {
            if (addr == 0x0000) {
                this->write_io_ddr(data);
                return pins; // Don't perform bus write
            } else if (addr == 0x0001) {
                this->write_io_data(data);
                return pins; // Don't perform bus write
            }
        }
        
        // Use memory callback if available
        if (this->mem_write != nullptr) {
            this->mem_write(this->mem_user_data, addr, data);
        }
        
        return pins;
    }
    
    /*
     * PHI2 Read with Hardware-Accurate RDY Behavior
     *
     * Both NMOS and CMOS processors respect RDY during read cycles, but the
     * implementation details vary slightly.
     *
     * PROCESSOR-VARIANT BEHAVIOR:
     * ===========================
     *
     * NMOS processors (6502, 6510, NES 6502):
     *   - Read cycles respect RDY and stretch when RDY is low
     *   - Address bus retains previous value when RDY is low
     *   - Data is still read from bus (for VIC-II compatibility)
     *
     * CMOS processors (65C02, 65C816):
     *   - Read cycles respect RDY and stretch when RDY is low
     *   - Address bus behavior is consistent with NMOS
     *   - Data reading behavior is identical to NMOS
     *
     * VIC-II BUS ARBITRATION - WHY phi2_read() IS ALWAYS CALLED:
     * ==========================================================
     * Hardware Behavior When RDY Signal Changes:
     * ┌─────────────────┬──────────────────┬─────────────────────────────┐
     * │   Component     │    RDY High      │         RDY Low             │
     * ├─────────────────┼──────────────────┼─────────────────────────────┤
     * │ CPU             │ Normal execution │ STALLED (repeats cycle)     │
     * │ VIC-II          │ Gets bus access  │ CONTINUES (needs bus)       │
     * │ Address Bus     │ CPU's address    │ VIC-II's address (previous) │
     * │ Memory Access   │ CPU operation    │ VIC-II operation            │
     * │ Bus Pins Update │ Required         │ STILL REQUIRED              │
     * └─────────────────┴──────────────────┴─────────────────────────────┘
     *
     * This is why phi2_read() must ALWAYS be called regardless of RDY state:
     * - When RDY=1: CPU performs its memory access normally
     * - When RDY=0: VIC-II continues its memory access, CPU waits but bus must be serviced
     * - Memory callbacks and pin updates are essential for VIC-II operation
     * - Address bus behavior changes based on RDY but memory access always occurs
     */
    
    // VIC-II compatible memory read with proper RDY handling (matching old implementation)
    bus_state_t phi2_read(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) {
        uint16_t address;

        // Set R/W̅ bit to indicate READ (1 = Read, 0 = Write)
        pins |= FAM65XX_RW;

        // Hardware-accurate RDY handling - address bus behavior matches old implementation
        if (FAM65XX_GET_RDY(pins)) {
            address = this->reg16[addr_reg];
            pins = BUS_SET_ADDR(pins, address);
            if constexpr (has_io_port<ProcessorTag>()) {
                // Handle 6510 I/O port access (only when RDY is high)
                if (address == 0x0000) {
                    this->reg8[data_reg] = this->read_io_port();
                    return pins; // Don't perform bus read
                } else if (address == 0x0001) {
                    this->reg8[data_reg] = this->io_port.direction;
                    return pins; // Don't perform bus read
                }
            }
        } else {
            address = BUS_GET_ADDR(pins);  // Keep existing bus address when RDY low
        }
    
        // Always perform memory read to service VIC-II even when CPU halted
        uint8_t current_bus_data = BUS_GET_DATA(pins);
        uint8_t data = 0xFF; // Default floating bus
        if (this->mem_read != nullptr) {
            data = this->mem_read(this->mem_user_data, address, current_bus_data);
        }
        pins = BUS_SET_DATA(pins, data);
        this->reg8[data_reg] = data;
        
        return pins;
    }
    
    // Optimized version with direct register targeting to eliminate copies
    inline bus_state_t phi2_read_operand(bus_state_t pins, reg8_t target_reg) {
        if (this->opcode_entry.am_index == AM_IMM) {
            // Immediate mode - read from PC directly into target register
            pins = phi2_read(pins, REG_PC, target_reg);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
            }
            return pins;
        }

        // Memory mode - read from target address directly into target register
        return phi2_read(pins, REG_AB, target_reg);
    }
    
    // ========================================================================
    // OPERATION IMPLEMENTATIONS (included via .inc.hpp files)
    // ========================================================================
    
    // Include all operation implementations
    // These .inc.hpp files contain function definitions that will be compiled
    // as part of this template class, allowing conditional compilation
    // based on ProcessorTag features
    
#include "operations/addressing_modes.inc.hpp"  // Addressing mode handlers
#include "operations/arithmetic.inc.hpp"        // ADC, SBC, CMP operations  
#include "operations/memory.inc.hpp"           // Load/store operations
#include "operations/control.inc.hpp"          // Control flow operations
#include "operations/branches.inc.hpp"         // Branch operations
#include "operations/stack.inc.hpp"            // Stack operations
#include "operations/transfers.inc.hpp"        // Register transfer operations
#include "operations/flags.inc.hpp"            // Flag manipulation operations
#include "operations/rmw.inc.hpp"              // Read-modify-write operations
#include "operations/illegal.inc.hpp"          // Illegal/undocumented opcodes
#include "operations/cmos.inc.hpp"             // 65C02 enhancements
#include "operations/wide.inc.hpp"             // 65C816 16-bit operations
    
    // ========================================================================
    // OPCODE TABLE GENERATION
    // ========================================================================
    
    // Generate processor-specific opcode table at compile time
    static constexpr opcode_info_t get_opcode_info(uint8_t opcode) {
        // This will be specialized per processor type after table generation
        return generate_opcode_table<ProcessorTag>()[opcode];
    }
    
    // ========================================================================
    // HELPER FUNCTIONS (needed by operation files)
    // ========================================================================

    // === Building blocks ===
    inline void set_flag(uint8_t flag_mask) {
        CPU_P(this) |= flag_mask;
    }
    
    inline void clear_flag(uint8_t flag_mask) {
        CPU_P(this) &= ~flag_mask;
    }
    
    // === Foundation: Single memory write ===
    inline void update_flags(uint8_t clear_mask, uint8_t set_mask) {
        CPU_P(this) = (CPU_P(this) & ~clear_mask) | set_mask;
    }
    
    inline void update_flag(uint8_t flag_mask, bool condition) {
        update_flags(flag_mask, -condition & flag_mask);
    }
    
    // === Helpers: Branchless flag calculations ===
    inline uint8_t calc_n_flag(uint8_t value) {
        return value & FLAG_N;  // Extract bit 7 (sign bit)
    }    

    inline uint8_t calc_z_flag(uint8_t value) {
        return -(value == 0) & FLAG_Z;
    }

    inline uint8_t calc_c_flag(uint16_t result) {
        return (result >> 8) & FLAG_C;  // Extract bit 8 (carry/borrow bit)
    }

    // Overflow for addition: sign bit of result differs from both inputs with same sign
    inline uint8_t calc_v_flag_add(uint8_t old_a, uint8_t operand, uint16_t result) {
        return (((old_a ^ result) & (operand ^ result)) >> 1) & FLAG_V;
    }

    // Overflow for subtraction: sign bit of result differs from minuend when inputs differ in sign
    inline uint8_t calc_v_flag_sub(uint8_t old_a, uint8_t operand, uint16_t result) {
        return (((old_a ^ operand) & (old_a ^ result)) >> 1) & FLAG_V;
    }

    // === Derived operations ===
    inline void update_c_flag(uint8_t value, uint8_t bit_position) {
        update_flag(FLAG_C,
                   (value >> bit_position) & FLAG_C);
    }

    inline void update_nz_flags(uint8_t value) {
        update_flags(FLAG_N | FLAG_Z, 
                    calc_n_flag(value) |
                    calc_z_flag(value));
    }                

    inline void update_nzc_flags(uint8_t minuend, uint8_t subtrahend) {
        uint16_t result = minuend - subtrahend;
        
        update_flags(FLAG_N | FLAG_Z | FLAG_C,
                    calc_n_flag(result) | 
                    calc_z_flag(result) |
                    calc_c_flag(~result));  // Inverted: carry set when no borrow
    }

    inline void update_nvz_flags(uint8_t operand, uint8_t and_result) {
        update_flags(FLAG_N | FLAG_V | FLAG_Z,
                    (operand & (FLAG_N | FLAG_V)) | 
                    calc_z_flag(and_result));
    }

    inline void update_flags_adc(uint8_t old_a, uint8_t operand, uint16_t result) {
        update_flags(FLAG_N | FLAG_V | FLAG_Z | FLAG_C,
                    calc_n_flag(result) | 
                    calc_z_flag(result) |
                    calc_c_flag(result) |     // Direct: carry set when overflow
                    calc_v_flag_add(old_a, operand, result));
    }

    inline void update_flags_sbc(uint8_t old_a, uint8_t operand, uint16_t result) {
        update_flags(FLAG_N | FLAG_V | FLAG_Z | FLAG_C,
                    calc_n_flag(result) | 
                    calc_z_flag(result) |
                    calc_c_flag(~result) |    // Inverted: carry set when no borrow
                    calc_v_flag_sub(old_a, operand, result));
    }

    // === Carry/Borrow Input Helpers ===
    inline uint8_t get_borrow_input() const {
        return (CPU_P(this) & FLAG_C) ^ FLAG_C;  // Branchless: XOR flips bit, result is 1 or 0
    }

    inline uint8_t get_carry_bit_7() const {
        return (CPU_P(this) & FLAG_C) << 7;  // Shift bit 0 to bit 7
    }

    inline uint8_t get_carry_bit_0() const {
        return CPU_P(this) & FLAG_C;  // Returns FLAG_C (0x01) or 0x00
    }

    // Check if page was crossed during addressing
    bool page_crossed(uint16_t addr1, uint16_t addr2) const {
        return ((addr1 ^ addr2) & 0x0100) != 0;
    }
    
private:
    // ========================================================================
    // INTERNAL HELPER FUNCTIONS AND DECLARATIONS
    // ========================================================================
    
    // Template-dependent function pointer type
    using InstructionHandler = bus_state_t (fam65xx_t<ProcessorTag>::*)(bus_state_t);
    
    // Lookup tables for handlers (initialized during init)
    std::array<InstructionHandler, OP_COUNT> operation_handlers;
    std::array<InstructionHandler, AM_COUNT> addressing_mode_handlers;
    
    // Essential helper functions for template functionality
    inline InstructionHandler get_instruction_handler() {
        // For addressing modes that need address calculation, start with addressing mode handler
        if (this->opcode_entry.am_index > AM_IMM) {
            return addressing_mode_handlers[this->opcode_entry.am_index];
        }
        
        // For immediate mode and implied operations, go directly to operation
        return this->operation_handlers[this->opcode_entry.op_index];
    }
    
    // Hardware-accurate interrupt detection (matching old implementation)
    bool process_interrupt_detection(bus_state_t pins) {
        // Load state into registers to reduce memory accesses
        uint32_t shift_reg = this->interrupt_shift_register;
        uint8_t nmi_prev = this->nmi_prev;
        
        // Shift and clear separators (prevent cross-over) in one operation
        shift_reg = (shift_reg << 1) & ~INT_SEPARATOR_MASK;
        
        // Extract interrupt pins (bits 32-34) and invert (active low)
        // After shift: bit 0=RES, bit 1=IRQ, bit 2=NMI
        uint32_t int_pins = (~pins) >> BUS_RES_BIT;
        
        // Pin bit offsets after extraction
        constexpr uint8_t IRQ_OFFSET = BUS_IRQ_BIT - BUS_RES_BIT;  // 1
        constexpr uint8_t NMI_OFFSET = BUS_NMI_BIT - BUS_RES_BIT;  // 2
        
        // Sample IRQ (extracted bit 1 -> shift_reg bit 0)
        shift_reg |= (int_pins >> IRQ_OFFSET) & (1 << INT_IRQ_START_BIT);
        
        // NMI edge detection (extracted bit 2)
        uint8_t nmi_current = (int_pins >> NMI_OFFSET) & 0x1;
        shift_reg |= (-(nmi_prev & !nmi_current)) & (1 << INT_NMI_START_BIT);
        
        // Sample RESET (extracted bit 0 -> shift_reg bit 8)
        shift_reg |= (int_pins & 0x1) << INT_RESET_START_BIT;
        
        // Store updated state
        this->interrupt_shift_register = shift_reg;
        this->nmi_prev = nmi_current;
        
        // Check for completed interrupt sequences in order of priority
        
        // Check if RESET has completed shift (3 consecutive cycles) - highest priority
        if ((shift_reg & INT_RESET_MASK) == INT_RESET_MASK) {
            this->brk_flags |= FAM65XX_BRK_RESET;
            return true;
        }
        
        // Check if NMI has completed shift (3 consecutive cycles) - middle priority
        if ((shift_reg & INT_NMI_MASK) == INT_NMI_MASK) {
            this->brk_flags |= FAM65XX_BRK_NMI;
            return true;
        }
        
        // Check if IRQ has completed shift (3 consecutive cycles) - lowest priority
        // IRQ is masked by the I flag (interrupt disable)
        if ((shift_reg & INT_IRQ_MASK) == INT_IRQ_MASK && !(CPU_P(this) & FLAG_I)) {
            this->brk_flags |= FAM65XX_BRK_IRQ;
            return true;
        }
        
        return false;
    }
    
    // Instruction fetch and decode
    bus_state_t fetch_opcode(bus_state_t pins) {
        trace_enter("fetch_opcode");
        
        // Read opcode from PC
        CPU_AB(this) = CPU_PC(this);
        trace("Fetching opcode from PC=%04X", CPU_PC(this));
        pins = this->phi2_read(pins, REG_AB, REG_IR);
        CPU_PC(this)++;
        
        // Set SYNC signal for opcode fetch
        pins |= FAM65XX_SYNC;
        
        // Decode opcode and set up instruction
        uint8_t opcode = CPU_IR(this);
        trace("Fetched opcode: %02X", opcode);
        this->opcode_entry = get_opcode_info(opcode);
        this->cycle_index = 0;
        
        // Set up first instruction cycle handler
        this->current_handler = this->get_instruction_handler();
        trace("Set up handler for opcode %02X", opcode);
        
        trace_exit("fetch_opcode");
        return pins;
    }
    
public:
    // Transition to next instruction fetch (public for bootstrap function)
    void transition_to_fetch() {
        this->current_handler = &fam65xx_t::fetch_opcode;
        this->cycle_index = 0;
    }

private:
    // ========================================================================
    // ESSENTIAL TEMPLATE FUNCTIONS (needed for real CPU implementation)
    // ========================================================================
    
    void transition_to_operation() {
        this->cycle_index = 0;
        this->current_handler = this->operation_handlers[this->opcode_entry.op_index];
    }
    
    void init_conditional_features() {
        // Initialize I/O port if present
        if constexpr (has_io_port<ProcessorTag>()) {
            this->init_io_port();
        }
        
        // BCD and CMOS state are now integrated into main class
        // 16-bit wide registers disabled for now
    }
    
    void init_opcode_table() {
        // Initialize operation handler lookup table with NOP as safe default
        operation_handlers.fill(&fam65xx_t::op_nop);
        
        // Memory operations (implemented)
        operation_handlers[OP_LDA] = &fam65xx_t::op_lda;
        operation_handlers[OP_LDX] = &fam65xx_t::op_ldx;
        operation_handlers[OP_LDY] = &fam65xx_t::op_ldy;
        operation_handlers[OP_STA] = &fam65xx_t::op_sta;
        operation_handlers[OP_STX] = &fam65xx_t::op_stx;
        operation_handlers[OP_STY] = &fam65xx_t::op_sty;
        operation_handlers[OP_AND] = &fam65xx_t::op_and;
        operation_handlers[OP_ORA] = &fam65xx_t::op_ora;
        operation_handlers[OP_EOR] = &fam65xx_t::op_eor;
        operation_handlers[OP_BIT] = &fam65xx_t::op_bit;
        
        // Arithmetic operations (implemented)
        operation_handlers[OP_ADC] = &fam65xx_t::op_adc;
        operation_handlers[OP_SBC] = &fam65xx_t::op_sbc;
        operation_handlers[OP_CMP] = &fam65xx_t::op_cmp;
        operation_handlers[OP_CPX] = &fam65xx_t::op_cpx;
        operation_handlers[OP_CPY] = &fam65xx_t::op_cpy;
        operation_handlers[OP_INC] = &fam65xx_t::op_inc;
        operation_handlers[OP_DEC] = &fam65xx_t::op_dec;
        operation_handlers[OP_NOP] = &fam65xx_t::op_nop;
        operation_handlers[OP_JAM] = &fam65xx_t::op_jam;
        
        // RMW operations (implemented)
        operation_handlers[OP_ASL] = &fam65xx_t::op_asl;
        operation_handlers[OP_LSR] = &fam65xx_t::op_lsr;
        operation_handlers[OP_ROL] = &fam65xx_t::op_rol;
        operation_handlers[OP_ROR] = &fam65xx_t::op_ror;
        
        // Control operations (implemented)
        operation_handlers[OP_JMP] = &fam65xx_t::op_jmp;
        operation_handlers[OP_JSR] = &fam65xx_t::op_jsr;
        operation_handlers[OP_RTS] = &fam65xx_t::op_rts;
        operation_handlers[OP_BRK] = &fam65xx_t::op_brk;
        operation_handlers[OP_RTI] = &fam65xx_t::op_rti;
        
        // Register operations (implemented)
        operation_handlers[OP_INX] = &fam65xx_t::op_inx;
        operation_handlers[OP_INY] = &fam65xx_t::op_iny;
        operation_handlers[OP_DEX] = &fam65xx_t::op_dex;
        operation_handlers[OP_DEY] = &fam65xx_t::op_dey;
        
        // Transfer operations (implemented)
        operation_handlers[OP_TAX] = &fam65xx_t::op_tax;
        operation_handlers[OP_TAY] = &fam65xx_t::op_tay;
        operation_handlers[OP_TXA] = &fam65xx_t::op_txa;
        operation_handlers[OP_TYA] = &fam65xx_t::op_tya;
        operation_handlers[OP_TSX] = &fam65xx_t::op_tsx;
        operation_handlers[OP_TXS] = &fam65xx_t::op_txs;
        
        // Stack operations (implemented)
        operation_handlers[OP_PHA] = &fam65xx_t::op_pha;
        operation_handlers[OP_PHP] = &fam65xx_t::op_php;
        operation_handlers[OP_PLA] = &fam65xx_t::op_pla;
        operation_handlers[OP_PLP] = &fam65xx_t::op_plp;
        
        // Branch operations (implemented)
        operation_handlers[OP_BCC] = &fam65xx_t::op_bcc;
        operation_handlers[OP_BCS] = &fam65xx_t::op_bcs;
        operation_handlers[OP_BEQ] = &fam65xx_t::op_beq;
        operation_handlers[OP_BNE] = &fam65xx_t::op_bne;
        operation_handlers[OP_BMI] = &fam65xx_t::op_bmi;
        operation_handlers[OP_BPL] = &fam65xx_t::op_bpl;
        operation_handlers[OP_BVC] = &fam65xx_t::op_bvc;
        operation_handlers[OP_BVS] = &fam65xx_t::op_bvs;
        
        // Flag operations (implemented)
        operation_handlers[OP_CLC] = &fam65xx_t::op_clc;
        operation_handlers[OP_SEC] = &fam65xx_t::op_sec;
        operation_handlers[OP_CLI] = &fam65xx_t::op_cli;
        operation_handlers[OP_SEI] = &fam65xx_t::op_sei;
        operation_handlers[OP_CLD] = &fam65xx_t::op_cld;
        operation_handlers[OP_SED] = &fam65xx_t::op_sed;
        operation_handlers[OP_CLV] = &fam65xx_t::op_clv;
        
        // Illegal operations (implemented, conditionally compiled)
        operation_handlers[OP_LAX] = &fam65xx_t::op_lax;
        operation_handlers[OP_SAX] = &fam65xx_t::op_sax;
        operation_handlers[OP_DCP] = &fam65xx_t::op_dcp;
        operation_handlers[OP_ISC] = &fam65xx_t::op_isc;
        operation_handlers[OP_SLO] = &fam65xx_t::op_slo;
        operation_handlers[OP_RLA] = &fam65xx_t::op_rla;
        operation_handlers[OP_SRE] = &fam65xx_t::op_sre;
        operation_handlers[OP_RRA] = &fam65xx_t::op_rra;
        operation_handlers[OP_ANC] = &fam65xx_t::op_anc;
        operation_handlers[OP_ARR] = &fam65xx_t::op_arr;
        operation_handlers[OP_ASR] = &fam65xx_t::op_asr;
        operation_handlers[OP_XAA] = &fam65xx_t::op_xaa;
        operation_handlers[OP_SBX] = &fam65xx_t::op_sbx;
        operation_handlers[OP_SHA] = &fam65xx_t::op_sha;
        operation_handlers[OP_SHS] = &fam65xx_t::op_shs;
        operation_handlers[OP_SHX] = &fam65xx_t::op_shx;
        operation_handlers[OP_SHY] = &fam65xx_t::op_shy;
        operation_handlers[OP_LAS] = &fam65xx_t::op_las;
        
        // Initialize addressing mode handler lookup table
        addressing_mode_handlers.fill(nullptr);  // Default to nullptr (safe for AM_NON/AM_IMM)
        
        // Addressing modes (implemented)
        addressing_mode_handlers[AM_NON] = nullptr;   // No handler needed (implicit/accumulator/relative)
        addressing_mode_handlers[AM_IMM] = nullptr;   // No handler (handled directly in operations)
        addressing_mode_handlers[AM_ZER] = &fam65xx_t::addr_zp;
        addressing_mode_handlers[AM_ZPX] = &fam65xx_t::addr_zpx;
        addressing_mode_handlers[AM_ZPY] = &fam65xx_t::addr_zpy;
        addressing_mode_handlers[AM_ABS] = &fam65xx_t::addr_abs;
        addressing_mode_handlers[AM_ABX] = &fam65xx_t::addr_abx;
        addressing_mode_handlers[AM_ABY] = &fam65xx_t::addr_aby;
        addressing_mode_handlers[AM_IND] = &fam65xx_t::addr_ind;
        addressing_mode_handlers[AM_INX] = &fam65xx_t::addr_inx;
        addressing_mode_handlers[AM_INY] = &fam65xx_t::addr_iny;
        
        // 65C02 addressing modes (if implemented)
        addressing_mode_handlers[AM_ZPI] = &fam65xx_t::addr_zp_ind;  // Zero Page Indirect
        addressing_mode_handlers[AM_ABI] = &fam65xx_t::addr_ind_abs; // Absolute Indexed Indirect
    }
};

// ============================================================================
// OPCODE TABLE GENERATION (processor-specific specializations were included above)
// ============================================================================

} // namespace fam65xx_cpp