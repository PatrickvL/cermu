#ifndef CPU_HPP
#define CPU_HPP

#include "cpu_defs.hpp"
#include "cpu_config.hpp"
#include "alu_operations.hpp" 
#include "memory_operations.hpp"
#include "cycle_tables.hpp"
#include "../../../core/system_lines.h"

namespace fam65xx_cpp {

template<typename Config>
class cpu_6510 {
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

public:
    // Constructor
    cpu_6510() {
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
    inline void step(bus_state_t& bus_state) {
        // Handle RDY line - NMOS behavior (constexpr conditions first)
        if constexpr (Config::cpu_variant == CpuVariant::NMOS_6502 ||
                     Config::cpu_variant == CpuVariant::NMOS_6510) {
            if (get_state(STATE_RDY_WAIT)) {
                bool rdy_blocks = false;
                
                if (cycle_step > 0) {
                    // NMOS: RDY blocks read cycles during instruction execution
                    rdy_blocks = true; // Simplified - should check if it's a read
                }
                
                // Handle BA (Bus Available) line for C64 6510
                if constexpr (Config::cpu_variant == CpuVariant::NMOS_6510) {
                    if (rdy_blocks) {
                        // Set BA line when blocked
                    } else {
                        // Clear BA line when not blocked
                    }
                }
                
                if (rdy_blocks) {
                    return; // Skip this cycle
                }
            }
        }
        
        // Handle SO (Set Overflow) edge detection (constexpr conditions first)
        if constexpr (Config::cpu_variant != CpuVariant::CMOS_65C02) {
            if (get_state(STATE_SO_EDGE)) {
                clear_state(STATE_SO_EDGE);
                // NMOS behavior: SO sets overflow flag
                if (cycle_step > 0) { // Only during instruction execution
                    reg[CpuReg::P] |= P_OVERFLOW;
                }
            }
        }
        
        // Handle reset
        if (get_state(STATE_RESET_PENDING)) {
            handle_reset(bus_state);
            return;
        }
        
        // Handle interrupts
        if (cycle_step == 0) {
            if (get_state(STATE_NMI_PENDING)) {
                handle_nmi(bus_state);
                return;
            } else if (get_state(STATE_IRQ_PENDING)) {
                if (!(reg[static_cast<uint8_t>(CpuReg::P)] & P_IRQ_DIS)) {
                    handle_irq(bus_state);
                    return;
                }
            }
        }
        
        // Execute instruction cycle
        execute_cycle(bus_state);
    }
    
    // Handle reset sequence
    inline void handle_reset(bus_state_t& bus_state) {
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
    }
    
    // Handle NMI interrupt
    inline void handle_nmi(bus_state_t& bus_state) {
        // NMI sequence - simplified
        clear_state(STATE_NMI_PENDING);
        reg[CpuReg::P] |= P_IRQ_DIS;
        BUS_SET_ADDR(bus_state, 0xFFFA);
    }
    
    // Handle IRQ interrupt  
    inline void handle_irq(bus_state_t& bus_state) {
        // IRQ sequence - simplified
        clear_state(STATE_IRQ_PENDING);
        reg[CpuReg::P] |= P_IRQ_DIS;
        BUS_SET_ADDR(bus_state, 0xFFFE);
    }
    
    // Execute one instruction cycle
    inline void execute_cycle(bus_state_t& bus_state) {
        // Fetch opcode on cycle 0
        if (cycle_step == 0) {
            opcode = BUS_GET_DATA(bus_state);
            cycle_step = 1;
            set_state(STATE_SYNC_NEXT);
            return;
        }
        
        // Get cycle description for current instruction step
        const fam65xx_cpp::cycle_desc_t cycle = GET_CYCLE(opcode, cycle_step);
        
        // Convert raw values to type-safe enums
        MemOp mem_op = static_cast<MemOp>(cycle.mem_op);
        DataOp data_op = static_cast<DataOp>(cycle.data_op);
        AluOp alu_op = static_cast<AluOp>(cycle.alu_op);
        
        // Execute memory operation
        memory_ops::execute_memory_operation(bus_state, reg, mem_op, data_op);
        
        // Handle write data if needed
        memory_ops::handle_write_data(bus_state, reg, mem_op, data_op);
        
        // Execute data operation
        execute_data_operation(cycle.data_op, BUS_GET_DATA(bus_state));
        
        // Execute ALU operation if specified
        if (alu_op != AluOp::NOP) {
            alu_ops::execute_alu_operation(reg, alu_op, data_op);
        }
        
        // Check if instruction is complete
        const uint8_t total_cycles = cycle_tables::get_cycle_count(opcode);
        if (cycle_step >= total_cycles - 1) {
            cycle_step = 0; // Start next instruction
        } else {
            cycle_step++;
        }
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

#endif // CPU_HPP