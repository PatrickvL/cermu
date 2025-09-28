#ifndef MOS6502_INTERRUPTS_HPP
#define MOS6502_INTERRUPTS_HPP

#include "mos6502_optimized.hpp"

namespace fam65xx_cpp {

/**
 * MOS6502 Interrupt Handling System
 * 
 * Provides hardware-accurate interrupt processing with:
 * - Edge detection for NMI, RESET, ABORT
 * - Level-sensitive IRQ handling with masking
 * - Proper interrupt priority and vector handling
 * - Cycle-accurate interrupt sequence timing
 * - Stack operations for interrupt context saving/restoration
 */

// Interrupt vectors
constexpr uint16_t VECTOR_NMI   = 0xFFFA;
constexpr uint16_t VECTOR_RESET = 0xFFFC;
constexpr uint16_t VECTOR_IRQ   = 0xFFFE;
constexpr uint16_t VECTOR_BRK   = 0xFFFE;  // Same as IRQ
constexpr uint16_t VECTOR_ABORT = 0xFFE8;  // 65C816 only
constexpr uint16_t VECTOR_COP   = 0xFFE4;  // 65C816 only

// Interrupt priority levels (higher number = higher priority)
enum class InterruptPriority : uint8_t {
    NONE = 0,
    IRQ = 1,
    BRK = 2,
    COP = 3,  // 65C816
    NMI = 4,
    ABORT = 5,  // 65C816, can interrupt any instruction
    RESET = 6   // Highest priority
};

template<typename Config>
class InterruptController {
private:
    MOS6502Optimized<Config>& cpu;
    
    // Interrupt state tracking
    bool nmi_edge_detected = false;
    bool irq_line_active = false;
    bool reset_line_active = false;
    bool abort_edge_detected = false;
    
    // Interrupt sequence state
    bool interrupt_in_progress = false;
    InterruptPriority current_interrupt = InterruptPriority::NONE;
    uint8_t interrupt_cycle = 0;
    
public:
    InterruptController(MOS6502Optimized<Config>& cpu_ref) : cpu(cpu_ref) {}
    
    // Hardware pin interface
    void set_nmi_pin(bool state) {
        static bool prev_nmi_state = true;
        if (prev_nmi_state && !state) {  // Falling edge detection
            nmi_edge_detected = true;
        }
        prev_nmi_state = state;
    }
    
    void set_irq_pin(bool state) {
        irq_line_active = !state;  // IRQ is active low
    }
    
    void set_reset_pin(bool state) {
        reset_line_active = !state;  // RESET is active low
        if (reset_line_active) {
            // RESET has highest priority - interrupt any current sequence
            start_interrupt_sequence(InterruptPriority::RESET);
        }
    }
    
    template<typename T = Config>
    typename std::enable_if<T::has_abort_pin, void>::type 
    set_abort_pin(bool state) {
        static bool prev_abort_state = true;
        if (prev_abort_state && !state) {  // Falling edge detection
            abort_edge_detected = true;
            // ABORT can interrupt at any cycle
            start_interrupt_sequence(InterruptPriority::ABORT);
        }
        prev_abort_state = state;
    }
    
    // Check for pending interrupts
    InterruptPriority check_pending_interrupts() const {
        // Check interrupts in priority order
        if (reset_line_active) return InterruptPriority::RESET;
        
        if constexpr (Config::has_abort_pin) {
            if (abort_edge_detected) return InterruptPriority::ABORT;
        }
        
        if (nmi_edge_detected) return InterruptPriority::NMI;
        
        // IRQ is maskable
        if (irq_line_active && !(cpu.get_p() & P_IRQ_DIS)) {
            return InterruptPriority::IRQ;
        }
        
        return InterruptPriority::NONE;
    }
    
