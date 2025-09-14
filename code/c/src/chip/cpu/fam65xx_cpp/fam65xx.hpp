#ifndef FAM65XX_HPP
#define FAM65XX_HPP

#include "cpu_defs.hpp"
#include "cpu_config.hpp"
#include "alu_operations.hpp"
#include "memory_operations.hpp"
#include "cycle_table_gen.hpp"
#include "../../../core/system_lines.h"

namespace fam65xx_cpp {

template<typename Config>
class fam65xx {
private:
    // CPU state
    uint16_t opcode = 0;           // Changed to uint16_t to support virtual opcodes 256+
    uint8_t cycle_step = 0;
    uint16_t state_flags = 0;
    uint8_t pending_data = 0;
    uint8_t pending_data_op = 0;
    
    // Hardware pin state tracking for interrupt edge detection
    bool prev_nmi_pin_state = true;  // NMI pin state tracking (starts high)
    bool prev_irq_pin_state = true;  // IRQ pin state tracking (starts high)
    uint8_t irq_sources = 0;         // Multiple IRQ source tracking (8 possible sources)
    
    // CPU registers - type-safe array that accepts CpuReg enum directly
    CpuRegisterArray reg;
    
    // Template helper classes
    using cycle_tables = CycleTables<Config>;
    using alu_ops = AluOperations<Config>;
    using memory_ops = MemoryOperations<Config>;
    using pin_config = cpu_pin_config<Config>;

public:
    // Constructor
    fam65xx() {
        init();
    }
    
    // Get cycle information for instruction
    static inline constexpr fam65xx_cpp::cycle_desc_t GET_CYCLE(uint16_t opcode, uint8_t step) {
        return cycle_tables::get_cycle(opcode, step);
    }
    
    // State flag helpers
    inline bool get_state(uint16_t flag) const {
        return (state_flags & flag) != 0;
    }
    
    inline void set_state(uint16_t flag) {
        state_flags |= flag;
    }
    
    inline void clear_state(uint16_t flag) {
        state_flags &= ~flag;
    }
    
    // Initialize CPU
    inline void init() {
        // Reset all registers
        for (auto& r : reg) {
            r = 0;
        }
        
        // Initialize key registers
        reg[CpuReg::S] = 0xFF;
        reg[CpuReg::P] = P_IRQ_DIS | P_UNUSED;
        
        // Reset state
        state_flags = STATE_RESET_PENDING;
        opcode = 0;
        cycle_step = 0;
        pending_data = 0;
        pending_data_op = 0;
        
        // Reset interrupt pin state tracking
        prev_nmi_pin_state = true;  // NMI pin starts high (inactive)
        prev_irq_pin_state = true;  // IRQ pin starts high (inactive)
        irq_sources = 0;            // No IRQ sources active
        
        // No separate interrupt_opcode field needed - using main opcode field
    }
    
    // Execute one CPU cycle
    inline bus_state_t cycle_tick(bus_state_t bus_state) {
        // Process input control lines first
        bus_state = process_input_pins(bus_state);
        
        // Handle RDY line - variant-specific behavior
        if (!(bus_state & BUS_BIT(BUS_RDY_BIT))) {
            bus_state = handle_rdy_wait(bus_state);
            if (get_state(STATE_RDY_WAIT)) {
                return bus_state; // Skip this cycle
            }
        }
        
        // Check if we're in an interrupt sequence
        if (get_state(STATE_INTERRUPT_SEQUENCE)) {
            return execute_interrupt_cycle(bus_state);
        }
        
        // Batch check critical state flags for speed
        const uint16_t critical_states = state_flags & (STATE_RESET_PENDING | STATE_NMI_PENDING | STATE_IRQ_PENDING);
        if (critical_states) {
            // Handle reset first (highest priority)
            if (critical_states & STATE_RESET_PENDING) {
                return start_interrupt_sequence(bus_state, VIRTUAL_OPCODE_RESET);
            }
            
            // Handle interrupts only at instruction boundaries for proper timing
            if (cycle_step == 0) {
                // NMI has highest priority among interrupts and is non-maskable
                if (critical_states & STATE_NMI_PENDING) {
                    // Clear the NMI edge flag when servicing the interrupt
                    clear_state(STATE_NMI_EDGE);
                    return start_interrupt_sequence(bus_state, VIRTUAL_OPCODE_NMI);
                }
                // IRQ has lower priority and is maskable - check I flag synchronously
                else if ((critical_states & STATE_IRQ_PENDING) &&
                          !(reg[static_cast<uint8_t>(CpuReg::P)] & P_IRQ_DIS)) {
                    return start_interrupt_sequence(bus_state, VIRTUAL_OPCODE_IRQ);
                }
            }
        }
        
        // Execute normal instruction cycle
        bus_state = execute_cycle(bus_state);
        
        // Process output control lines
        bus_state = process_output_pins(bus_state);
        
        return bus_state;
    }
    
