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
 * fam65xx::fam65xx_t<MOS6510Tag> c64_cpu;
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

// Include instruction decoder for disassembly
extern "C" {
    #include "fam65xx_decoder.h"
}

#include "fam65xx_types.h"
#include "fam65xx_processor_traits.hpp"
#include "fam65xx_register_mixins.hpp"
#include "fam65xx_mixins.hpp"

// ============================================================================
// C++ NAMESPACE - MAIN CPU TEMPLATE IMPLEMENTATION
// ============================================================================

namespace fam65xx {

// ============================================================================
// FORWARD DECLARATIONS FOR OPCODE TABLE GENERATION
// ============================================================================

// Forward declaration for opcode table generation function
constexpr std::array<opcode_info_t, 256> generate_opcode_table_for_traits(const CPUTraits& traits);

// Include opcode table generation implementation first
#include "operations/opcode_tables.inc.hpp"

// Now define the template function
template<const CPUTraits& Traits>
constexpr std::array<opcode_info_t, 256> generate_opcode_table() {
    // This will use the generate_opcode_table_for_traits function from opcode_tables.inc.hpp
    return generate_opcode_table_for_traits(Traits);
}

// ============================================================================
// INTERRUPT SHIFT REGISTER CONSTANTS (use definitions from fam65xx_types.h)
// ============================================================================
// Note: INT_* constants are defined in fam65xx_types.h and used here via C header inclusion

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template<const CPUTraits& Traits>
class fam65xx_t :
    public io_port_base_t<Traits>,
    public apu_base_t<Traits>,
    public register_base_t<Traits>
{
public:
    // Data type alias from the register mixin
    using data_t = typename register_base_t<Traits>::data_t;
    
    // CPUTraits-based feature detection helpers for operations files
    static constexpr bool has_illegal_opcodes() { return Traits.has(CPUCoreFlags::ILLEGAL_OPCODES); }
    static constexpr bool has_bcd() { return Traits.has(CPUCoreFlags::HAS_DECIMAL_MODE); }
    static constexpr bool has_bcd_extra_cycle() { return Traits.has(CPUCoreFlags::BCD_EXTRA_CYCLE); }
    static constexpr bool has_bcd_nmos_flags() { return Traits.has(CPUCoreFlags::BCD_NMOS_FLAGS); }
    static constexpr bool has_optimized_cycles() { return Traits.has(CPUCoreFlags::OPTIMIZED_CYCLES); }
    static constexpr bool has_cmos() { return Traits.has(CPUCoreFlags::CMOS_BASE); }
    static constexpr bool has_wide_registers() { return Traits.has(CPUCoreFlags::C816_16BIT); }
    static constexpr bool has_nmos_bugs() { return Traits.is_nmos(); }
    static constexpr bool has_io_port() { return Traits.has_io_port(); }
    static constexpr bool has_apu() { return Traits.has_apu(); }
    static constexpr bool has_rmw_dummy_write() { return Traits.has(CPUCoreFlags::RMW_DUMMY_WRITE); }
    static constexpr bool has_nmi_line() { return !Traits.has(CPUCoreFlags::NO_NMI_LINE); }
    static constexpr bool has_irq_line() { return !Traits.has(CPUCoreFlags::NO_IRQ_LINE); }
    
    // ========================================================================
    // CPU STATE
    // ========================================================================
    
    /* Current execution state */
    opcode_info_t opcode_entry;         /* Cached opcode entry (copied once) */
    bus_state_t (fam65xx_t::*current_handler)(bus_state_t);  /* Current instruction handler */
    uint8_t cycle_index;                /* Current cycle within instruction */
    
    /* Interrupt state - hardware-accurate shift register system */
    interrupt_t active_interrupt;       /* Currently active interrupt (enum serves as vector index) */
    uint8_t nmi_prev;                   /* Previous NMI line state for edge detection */
    uint32_t interrupt_shift_register;  /* Combined shift register for all interrupt types */
    
    /* Memory callbacks (for test runner compatibility) */
    fam65xx_mem_read_t mem_read;        /* Memory read callback */
    fam65xx_mem_write_t mem_write;      /* Memory write callback */
    void* mem_user_data;                /* User data for memory callbacks */
    
    /* 65C02 extended state */
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
        opcode_entry = {};
        current_handler = nullptr;
        cycle_index = 0;
        active_interrupt = FAM65XX_INT_NONE;
        nmi_prev = 0;
        interrupt_shift_register = 0;
        mem_read = nullptr;
        mem_write = nullptr;
        mem_user_data = nullptr;
        wait_for_interrupt = false;
        stopped = false;
        trace_indent = 0;
        
        // Initialize registers
        this->init_registers();
        
        // Initialize conditional features
        this->init_conditional_features();
        
        // Initialize operation and addressing mode handlers
        this->init_opcode_table();
    }
    
    ~fam65xx_t() {
        // Cleanup conditional features
        if constexpr (has_apu()) {
            this->destroy_apu();
        }
    }
    
    // ========================================================================
    // DEBUG TRACING HELPERS
    // ========================================================================
    
