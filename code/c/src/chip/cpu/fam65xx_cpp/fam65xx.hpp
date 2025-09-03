#ifndef FAM65XX_HPP
#define FAM65XX_HPP

#include "cpu_defs.hpp"
#include "cpu_config.hpp"
#include "alu_operations.hpp"
#include "memory_operations.hpp"
#include "cycle_tables.hpp"
#include "../../../core/system_lines.h"

namespace fam65xx_cpp {

template<typename Config>
class fam65xx {
private:
    // CPU state
    uint8_t opcode = 0;
    uint8_t cycle_step = 0;
    uint16_t state_flags = 0;
    uint8_t pending_data = 0;
    uint8_t pending_data_op = 0;
    
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
    static inline constexpr fam65xx_cpp::cycle_desc_t GET_CYCLE(uint8_t opcode, uint8_t step) {
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
        
        // Handle reset
        if (get_state(STATE_RESET_PENDING)) {
            return handle_reset(bus_state);
        }
        
        // Handle interrupts
        if (cycle_step == 0) {
            if (get_state(STATE_NMI_PENDING)) {
                return handle_nmi(bus_state);
            } else if (get_state(STATE_IRQ_PENDING)) {
                if (!(reg[static_cast<uint8_t>(CpuReg::P)] & P_IRQ_DIS)) {
                    return handle_irq(bus_state);
                }
            }
        }
        
        // Execute instruction cycle
        bus_state = execute_cycle(bus_state);
        
        // Process output control lines
        bus_state = process_output_pins(bus_state);
        
        return bus_state;
    }
    
    // Handle reset sequence
    inline bus_state_t handle_reset(bus_state_t bus_state) {
        // Reset takes 7 cycles, simplified implementation
        if (cycle_step == 0) {
            reg[CpuReg::S] = 0xFF;
            reg[CpuReg::P] = P_IRQ_DIS | P_UNUSED;
            cycle_step = 1;
        }
        
        if (cycle_step >= 7) {
            // Load reset vector
            BUS_SET_ADDR(bus_state, 0xFFFC);
            clear_state(STATE_RESET_PENDING);
            cycle_step = 0;
        } else {
            cycle_step++;
        }
        return bus_state;
    }
    
    // Handle NMI interrupt
    inline bus_state_t handle_nmi(bus_state_t bus_state) {
        // NMI sequence - simplified
        clear_state(STATE_NMI_PENDING);
        reg[CpuReg::P] |= P_IRQ_DIS;
        BUS_SET_ADDR(bus_state, 0xFFFA);
        return bus_state;
    }
    