    // Start interrupt sequence using cycle-based approach
    inline bus_state_t start_interrupt_sequence(bus_state_t bus_state, uint16_t virtual_opcode) {
        // Clear the appropriate interrupt pending flag
        switch (virtual_opcode) {
            case VIRTUAL_OPCODE_RESET:
                clear_state(STATE_RESET_PENDING);
                // Reset special initialization
                reg[CpuReg::S] = 0xFF;
                reg[CpuReg::P] = P_IRQ_DIS | P_UNUSED;
                break;
            case VIRTUAL_OPCODE_NMI:
                clear_state(STATE_NMI_PENDING);
                break;
            case VIRTUAL_OPCODE_IRQ:
                clear_state(STATE_IRQ_PENDING);
                break;
        }
        
        // Initialize interrupt sequence state - use main opcode field
        set_state(STATE_INTERRUPT_SEQUENCE);
        opcode = virtual_opcode;  // Store virtual opcode in main opcode field
        cycle_step = 1;  // Start interrupt cycle sequence
        
        return execute_interrupt_cycle(bus_state);
    }
    
    // Execute one cycle of interrupt sequence using cycle table approach
    inline bus_state_t execute_interrupt_cycle(bus_state_t bus_state) {
        // Get cycle description for current interrupt step using main opcode field
        const fam65xx_cpp::cycle_desc_t cycle = GET_CYCLE(opcode, cycle_step);
        
        // Convert raw values to type-safe enums
        MemOp mem_op = static_cast<MemOp>(cycle.mem_op);
        DataOp data_op = static_cast<DataOp>(cycle.data_op);
        AluOp alu_op = static_cast<AluOp>(cycle.alu_op);
        
        // Set interrupt vector addresses for READ_INDIRECT operations during interrupts
        if (mem_op == MemOp::READ_INDIRECT) {
            switch (opcode) {
                case VIRTUAL_OPCODE_RESET:
                    reg[CpuReg::ABL] = (cycle_step == 6) ? 0xFC : 0xFD;
                    reg[CpuReg::ABH] = 0xFF;
                    break;
                case VIRTUAL_OPCODE_NMI:
                    reg[CpuReg::ABL] = (cycle_step == 6) ? 0xFA : 0xFB;
                    reg[CpuReg::ABH] = 0xFF;
                    break;
                case VIRTUAL_OPCODE_IRQ:
                    reg[CpuReg::ABL] = (cycle_step == 6) ? 0xFE : 0xFF;
                    reg[CpuReg::ABH] = 0xFF;
                    break;
            }
        }
        
        // Execute memory operation
        bus_state = memory_ops::execute_memory_operation(bus_state, reg, mem_op, data_op);
        
        // Handle write data if needed
        bus_state = memory_ops::handle_write_data(bus_state, reg, mem_op, data_op);
        
        // Execute data operation and ALU operation using same bus data
        uint8_t bus_data = BUS_GET_DATA(bus_state);
        execute_data_operation(cycle.data_op, bus_data);
        
        // Execute ALU operation if specified (for setting interrupt disable flag)
        if (alu_op != AluOp::NOP) {
            alu_ops::execute_alu_operation(reg, alu_op, bus_data);
        }
        
        // Check if interrupt sequence is complete using sync bit
        if (cycle.is_sync()) {
            clear_state(STATE_INTERRUPT_SEQUENCE);  // End interrupt sequence
            cycle_step = 0;                         // Ready for next instruction
        } else {
            cycle_step++;
        }
        
        return bus_state;
    }
    
