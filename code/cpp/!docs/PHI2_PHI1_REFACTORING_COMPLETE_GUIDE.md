# PHI2/PHI1 Complete Refactoring Guide

## Revolutionary Architecture Change

The CPU no longer performs memory accesses internally. Instead:

1. **PHI2 (Even cycles)**: CPU sets up bus signals (address, R/W, data for writes)
2. **External Memory Tick**: External code reads bus state and performs memory operation
3. **PHI1 (Odd cycles)**: CPU samples bus data and performs internal operations

## Key Architectural Changes

### 1. Remove Memory Callbacks from CPU

**DELETE from fam65xx_t**:
```cpp
// REMOVE THESE:
fam65xx_mem_read_t mem_read;
fam65xx_mem_write_t mem_write;
void* mem_user_data;
void set_memory_callbacks(...);  // DELETE entire function
```

### 2. Rename phi2_* Functions

**OLD NAME** → **NEW NAME** (Purpose):
- `phi2_read()` → `bus_setup_read()` - Sets up bus for read, doesn't read
- `phi2_write()` → `bus_setup_write()` - Sets up bus for write, doesn't write  
- `phi2_dummy_read()` → `bus_setup_dummy()` - Sets up bus, no actual access
- `phi2_access()` → `bus_setup()` - Generic bus setup template

### 3. Remove Memory Access from bus_setup_*

**BEFORE**:
```cpp
template<bool IsWrite, bool IsDummy, Addr addr_arg, Bank bank_arg>
bus_state_t phi2_access(bus_state_t pins, uint8_t data) {
    // ... setup bus ...
    
    if constexpr (IsWrite) {
        return phi2_write_impl(pins, addr, data);  // ❌ PERFORMS WRITE
    } else {
        return phi2_read_impl<IsDummy>(pins, addr, bus_data);  // ❌ PERFORMS READ
    }
}
```

**AFTER**:
```cpp
template<bool IsWrite, bool IsDummy, Addr addr_arg, Bank bank_arg>
bus_state_t bus_setup(bus_state_t pins, uint8_t data) {
    // Get address from register
    uint32_t addr = this->get(addr_reg);
    addr |= static_cast<uint32_t>(get_address_bank<addr_arg>(bank_arg)) << 16;
    
    // Update bus pins
    pins = FAM65XX_SET_ADDR(pins, addr & 0xFFFF);
    if constexpr (IsWrite) {
        pins &= ~FAM65XX_RW;  // Write mode
        pins = FAM65XX_SET_DATA(pins, data);  // Output data
    } else {
        pins |= FAM65XX_RW;   // Read mode
        // External code will put data on bus
    }
    
    return pins;  // ✅ NO MEMORY ACCESS
}

// Helper to load register from bus data (combines get + load)
inline void bus_load_reg(reg8_t data_reg, bus_state_t pins) {
    this->set(data_reg, FAM65XX_GET_DATA(pins));
}

// Helper to get data from bus (for when you need the value)
inline uint8_t bus_get_data(bus_state_t pins) const {
    return FAM65XX_GET_DATA(pins);
}
```

### 4. Split tick() Into Template Functions

**ELEGANT SOLUTION: Use template parameter for compile-time phase selection**

```cpp
// Phase enum for template parameter
enum class Phase { PHI2, PHI1 };

// Template tick function - zero runtime overhead!
template<Phase phase>
bus_state_t tick(bus_state_t pins) {
    if constexpr (phase == Phase::PHI2) {
        // PHI2: Check RDY and set up bus
        if (!FAM65XX_GET_RDY(pins)) {
            // RDY low - external DMA active
            // DO NOT call handler, DO NOT increment
            return pins;
        }
        
        // Call handler to set up bus
        pins = call_current_handler(pins);
        
        // tick() increments from even to odd
        cycle_index++;
        
    } else {  // PHI1
        // Call handler to perform internal operations
        pins = call_current_handler(pins);
        // Handler increments cycle_index by 1 (to next even)
    }
    
    return pins;
}
```

**Why This Is Perfect**:
- ✅ **Zero runtime cost** - `if constexpr` eliminates unused branch at compile time
- ✅ **Explicit call site** - `tick<Phase::PHI2>(pins)` makes intent crystal clear
- ✅ **Type safe** - compiler enforces correct phase ordering
- ✅ **Clean separation** - each phase is conceptually separate function
- ✅ **No cycle_index & 1 check** - phase is known at compile time

### 5. Handler Structure Changes

**PHI2 Handler (Even Cycle)**:
```cpp
case 0: // PHI2: Set up bus for read from PC
    // NO RDY CHECK - done in tick()
    // NO data_reg argument - we set up bus only
    pins = bus_setup_read<Addr::PC>(pins);
    // DO NOT increment - tick() does it (0 → 1)
    return pins;
```