    template<const bool trace_instructions = false>
    void print_instruction_trace() const {
        
        if constexpr (trace_instructions)
        {
            static int instruction_count = 0;
            if (instruction_count < 100) {
                uint16_t pc = this->get(REG_PC);
                uint16_t ab = this->get(REG_AB);  // Get address bus from register
                uint8_t ir = this->get(REG_IR);  // Get instruction register
                
                // Use the current opcode_entry which is properly set during reset to BRK
                // This shows the logical instruction being executed (BRK during reset)
                // rather than whatever random data is in the IR register
                const char* opcode_name = fam65xx_get_opcode_name(this->opcode_entry.op_index);
                
                printf("[%03d] PC=$%04X AB=$%04X IR=$%02X (%s) ", 
                    instruction_count, pc, ab, ir, opcode_name);
                
                // Get instruction info if opcode is valid
                printf("A=$%02X X=$%02X Y=$%02X S=$%02X P=$%02X\n", 
                    this->get(REG_A), this->get(REG_X), this->get(REG_Y), 
                    this->get(REG_SPL), this->get(REG_P));
                    
                instruction_count++;
                if (instruction_count == 100) {
                    printf("=== Instruction trace complete (%d instructions) ===\n", instruction_count);
                }
            }
        }
    }
    
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
                  this->get(REG_PC), this->get(REG_A), this->get(REG_X), this->get(REG_Y),
                  this->get(REG_P), this->get(REG_S));
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
    
    bus_state_t init(const chip_descriptor_t* /*desc*/) {
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
        * SP = 0x01FF (stack starts at top of page 1)
        */
        this->set(REG_SP, 0x01FF); /* Stack pointer (page 1, starts at 0xFF) */

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
        this->active_interrupt = FAM65XX_INT_NONE;
        
        /* CRITICAL: Reset interrupt shift register to prevent false triggers */
        this->interrupt_shift_register = 0x00000000;  /* No interrupt activity detected yet */
        this->nmi_prev = 1;  /* NMI line starts high (inactive) for edge detection */
        
        /* Set up for instruction fetch - CPU ready to execute next instruction */
        this->transition_to_fetch();
        
        return pins;
    }
    
    bus_state_t reset(bus_state_t pins) {
        // Reset CPU state
        this->set(REG_A, 0x00);
        this->set(REG_X, 0x00);
        this->set(REG_Y, 0x00);
        this->set(REG_SP, 0x01FF);
        this->set(REG_P, FLAG_U | FLAG_I); // Unused bit set, interrupts disabled
        
        // Reset interrupt state
        this->nmi_prev = 0;
        this->interrupt_shift_register = 0;
        
        // Reset 65C02 extended state
        this->wait_for_interrupt = false;
        this->stopped = false;
        
        // Reset processor-specific features
        this->init_conditional_features();
        
        // Use unified interrupt handler for vector loading
        // Skip stack operations (cycles 0-3) and jump to vector loading (cycles 4-5)
        this->active_interrupt = FAM65XX_INT_RESET;
        
        // Set up opcode_entry for BRK (opcode $00) so tracing shows correct instruction
        this->opcode_entry = get_opcode_info(0x00);// = {OP_BRK, AM_NON, OF_NONE};
        this->current_handler = &fam65xx_t::op_brk;
        this->cycle_index = 4;  // Jump to vector loading phase
        this->set(REG_AB, this->get_vector_addr()); // Do the same memory setup as preceding op_brk cycle 3
        
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
            if (this->active_interrupt == FAM65XX_INT_RESET) {
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
        
        // Clock APU if present (every CPU cycle)
        if constexpr (has_apu()) {
            pins = this->clock_apu(pins);
        }
        
        // Print instruction trace for debugging (covers all memory accesses including reset)
        this->print_instruction_trace();
        
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
        if constexpr (has_nmos_bugs()) {
            // NMOS: Always complete write regardless of RDY state (matches hardware bug)
            return true;
        } else {
            // CMOS: Only complete write when RDY is high (proper behavior)
            return FAM65XX_GET_RDY(pins);
        }
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
     * 
     * This template method provides a unified interface for all bus operations
     * with compile-time optimization based on processor traits. It serves as
     * the foundation for implementing cycle-accurate, processor-variant-aware
     * bus operations.
     * 
     * Template Parameters:
     * - IsWrite: true for write operations, false for read operations
     * - IsDummy: true for dummy/internal cycles, false for data cycles
     * 
     * Features:
     * - Zero-overhead abstractions through template specialization
     * - Hardware-accurate RDY handling per processor variant
     * - Conditional DMA callback support for external devices
     * - Optimized bus line updates for simulation environments
     * - Memory callback integration with floating bus simulation
     * 
     * @param pins Current bus state (modified and returned)
     * @param addr Physical address for bus operation
     * @param data Data byte (for write operations, ignored for reads)
     * @return Updated bus state with operation results
     */
    template<bool IsWrite, bool IsDummy>
    bus_state_t phi2_access(bus_state_t pins, reg16_t addr_reg, uint8_t data = 0) {
        return phi2_access_impl<IsWrite, IsDummy>(pins, addr_reg, data);
    }
    
private:
    // Separate implementation to avoid unreachable code warnings
    template<bool IsWrite, bool IsDummy>
    bus_state_t phi2_access_impl(bus_state_t pins, reg16_t addr_reg, uint8_t data) {
        // Skip dummy cycles if not simulating internal timing
        if constexpr (IsDummy && !Traits.accurate_internal_cycles()) {
            return pins;
        }
        
        // Hardware-accurate RDY check - DMA device may have bus control
        if (!FAM65XX_GET_RDY(pins)) {
            // When RDY is low, external device (e.g., VIC-II) controls the bus
            // DMA device has bus control - use existing address/data from pins
            uint32_t dma_addr = FAM65XX_GET_ADDR(pins);
            uint8_t bus_data = FAM65XX_GET_DATA(pins);
            
            // Perform DMA memory access (typically read for VIC-II)
            uint8_t result_data = (this->mem_read != nullptr)
                ? this->mem_read(this->mem_user_data, dma_addr, bus_data)
                : bus_data;
            
            return FAM65XX_SET_DATA(pins, result_data);
        }
        
        // CPU has bus control - proceed with normal operation
        uint32_t addr = this->get(addr_reg);
        
        // Update bus lines if enabled (for test/simulation environments)
        if constexpr (Traits.update_bus_lines()) {
            pins = FAM65XX_SET_ADDR(pins, addr & Traits.address_mask());
            
            if constexpr (IsWrite) {
                pins &= ~FAM65XX_RW;  // Clear RW for write
            } else {
                pins |= FAM65XX_RW;   // Set RW for read
            }
        }
        
        uint8_t bus_data = FAM65XX_GET_DATA(pins);
        
        if constexpr (IsWrite) {
            return phi2_write_impl<IsDummy>(pins, addr, data);
        } else {
            return phi2_read_impl<IsDummy>(pins, addr, bus_data);
        }
    }
    
    template<bool IsDummy>
    bus_state_t phi2_write_impl(bus_state_t pins, uint32_t addr, uint8_t data) {
        // Handle processor-specific I/O port access (compile-time conditional)
        if constexpr (Traits.has_io_port()) {
            if (addr <= 0x0001) {
                if (addr == 0x0000) {
                    this->write_io_ddr(data);
                } else {
                    this->write_io_data(data);
                }
                // Still call memory callback for test compatibility
                if (this->mem_write != nullptr) {
                    this->mem_write(this->mem_user_data, addr, data);
                }
                return FAM65XX_SET_DATA(pins, data);
            }
        }
        
        // Handle APU register access (compile-time conditional)
        if constexpr (Traits.has_apu()) {
            if (this->write_apu_register(addr, data)) {
                // APU register handled, but still call memory callback
                if (this->mem_write != nullptr) {
                    this->mem_write(this->mem_user_data, addr, data);
                }
                return FAM65XX_SET_DATA(pins, data);
            }
        }
        
        // Standard memory write
        if (this->mem_write != nullptr) {
            this->mem_write(this->mem_user_data, addr, data);
        }
        
        return FAM65XX_SET_DATA(pins, data);
    }
    
    template<bool IsDummy>
    bus_state_t phi2_read_impl(bus_state_t pins, uint32_t addr, uint8_t bus_data) {
        // Handle processor-specific I/O port access first (compile-time conditional)
        if constexpr (Traits.has_io_port()) {
            if (addr <= 0x0001) {
                const uint8_t io_data = (addr == 0x0000)
                    ? this->read_io_port()
                    : this->io_port.direction;
                
                // For dummy reads, data is read but not used by CPU
                if constexpr (!IsDummy) {
                    pins = FAM65XX_SET_DATA(pins, io_data);
                }
                return pins;
            }
        }
        
        // Handle APU register access (compile-time conditional)
        if constexpr (Traits.has_apu()) {
            uint8_t apu_data;
            if (this->read_apu_register(addr, apu_data)) {
                // For dummy reads, data is read but not used by CPU
                if constexpr (!IsDummy) {
                    pins = FAM65XX_SET_DATA(pins, apu_data);
                }
                return pins;
            }
        }
        
        // Standard memory access for all other addresses
        uint8_t read_data = (this->mem_read != nullptr)
            ? this->mem_read(this->mem_user_data, addr, bus_data)
            : bus_data;  // Floating bus fallback
        
        // For dummy reads, data is read but not used by CPU
        if constexpr (!IsDummy) {
            pins = FAM65XX_SET_DATA(pins, read_data);
        }
        
        return pins;
    }

public:
    
    // ========================================================================
    // CONCRETE BUS ACCESS WRAPPERS (Reference Implementation Style)
    // ========================================================================
    
    /**
     * Concrete wrapper functions for the phi2_access template
     * These provide type-safe, easy-to-use interfaces while maintaining
     * the zero-overhead benefits of the template implementation.
     */
    
    inline bus_state_t phi2_read(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) {
        // Use the phi2_access template for read operation
        pins = phi2_access<false, false>(pins, addr_reg);
        
        // Store the result in the specified data register if CPU has bus control
        if (FAM65XX_GET_RDY(pins)) {
            this->load(data_reg, pins); // TOOD : Move to cycle code?
        }
        
        return pins;
    }
    
    // Optimized version with direct register targeting to eliminate copies
    inline bus_state_t phi2_read_operand(bus_state_t pins, reg8_t target_reg) {
        if (this->opcode_entry.am_index == to_index(AM::IMM)) {
            // Immediate mode - read from PC directly into target register
            pins = phi2_read(pins, REG_PC, target_reg);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
            }
            return pins;
        }
        // Memory mode - read from target address directly into target register
        return phi2_read(pins, REG_AB, target_reg);
    }

    inline bus_state_t phi2_write(bus_state_t pins, reg16_t addr_reg, uint8_t data) {
        return phi2_access<true, false>(pins, addr_reg, data);
    }
    
    inline bus_state_t phi2_dummy_read(bus_state_t pins, reg16_t addr_reg) {
        return phi2_access<false, true>(pins, addr_reg);
    }
    
    inline bus_state_t phi2_dummy_write(bus_state_t pins, reg16_t addr_reg, uint8_t data) {
        return phi2_access<true, true>(pins, addr_reg, data);
    }
    
    // ========================================================================
    // OPERATION IMPLEMENTATIONS (included via .inc.hpp files)
    // ========================================================================
    
    // Include all operation implementations
    // These .inc.hpp files contain function definitions that will be compiled
    // as part of this template class, allowing conditional compilation
    // based on CPUTraits features
    
    // Define template context guard for .inc.hpp files BEFORE including them
    #define FAM65XX_TEMPLATE_CONTEXT
    
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
#include "operations/rockwell.inc.hpp"         // Rockwell 65C02 bit manipulation
#include "operations/wide.inc.hpp"             // 65C816 16-bit operations
    
    // Undefine the guard after inclusion
    #undef FAM65XX_TEMPLATE_CONTEXT
    
    // ========================================================================
    // OPCODE TABLE GENERATION
    // ========================================================================
    
    // Generate processor-specific opcode table at compile time
    static constexpr opcode_info_t get_opcode_info(uint8_t opcode) {
        // This will be specialized per processor type after table generation
        return generate_opcode_table<Traits>()[opcode];
    }
    
    // Note: Register accessor functions are now provided by the register mixin
    // The mixin provides get(), set(), inc(), dec(), and load() functions
    // that automatically handle 8-bit vs 16-bit operation based on M/X flags
    // for 65C816 or provide simple 8-bit access for other CPUs.

    // ========================================================================
    // HELPER FUNCTIONS (needed by operation files)
    // ========================================================================

    // === Building blocks ===
    inline void set_flag(uint8_t flag_mask) {
        this->set(REG_P, this->get(REG_P) | flag_mask);
    }
    
    inline void clear_flag(uint8_t flag_mask) {
        this->set(REG_P, this->get(REG_P) & ~flag_mask);
    }
    
    // === Foundation: Single memory write ===
    inline void update_flags(uint8_t clear_mask, uint8_t set_mask) {
        this->set(REG_P, (this->get(REG_P) & ~clear_mask) | set_mask);
    }
    
    inline void update_flag(uint8_t flag_mask, bool condition) {
        update_flags(flag_mask, (uint8_t)condition * flag_mask);
    }
    
    // ========================================================================
    // OPTIMIZED BRANCHLESS FLAG CALCULATION HELPERS
    // ========================================================================
    
    /**
     * Branchless N flag calculation
     * Extract bit 7 directly - zero overhead on most architectures
     */
    inline uint8_t calc_n_flag(uint8_t value) {
        return value & FLAG_N;
    }
    
    /**
     * Branchless Z flag calculation
     */
    inline uint8_t calc_z_flag(uint8_t value) {
        return (value == 0) * FLAG_Z;
    }
    
    /**
     * Branchless C flag calculation from 16-bit result
     * Extract carry bit directly from bit 8
     */
    inline uint8_t calc_c_flag(uint16_t result) {
        return (result >> 8) & FLAG_C;
    }
    
    /**
     * Branchless V flag calculation for addition
     * Hardware-accurate overflow detection using XOR logic
     */
    inline uint8_t calc_v_flag_add(uint8_t a, uint8_t b, uint16_t result) {
        return (((a ^ result) & (b ^ result)) >> 1) & FLAG_V;
    }
    
    /**
     * Branchless V flag calculation for subtraction
     * Hardware-accurate overflow detection for SBC/CMP operations
     */
    inline uint8_t calc_v_flag_sub(uint8_t a, uint8_t b, uint16_t result) {
        return (((a ^ b) & (a ^ result)) >> 1) & FLAG_V;
    }
    
    /**
     * Combined NZ flag calculation
     * Optimized for common case where both N and Z need updating
     */
    inline uint8_t calc_nz_flags(uint8_t value) {
        return calc_n_flag(value) | calc_z_flag(value);
    }
    
    /**
     * Combined NZC flag calculation for compare operations
     * Optimized for CMP, CPX, CPY instructions
     */
    inline uint8_t calc_nzc_flags(uint8_t minuend, uint8_t subtrahend) {
        uint16_t result = minuend - subtrahend;
        return calc_n_flag(static_cast<uint8_t>(result)) |
               calc_z_flag(static_cast<uint8_t>(result)) |
               calc_c_flag(~result);  // Inverted for subtraction
    }

    // ========================================================================
    // DERIVED OPERATIONS (updated to use optimized functions)
    // ========================================================================
    
    inline void update_c_flag(uint8_t value, uint8_t bit_position) {
        update_flag(FLAG_C, (value >> bit_position) & FLAG_C);
    }

    inline void update_nz_flags(uint8_t value) {
        update_flags(FLAG_N | FLAG_Z, calc_nz_flags(value));
    }

    // ========================================================================
    // HARDWARE-ACCURATE BCD ARITHMETIC HELPERS
    // ========================================================================
    
    /**
     * Hardware-accurate 6502 BCD addition
     * Based on MAME and floooh implementations with exact hardware timing
     *
     * This implementation matches the actual 6502 silicon behavior including:
     * - Correct N and Z flag behavior in BCD mode
     * - Proper carry handling
     * - Exact overflow flag calculation
     *    
     * ADC operation with BCD support
     * Handles both binary and BCD modes with proper flag calculation
     *
     * Template parameter allows compile-time processor-specific optimizations:
     * - NES 6502: BCD disabled, simplified binary-only path
     * - MOS 6502/6510: Full BCD support with hardware-accurate behavior
     * - 65C02: Enhanced BCD with corrected flag behavior
     */
    inline void perform_adc(uint8_t operand) {
        const uint8_t old_a = this->get(REG_A);
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint16_t full_result = old_a + operand + carry_in;
        const uint8_t result = static_cast<uint8_t>(full_result);

        if constexpr (has_bcd()) {
            if (this->get(REG_P) & FLAG_D) {
                // BCD mode calculation
                uint8_t al = (old_a & 0x0F) + (operand & 0x0F) + carry_in;
                if (al > 9) al += 6;
                
                uint8_t ah = (old_a >> 4) + (operand >> 4) + (al > 0x0F);
                
                // Calculate flags based on processor type
                uint8_t n_flag, v_flag, z_flag, c_flag;
                
                if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
                    // CMOS processors: All flags from binary result, except when otherwise specified
                    if constexpr (Traits.has(CPUCoreFlags::BCD_NMOS_FLAGS)) {
                        // Some CMOS processors still use NMOS-style flag calculation
                        n_flag = (result != 0) * ((ah << 4) & FLAG_N);
                        v_flag = ((~(old_a ^ operand) & (old_a ^ (ah << 4))) >> 1) & FLAG_V;
                        if (ah > 9) ah += 6;
                        z_flag = (result == 0) * FLAG_Z;
                        c_flag = (ah > 15) ? FLAG_C : 0;
                    } else {
                        // Standard CMOS (WDC65C02 and Synertek65C02): Different flag behavior
                        // Calculate intermediate BCD result before carry adjustment for flag calculations
                        uint8_t intermediate_bcd = (ah << 4) | (al & 0x0F);
                        
                        // BCD adjustment for carry calculation
                        c_flag = (ah > 9) ? FLAG_C : 0;  // C flag based on decimal carry
                        if (ah > 9) ah += 6;
                        uint8_t bcd_result = (ah << 4) | (al & 0x0F);
                        
                        // The Synertek 65C02 has unique BCD flag behavior:
                        // - V flag: calculated from intermediate BCD (before high nibble adjustment)
                        // - Z flag: calculated from BCD result (after adjustments)
                        // - N flag: from BCD result
                        // - C flag: from decimal carry
                        
                        // V flag from intermediate BCD result (hardware-accurate for Synertek)
                        v_flag = calc_v_flag_add(old_a, operand, (uint16_t)intermediate_bcd);
                        // Z flag from BCD result (hardware-accurate for Synertek)
                        z_flag = calc_z_flag(bcd_result);
                        // N flag: sign from BCD result
                        n_flag = bcd_result & FLAG_N;
                    }
                } else {
                    // NMOS processors: Complex flag calculation with hardware quirks
                    n_flag = (result != 0) * ((ah << 4) & FLAG_N);
                    v_flag = ((~(old_a ^ operand) & (old_a ^ (ah << 4))) >> 1) & FLAG_V;
                    if (ah > 9) ah += 6;
                    z_flag = (result == 0) * FLAG_Z;
                    c_flag = (ah > 15) ? FLAG_C : 0;
                }
                
                // Apply result and flags
                this->set(REG_A, (ah << 4) | (al & 0x0F));
                this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                    n_flag | v_flag | z_flag | c_flag);
                return;
            }
        }
        
        // Binary mode
        this->set(REG_A, result);
        this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
            calc_nz_flags(result) |
            calc_v_flag_add(old_a, operand, result) |
            calc_c_flag(full_result));
    }

    /**
     * SBC operation with BCD support - Hardware-accurate implementation
     * Based on ProcessorTests validation and actual 65xx silicon behavior
     */
    inline void perform_sbc(uint8_t operand) {
        const uint8_t old_a = this->get(REG_A);
        const uint8_t borrow_in = (this->get(REG_P) & FLAG_C) ^ 1;  // Invert carry for borrow
        const uint16_t full_result = old_a - operand - borrow_in;
        const uint8_t result = static_cast<uint8_t>(full_result);

        if constexpr (has_bcd()) {
            if (this->get(REG_P) & FLAG_D) {
                // Hardware-accurate 6502 BCD subtraction
                // Different algorithms for NMOS vs CMOS processors
                
                // Calculate flags from binary result (always for V and C)
                uint8_t c_flag = !(full_result & 0x0100) ? FLAG_C : 0;
                uint8_t v_flag = calc_v_flag_sub(old_a, operand, full_result);
                
                uint8_t bcd_result;
                
                if constexpr (Traits.is_nmos()) {
                    // NMOS BCD algorithm (MOS 6502)
                    uint8_t al = (old_a & 0x0F) - (operand & 0x0F) - borrow_in;
                    bool low_borrow = false;
                    if (al & 0x10) {
                        al -= 6;
                        low_borrow = true;
                    }
                    
                    uint8_t ah = (old_a >> 4) - (operand >> 4) - (low_borrow ? 1 : 0);
                    if (ah & 0x10) {
                        ah -= 6;
                    }
                    
                    bcd_result = ((ah & 0x0F) << 4) | (al & 0x0F);
                } else {
                    // CMOS BCD algorithm - exact match for ProcessorTests ground truth
                    // Based on the WDC 65C02 datasheet and verified implementations
                    uint16_t bcd_calc_result = old_a - operand - borrow_in + 0x100;
                    
                    // Low nibble correction
                    if ((old_a & 0x0F) < ((operand & 0x0F) + borrow_in)) {
                        bcd_calc_result -= 6;
                    }
                    
                    // High nibble correction - check for borrow from low nibble
                    uint8_t effective_high_operand = (operand >> 4);
                    if ((old_a & 0x0F) < ((operand & 0x0F) + borrow_in)) {
                        effective_high_operand++;
                    }
                    
                    if ((old_a >> 4) < effective_high_operand) {
                        bcd_calc_result -= 0x60;
                    }
                    
                    bcd_result = bcd_calc_result & 0xFF;
                }
                
                // Flag calculation based on processor type
                uint8_t n_flag, z_flag;
                if constexpr (Traits.has(CPUCoreFlags::BCD_NMOS_FLAGS)) {
                    // NMOS-style or CMOS with NMOS flags: N,Z from binary result
                    n_flag = result & FLAG_N;
                    z_flag = calc_z_flag(result);
                } else {
                    // Pure CMOS: N,Z from BCD result
                    n_flag = bcd_result & FLAG_N;
                    z_flag = calc_z_flag(bcd_result);
                }
                
                // Apply result and flags
                this->set(REG_A, bcd_result);
                this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                    n_flag | v_flag | z_flag | c_flag);
                return;
            }
        }
        
        // Binary mode
        this->set(REG_A, result);
        this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
            calc_nz_flags(result) |
            calc_v_flag_sub(old_a, operand, full_result) |
            (!(full_result & 0x0100) ? FLAG_C : 0));
    }

    /**
     * Compare operation (CMP/CPX/CPY)
     * Optimized implementation with branchless flag calculation
     */
    inline void perform_compare(uint8_t reg_value, uint8_t operand) {
        // Update flags using branchless calculations
        this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      calc_nzc_flags(reg_value, operand));
    }

    // ========================================================================
    // ADDRESSING AND PAGE CROSSING HELPERS
    // ========================================================================
    
    /**
     * Check if page boundary was crossed during addressing
     * Used for determining extra cycle penalties
     */
    inline bool page_crossed(uint16_t addr1, uint16_t addr2) const {
        return ((addr1 ^ addr2) & 0x0100) != 0;
    }
    
    /**
     * Zero page address calculation with wrap-around
     * Hardware-accurate 8-bit addition for zero page addressing
     */
    inline uint16_t calc_zp_addr(uint8_t base, uint8_t offset) const {
        return (base + offset) & 0xFF;  // Wrap within zero page
    }
    
    /**
     * Absolute address calculation with page crossing detection
     * Returns address and sets page_crossed flag for cycle timing
     */
    inline uint16_t calc_abs_addr_indexed(uint16_t base, uint8_t index, bool& page_crossed_out) const {
        uint16_t result = base + index;
        page_crossed_out = page_crossed(base, result);
        return result;
    }

