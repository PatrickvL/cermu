# MOS6510 CPU Architecture Evolution Plan

## Project Overview

The aiemu C64 emulator is evolving through a sophisticated CPU architecture transition designed to achieve the ultimate goal: **the world's fastest hardware-accurate C64 emulator**.

## Current Architecture State

### Legacy CPU Implementation (Active)
**Location**: `src/chip/cpu/mos6510/` and `src/chip/cpu/fam65xx/`

**Characteristics**:
- **Instruction-based stepping**: `mos6510_step()` executes complete instructions
- **Memory access triggers**: Other chips (VIC-II, SID, CIA) tick when CPU performs memory access
- **Observer perspective**: Appears non-cycle-accurate from external viewpoint
- **Status**: ✅ **Working** - Currently active in main C64 system
- **Performance**: Good, but not optimal
- **Accuracy**: Functionally correct but not cycle-perfect

**Architecture Pattern**:
```
CPU.step() → executes full instruction
    ↓
Memory access → triggers other chips to tick
    ↓
System appears synchronized but not cycle-accurate
```

### Cycle-Accurate CPU Implementation (Under Development)
**Location**: `src/chip/cpu/mos6510_cycle/`

**Characteristics**:
- **Cycle-based ticking**: `mos6510_tick()` like all other chips
- **Visual6502-based**: Hardware-accurate internal state representation
- **True cycle accuracy**: Each tick represents one hardware cycle
- **Status**: 🔄 **85% Complete** - Core implementation done, integration pending
- **Performance**: Unknown (not benchmarked yet)
- **Accuracy**: Hardware-perfect (visual6502 transistor-level accurate)

**Architecture Pattern**:
```
System.tick()
    ↓
VIC-II.tick() → advances one cycle  
CPU.tick() → advances one cycle
Memory.tick() → advances one cycle
SID.tick() → advances one cycle
CIA1.tick() → advances one cycle
CIA2.tick() → advances one cycle
    ↓
True hardware synchronization
```

## Strategic Goals

### Phase 1: Foundation (COMPLETED ✅)
**Goal**: Establish optimized infrastructure
- ✅ Unified memory system with register-optimized `c64_memory_tick()`
- ✅ I/O coordination system with `IO_MEM_ACCESS_PENDING` flags
- ✅ Bus state optimization (32-bit packed with high-performance macros)
- ✅ Performance-first design patterns established

### Phase 2: Intermediate Step (CURRENT 🔄)
**Goal**: Create hardware-accurate foundation
- 🔄 **Visual6502-based CPU**: Transistor-level accurate implementation
- 🔄 **Dual CPU support**: Run both architectures in parallel
- 🔄 **Validation framework**: Compare new CPU against legacy CPU
- 🔄 **Integration**: Connect cycle-accurate CPU to main system
- 🔄 **Benchmarking**: Establish performance baselines

### Phase 3: Ultimate Goal (FUTURE 🎯)
**Goal**: World's fastest hardware-accurate C64 emulator
- 🎯 **Performance-optimized CPU**: Derived from visual6502 but maximum speed
- 🎯 **Hardware accuracy**: Identical behavior including all corner cases
- 🎯 **Unified architecture**: Consistent with memory/IO optimization patterns
- 🎯 **Benchmark leader**: Fastest C64 emulator while maintaining full accuracy

## Implementation Strategy

### Dual CPU Architecture Design

**Purpose**: Enable safe transition from legacy to cycle-accurate CPU while maintaining validation capability.

**Components**:
1. **CPU Selection System**: Runtime choice between legacy and cycle-accurate
2. **Parallel Execution**: Run both CPUs simultaneously for comparison
3. **State Synchronization**: Ensure both CPUs maintain identical architectural state
4. **Performance Monitoring**: Track cycle counts, instruction throughput, accuracy
5. **Validation Framework**: Automated testing to verify behavioral equivalence

### Integration Points

**Main System Integration**:
```c
// Current system
c64_cpu_cycle(c64->cpu);  // Legacy instruction-based

// Target system
c64_cpu_tick(c64->cpu);   // Cycle-accurate tick-based
```

**Memory System Coordination**:
- Legacy: Memory access triggers other chip ticks
- New: All chips tick synchronously each cycle
- Transition: Maintain compatibility during switchover