**PHI1 Handler (Odd Cycle) - Regular Read**:
```cpp
case 1: // PHI1: Sample data, perform operations
    // Data already on pins from external memory access
    // Use bus_load_reg() - clean 1-line bus → register transfer
    this->bus_load_reg(REG_ABL, pins);
    this->inc(REG_PC);
    this->cycle_index++;  // Increment to next PHI2 (1 → 2)
    return pins;
```

**PHI1 Handler - When You Need the Value**:
```cpp
case 1: // PHI1: Need value for calculation
    uint8_t data = this->bus_get_data(pins);
    this->set(REG_A, data + 1);  // Use value in expression
    this->cycle_index++;  // To next PHI2 (1 → 2)
    return pins;
```

**PHI1 Handler - Dummy Read**:
```cpp
case 3: // PHI1: Dummy read - ignore data
    // Data is on bus but we don't use it
    // No bus interaction needed at all
    this->inc(REG_PC);
    this->cycle_index++;  // To next PHI2 (3 → 4)
    return pins;
```

### 6. External Usage Pattern

**System tick loop with explicit phases**:
```cpp
// PHI2 tick - CPU sets up bus
pins = cpu.tick<Phase::PHI2>(pins);  // ✅ Explicit phase

// Memory tick (external)
if (pins & FAM65XX_RW) {
    // Read cycle
    uint8_t data = memory.read(FAM65XX_GET_ADDR(pins));
    pins = FAM65XX_SET_DATA(pins, data);
} else {
    // Write cycle
    memory.write(FAM65XX_GET_ADDR(pins), FAM65XX_GET_DATA(pins));
}

// PHI1 tick - CPU processes result
pins = cpu.tick<Phase::PHI1>(pins);  // ✅ Explicit phase

// VIC-II/CIA/SID can tick here too
vic.tick();
cia.tick();
```

**Benefits of Explicit Phases**:
- Code clearly shows PHI2 → Memory → PHI1 sequence
- Impossible to call phases in wrong order (type system enforces)
- Zero runtime overhead vs separate functions
- Easier to understand and debug

### 7. cycle_index Management - Simplified with Templates

**With template phases, cycle management is crystal clear**:

```cpp
template<Phase phase>
bus_state_t tick(bus_state_t pins) {
    if constexpr (phase == Phase::PHI2) {
        if (!RDY) return pins;
        pins = call_handler(pins);  // Handler does NOT increment
        cycle_index++;               // tick<PHI2> increments even→odd
        return pins;
    } else {  // PHI1
        pins = call_handler(pins);   // Handler increments odd→even
        return pins;
    }
}
```

**The Rules** (unchanged):
- **PHI2 handlers**: NEVER touch cycle_index - `tick<PHI2>` handles it
- **PHI1 handlers**: ALWAYS increment by 1 to reach next even number
- **Transitions**: Set cycle_index to 0

**Handler Example**:
```cpp
bus_state_t am_zp(bus_state_t pins) {
    switch (cycle_index) {
        case 0: // PHI2: Set up bus
            pins = bus_setup_read<Addr::PC>(pins);
            return pins;  // tick<PHI2> will increment to 1
            
        case 1: // PHI1: Process
            this->bus_load_reg(REG_ABL, pins);
            this->set(REG_ABH, 0x00);
            this->inc(REG_PC);
            this->transition_to_operation();  // Sets cycle_index=0
            return pins;
    }
}
```

**Call Sequence**:
```cpp
// cycle_index=0 (even)
pins = cpu.tick<Phase::PHI2>(pins);  // Handler runs, tick increments to 1
pins = memory.access(pins);           // External memory
// cycle_index=1 (odd)
pins = cpu.tick<Phase::PHI1>(pins);  // Handler runs, increments to 2
// cycle_index=2 (even) - ready for next PHI2
```

### 8. Handler Pattern - Always Use Switch

**ALWAYS use `switch(cycle_index)` - even for single-cycle handlers**

**Why?**
1. **Consistency** - same pattern everywhere makes code easier to read/maintain
2. **Branch prediction** - modern CPUs predict switch statements better than if/else chains
3. **Compiler optimization** - switch generates jump table for O(1) predictable performance
4. **Future-proof** - easy to add cycles later without refactoring the structure

**Single-Cycle Handler** (like CLC, SEC, NOP):
```cpp
bus_state_t op_clc(bus_state_t pins) {
    switch (cycle_index) {
        case 0: // PHI2
            pins = bus_setup_dummy<Addr::PC>(pins);
            return pins;
            
        case 1: // PHI1
            this->update_flag(FLAG_C, false);
            this->transition_to_fetch();
            return pins;
    }
    return pins;  // Should never reach
}
```