    // Start interrupt sequence
    bool start_interrupt_sequence(InterruptPriority priority) {
        if (interrupt_in_progress && priority <= current_interrupt) {
            return false;  // Can't interrupt higher or equal priority
        }
        
        // Clear the corresponding edge flag
        switch (priority) {
            case InterruptPriority::NMI:
                nmi_edge_detected = false;
                break;
            case InterruptPriority::ABORT:
                if constexpr (Config::has_abort_pin) {
                    abort_edge_detected = false;
                }
                break;
            default:
                break;
        }
        
        interrupt_in_progress = true;
        current_interrupt = priority;
        interrupt_cycle = 1;
        
        return true;
    }
    
    // Execute one cycle of interrupt sequence
    bus_state_t execute_interrupt_cycle(bus_state_t bus_state) {
        if (!interrupt_in_progress) return bus_state;
        
        switch (interrupt_cycle) {
            case 1:
                // Cycle 1: Internal operation (finish current instruction partially)
                break;
                
            case 2:
                // Cycle 2: Internal operation
                break;
                
            case 3:
                // Cycle 3: Push PCH to stack
                bus_state = push_to_stack(bus_state, cpu.get_register(CpuReg8::PCH));
                break;
                
            case 4:
                // Cycle 4: Push PCL to stack
                bus_state = push_to_stack(bus_state, cpu.get_register(CpuReg8::PCL));
                break;
                
            case 5:
                // Cycle 5: Push P to stack (with B flag set for BRK)
                {
                    uint8_t status = cpu.get_p();
                    if (current_interrupt == InterruptPriority::BRK) {
                        status |= P_BREAK;  // Set B flag for BRK
                    }
                    status |= P_UNUSED;  // Always set unused flag
                    bus_state = push_to_stack(bus_state, status);
                    
                    // Set I flag to disable further IRQs
                    cpu.set_p(cpu.get_p() | P_IRQ_DIS);
                }
                break;
                
            case 6:
                // Cycle 6: Read interrupt vector low byte
                {
                    uint16_t vector = get_interrupt_vector(current_interrupt);
                    bus_state = BUS_SET_ADDR(bus_state, vector);
                    bus_state |= BUS_BIT(BUS_RW_BIT);  // Read
                    cpu.set_register(CpuReg8::PCL, BUS_GET_DATA(bus_state));
                }
                break;
                
            case 7:
                // Cycle 7: Read interrupt vector high byte
                {
                    uint16_t vector = get_interrupt_vector(current_interrupt);
                    bus_state = BUS_SET_ADDR(bus_state, vector + 1);
                    bus_state |= BUS_BIT(BUS_RW_BIT);  // Read
                    cpu.set_register(CpuReg8::PCH, BUS_GET_DATA(bus_state));
                    
                    // Interrupt sequence complete
                    interrupt_in_progress = false;
                    current_interrupt = InterruptPriority::NONE;
                    interrupt_cycle = 0;
                    return bus_state;
                }
                break;
        }
        
        interrupt_cycle++;
        return bus_state;
    }
    
    // Check if interrupt sequence is in progress
    bool is_interrupt_in_progress() const {
        return interrupt_in_progress;
    }
    
    // Get current interrupt type
    InterruptPriority get_current_interrupt() const {
        return current_interrupt;
    }
    
private:
    // Get interrupt vector address
    uint16_t get_interrupt_vector(InterruptPriority priority) const {
        switch (priority) {
            case InterruptPriority::RESET: return VECTOR_RESET;
            case InterruptPriority::NMI:   return VECTOR_NMI;
            case InterruptPriority::IRQ:   return VECTOR_IRQ;
            case InterruptPriority::BRK:   return VECTOR_BRK;
            case InterruptPriority::ABORT: 
                if constexpr (Config::has_abort_pin) {
                    return VECTOR_ABORT;
                }
                break;
            case InterruptPriority::COP:   
                if constexpr (Config::has_abort_pin) {  // COP exists on same chips as ABORT
                    return VECTOR_COP;
                }
                break;
            default: break;
        }
        return VECTOR_IRQ;  // Default fallback
    }
    
    // Push data to stack
    bus_state_t push_to_stack(bus_state_t bus_state, uint8_t data) {
        uint16_t stack_addr = 0x0100 | cpu.get_sp();
        bus_state = BUS_SET_ADDR(bus_state, stack_addr);
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state &= ~BUS_BIT(BUS_RW_BIT);  // Write
        
        // Decrement stack pointer
        cpu.set_sp(cpu.get_sp() - 1);
        
        return bus_state;
    }
    