private:
    // ========================================================================
    // INTERNAL HELPER FUNCTIONS AND DECLARATIONS
    // ========================================================================
    
    // Template-dependent function pointer type
    using InstructionHandler = bus_state_t (fam65xx_t<Traits>::*)(bus_state_t);
    
    // Lookup tables for handlers (initialized during init)
    std::array<InstructionHandler, to_index(OP::COUNT)> operation_handlers;
    std::array<InstructionHandler, to_index(AM::COUNT)> addressing_mode_handlers;
    
    // Essential helper functions for template functionality
    inline InstructionHandler get_instruction_handler() {
        // For addressing modes that need address calculation, start with addressing mode handler
        if (this->opcode_entry.am_index > to_index(AM::IMM)) {
            return addressing_mode_handlers[this->opcode_entry.am_index];
        }
        
        // For immediate mode and implied operations, go directly to operation
        return this->operation_handlers[this->opcode_entry.op_index];
    }
    
    // Hardware-accurate interrupt detection with priority-order processing
    bool process_interrupt_detection(bus_state_t pins) {
        // Load state into registers to reduce memory accesses
        uint32_t shift_reg = this->interrupt_shift_register;
        
        // Shift and clear separators (prevent cross-over) in one operation
        shift_reg = (shift_reg << 1) & ~INT_SEPARATOR_MASK;
        
        // Extract interrupt pins (bits 32-34) and invert (active low)
        // After shift: bit 0=RES, bit 1=IRQ, bit 2=NMI
        uint32_t int_pins = (~pins) >> BUS_RES_BIT;
        
        // Pin bit offsets after extraction
        constexpr uint8_t IRQ_OFFSET = BUS_IRQ_BIT - BUS_RES_BIT;  // 1
        constexpr uint8_t NMI_OFFSET = BUS_NMI_BIT - BUS_RES_BIT;  // 2
        
        // Sample IRQ (extracted bit 1 -> shift_reg bit 0) - only if IRQ line exists
        if constexpr (has_irq_line()) {
            shift_reg |= (int_pins >> IRQ_OFFSET) & (1 << INT_IRQ_START_BIT);
        }
        
        // NMI edge detection (extracted bit 2) - only if NMI line exists
        if constexpr (has_nmi_line()) {
            uint8_t nmi_current = (int_pins >> NMI_OFFSET) & 0x1;
            shift_reg |= (-(this->nmi_prev & !nmi_current)) & (1 << INT_NMI_START_BIT);
            this->nmi_prev = nmi_current;
        }
        
        // Sample RESET (extracted bit 0 -> shift_reg bit 8)
        shift_reg |= (int_pins & 0x1) << INT_RESET_START_BIT;
        
        // Store updated state
        this->interrupt_shift_register = shift_reg;
        
        // Check for completed interrupt sequences in priority order (highest first)
        // Set active_interrupt to highest priority interrupt detected
        
        // Check RESET first (highest priority)
        if ((shift_reg & INT_RESET_MASK) == INT_RESET_MASK) {
            this->active_interrupt = FAM65XX_INT_RESET;
            return true;
        }
        
        // Check ABORT (65C816 only, second highest)
        if constexpr (has_wide_registers()) {
            // ABORT detection logic would go here when implemented
            // For now, ABORT is not connected to hardware pins
        }
        
        // Check NMI (third highest priority)
        if constexpr (has_nmi_line()) {
            if ((shift_reg & INT_NMI_MASK) == INT_NMI_MASK) {
                this->active_interrupt = FAM65XX_INT_NMI;
                return true;
            }
        }
        
        // Check COP (65C816 software interrupt, fourth priority)
        // COP is triggered by software, not hardware pins - handled elsewhere
        
        // Check IRQ (fifth priority, maskable by I flag)
        if constexpr (has_irq_line()) {
            if ((shift_reg & INT_IRQ_MASK) == INT_IRQ_MASK && !(this->get(REG_P) & FLAG_I)) {
                this->active_interrupt = FAM65XX_INT_IRQ;
                return true;
            }
        }
        
        // Check BRK (software interrupt, sixth priority)
        // BRK is triggered by software, not hardware pins - handled in op_brk
        
        // No interrupt detected
        return false;
    }
    
    // Instruction fetch and decode
    bus_state_t fetch_opcode(bus_state_t pins) {
        // Read opcode from PC
        pins = this->phi2_read(pins, REG_PC, REG_IR);
        this->set(REG_AB, this->get(REG_PC));
        this->inc(REG_PC);
        // Set SYNC signal for opcode fetch
        pins |= FAM65XX_SYNC;
        // Decode opcode and set up instruction
        uint8_t opcode = this->get(REG_IR);

        this->opcode_entry = get_opcode_info(opcode);
        this->cycle_index = 0;
        // Set up first instruction cycle handler
        this->current_handler = this->get_instruction_handler();
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
    // BUS CONTROL HELPER FUNCTIONS
    // ========================================================================
    
    /**
     * Check if CPU has bus control (hardware-accurate RDY handling)
     * 
     * This function determines whether the CPU or an external DMA device
     * (such as VIC-II) controls the bus based on the RDY pin state.
     * 
     * Returns true if CPU has bus control, false if DMA device has control.
     */
    static inline bool cpu_has_bus(bus_state_t pins) {
        return FAM65XX_GET_RDY(pins);
    }
        
    // ========================================================================
    // ESSENTIAL TEMPLATE FUNCTIONS (needed for real CPU implementation)
    // ========================================================================
    
    void transition_to_operation() {
        this->cycle_index = 0;
        this->current_handler = this->operation_handlers[this->opcode_entry.op_index];
    }
    
    void init_conditional_features() {
        // Initialize I/O port if present
        if constexpr (has_io_port()) {
            this->init_io_port();
        }
        
        // Initialize APU if present
        if constexpr (has_apu()) {
            this->init_apu();
        }
        
        // Register initialization is handled by register mixin
    }
    
    void init_opcode_table() {
        // Initialize operation handler lookup table with NOP as safe default
        operation_handlers.fill(&fam65xx_t::op_nop);
        
        // Sorted alphabetically for easier maintenance
        operation_handlers[to_index(OP::ADC)] = &fam65xx_t::op_adc;
        operation_handlers[to_index(OP::ANC)] = &fam65xx_t::op_anc;
        operation_handlers[to_index(OP::AND)] = &fam65xx_t::op_and;
        operation_handlers[to_index(OP::ARR)] = &fam65xx_t::op_arr;
        operation_handlers[to_index(OP::ASL)] = &fam65xx_t::op_asl;
        operation_handlers[to_index(OP::ASR)] = &fam65xx_t::op_asr;
        operation_handlers[to_index(OP::BBR0)] = &fam65xx_t::op_bbr0;
        operation_handlers[to_index(OP::BBR1)] = &fam65xx_t::op_bbr1;
        operation_handlers[to_index(OP::BBR2)] = &fam65xx_t::op_bbr2;
        operation_handlers[to_index(OP::BBR3)] = &fam65xx_t::op_bbr3;
        operation_handlers[to_index(OP::BBR4)] = &fam65xx_t::op_bbr4;
        operation_handlers[to_index(OP::BBR5)] = &fam65xx_t::op_bbr5;
        operation_handlers[to_index(OP::BBR6)] = &fam65xx_t::op_bbr6;
        operation_handlers[to_index(OP::BBR7)] = &fam65xx_t::op_bbr7;
        operation_handlers[to_index(OP::BBS0)] = &fam65xx_t::op_bbs0;
        operation_handlers[to_index(OP::BBS1)] = &fam65xx_t::op_bbs1;
        operation_handlers[to_index(OP::BBS2)] = &fam65xx_t::op_bbs2;
        operation_handlers[to_index(OP::BBS3)] = &fam65xx_t::op_bbs3;
        operation_handlers[to_index(OP::BBS4)] = &fam65xx_t::op_bbs4;
        operation_handlers[to_index(OP::BBS5)] = &fam65xx_t::op_bbs5;
        operation_handlers[to_index(OP::BBS6)] = &fam65xx_t::op_bbs6;
        operation_handlers[to_index(OP::BBS7)] = &fam65xx_t::op_bbs7;
        operation_handlers[to_index(OP::BCC)] = &fam65xx_t::op_bcc;
        operation_handlers[to_index(OP::BCS)] = &fam65xx_t::op_bcs;
        operation_handlers[to_index(OP::BEQ)] = &fam65xx_t::op_beq;
        operation_handlers[to_index(OP::BIT)] = &fam65xx_t::op_bit;
        operation_handlers[to_index(OP::BMI)] = &fam65xx_t::op_bmi;
        operation_handlers[to_index(OP::BNE)] = &fam65xx_t::op_bne;
        operation_handlers[to_index(OP::BPL)] = &fam65xx_t::op_bpl;
        operation_handlers[to_index(OP::BRA)] = &fam65xx_t::op_bra;
        operation_handlers[to_index(OP::BRK)] = &fam65xx_t::op_brk;
        operation_handlers[to_index(OP::BVC)] = &fam65xx_t::op_bvc;
        operation_handlers[to_index(OP::BVS)] = &fam65xx_t::op_bvs;
        operation_handlers[to_index(OP::CLC)] = &fam65xx_t::op_clc;
        operation_handlers[to_index(OP::CLD)] = &fam65xx_t::op_cld;
        operation_handlers[to_index(OP::CLI)] = &fam65xx_t::op_cli;
        operation_handlers[to_index(OP::CLV)] = &fam65xx_t::op_clv;
        operation_handlers[to_index(OP::CMP)] = &fam65xx_t::op_cmp;
        operation_handlers[to_index(OP::CPX)] = &fam65xx_t::op_cpx;
        operation_handlers[to_index(OP::CPY)] = &fam65xx_t::op_cpy;
        operation_handlers[to_index(OP::DCP)] = &fam65xx_t::op_dcp;
        operation_handlers[to_index(OP::DEC)] = &fam65xx_t::op_dec;
        operation_handlers[to_index(OP::DEX)] = &fam65xx_t::op_dex;
        operation_handlers[to_index(OP::DEY)] = &fam65xx_t::op_dey;
        operation_handlers[to_index(OP::EOR)] = &fam65xx_t::op_eor;
        operation_handlers[to_index(OP::INC)] = &fam65xx_t::op_inc;
        operation_handlers[to_index(OP::INX)] = &fam65xx_t::op_inx;
        operation_handlers[to_index(OP::INY)] = &fam65xx_t::op_iny;
        operation_handlers[to_index(OP::ISC)] = &fam65xx_t::op_isc;
        operation_handlers[to_index(OP::JAM)] = &fam65xx_t::op_jam;
        operation_handlers[to_index(OP::JMP)] = &fam65xx_t::op_jmp;
        operation_handlers[to_index(OP::JSR)] = &fam65xx_t::op_jsr;
        operation_handlers[to_index(OP::LAS)] = &fam65xx_t::op_las;
        operation_handlers[to_index(OP::LAX)] = &fam65xx_t::op_lax;
        operation_handlers[to_index(OP::LDA)] = &fam65xx_t::op_lda;
        operation_handlers[to_index(OP::LDX)] = &fam65xx_t::op_ldx;
        operation_handlers[to_index(OP::LDY)] = &fam65xx_t::op_ldy;
        operation_handlers[to_index(OP::LSR)] = &fam65xx_t::op_lsr;
        operation_handlers[to_index(OP::NOP)] = &fam65xx_t::op_nop;
        operation_handlers[to_index(OP::ORA)] = &fam65xx_t::op_ora;
        operation_handlers[to_index(OP::PEA)] = &fam65xx_t::op_pea;
        operation_handlers[to_index(OP::PER)] = &fam65xx_t::op_per;
        operation_handlers[to_index(OP::PEI)] = &fam65xx_t::op_pei;
        operation_handlers[to_index(OP::PHB)] = &fam65xx_t::op_phb;
        operation_handlers[to_index(OP::PHD)] = &fam65xx_t::op_phd;
        operation_handlers[to_index(OP::PHK)] = &fam65xx_t::op_phk;
        operation_handlers[to_index(OP::PLB)] = &fam65xx_t::op_plb;
        operation_handlers[to_index(OP::PLD)] = &fam65xx_t::op_pld;
        operation_handlers[to_index(OP::PHA)] = &fam65xx_t::op_pha;
        operation_handlers[to_index(OP::PHP)] = &fam65xx_t::op_php;
        operation_handlers[to_index(OP::PHX)] = &fam65xx_t::op_phx;
        operation_handlers[to_index(OP::PHY)] = &fam65xx_t::op_phy;
        operation_handlers[to_index(OP::PLA)] = &fam65xx_t::op_pla;
        operation_handlers[to_index(OP::PLP)] = &fam65xx_t::op_plp;
        operation_handlers[to_index(OP::PLX)] = &fam65xx_t::op_plx;
        operation_handlers[to_index(OP::PLY)] = &fam65xx_t::op_ply;
        operation_handlers[to_index(OP::REP)] = &fam65xx_t::op_rep;
        operation_handlers[to_index(OP::RLA)] = &fam65xx_t::op_rla;
        operation_handlers[to_index(OP::RMB0)] = &fam65xx_t::op_rmb0;
        operation_handlers[to_index(OP::RMB1)] = &fam65xx_t::op_rmb1;
        operation_handlers[to_index(OP::RMB2)] = &fam65xx_t::op_rmb2;
        operation_handlers[to_index(OP::RMB3)] = &fam65xx_t::op_rmb3;
        operation_handlers[to_index(OP::RMB4)] = &fam65xx_t::op_rmb4;
        operation_handlers[to_index(OP::RMB5)] = &fam65xx_t::op_rmb5;
        operation_handlers[to_index(OP::RMB6)] = &fam65xx_t::op_rmb6;
        operation_handlers[to_index(OP::RMB7)] = &fam65xx_t::op_rmb7;
        operation_handlers[to_index(OP::ROL)] = &fam65xx_t::op_rol;
        operation_handlers[to_index(OP::ROR)] = &fam65xx_t::op_ror;
        operation_handlers[to_index(OP::RRA)] = &fam65xx_t::op_rra;
        operation_handlers[to_index(OP::RTI)] = &fam65xx_t::op_rti;
        operation_handlers[to_index(OP::RTL)] = &fam65xx_t::op_rtl;
        operation_handlers[to_index(OP::JSL)] = &fam65xx_t::op_jsl;
        operation_handlers[to_index(OP::JML)] = &fam65xx_t::op_jmp;
        operation_handlers[to_index(OP::MVN)] = &fam65xx_t::op_mvn;
        operation_handlers[to_index(OP::MVP)] = &fam65xx_t::op_mvp;
        operation_handlers[to_index(OP::RTS)] = &fam65xx_t::op_rts;
        operation_handlers[to_index(OP::SAX)] = &fam65xx_t::op_sax;
        operation_handlers[to_index(OP::SBC)] = &fam65xx_t::op_sbc;
        operation_handlers[to_index(OP::SBX)] = &fam65xx_t::op_sbx;
        operation_handlers[to_index(OP::SEC)] = &fam65xx_t::op_sec;
        operation_handlers[to_index(OP::SED)] = &fam65xx_t::op_sed;
        operation_handlers[to_index(OP::SEI)] = &fam65xx_t::op_sei;
        operation_handlers[to_index(OP::SEP)] = &fam65xx_t::op_sep;
        operation_handlers[to_index(OP::SHA)] = &fam65xx_t::op_sha;
        operation_handlers[to_index(OP::SHS)] = &fam65xx_t::op_shs;
        operation_handlers[to_index(OP::SHX)] = &fam65xx_t::op_shx;
        operation_handlers[to_index(OP::SHY)] = &fam65xx_t::op_shy;
        operation_handlers[to_index(OP::SLO)] = &fam65xx_t::op_slo;
        operation_handlers[to_index(OP::SMB0)] = &fam65xx_t::op_smb0;
        operation_handlers[to_index(OP::SMB1)] = &fam65xx_t::op_smb1;
        operation_handlers[to_index(OP::SMB2)] = &fam65xx_t::op_smb2;
        operation_handlers[to_index(OP::SMB3)] = &fam65xx_t::op_smb3;
        operation_handlers[to_index(OP::SMB4)] = &fam65xx_t::op_smb4;
        operation_handlers[to_index(OP::SMB5)] = &fam65xx_t::op_smb5;
        operation_handlers[to_index(OP::SMB6)] = &fam65xx_t::op_smb6;
        operation_handlers[to_index(OP::SMB7)] = &fam65xx_t::op_smb7;
        operation_handlers[to_index(OP::SRE)] = &fam65xx_t::op_sre;
        operation_handlers[to_index(OP::STA)] = &fam65xx_t::op_sta;
        operation_handlers[to_index(OP::STP)] = &fam65xx_t::op_stp;
        operation_handlers[to_index(OP::STX)] = &fam65xx_t::op_stx;
        operation_handlers[to_index(OP::STY)] = &fam65xx_t::op_sty;
        operation_handlers[to_index(OP::STZ)] = &fam65xx_t::op_stz;
        operation_handlers[to_index(OP::TAX)] = &fam65xx_t::op_tax;
        operation_handlers[to_index(OP::TAY)] = &fam65xx_t::op_tay;
        operation_handlers[to_index(OP::TRB)] = &fam65xx_t::op_trb;
        operation_handlers[to_index(OP::TSB)] = &fam65xx_t::op_tsb;
        operation_handlers[to_index(OP::TSX)] = &fam65xx_t::op_tsx;
        operation_handlers[to_index(OP::TXA)] = &fam65xx_t::op_txa;
        operation_handlers[to_index(OP::TXS)] = &fam65xx_t::op_txs;
        operation_handlers[to_index(OP::TYA)] = &fam65xx_t::op_tya;
        operation_handlers[to_index(OP::WAI)] = &fam65xx_t::op_wai;
        operation_handlers[to_index(OP::XAA)] = &fam65xx_t::op_xaa;
        operation_handlers[to_index(OP::XBA)] = &fam65xx_t::op_xba;
        operation_handlers[to_index(OP::XCE)] = &fam65xx_t::op_xce;
        operation_handlers[to_index(OP::COP)] = &fam65xx_t::op_cop;
        operation_handlers[to_index(OP::WDM)] = &fam65xx_t::op_wdm;
        
        // Initialize addressing mode handler lookup table
        addressing_mode_handlers.fill(nullptr);  // Default to nullptr (safe for AM_NON/AM_IMM)
        
        // Addressing modes (implemented)
        addressing_mode_handlers[to_index(AM::NON)] = nullptr;   // No handler needed (implicit/accumulator/relative)
        addressing_mode_handlers[to_index(AM::IMM)] = nullptr;   // No handler (handled directly in operations)
        addressing_mode_handlers[to_index(AM::ZER)] = &fam65xx_t::am_zp;
        addressing_mode_handlers[to_index(AM::ZPX)] = &fam65xx_t::am_zpx;
        addressing_mode_handlers[to_index(AM::ZPY)] = &fam65xx_t::am_zpy;
        addressing_mode_handlers[to_index(AM::ABS)] = &fam65xx_t::am_abs;
        addressing_mode_handlers[to_index(AM::ABX)] = &fam65xx_t::am_abx;
        addressing_mode_handlers[to_index(AM::ABY)] = &fam65xx_t::am_aby;
        addressing_mode_handlers[to_index(AM::IND)] = &fam65xx_t::am_ind;
        addressing_mode_handlers[to_index(AM::INX)] = &fam65xx_t::am_inx;
        addressing_mode_handlers[to_index(AM::INY)] = &fam65xx_t::am_iny;
        
        // Rockwell 65C02 addressing modes
        addressing_mode_handlers[to_index(AM::ZPR)] = &fam65xx_t::am_zpr;  // Zero Page Relative (for BBR/BBS)
        
        // 65C02 addressing modes (if implemented)
        addressing_mode_handlers[to_index(AM::ZPI)] = &fam65xx_t::am_zpi;  // Zero Page Indirect

        // 65C816 addressing modes (if implemented)
        addressing_mode_handlers[to_index(AM::ABI)] = &fam65xx_t::am_abi;  // Absolute Indexed Indirect
        if constexpr (has_wide_registers()) {
            addressing_mode_handlers[to_index(AM::SR)] = &fam65xx_t::amr_sr;    // Stack Relative (65C816 only)
            addressing_mode_handlers[to_index(AM::SRI)] = &fam65xx_t::am_sri;  // Stack Relative Indirect Indexed (65C816 only)
        }
    }
};