**Multi-Cycle Handler** (same structure, just more cases):
```cpp
bus_state_t am_abs(bus_state_t pins) {
    switch (cycle_index) {
        case 0: // PHI2: Read low byte
            pins = bus_setup_read<Addr::PC>(pins);
            return pins;
            
        case 1: // PHI1: Process low byte
            this->bus_load_reg(REG_ABL, pins);
            this->inc(REG_PC);
            this->cycle_index++;
            return pins;
            
        case 2: // PHI2: Read high byte
            pins = bus_setup_read<Addr::PC>(pins);
            return pins;
            
        case 3: // PHI1: Complete
            this->bus_load_reg(REG_ABH, pins);
            this->inc(REG_PC);
            this->transition_to_operation();
            return pins;
    }
    return pins;  // Should never reach
}
```

**Performance Benefits**:
- **Jump table**: Switch compiles to O(1) jump table for sequential case values
- **Predictable**: CPU branch predictor handles switch perfectly
- **Cache friendly**: All cases in same code locality

**DON'T use if/else**:
```cpp
// ❌ AVOID THIS - harder to read, potentially slower
bus_state_t op_clc(bus_state_t pins) {
    if (cycle_index == 0) {
        pins = bus_setup_dummy<Addr::PC>(pins);
        return pins;
    } else {
        this->update_flag(FLAG_C, false);
        this->transition_to_fetch();
        return pins;
    }
}
```

## Complete Refactoring Checklist

### Phase 1: Core Infrastructure
- [ ] Remove mem_read, mem_write, mem_user_data from fam65xx_t
- [ ] Remove set_memory_callbacks()
- [ ] Remove phi2_write_impl() and phi2_read_impl()
- [ ] Rename phi2_access → bus_setup
- [ ] Rename phi2_read → bus_setup_read
- [ ] Rename phi2_write → bus_setup_write
- [ ] Rename phi2_dummy_read → bus_setup_dummy
- [ ] Remove all memory callback invocations
- [ ] Rewrite tick() with centralized RDY check
- [ ] Remove should_complete_write_cycle() (obsolete)

### Phase 2: Addressing Modes  
- [ ] am_zp
- [ ] am_zpx
- [ ] am_zpy
- [ ] am_abs
- [ ] am_abx
- [ ] am_aby
- [ ] am_ind
- [ ] am_inx
- [ ] am_iny
- [ ] am_zpi (65C02)
- [ ] am_abi (65C02)
- [ ] All 65C816 modes

### Phase 3: Operations
- [ ] All load operations (LDA, LDX, LDY)
- [ ] All store operations (STA, STX, STY, STZ)
- [ ] All RMW operations (ASL, LSR, ROL, ROR, INC, DEC)
- [ ] All arithmetic (ADC, SBC, CMP, CPX, CPY)
- [ ] All logical (AND, ORA, EOR, BIT)
- [ ] All branches
- [ ] All stack operations
- [ ] All jumps/calls
- [ ] Interrupt handling (BRK, IRQ, NMI, RTI)

### Phase 4: I/O Port Handling
Since I/O port is internal to CPU:
- [ ] Keep I/O port read/write internal
- [ ] Handle in bus_setup functions without memory callback
- [ ] Same for APU registers (NES)

### Phase 5: Testing & Validation
- [ ] Update test harnesses for new external memory model
- [ ] Verify cycle counts unchanged
- [ ] Verify functional correctness
- [ ] Verify RDY behavior
- [ ] Verify VIC-II DMA works

## Benefits

✅ **Perfect hardware accuracy**: CPU only drives bus, doesn't access memory
✅ **Clean separation**: Memory system completely external
✅ **Easy VIC-II DMA**: External code handles all memory access
✅ **Simplified RDY**: Single check in tick(), no scattered checks
✅ **True PHI2/PHI1**: Matches real hardware behavior exactly

## Migration Path for Tests

**OLD**:
```cpp
cpu.set_memory_callbacks(read_fn, write_fn, user_data);
pins = cpu.tick(pins);
```

**NEW**:
```cpp
// PHI2: CPU sets up bus
pins = cpu.tick(pins);

// External memory access
if (FAM65XX_GET_RW(pins)) {
    // Read cycle - put data on bus
    pins = FAM65XX_SET_DATA(pins, read_fn(FAM65XX_GET_ADDR(pins)));
} else {
    // Write cycle - read data from bus and write to memory
    write_fn(FAM65XX_GET_ADDR(pins), FAM65XX_GET_DATA(pins));
}

// PHI1: CPU samples bus and processes
pins = cpu.tick(pins);
```

---

**Status**: Architecture design complete, ready for implementation  
**Effort**: 2-3 weeks of systematic refactoring  
**Risk**: High - requires updating all handlers and all test code