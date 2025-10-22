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

#include "fam65xx_types.h"
#include "fam65xx_processor_traits.hpp"
#include "fam65xx_mixins.hpp"

// ============================================================================
// C++ NAMESPACE - MAIN CPU TEMPLATE IMPLEMENTATION
// ============================================================================

namespace fam65xx_cpp {

// ============================================================================
// INTERRUPT SHIFT REGISTER CONSTANTS (matching old implementation)
// ============================================================================

constexpr uint32_t INT_IRQ_START_BIT = 0;
constexpr uint32_t INT_NMI_START_BIT = 8;
constexpr uint32_t INT_RESET_START_BIT = 16;
constexpr uint32_t INT_IRQ_MASK = 0x07;      // 3 bits for IRQ
constexpr uint32_t INT_NMI_MASK = 0x0700;    // 3 bits for NMI
constexpr uint32_t INT_RESET_MASK = 0x070000; // 3 bits for RESET
constexpr uint32_t INT_SEPARATOR_MASK = INT_IRQ_MASK | INT_NMI_MASK | INT_RESET_MASK;

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template<typename ProcessorTag>
class fam65xx_t : 
    public io_port_base_t<ProcessorTag>,       // Conditional I/O port
    public bcd_base_t<ProcessorTag>,           // Conditional BCD arithmetic  
    public cmos_state_base_t<ProcessorTag>,    // Conditional CMOS state
    public wide_registers_base_t<ProcessorTag> // Conditional 16-bit registers
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
    
    /* 65C02 extended state */
    bool wait_for_interrupt;            /* WAI instruction state */
    bool stopped;                       /* STP instruction state */
    
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
        