    // Execute one instruction cycle
    inline bus_state_t execute_cycle(bus_state_t bus_state) {
        // Fetch opcode on cycle 0
        if (cycle_step == 0) {
            opcode = BUS_GET_DATA(bus_state);
            cycle_step = 1;
            set_state(STATE_SYNC_NEXT);
            return bus_state;
        }
        
        // Get cycle description for current instruction step
        const fam65xx_cpp::cycle_desc_t cycle = GET_CYCLE(opcode, cycle_step);
        
        // Convert raw values to type-safe enums
        MemOp mem_op = static_cast<MemOp>(cycle.mem_op);
        DataOp data_op = static_cast<DataOp>(cycle.data_op);
        AluOp alu_op = static_cast<AluOp>(cycle.alu_op);
        
        // Execute memory operation
        bus_state = memory_ops::execute_memory_operation(bus_state, reg, mem_op, data_op);

        // Handle write data if needed
        bus_state = memory_ops::handle_write_data(bus_state, reg, mem_op, data_op);
        
        // Execute data operation and ALU operation using same bus data
        uint8_t bus_data = BUS_GET_DATA(bus_state);
        execute_data_operation(cycle.data_op, bus_data);
        
        // Execute ALU operation if specified
        if (alu_op != AluOp::NOP) {
            alu_ops::execute_alu_operation(reg, alu_op, bus_data);
            
            // Handle interrupt flag changes for SEI/CLI instructions
            if (alu_op == AluOp::SEI || alu_op == AluOp::CLI) {
                handle_interrupt_flag_change();
            }
            
            // Handle decimal mode bugs for NMOS variants
            handle_decimal_mode_bugs(reg[CpuReg::A], alu_op);
        }
        
        // Process SO pin edge detection (variant-specific timing)
        process_so_pin_edge();
        
        // Check if instruction is complete using sync bit
        if (cycle.is_sync()) {
            cycle_step = 0; // Start next instruction
        } else {
            cycle_step++;
        }
        return bus_state;
    }
    
    // Execute data operation
    inline void execute_data_operation(uint8_t data_op, uint8_t data) {
        
        // Store data for potential ALU use
        reg[CpuReg::DL] = data;
        
        // Check if data_op corresponds to a direct register load (values 0-5: A, X, Y, S, P, PCL)
        // Values 6+ (PCH, ABL, ABH, DL, etc.) and special operations should use switch statement
        if (data_op <= static_cast<uint8_t>(CpuReg::P)) {
            // Direct register load for A, X, Y, S, P only
            reg[data_op] = data;
        } else {
            switch (static_cast<DataOp>(data_op)) {
                case DataOp::ALU:
                    pending_data = data;
                    break;
                case DataOp::ADDR_CALC_LOW:
                    reg[CpuReg::ABL] = data;
                    break;
                case DataOp::ADDR_CALC_HIGH:
                    reg[CpuReg::ABH] = data;
                    break;
                case DataOp::BRANCH:
                    // Handle branch instructions (BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS, BRA)
                    handle_branch_instruction(data);
                    break;
                case DataOp::JMP:
                    // Execute jump using the address calculated in ABL/ABH
                    {
                        const uint16_t addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
                        reg[CpuReg::PCL] = addr & 0xFF;
                        reg[CpuReg::PCH] = (addr >> 8) & 0xFF;
                    }
                    break;
                case DataOp::INDIRECT_LOW:
                    reg[CpuReg::ABL] = data;
                    break;
                case DataOp::INDIRECT_HIGH:
                    reg[CpuReg::ABH] = data;
                    // Handle JMP indirect with proper 6502 page boundary bug vs CMOS fix
                    if (opcode == 0x6C && cycle_step == 5) { // JMP ($nnnn) final cycle
                        handle_jmp_indirect_bug();
                    }
                    break;
                case DataOp::STACK_PULL:
                    handle_stack_pull(data);
                    break;
                case DataOp::INTERRUPT_VEC:
                    handle_interrupt_vector(data);
                    break;
                default:
                    break;
            }
        }
        
        // Set pending data operation for next cycle
        pending_data_op = data_op;
    }
    