    // Pull data from stack
    bus_state_t pull_from_stack(bus_state_t bus_state, CpuReg8 target_reg) {
        // Increment stack pointer first
        cpu.set_sp(cpu.get_sp() + 1);
        
        uint16_t stack_addr = 0x0100 | cpu.get_sp();
        bus_state = BUS_SET_ADDR(bus_state, stack_addr);
        bus_state |= BUS_BIT(BUS_RW_BIT);  // Read
        
        // Store pulled data in target register
        cpu.set_register(target_reg, BUS_GET_DATA(bus_state));
        
        return bus_state;
    }
};

/**
 * Stack Operations Helper
 * 
 * Provides stack operation implementations for:
 * - PHA, PHP (Push operations)
 * - PLA, PLP (Pull operations)  
 * - JSR, RTS (Subroutine operations)
 * - Interrupt context save/restore
 */
template<typename Config>
class StackOperations {
private:
    MOS6502Optimized<Config>& cpu;
    
public:
    StackOperations(MOS6502Optimized<Config>& cpu_ref) : cpu(cpu_ref) {}
    
    // PHA - Push Accumulator (3 cycles)
    bus_state_t execute_pha_cycle(bus_state_t bus_state, uint8_t cycle) {
        switch (cycle) {
            case 1:
                // Internal operation
                break;
            case 2:
                // Push accumulator to stack
                return push_to_stack(bus_state, cpu.get_a());
        }
        return bus_state;
    }
    
    // PHP - Push Processor Status (3 cycles)
    bus_state_t execute_php_cycle(bus_state_t bus_state, uint8_t cycle) {
        switch (cycle) {
            case 1:
                // Internal operation
                break;
            case 2:
                // Push status with B and U flags set
                {
                    uint8_t status = cpu.get_p() | P_BREAK | P_UNUSED;
                    return push_to_stack(bus_state, status);
                }
        }
        return bus_state;
    }
    
    // PLA - Pull Accumulator (4 cycles)
    bus_state_t execute_pla_cycle(bus_state_t bus_state, uint8_t cycle) {
        switch (cycle) {
            case 1:
                // Internal operation
                break;
            case 2:
                // Internal operation (increment SP)
                cpu.set_sp(cpu.get_sp() + 1);
                break;
            case 3:
                // Pull accumulator from stack
                bus_state = pull_from_stack(bus_state, CpuReg8::A);
                // Set N and Z flags based on pulled value
                update_nz_flags(cpu.get_a());
                break;
        }
        return bus_state;
    }
    
    // PLP - Pull Processor Status (4 cycles)
    bus_state_t execute_plp_cycle(bus_state_t bus_state, uint8_t cycle) {
        switch (cycle) {
            case 1:
                // Internal operation
                break;
            case 2:
                // Internal operation (increment SP)
                cpu.set_sp(cpu.get_sp() + 1);
                break;
            case 3:
                // Pull status from stack
                bus_state = pull_from_stack(bus_state, CpuReg8::P);
                // Clear B flag and set U flag (hardware behavior)
                uint8_t status = cpu.get_p();
                status &= ~P_BREAK;
                status |= P_UNUSED;
                cpu.set_p(status);
                break;
        }
        return bus_state;
    }
    
    // JSR - Jump to Subroutine (6 cycles)
    bus_state_t execute_jsr_cycle(bus_state_t bus_state, uint8_t cycle) {
        static uint16_t target_address = 0;
        
        switch (cycle) {
            case 1:
                // Fetch target address low
                target_address = (target_address & 0xFF00) | BUS_GET_DATA(bus_state);
                break;
            case 2:
                // Internal operation
                break;
            case 3:
                // Push return address high byte
                {
                    uint16_t return_addr = cpu.get_pc();
                    bus_state = push_to_stack(bus_state, (return_addr >> 8) & 0xFF);
                }
                break;
            case 4:
                // Push return address low byte
                {
                    uint16_t return_addr = cpu.get_pc();
                    bus_state = push_to_stack(bus_state, return_addr & 0xFF);
                }
                break;
            case 5:
                // Fetch target address high
                target_address = (target_address & 0x00FF) | (BUS_GET_DATA(bus_state) << 8);
                // Set PC to target address
                cpu.set_pc(target_address);
                break;
        }
        return bus_state;
    }
    