        // Initialize conditional features
        init_conditional_features();
    }
    
    // ========================================================================
    // PROCESSOR-SPECIFIC INITIALIZATION  
    // ========================================================================
    
    bus_state_t init(const chip_descriptor_t* desc) {
        // Initialize memory callbacks (will be set up by system)
        // The chip_descriptor_t contains the creation/destroy functions
        // Memory callbacks will be set up when the CPU is attached to the bus
        
        // Initialize processor-specific features
        init_conditional_features();
        
        // Initialize opcode table for this processor type
        init_opcode_table();
        
        // Return initial pin state
        bus_state_t pins = 0;
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
        init_conditional_features();
        
        // Load reset vector
        CPU_AB(this) = 0xFFFC;
        pins = this->phi2_read(pins, REG_AB, REG_PCL);
        CPU_AB(this) = 0xFFFD;
        pins = this->phi2_read(pins, REG_AB, REG_PCH);
        
        // Start fetch cycle
        transition_to_fetch();
        
        return pins;
    }
    
    bus_state_t tick(bus_state_t pins) {
        // SYNC pin management - asserted during opcode fetch cycles
        if (this->current_handler == nullptr && this->cycle_index == 0) {
            pins |= FAM65XX_SYNC;
        } else {
            pins &= ~FAM65XX_SYNC;
        }
        
        // Hardware-accurate interrupt detection every cycle (matching old implementation)
        if (process_interrupt_detection(pins)) {
            // Interrupt detected - check if we should hijack current instruction
            if (this->brk_flags & FAM65XX_BRK_RESET) {
                // RESET has highest priority - immediately start RESET sequence
                return reset(pins);
            } else if (this->current_handler == nullptr && this->cycle_index == 0) {
                // At instruction boundary - start interrupt sequence
                this->current_handler = &fam65xx_t::interrupt_sequence;
                this->cycle_index = 0;
                return pins;
            }
        }
        
        // Execute current instruction cycle
        if (this->current_handler != nullptr) {
            pins = (this->*this->current_handler)(pins);
        } else {
            // Start new instruction fetch
            pins = fetch_opcode(pins);
        }
        
        return pins;
    }
    
    bool opdone() const {
        return this->current_handler == nullptr;
    }
    
    // ========================================================================
    // FEATURE-SPECIFIC MEMORY ACCESS OVERRIDES
    // ========================================================================
    
    // Template-aware memory read with processor-specific handling
    bus_state_t phi2_read(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) {
        // Get address for both checks and bus operations
        uint16_t addr = this->reg16[addr_reg];
        
        if constexpr (has_io_port<ProcessorTag>()) {
            // Handle 6510 I/O port access
            if (addr == 0x0000) {
                this->reg8[data_reg] = this->read_io_port();
                return pins; // Don't perform bus read
            } else if (addr == 0x0001) {
                this->reg8[data_reg] = this->io_port.direction;
                return pins; // Don't perform bus read
            }
        }
        
        // Standard bus read for all other addresses
        pins = BUS_SET_ADDR(pins, addr);
        pins |= FAM65XX_RW; // Set READ mode
        
        // Use memory callback if available
        if (this->mem_read != nullptr) {
            this->reg8[data_reg] = this->mem_read(this->mem_user_data, addr, pins & 0xFF);
        } else {
            // No callback - return floating bus
            this->reg8[data_reg] = 0xFF;
        }
        
        return pins;
    }
    
    // Template-aware memory write with processor-specific handling
    bus_state_t phi2_write(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) {
        // Get address and data for both checks and bus operations
        uint16_t addr = this->reg16[addr_reg];
        uint8_t data = this->reg8[data_reg];
        
        if constexpr (has_io_port<ProcessorTag>()) {
            // Handle 6510 I/O port access
            if (addr == 0x0000) {
                this->write_io_ddr(data);
                return pins; // Don't perform bus write
            } else if (addr == 0x0001) {
                this->write_io_data(data);
                return pins; // Don't perform bus write
            }
        }
        
        // Standard bus write for all other addresses
        pins = BUS_SET_ADDR(pins, addr);
        pins = BUS_SET_DATA(pins, data);
        pins &= ~FAM65XX_RW; // Set WRITE mode
        
        // Use memory callback if available
        if (this->mem_write != nullptr) {
            this->mem_write(this->mem_user_data, addr, data);
        }
        
        return pins;
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
        // This will be specialized per processor type
        return generate_opcode_table<ProcessorTag>()[opcode];
    }
    
    // ========================================================================
    // REGISTER ACCESS HELPERS (template-aware)
    // ========================================================================
    
    // Accumulator access (8-bit or 16-bit depending on processor)
    uint16_t get_accumulator() const {
        if constexpr (has_wide_registers<ProcessorTag>()) {
            return is_accumulator_16bit() ? this->wide_state.A_full : CPU_A(this);
        } else {
            return CPU_A(this);
        }
    }
    
    void set_accumulator(uint16_t value) {
        if constexpr (has_wide_registers<ProcessorTag>()) {
            if (is_accumulator_16bit()) {
                this->wide_state.A_full = value;
                CPU_A(this) = value & 0xFF; // Keep low byte in sync
            } else {
                CPU_A(this) = value & 0xFF;
            }
        } else {
            CPU_A(this) = value & 0xFF;
        }
    }
    
    // X register access
    uint16_t get_x_register() const {
        if constexpr (has_wide_registers<ProcessorTag>()) {
            return are_indexes_16bit() ? this->wide_state.X_full : CPU_X(this);
        } else {
            return CPU_X(this);
        }
    }
    
    void set_x_register(uint16_t value) {
        if constexpr (has_wide_registers<ProcessorTag>()) {
            if (are_indexes_16bit()) {
                this->wide_state.X_full = value;
                CPU_X(this) = value & 0xFF; // Keep low byte in sync
            } else {
                CPU_X(this) = value & 0xFF;
            }
        } else {
            CPU_X(this) = value & 0xFF;
        }
    }
    
    // Y register access
    uint16_t get_y_register() const {
        if constexpr (has_wide_registers<ProcessorTag>()) {
            return are_indexes_16bit() ? this->wide_state.Y_full : CPU_Y(this);
        } else {
            return CPU_Y(this);
        }
    }
    
    void set_y_register(uint16_t value) {
        if constexpr (has_wide_registers<ProcessorTag>()) {
            if (are_indexes_16bit()) {
                this->wide_state.Y_full = value;
                CPU_Y(this) = value & 0xFF; // Keep low byte in sync
            } else {
                CPU_Y(this) = value & 0xFF;
            }
        } else {
            CPU_Y(this) = value & 0xFF;
        }
    }
    