    // Handle branch instructions (BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS, BRA)
    inline void handle_branch_instruction(uint8_t offset) {
        // Special handling for BRA (Branch Always) - 65C02 unconditional branch
        if (opcode == 0x80) {
            // BRA is always a simple 2-cycle unconditional branch
            // Calculate target address
            const uint16_t current_pc = (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL];
            int16_t signed_offset = static_cast<int8_t>(offset); // Sign extend
            const uint16_t target_pc = current_pc + signed_offset;

            // Update PC immediately - BRA doesn't use conditional branch logic
            reg[CpuReg::PCL] = target_pc & 0xFF;
            reg[CpuReg::PCH] = (target_pc >> 8) & 0xFF;
            
            // BRA doesn't set STATE_BRANCH_TAKEN or STATE_PAGE_CROSSED
            // It always executes in exactly 2 cycles regardless of page crossing
            return;
        }
        
        // Conditional branch handling for other branch instructions
        bool should_branch = false;
        
        // Determine if branch should be taken based on opcode
        switch (opcode) {
            case 0x10: // BPL (Branch if PLus)
                should_branch = !(reg[CpuReg::P] & P_NEGATIVE);
                break;
            case 0x30: // BMI (Branch if MInus)
                should_branch = (reg[CpuReg::P] & P_NEGATIVE);
                break;
            case 0x50: // BVC (Branch if oVerflow Clear)
                should_branch = !(reg[CpuReg::P] & P_OVERFLOW);
                break;
            case 0x70: // BVS (Branch if oVerflow Set)
                should_branch = (reg[CpuReg::P] & P_OVERFLOW);
                break;
            case 0x90: // BCC (Branch if Carry Clear)
                should_branch = !(reg[CpuReg::P] & P_CARRY);
                break;
            case 0xB0: // BCS (Branch if Carry Set)
                should_branch = (reg[CpuReg::P] & P_CARRY);
                break;
            case 0xD0: // BNE (Branch if Not Equal)
                should_branch = !(reg[CpuReg::P] & P_ZERO);
                break;
            case 0xF0: // BEQ (Branch if EQual)
                should_branch = (reg[CpuReg::P] & P_ZERO);
                break;
            default:
                // Unknown branch instruction
                break;
        }
        
        if (should_branch) {
            // Calculate branch target address
            const uint16_t current_pc = (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL];
            int16_t signed_offset = static_cast<int8_t>(offset); // Sign extend
            const uint16_t target_pc = current_pc + signed_offset;
            
            // Update PC
            reg[CpuReg::PCL] = target_pc & 0xFF;
            reg[CpuReg::PCH] = (target_pc >> 8) & 0xFF;
            
            // Set branch taken flag for cycle timing
            set_state(STATE_BRANCH_TAKEN);
            
            // Check for page crossing (affects cycle count)
            if ((current_pc ^ target_pc) & 0xFF00) {
                set_state(STATE_PAGE_CROSSED);
            }
        }
    }
    
    // Handle stack pull operations
    inline void handle_stack_pull(uint8_t data) {
        // Stack pull operations (RTI, RTS, PLA, etc.)
        if (opcode == 0x40) { // RTI
            if (cycle_step == 4) reg[CpuReg::P] = data;
            else if (cycle_step == 5) reg[CpuReg::PCL] = data;
            else if (cycle_step == 6) reg[CpuReg::PCH] = data;
        }
    }
    
    // Handle interrupt vector reads
    inline void handle_interrupt_vector(uint8_t data) {
        // Cycle 6 reads low byte, cycle 7 reads high byte for all interrupt types
        if (cycle_step == 6) {
            reg[CpuReg::PCL] = data;  // Store low byte
        } else if (cycle_step == 7) {
            reg[CpuReg::PCH] = data;  // Store high byte
        }
    }
    
    // Get register value for debugging
    inline uint8_t get_reg(CpuReg register_id) const {
        return reg[register_id];
    }
    
    // Set register value for debugging
    inline void set_reg(CpuReg register_id, uint8_t value) {
        reg[register_id] = value;
    }
    
    // Get current opcode
    inline uint16_t get_opcode() const { return opcode; }
    
    // Get current cycle step
    inline uint8_t get_cycle_step() const { return cycle_step; }
    
    // Get state flags
    inline uint16_t get_state_flags() const { return state_flags; }
    
    // === API COMPATIBILITY METHODS ===
    // Register access methods for C wrapper compatibility
    inline uint8_t get_a() const { return reg[CpuReg::A]; }
    inline uint8_t get_x() const { return reg[CpuReg::X]; }
    inline uint8_t get_y() const { return reg[CpuReg::Y]; }
    inline uint8_t get_s() const { return reg[CpuReg::S]; }
    inline uint8_t get_p() const { return reg[CpuReg::P]; }
    inline uint16_t get_pc() const { return (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL]; }
    