    // RTS - Return from Subroutine (6 cycles)
    bus_state_t execute_rts_cycle(bus_state_t bus_state, uint8_t cycle) {
        static uint16_t return_address = 0;
        
        switch (cycle) {
            case 1:
                // Internal operation
                break;
            case 2:
                // Internal operation (increment SP)
                cpu.set_sp(cpu.get_sp() + 1);
                break;
            case 3:
                // Pull return address low byte
                bus_state = pull_from_stack(bus_state, CpuReg8::DL);
                return_address = (return_address & 0xFF00) | cpu.get_register(CpuReg8::DL);
                break;
            case 4:
                // Pull return address high byte
                bus_state = pull_from_stack(bus_state, CpuReg8::DL);
                return_address = (return_address & 0x00FF) | (cpu.get_register(CpuReg8::DL) << 8);
                break;
            case 5:
                // Internal operation, increment return address
                return_address++;
                cpu.set_pc(return_address);
                break;
        }
        return bus_state;
    }
    
    // RTI - Return from Interrupt (6 cycles)
    bus_state_t execute_rti_cycle(bus_state_t bus_state, uint8_t cycle) {
        static uint16_t return_address = 0;
        
        switch (cycle) {
            case 1:
                // Internal operation
                break;
            case 2:
                // Internal operation (increment SP)
                cpu.set_sp(cpu.get_sp() + 1);
                break;
            case 3:
                // Pull processor status
                bus_state = pull_from_stack(bus_state, CpuReg8::P);
                // Clear B flag and set U flag
                {
                    uint8_t status = cpu.get_p();
                    status &= ~P_BREAK;
                    status |= P_UNUSED;
                    cpu.set_p(status);
                }
                break;
            case 4:
                // Pull return address low byte
                bus_state = pull_from_stack(bus_state, CpuReg8::DL);
                return_address = (return_address & 0xFF00) | cpu.get_register(CpuReg8::DL);
                break;
            case 5:
                // Pull return address high byte
                bus_state = pull_from_stack(bus_state, CpuReg8::DL);
                return_address = (return_address & 0x00FF) | (cpu.get_register(CpuReg8::DL) << 8);
                cpu.set_pc(return_address);
                break;
        }
        return bus_state;
    }
    
private:
    // Helper functions
    bus_state_t push_to_stack(bus_state_t bus_state, uint8_t data) {
        uint16_t stack_addr = 0x0100 | cpu.get_sp();
        bus_state = BUS_SET_ADDR(bus_state, stack_addr);
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state &= ~BUS_BIT(BUS_RW_BIT);  // Write
        cpu.set_sp(cpu.get_sp() - 1);
        return bus_state;
    }
    
    bus_state_t pull_from_stack(bus_state_t bus_state, CpuReg8 target_reg) {
        cpu.set_sp(cpu.get_sp() + 1);
        uint16_t stack_addr = 0x0100 | cpu.get_sp();
        bus_state = BUS_SET_ADDR(bus_state, stack_addr);
        bus_state |= BUS_BIT(BUS_RW_BIT);  // Read
        cpu.set_register(target_reg, BUS_GET_DATA(bus_state));
        return bus_state;
    }
    
    void update_nz_flags(uint8_t value) {
        uint8_t flags = cpu.get_p() & ~(P_NEGATIVE | P_ZERO);
        flags |= (value & 0x80) ? P_NEGATIVE : 0;
        flags |= (value == 0) ? P_ZERO : 0;
        cpu.set_p(flags);
    }
};

} // namespace fam65xx_cpp

#endif // MOS6502_INTERRUPTS_HPP