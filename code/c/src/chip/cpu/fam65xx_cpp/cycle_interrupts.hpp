#ifndef CYCLE_INTERRUPTS_HPP
#define CYCLE_INTERRUPTS_HPP

#include "cycle_types.hpp"
#include "cycle_addressing.hpp"

namespace fam65xx_cpp {

template<typename BusConfig>
class CycleInterrupts {
public:
    // Short alias to avoid repetitive typing
    using addr = CycleAddressing<BusConfig>;

    // === INTERRUPT CYCLE SEQUENCES (unified using shared patterns) ===
    
    // Base interrupt sequence generator - used by BRK, NMI, IRQ, and RESET
    static constexpr cycle_desc_t get_reset_cycle(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);           // Dummy read (RESET specific)
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);           // Dummy read (RESET specific)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return addr::make_empty_cycle();
    }
    
    // NMI sequence - uses shared interrupt pattern with different initial cycles
    static constexpr cycle_desc_t get_nmi_cycle(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);     // Read next instruction byte
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);         // Read next instruction byte (dummy)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return addr::make_empty_cycle();
    }
    
    // IRQ sequence - identical to NMI (different vector handled by DataOp::INTERRUPT_VEC)
    static constexpr cycle_desc_t get_irq_cycle(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);     // Read next instruction byte
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);         // Read next instruction byte (dummy)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return addr::make_empty_cycle();
    }
    
    // General interrupt sequence helper
    static constexpr cycle_desc_t make_interrupt_sequence(uint8_t cycle, AluOp flag_op = AluOp::SEI) {
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, flag_op);     // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP);    // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return addr::make_empty_cycle(); // Invalid cycle
    }
    
    // BRK - 7 cycles (explicit implementation)
    static constexpr cycle_desc_t make_brk(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Read next byte (dummy)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_VECTOR, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return addr::make_empty_cycle();
    }
};

} // namespace fam65xx_cpp

#endif // CYCLE_INTERRUPTS_HPP