// ============================================================================
// MSVC COMPATIBILITY TYPE ALIASES
// ============================================================================

// Create concrete type aliases for MSVC template compatibility
// MSVC has stricter requirements for non-type template parameters
using mos6502_cpu_impl_t = fam65xx_t<MOS6502>;
using mos6510_cpu_impl_t = fam65xx_t<MOS6510>;
using nes6502_cpu_impl_t = fam65xx_t<RICOH_2A03>;
using wdc65c02_cpu_impl_t = fam65xx_t<WDC_65C02_EARLY>;
using rockwell65c02_cpu_impl_t = fam65xx_t<ROCKWELL_R65C02>;
using wdc65c816_cpu_impl_t = fam65xx_t<WDC_65C816>;

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS (required for MSVC compatibility)
// ============================================================================

// Explicit template instantiations for all CPU variants
// This ensures MSVC can properly resolve template parameters
template class fam65xx_t<MOS6502>;
template class fam65xx_t<MOS6510>;
template class fam65xx_t<RICOH_2A03>;
template class fam65xx_t<WDC_65C02_EARLY>;
template class fam65xx_t<ROCKWELL_R65C02>;
template class fam65xx_t<WDC_65C816>;

// ============================================================================
// OPCODE TABLE GENERATION (processor-specific specializations were included above)
// ============================================================================

} // namespace fam65xx