private:
    // ========================================================================
    // INTERNAL HELPER FUNCTIONS
    // ========================================================================
    
    // Hardware-accurate interrupt detection (matching old implementation)
    bool process_interrupt_detection(bus_state_t pins) {
        // Use intermediate variable to reduce memory accesses
        uint32_t shift_reg = this->interrupt_shift_register;
        
        // Shift the register left by one bit
        shift_reg <<= 1;
        
        // Sample IRQ line and insert into IRQ bits (active low)
        if (!(pins & FAM65XX_IRQ)) {
            shift_reg |= (1 << INT_IRQ_START_BIT);
        }
        
        // NMI Edge Detection - only trigger on falling edge
        uint8_t nmi_current = (pins & FAM65XX_NMI) ? 1 : 0;
        if (this->nmi_prev && !nmi_current) {
            // Falling edge detected - insert into NMI bits
            shift_reg |= (1 << INT_NMI_START_BIT);
        }
        this->nmi_prev = nmi_current;
        
        // Sample RESET line and insert into RESET bits (active low)
        if (!(pins & FAM65XX_RES)) {
            shift_reg |= (1 << INT_RESET_START_BIT);
        }
        
        // Clear separator bits to prevent cross-over
        shift_reg &= ~INT_SEPARATOR_MASK;
        
        // Store back the updated shift register
        this->interrupt_shift_register = shift_reg;
        
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
        // Read opcode from PC
        CPU_AB(this) = CPU_PC(this);
        pins = this->phi2_read(pins, REG_AB, REG_IR);
        CPU_PC(this)++;
        
        // Set SYNC signal for opcode fetch
        pins |= FAM65XX_SYNC;
        
        // Decode opcode and set up instruction
        uint8_t opcode = CPU_IR(this);
        this->opcode_entry = get_opcode_info(opcode);
        this->cycle_index = 0;
        
        // Set up first instruction cycle handler
        this->current_handler = get_instruction_handler(opcode);
        
        return pins;
    }
    
    // Generic interrupt sequence handler
    bus_state_t interrupt_sequence(bus_state_t pins) {
        // Implementation of interrupt sequence
        // This is a simplified placeholder - full implementation would match old BRK handler
        transition_to_fetch();
        return pins;
    }
    
    // Transition to next instruction fetch
    void transition_to_fetch() {
        this->current_handler = nullptr;
        this->cycle_index = 0;
    }
    
    void init_conditional_features() {
        // Initialize I/O port if present
        if constexpr (has_io_port<ProcessorTag>()) {
            this->init_io_port();
        }
        
        // Initialize 16-bit registers if present
        if constexpr (has_wide_registers<ProcessorTag>()) {
            this->init_wide_registers();
        }
        
        // Note: BCD and CMOS state mixins don't need initialization
        // as they are algorithmic and use existing CPU state
    }
    

    
    // Instruction fetch and decode
    bus_state_t fetch_opcode(bus_state_t pins) {
        // Read opcode from PC
        CPU_AB(this) = CPU_PC(this);
        pins = this->phi2_read(pins, REG_AB, REG_IR);
        CPU_PC(this)++;
        
        // Set SYNC signal for opcode fetch
        pins |= FAM65XX_SYNC;
        
        // Decode opcode and set up instruction
        uint8_t opcode = CPU_IR(this);
        this->opcode_entry = get_opcode_info(opcode);
        this->cycle_index = 0;
        
        // Set up first instruction cycle handler
        this->current_handler = get_instruction_handler(opcode);
        
        return pins;
    }
    
    // Interrupt handling
    bus_state_t handle_interrupts(bus_state_t pins) {
        // Check for NMI edge (high to low transition)
        bool nmi_current = !FAM65XX_GET_NMI(pins);
        bool nmi_edge = !this->nmi_prev && nmi_current;
        this->nmi_prev = nmi_current;
        
        // Check for IRQ level
        bool irq_asserted = !FAM65XX_GET_IRQ(pins) && !(CPU_P(this) & FLAG_I);
        
        if (nmi_edge) {
            this->brk_flags |= FAM65XX_BRK_NMI;
            this->current_handler = &fam65xx_t::interrupt_sequence;
        } else if (irq_asserted) {
            this->brk_flags |= FAM65XX_BRK_IRQ;
            this->current_handler = &fam65xx_t::interrupt_sequence;
        }
        
        return pins;
    }
    
    // Transition to next instruction fetch
    void transition_to_fetch() {
        this->current_handler = nullptr;
        this->cycle_index = 0;
    }
    
    // Transition from addressing mode to operation
    void transition_to_operation() {
        this->cycle_index = 0;
        this->current_handler = get_operation_handler(this->opcode_entry.op_index);
    }
    
    // Update N and Z flags based on value
    void update_nz_flags(uint8_t value) {
        CPU_P(this) = (CPU_P(this) & 0x7D) |  // Clear N,Z
                      (value & 0x80) |         // N flag
                      (value == 0 ? FLAG_Z : 0); // Z flag
    }
    
    // Read operand with immediate mode handling (like original C helper)
    bus_state_t read_operand_immediate_or_memory(bus_state_t pins) {
        if (this->opcode_entry.am_index == AM_IMM) {
            // Immediate mode - read from PC and increment
            pins = this->phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
        } else {
            // Memory mode - read from target address in REG_AB
            pins = this->phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
        }
        return pins;
    }
    
    // Fast page cross detection using XOR and bit 8 check
    bool page_crossed(uint16_t addr1, uint16_t addr2) {
        return (addr1 ^ addr2) & 0x0100;
    }
    
    // Instruction handler function pointer type
    using InstructionHandler = bus_state_t (fam65xx_t::*)(bus_state_t);
    
    // Static lookup tables - populated once per processor type
    static constexpr std::array<InstructionHandler, OP_COUNT> operation_table = init_operation_table();
    static constexpr std::array<InstructionHandler, AM_COUNT> addressing_mode_table = init_addressing_mode_table();
    
    // Get instruction handler for opcode
    InstructionHandler get_instruction_handler(uint8_t opcode) {
        // Get opcode information to determine addressing mode and operation
        opcode_info_t info = get_opcode_info(opcode);
        
        // For immediate mode and implied operations, go directly to operation
        if (info.am_index <= AM_IMM) {
            return get_operation_handler(info.op_index);
        }
        
        // For addressing modes that need address calculation, start with addressing mode handler
        return get_addressing_mode_handler(info.am_index);
    }
    
    // Get operation handler based on operation index (simple table lookup)
    InstructionHandler get_operation_handler(uint8_t op_index) {
        return operation_table[op_index];
    }
    
    // Get addressing mode handler based on addressing mode index (simple table lookup) 
    InstructionHandler get_addressing_mode_handler(uint8_t am_index) {
        return addressing_mode_table[am_index];
    }
    
    // Initialize operation handler lookup table (constexpr for compile-time generation)
    static constexpr std::array<InstructionHandler, OP_COUNT> init_operation_table() {
        std::array<InstructionHandler, OP_COUNT> table = {};
        
        // Initialize with NOP as default
        for (size_t i = 0; i < OP_COUNT; ++i) {
            table[i] = &fam65xx_t::op_nop;
        }
        
        // Set up actual operation handlers
        table[OP_LDA] = &fam65xx_t::op_lda;  table[OP_LDX] = &fam65xx_t::op_ldx;  table[OP_LDY] = &fam65xx_t::op_ldy;
        table[OP_STA] = &fam65xx_t::op_sta;  table[OP_STX] = &fam65xx_t::op_stx;  table[OP_STY] = &fam65xx_t::op_sty;
        table[OP_ADC] = &fam65xx_t::op_adc;  table[OP_SBC] = &fam65xx_t::op_sbc;  table[OP_CMP] = &fam65xx_t::op_cmp;
        table[OP_CPX] = &fam65xx_t::op_cpx;  table[OP_CPY] = &fam65xx_t::op_cpy;
        table[OP_AND] = &fam65xx_t::op_and;  table[OP_ORA] = &fam65xx_t::op_ora;  table[OP_EOR] = &fam65xx_t::op_eor;
        table[OP_BIT] = &fam65xx_t::op_bit;
        table[OP_ASL] = &fam65xx_t::op_asl;  table[OP_LSR] = &fam65xx_t::op_lsr;
        table[OP_ROL] = &fam65xx_t::op_rol;  table[OP_ROR] = &fam65xx_t::op_ror;
        table[OP_INC] = &fam65xx_t::op_inc;  table[OP_DEC] = &fam65xx_t::op_dec;
        table[OP_INX] = &fam65xx_t::op_inx;  table[OP_INY] = &fam65xx_t::op_iny;
        table[OP_DEX] = &fam65xx_t::op_dex;  table[OP_DEY] = &fam65xx_t::op_dey;
        table[OP_TAX] = &fam65xx_t::op_tax;  table[OP_TAY] = &fam65xx_t::op_tay;
        table[OP_TXA] = &fam65xx_t::op_txa;  table[OP_TYA] = &fam65xx_t::op_tya;
        table[OP_TSX] = &fam65xx_t::op_tsx;  table[OP_TXS] = &fam65xx_t::op_txs;
        table[OP_PHA] = &fam65xx_t::op_pha;  table[OP_PLA] = &fam65xx_t::op_pla;
        table[OP_PHP] = &fam65xx_t::op_php;  table[OP_PLP] = &fam65xx_t::op_plp;
        table[OP_BPL] = &fam65xx_t::op_bpl;  table[OP_BMI] = &fam65xx_t::op_bmi;
        table[OP_BVC] = &fam65xx_t::op_bvc;  table[OP_BVS] = &fam65xx_t::op_bvs;
        table[OP_BCC] = &fam65xx_t::op_bcc;  table[OP_BCS] = &fam65xx_t::op_bcs;
        table[OP_BNE] = &fam65xx_t::op_bne;  table[OP_BEQ] = &fam65xx_t::op_beq;
        table[OP_JMP] = &fam65xx_t::op_jmp;  table[OP_JSR] = &fam65xx_t::op_jsr;
        table[OP_RTS] = &fam65xx_t::op_rts;  table[OP_BRK] = &fam65xx_t::op_brk;
        table[OP_RTI] = &fam65xx_t::op_rti;
        table[OP_CLC] = &fam65xx_t::op_clc;  table[OP_SEC] = &fam65xx_t::op_sec;
        table[OP_CLI] = &fam65xx_t::op_cli;  table[OP_SEI] = &fam65xx_t::op_sei;
        table[OP_CLV] = &fam65xx_t::op_clv;  table[OP_CLD] = &fam65xx_t::op_cld;
        table[OP_SED] = &fam65xx_t::op_sed;  table[OP_NOP] = &fam65xx_t::op_nop;
        
        return table;
    }
    
    // Initialize addressing mode handler lookup table (constexpr for compile-time generation)
    static constexpr std::array<InstructionHandler, AM_COUNT> init_addressing_mode_table() {
        std::array<InstructionHandler, AM_COUNT> table = {};
        
        // Initialize addressing mode handlers
        table[AM_NON] = nullptr;                   table[AM_IMM] = nullptr; // These should not use addressing mode handlers
        table[AM_ZER] = &fam65xx_t::addr_zp;       table[AM_ZPX] = &fam65xx_t::addr_zpx;
        table[AM_ZPY] = &fam65xx_t::addr_zpy;      table[AM_ABS] = &fam65xx_t::addr_abs;
        table[AM_ABX] = &fam65xx_t::addr_abx;      table[AM_ABY] = &fam65xx_t::addr_aby;
        table[AM_INX] = &fam65xx_t::addr_inx;      table[AM_INY] = &fam65xx_t::addr_iny;
        table[AM_IND] = &fam65xx_t::addr_ind;
        
        // Fill remaining slots with nullptr (unused addressing mode indices)
        for (size_t i = 12; i < AM_COUNT; ++i) {
            table[i] = nullptr;
        }
        
        return table;
    }
    
    // Initialize opcode table for this processor type
    void init_opcode_table() {
        // Static tables are initialized at compile time
    }
    
    // Generic interrupt sequence handler
    bus_state_t interrupt_sequence(bus_state_t pins) {
        // Implementation of interrupt sequence
        // This is a simplified placeholder
        transition_to_fetch();
        return pins;
    }
};

// ============================================================================
// OPCODE TABLE GENERATION (processor-specific specializations)
// ============================================================================

// Forward declaration - will be specialized for each processor
template<typename ProcessorTag>
constexpr std::array<opcode_info_t, 256> generate_opcode_table();

// Include processor-specific opcode table specializations
#include "operations/opcode_tables.inc.hpp"

} // namespace fam65xx_cpp

#endif // __cplusplus