    inline void set_a(uint8_t val) { reg[CpuReg::A] = val; }
    inline void set_x(uint8_t val) { reg[CpuReg::X] = val; }
    inline void set_y(uint8_t val) { reg[CpuReg::Y] = val; }
    inline void set_s(uint8_t val) { reg[CpuReg::S] = val; }
    inline void set_p(uint8_t val) { reg[CpuReg::P] = val; }
    inline void set_pc(uint16_t val) {
        reg[CpuReg::PCL] = val & 0xFF;
        reg[CpuReg::PCH] = (val >> 8) & 0xFF;
    }
    
    // Debug functions - inline for performance
    inline uint8_t get_reg(int r) const {
        return (r < static_cast<int>(CpuReg::COUNT)) ? reg[r] : 0;
    }
    inline void set_reg(int r, uint8_t val) {
        if (r < static_cast<int>(CpuReg::COUNT)) reg[r] = val;
    }
    
    // Interrupt control - inline for performance
    
    // Hardware-accurate NMI pin control with proper edge detection
    inline void nmi_pin(bool pin_state) {
        // NMI is active-low, so detect falling edge (high to low transition)
        if (prev_nmi_pin_state && !pin_state) {
            // Falling edge detected - latch NMI interrupt
            set_state(STATE_NMI_EDGE | STATE_NMI_PENDING);
        }
        prev_nmi_pin_state = pin_state;
    }
    
    // Legacy NMI trigger method for compatibility (software-triggered NMI)
    inline void nmi() {
        // Software-triggered NMI - immediately set edge and pending
        set_state(STATE_NMI_EDGE | STATE_NMI_PENDING);
    }
    
    // Enhanced IRQ pin control with proper level-sensitive detection
    inline void irq(bool pin_state) {
        // Track pin state changes for debugging and edge case handling
        // bool pin_changed = (prev_irq_pin_state != pin_state);  // Reserved for future use
        prev_irq_pin_state = pin_state;
        
        if (!pin_state) {
            // IRQ pin active (low) - set line state
            set_state(STATE_IRQ_LINE);
            // Update IRQ pending state based on current mask status
            update_irq_pending_state();
        } else {
            // IRQ pin inactive (high) - clear line and pending states
            clear_state(STATE_IRQ_LINE | STATE_IRQ_PENDING);
        }
    }
    
    // Enhanced IRQ control with multiple source support
    inline void irq_source(uint8_t source_bit, bool active) {
        if (active) {
            irq_sources |= (1 << source_bit);  // Set source bit
        } else {
            irq_sources &= ~(1 << source_bit); // Clear source bit
        }
        
        // Update IRQ line state based on any active sources
        if (irq_sources != 0) {
            set_state(STATE_IRQ_LINE);
            update_irq_pending_state();
        } else {
            clear_state(STATE_IRQ_LINE | STATE_IRQ_PENDING);
        }
    }
    
    // Check if IRQ is currently masked by I flag
    inline bool is_irq_masked() const {
        return (reg[CpuReg::P] & P_IRQ_DIS) != 0;
    }
    
    // Update IRQ pending state based on current mask status
    inline void update_irq_pending_state() {
        if (get_state(STATE_IRQ_LINE)) {
            // IRQ line is active - set pending only if not masked
            if (!is_irq_masked()) {
                set_state(STATE_IRQ_PENDING);
            } else {
                clear_state(STATE_IRQ_PENDING);
            }
        } else {
            // IRQ line inactive - clear pending
            clear_state(STATE_IRQ_PENDING);
        }
    }
    
    // Handle SEI/CLI instruction effects on IRQ processing
    inline void handle_interrupt_flag_change() {
        // When I flag changes, update IRQ pending state accordingly
        // This ensures proper hardware-accurate behavior when CLI/SEI are executed
        update_irq_pending_state();
    }
    
    // RESET pin control
    inline void reset() {
        set_state(STATE_RESET_PENDING);
        // Reset also clears all pending interrupts and state tracking
        clear_state(STATE_NMI_PENDING | STATE_IRQ_PENDING | STATE_NMI_EDGE | STATE_IRQ_LINE);
        prev_nmi_pin_state = true; // Reset NMI pin state to high
        prev_irq_pin_state = true; // Reset IRQ pin state to high
        irq_sources = 0;           // Clear all IRQ sources
    }
    