**Bus State Integration**:
- Both CPUs use optimized 32-bit packed `bus_state_t`
- Unified bus state macros (`BUS_GET/SET_ADDR/DATA/LINES`)
- Single memory system interface through `c64_memory_tick()`

## Performance Optimization Strategy

### Lessons from Previous Optimizations

**Memory System Success**:
- ✅ **Register-optimized calls**: Eliminated parameter passing overhead
- ✅ **Configuration-driven**: Dynamic allocation saves up to 16KB
- ✅ **Fast-path logic**: Branchless address calculations
- ✅ **Unified functions**: Single `c64_memory_tick()` for all access patterns

**I/O System Success**:
- ✅ **Flag-based coordination**: `IO_MEM_ACCESS_PENDING` eliminates callbacks
- ✅ **Consolidated tick functions**: Single entry point per chip
- ✅ **Enumeration simplification**: Reduced branching overhead

**Bus State Success**:
- ✅ **Packed 32-bit state**: Single register operations
- ✅ **Macro-based access**: Compiler optimization friendly
- ✅ **No alignment padding**: Maximum cache efficiency

### Future CPU Optimization Techniques

**Performance Strategies to Apply**:
1. **Instruction prediction**: Pre-decode common instruction sequences
2. **Branch optimization**: "Evil shortcuts" for performance-critical paths
3. **Memory access batching**: Group memory operations when possible
4. **State compression**: Ultra-compact internal state representation
5. **Lookup table optimization**: Pre-computed operation tables
6. **SIMD utilization**: Vectorized operations where applicable

## Risk Mitigation

### Dual CPU Validation Benefits

**Behavioral Verification**:
- Real-time comparison of CPU architectural state
- Instruction-by-instruction validation
- Memory access pattern verification
- Interrupt timing validation
- Flag computation verification

**Performance Analysis**:
- Cycle count comparison
- Memory access efficiency measurement
- Branch prediction effectiveness
- Cache utilization analysis

**Rollback Capability**:
- Immediate fallback to legacy CPU if issues arise
- Selective feature activation (address modes, instructions, etc.)
- Gradual migration path for complex programs
- Debugging support for both implementations

## Success Metrics

### Accuracy Metrics
- ✅ **Functional correctness**: All instructions produce correct results
- 🔄 **Cycle accuracy**: Exact cycle timing matches real hardware
- 🎯 **Corner case handling**: All documented hardware bugs/quirks emulated
- 🎯 **Interrupt timing**: Precise T5φ1→T1φ1 timing windows

### Performance Metrics
- 🔄 **Baseline establishment**: Current legacy CPU performance measurement
- 🎯 **Throughput improvement**: Instructions per second vs legacy
- 🎯 **Memory efficiency**: Reduced cache misses vs legacy
- 🎯 **Power efficiency**: Lower CPU utilization vs other emulators
- 🎯 **Benchmark leadership**: Fastest among accurate C64 emulators

## Implementation Phases

### Phase 6.2: Dual CPU Integration (CURRENT)
**Deliverables**:
- Dual CPU support in main C64 system
- State synchronization framework
- Comparison validation suite
- Performance benchmarking tools
- Runtime CPU selection mechanism

### Phase 6.3: Validation and Optimization (NEXT)
**Deliverables**:
- Comprehensive test suite validation
- Performance optimization identification
- Memory access pattern analysis
- Interrupt timing perfection
- Branch optimization implementation

### Phase 7: Ultimate CPU Design (FUTURE)
**Deliverables**:
- Performance-optimized CPU architecture
- Maintain visual6502 accuracy with maximum speed
- Integration with existing optimization patterns
- World-class benchmark results
- Production-ready implementation

## Technical Foundation

### Visual6502 Integration Benefits
- **Transistor-level accuracy**: Perfect hardware behavior replication
- **Internal state representation**: Complete processor state modeling
- **Timing precision**: Exact φ1/φ2 phase behavior
- **Documentation reference**: Self-documenting implementation
- **Corner case coverage**: All hardware quirks naturally represented

### Performance Foundation Benefits
- **Optimized infrastructure**: Memory/IO/Bus systems already performance-tuned
- **Proven patterns**: Successful optimization techniques established
- **Benchmarking framework**: Performance measurement capabilities
- **Scalable design**: Architecture supports further optimization
- **Compatibility**: Maintains existing system integration points

---

**Document Status**: Living document, updated as implementation progresses
**Last Updated**: 2025-08-21
**Phase**: 6.2 - Dual CPU Integration