    // Handle IRQ interrupt
    inline bus_state_t handle_irq(bus_state_t bus_state) {
        // IRQ sequence - simplified
        clear_state(STATE_IRQ_PENDING);
        reg[CpuReg::P] |= P_IRQ_DIS;
        BUS_SET_ADDR(bus_state, 0xFFFE);
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
        
        // Execute data operation
        execute_data_operation(cycle.data_op, BUS_GET_DATA(bus_state));
        
        // Execute ALU operation if specified
        if (alu_op != AluOp::NOP) {
            alu_ops::execute_alu_operation(reg, alu_op, BUS_GET_DATA(bus_state));
        }
        
        // Check if instruction is complete
        const uint8_t total_cycles = cycle_tables::get_cycle_count(opcode);
        if (cycle_step >= total_cycles - 1) {
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
        
        if (pending_data_op < static_cast<uint8_t>(CpuReg::COUNT)) {
            // Direct register load
            reg[pending_data_op] = data;
        } else {
            switch (static_cast<DataOp>(pending_data_op)) {
                case DataOp::ALU:
                    pending_data = data;
                    break;
                case DataOp::ADDR_CALC_LOW:
                    reg[CpuReg::ABL] = data;
                    break;
                case DataOp::ADDR_CALC_HIGH:
                    reg[CpuReg::ABH] = data;
                    // For JMP absolute, execute the jump now
                    if (opcode == 0x4C) {
                        const uint16_t addr = (reg[CpuReg::ABH] << 8) | reg[CpuReg::ABL];
                        reg[CpuReg::PCL] = addr & 0xFF;
                        reg[CpuReg::PCH] = (addr >> 8) & 0xFF;
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
        if (cycle_step % 2 == 1) {
            reg[CpuReg::PCL] = data;
        } else {
            reg[CpuReg::PCH] = data;
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
    inline uint8_t get_opcode() const { return opcode; }
    
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
    inline void nmi() {
        set_state(STATE_NMI_EDGE | STATE_NMI_PENDING);
    }
    inline void irq(bool state) {
        if (!state) {
            set_state(STATE_IRQ_LINE);
            if (!(reg[CpuReg::P] & P_IRQ_DIS)) {
                set_state(STATE_IRQ_PENDING);
            }
        } else {
            clear_state(STATE_IRQ_LINE);
        }
    }
    inline void reset() {
        set_state(STATE_RESET_PENDING);
    }
    
    // === CONTROL LINE PROCESSING ===
    
    // Process input control lines - hardware-accurate pin handling
    inline bus_state_t process_input_pins(bus_state_t bus_state) {
        // SO (Set Overflow) pin - edge detection for NMOS variants
        if constexpr (Config::has_so_pin) {
            static bool prev_so_state = true; // SO is active-low
            bool current_so = (bus_state & BUS_BIT(BUS_SO_BIT)) != 0;
            
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
            if (!(bus_state & BUS_BIT(BUS_BE_BIT))) {
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
            if (!(bus_state & BUS_BIT(BUS_ABORT_BIT))) {
                // ABORT is active-low, triggers abort interrupt
                if (!get_state(STATE_RESET_PENDING)) {
                    // TODO: Implement full ABORT interrupt sequence
                    set_state(STATE_IRQ_PENDING); // Simplified for now
                }
            }
        }
        return bus_state;
    }
    
    // Handle RDY pin - variant-specific behavior
    inline bus_state_t handle_rdy_wait(bus_state_t bus_state) {
        // RDY is active-high (0 = not ready, 1 = ready)
        bool rdy_blocks = false;
        
        if constexpr (Config::rdy_affects_writes) {
            // CMOS behavior: RDY affects all cycles
            rdy_blocks = true;
        } else {
            // NMOS behavior: RDY only affects read cycles during instruction execution
            if (cycle_step > 0) {
                // Check if current cycle is a read (simplified)
                const bool is_read = (bus_state & BUS_BIT(BUS_RW_BIT)) != 0;
                rdy_blocks = is_read;
            }
        }
        
        if (rdy_blocks) {
            set_state(STATE_RDY_WAIT);
            
            // Handle BA (Bus Available) line for 6510 AEC/BA DMA
            if constexpr (Config::has_aec_pin) {
                // Set BA line when CPU is blocked by RDY
                bus_state |= BUS_BIT(BUS_BA_BIT);
            }
        } else {
            clear_state(STATE_RDY_WAIT);
            
            // Clear BA line when CPU is not blocked
            if constexpr (Config::has_aec_pin) {
                bus_state &= ~BUS_BIT(BUS_BA_BIT);
            }
        }
        
        return bus_state;
    }
    
    // Process output control lines
    inline bus_state_t process_output_pins(bus_state_t bus_state) {
        // SYNC pin - indicates opcode fetch cycle
        if constexpr (Config::has_sync_pin) {
            if (get_state(STATE_SYNC_NEXT)) {
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
    
    // === PAGE CROSSING DETECTION ===
    inline constexpr uint16_t page_crossed(uint16_t addr1, uint16_t addr2) {
        return (addr1 ^ addr2) & 0xFF00;
    }
};

} // namespace fam65xx_cpp

#endif // FAM65XX_HPP