    // === CONTROL LINE PROCESSING ===
    
    // Process input control lines - hardware-accurate pin handling
    inline bus_state_t process_input_pins(bus_state_t bus_state) {
        // Batch check all control pins at once for maximum speed
        constexpr bus_state_t control_pin_mask =
            (Config::has_so_pin ? BUS_BIT(BUS_SO_BIT) : 0) |
            (Config::has_be_pin ? BUS_BIT(BUS_BE_BIT) : 0) |
            (Config::has_abort_pin ? BUS_BIT(BUS_ABORT_BIT) : 0);
        
        const bus_state_t active_pins = bus_state & control_pin_mask;
        
        // Only process if any control pins are relevant
        if constexpr (control_pin_mask != 0) {
            // SO (Set Overflow) pin - edge detection for NMOS variants
            if constexpr (Config::has_so_pin) {
                static bool prev_so_state = true; // SO is active-low
                const bool current_so = (active_pins & BUS_BIT(BUS_SO_BIT)) != 0;
                
                // Edge detection: transition from high to low
                if (prev_so_state && !current_so) {
                    set_state(STATE_SO_EDGE);
                }
                prev_so_state = current_so;
                
                // Handle SO edge during instruction execution (NMOS behavior)
                if constexpr (Config::cpu_variant == CpuVariant::NMOS_6502 ||
                             Config::cpu_variant == CpuVariant::NMOS_6510) {
                    if (get_state(STATE_SO_EDGE) && cycle_step > 0) {
                        clear_state(STATE_SO_EDGE);
                        reg[CpuReg::P] |= P_OVERFLOW;
                    }
                }
            }
            
            // BE (Bus Enable) pin - 65C02/65C816 bus control
            if constexpr (Config::has_be_pin) {
                if (!(active_pins & BUS_BIT(BUS_BE_BIT))) {
                    // BE low: CPU should tri-state its outputs
                    // Set internal flag to indicate bus is disabled
                    set_state(STATE_DMA_CYCLE);
                    return bus_state; // Skip processing when bus is disabled
                } else {
                    clear_state(STATE_DMA_CYCLE);
                }
            }
            
            // ABORT pin - 65C816 abort interrupt
            if constexpr (Config::has_abort_pin) {
                if (!(active_pins & BUS_BIT(BUS_ABORT_BIT))) {
                    // ABORT is active-low, triggers abort interrupt
                    if (!get_state(STATE_RESET_PENDING)) {
                        // TODO: Implement full ABORT interrupt sequence
                        set_state(STATE_IRQ_PENDING); // Simplified for now
                    }
                }
            }
        }
        return bus_state;
    }
    
    // Handle RDY pin - variant-specific behavior with proper timing
    inline bus_state_t handle_rdy_wait(bus_state_t bus_state) {
        // RDY is active-high (0 = not ready, 1 = ready)
        bool rdy_blocks = false;
        
        if constexpr (Config::rdy_affects_writes) {
            // CMOS behavior: RDY affects all cycles
            rdy_blocks = true;
        } else {
            // NMOS behavior: RDY only affects read cycles during instruction execution
            if (cycle_step > 0) {
                // Get current cycle description to determine if it's a read
                const fam65xx_cpp::cycle_desc_t cycle = GET_CYCLE(opcode, cycle_step);
                const MemOp mem_op = static_cast<MemOp>(cycle.mem_op);
                
                // Check if current cycle is a read operation
                const bool is_read_cycle = (mem_op == MemOp::READ_PC_INC ||
                                          mem_op == MemOp::READ_PC ||
                                          mem_op == MemOp::READ_ABS ||
                                          mem_op == MemOp::READ_ZP ||
                                          mem_op == MemOp::READ_SP ||
                                          mem_op == MemOp::READ_SP_INC ||
                                          mem_op == MemOp::READ_INDIRECT);
                rdy_blocks = is_read_cycle;
            }
        }
        
        if (rdy_blocks) {
            set_state(STATE_RDY_WAIT);
            
            // Handle BA (Bus Available) line for 6510 AEC/BA DMA with proper timing
            if constexpr (Config::has_aec_pin) {
                // 6510 AEC/BA timing: BA goes low 3 cycles before AEC goes low
                // This is critical for VIC-II DMA timing accuracy
                static uint8_t ba_delay_counter = 0;
                
                if (ba_delay_counter < 3) {
                    ba_delay_counter++;
                    bus_state |= BUS_BIT(BUS_BA_BIT); // BA high (CPU has bus)
                } else {
                    bus_state &= ~BUS_BIT(BUS_BA_BIT); // BA low (DMA can take bus)
                    // AEC signal would also go low here in real hardware
                }
            }
        } else {
            clear_state(STATE_RDY_WAIT);
            
            // Clear BA line when CPU is not blocked
            if constexpr (Config::has_aec_pin) {
                bus_state |= BUS_BIT(BUS_BA_BIT); // BA high (CPU has bus)
            }
        }
        
        return bus_state;
    }
    
    // Decimal mode bug handling - NMOS variants have incorrect N and Z flag handling
    inline void handle_decimal_mode_bugs(uint8_t result, AluOp operation) {
        if constexpr (!Config::has_cmos_fixes) {
            // NMOS 6502/6510 decimal mode bugs
            if (reg[CpuReg::P] & P_DECIMAL) {
                switch (operation) {
                    case AluOp::ADC:
                    case AluOp::SBC:
                        // NMOS bug: N and Z flags are set based on binary result, not BCD result
                        // The ALU operation already set these flags incorrectly, so we leave them
                        // This matches real NMOS hardware behavior
                        break;
                    default:
                        break;
                }
            }
        } else {
            // CMOS variants (65C02, 65C816) fix decimal mode
            if (reg[CpuReg::P] & P_DECIMAL) {
                switch (operation) {
                    case AluOp::ADC:
                    case AluOp::SBC:
                        // CMOS fix: N and Z flags correctly reflect BCD result
                        reg[CpuReg::P] &= ~(P_NEGATIVE | P_ZERO);
                        if (result == 0) reg[CpuReg::P] |= P_ZERO;
                        if (result & 0x80) reg[CpuReg::P] |= P_NEGATIVE;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
    // CMOS timing improvements - 65C02 fixes several timing edge cases
    inline bool has_cmos_timing_fix(uint8_t opcode) {
        if constexpr (Config::has_cmos_fixes) {
            switch (opcode) {
                // 65C02 fixes: JMP ($xxxx) now correctly handles page boundaries
                case 0x6C: // JMP ($nnnn)
                    return true;
                    
                // 65C02 fixes: Indexed addressing modes handle page crossing consistently
                case 0xBE: // LDX $nnnn,Y
                case 0xBC: // LDY $nnnn,X
                    return true;
                    
                // 65C02 adds proper cycle timing for new instructions
                case 0x80: // BRA
                case 0x89: // BIT #$nn
                case 0x34: // BIT $nn,X
                case 0x3C: // BIT $nnnn,X
                    return true;
                    
                default:
                    return false;
            }
        }
        return false;
    }
    
    // Enhanced SO pin edge detection with NMOS vs CMOS differences
    inline void process_so_pin_edge() {
        if constexpr (Config::has_so_pin) {
            // Batch check SO-related states
            const uint16_t so_states = state_flags & STATE_SO_EDGE;
            if (so_states) {
                if constexpr (Config::cpu_variant == CpuVariant::NMOS_6502 ||
                             Config::cpu_variant == CpuVariant::NMOS_6510) {
                    // NMOS behavior: SO edge can occur at any point during instruction
                    // and immediately sets overflow flag
                    reg[CpuReg::P] |= P_OVERFLOW;
                    clear_state(STATE_SO_EDGE);
                } else {
                    // CMOS behavior: SO edge is synchronized to instruction boundaries
                    if (cycle_step == 0) { // Only process at instruction start
                        reg[CpuReg::P] |= P_OVERFLOW;
                        clear_state(STATE_SO_EDGE);
                    }
                }
            }
        }
    }
    
    // Process output control lines
    inline bus_state_t process_output_pins(bus_state_t bus_state) {
        // Batch check output state flags for speed
        const uint16_t output_states = state_flags & (STATE_SYNC_NEXT);
        
        // SYNC pin - indicates opcode fetch cycle
        if constexpr (Config::has_sync_pin) {
            if (output_states & STATE_SYNC_NEXT) {
                bus_state |= BUS_BIT(BUS_SYNC_BIT);
                clear_state(STATE_SYNC_NEXT);
            } else {
                bus_state &= ~BUS_BIT(BUS_SYNC_BIT);
            }
        }
        
        // VP (Vector Pull) pin - indicates interrupt vector fetch
        if constexpr (Config::has_vp_pin) {
            const uint16_t pc = get_pc();
            const bool is_vector_area = (pc >= 0xFFFA && pc <= 0xFFFF);
            
            if (is_vector_area && cycle_step > 0) {
                bus_state |= BUS_BIT(BUS_VP_BIT);
            } else {
                bus_state &= ~BUS_BIT(BUS_VP_BIT);
            }
        }
        
        // ML (Memory Lock) pin - 65C816 memory protection
        if constexpr (Config::has_ml_pin) {
            // TODO: Implement proper memory lock detection
            // For now, always clear (no memory lock active)
            bus_state &= ~BUS_BIT(BUS_ML_BIT);
        }
        return bus_state;
    }
    
    // === I/O PORT HANDLING (6510 SPECIFIC) ===
    template<typename IOCallback = void*>
    inline void set_io_callback(IOCallback callback = nullptr) {
        // Implementation depends on configuration
        // For now, store callback if needed by variant
    }
    
    // Handle JMP indirect page boundary bug/fix
    inline void handle_jmp_indirect_bug() {
        const uint16_t indirect_addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
        uint16_t target_addr;
        
        if constexpr (Config::has_cmos_fixes) {
            // CMOS fix: JMP ($xxFF) correctly reads from $xxFF and $xx00+1
            target_addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
        } else {
            // NMOS bug: JMP ($xxFF) reads from $xxFF and $xx00 instead of $xx00+1
            if ((indirect_addr & 0xFF) == 0xFF) {
                // Page boundary bug: high byte comes from same page
                // The bug has already been captured in the cycle, ABH contains wrong data
                target_addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
            } else {
                // Normal case: no page boundary crossed
                target_addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
            }
        }
        
        // Execute the jump
        reg[CpuReg::PCL] = target_addr & 0xFF;
        reg[CpuReg::PCH] = (target_addr >> 8) & 0xFF;
    }
    
    // Enhanced variant-specific instruction handling
    inline bool handle_variant_specific_instruction(uint8_t opcode) {
        // Handle illegal opcodes for NMOS variants
        if constexpr (Config::has_illegal_opcodes) {
            switch (opcode) {
                // JAM instructions - lock up the CPU (NMOS only)
                case 0x02: case 0x12: case 0x22: case 0x32:
                case 0x42: case 0x52: case 0x62: case 0x72:
                case 0x92: case 0xB2: case 0xD2: case 0xF2:
                    set_state(STATE_JAM_STATE);
                    return true;
                    
                // NOPD/NOPI instructions - effectively NOPs with different timing
                case 0x04: case 0x14: case 0x34: case 0x44:
                case 0x54: case 0x64: case 0x74: case 0x80:
                case 0x82: case 0x89: case 0xC2: case 0xD4:
                case 0xE2: case 0xF4:
                    // These are handled by the cycle table as NOPs
                    return true;
                    
                default:
                    break;
            }
        } else if constexpr (Config::has_cmos_fixes) {
            // CMOS variants treat illegal opcodes as NOPs
            switch (opcode) {
                case 0x02: case 0x12: case 0x22: case 0x32:
                case 0x42: case 0x52: case 0x62: case 0x72:
                case 0x92: case 0xB2: case 0xD2: case 0xF2:
                    // CMOS: illegal opcodes become NOPs instead of jamming
                    return false; // Let normal NOP handling take over
                    
                default:
                    break;
            }
        }
        
        // Handle CMOS-specific enhancements
        if constexpr (Config::has_cmos_fixes) {
            if (has_cmos_timing_fix(opcode)) {
                // Apply CMOS timing improvements
                return true;
            }
        }
        
        return false; // Continue with normal instruction processing
    }
    
    // === PAGE CROSSING DETECTION ===
    inline constexpr uint16_t page_crossed(uint16_t addr1, uint16_t addr2) {
        return (addr1 ^ addr2) & 0xFF00;
    }
};

} // namespace fam65xx_cpp

#endif // FAM65XX_HPP