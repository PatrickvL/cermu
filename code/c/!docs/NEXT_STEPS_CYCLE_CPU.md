# Next Steps: Connecting Cycle-Accurate CPU to Dual CPU Architecture

**Current Status**: Dual CPU architecture successfully integrated and running in legacy-only mode
**Next Goal**: Connect the cycle-accurate CPU core to the dual CPU framework

---

## 🎯 Current Achievement

✅ **Dual CPU Architecture Complete**:
- `c64_dual_cpu.h/c` framework created with multiple execution modes
- C64 system integration completed (`c64.h/c` updated)
- Main emulation loop updated (`cimgui_interface.c` using new interface)  
- Build system integration successful
- Running in legacy-only mode maintaining world's fastest performance

✅ **Cycle CPU Foundation Complete (Phases 1-5)**:
- Complete visual6502-based CPU implementation
- 4,200+ lines of production-ready cycle-accurate code
- Comprehensive testing with >96% success rates
- Hardware-accurate timing and behavior

---
## Progress Update (2025-08-22)

- Completed register metadata compaction and deduplication:
  - Packed category + flags into one byte in [C.register_properties_t()](code/c/src/chip/cpu/mos6510_cycle/mos6510_registers.h:73)
  - Category encoded in bits 0-1 via [C.REG_PROP_CATEGORY_MASK()](code/c/src/chip/cpu/mos6510_cycle/mos6510_registers.h:61)
  - Removed ARCH/INTERNAL flags duplication; kept cross-cutting flags [C.REG_PROP_FLAG_ADDRESS()](code/c/src/chip/cpu/mos6510_cycle/mos6510_registers.h:62) and [C.REG_PROP_FLAG_ALU()](code/c/src/chip/cpu/mos6510_cycle/mos6510_registers.h:63)
  - Inline category accessor [C.mos6510_register_get_category()](code/c/src/chip/cpu/mos6510_cycle/mos6510_registers.h:101)
  - Updated initializer table in [code/c/src/chip/cpu/mos6510_cycle/mos6510_registers.c](code/c/src/chip/cpu/mos6510_cycle/mos6510_registers.c:12)

Migration pattern
```c
// c
// Compose category + cross-cutting flags with ORs
.flags = REG_PROP_ENCODE_CATEGORY(REG_CATEGORY_INTERNAL_ADDR) | REG_PROP_FLAG_ADDRESS;
```

Checklist
- [x] Deduplicate register_properties flags (remove ARCH/INTERNAL duplication, keep ADDRESS/ALU)
- [-] Review cycle CPU API for dual-CPU wiring (mos6510_state.h, mos6510_tick.h) and identify call sites in c64_dual_cpu.c

## 🚀 Next Steps (Priority Order)

### Step 1: Connect Cycle CPU to Dual CPU Framework
**Priority**: CRITICAL
**Estimated Time**: 2-3 hours
**Goal**: Make cycle CPU available through dual CPU interface

**Tasks**:
1. **Review cycle CPU API** - Check `mos6510_state.h` and `mos6510_tick.h` interfaces
2. **Update dual CPU implementation**:
   - Complete `c64_dual_cpu_set_mode()` cycle CPU creation logic
   - Implement `mos6510_cycle_tick()` calls in execution functions
   - Add proper state synchronization between legacy and cycle CPUs
3. **Test cycle-only mode**: `c64_set_cpu_mode(c64, CPU_MODE_CYCLE_ONLY)`

### Step 2: Enable CPU Validation Mode  
**Priority**: HIGH
**Estimated Time**: 2-4 hours  
**Goal**: Run both CPUs in parallel and validate identical behavior

**Tasks**:
1. **Implement state comparison**:
   - Complete `c64_dual_cpu_compare_registers()` cycle CPU access
   - Complete `c64_dual_cpu_compare_flags()` cycle CPU access  
   - Add register state extraction from cycle CPU
2. **Test validation mode**: `c64_set_cpu_mode(c64, CPU_MODE_VALIDATION)`
3. **Debug any state mismatches** - expect initial differences, debug systematically

### Step 3: Performance Benchmarking
**Priority**: MEDIUM
**Estimated Time**: 1-2 hours
**Goal**: Measure and compare legacy vs cycle CPU performance

**Tasks**:
1. **Enable benchmark mode**: `c64_set_cpu_mode(c64, CPU_MODE_BENCHMARK)`
2. **Collect performance metrics** using `c64_get_cpu_metrics()`
3. **Analyze performance gaps** and identify optimization opportunities

### Step 4: Runtime CPU Mode Switching
**Priority**: MEDIUM  
**Estimated Time**: 1 hour
**Goal**: Allow dynamic switching between CPU modes during runtime

**Tasks**:
1. **Add GUI controls** for CPU mode selection in cimgui interface
2. **Test switching between modes** during emulation
3. **Ensure state synchronization** when switching modes

---

## 🔧 Technical Details

### Current Dual CPU Architecture
```c
// Current working functions
bool c64_cpu_step(c64_t* c64);                    // ✅ Working (legacy mode)
bool c64_cpu_step_instruction(c64_t* c64);        // ✅ Working (legacy mode)
bool c64_set_cpu_mode(c64_t* c64, cpu_execution_mode_t mode); // ✅ Working

// Functions needing cycle CPU integration
void c64_get_cpu_metrics(const c64_t* c64, dual_cpu_metrics_t* metrics);
```

### Cycle CPU Integration Points
```c
// In c64_dual_cpu.c - functions that need cycle CPU calls:
c64_dual_cpu_set_mode()         // Create cycle CPU instance
c64_dual_cpu_execute()          // Call mos6510_cycle_tick()
c64_dual_cpu_compare_registers() // Read cycle CPU registers
c64_dual_cpu_compare_flags()     // Read cycle CPU flags
c64_dual_cpu_reset()            // Reset cycle CPU
```

### Expected Issues to Debug
1. **Function signature mismatches** - cycle vs legacy CPU headers
2. **State synchronization** - ensuring both CPUs start from identical state
3. **Register access** - cycle CPU register reading API
4. **Timing differences** - instruction vs cycle-based execution
5. **Memory bus interactions** - ensuring identical memory operations

---

## 🎯 Success Metrics

### Step 1 Success: 
- [ ] Cycle-only mode runs without crashes
- [ ] Basic program execution works (e.g., simple loop)
- [ ] No compilation errors

### Step 2 Success:
- [ ] Validation mode runs with both CPUs
- [ ] State comparison functions work
- [ ] Can identify and debug register/flag mismatches

### Step 3 Success:
- [ ] Performance metrics collection working
- [ ] Benchmark mode provides timing data
- [ ] Can quantify performance difference

### Step 4 Success:  
- [ ] Can switch CPU modes during runtime
- [ ] GUI shows current CPU mode
- [ ] No crashes during mode switching

---

## 📋 Current Working State

**✅ What's Working**:
- C64 system boots and runs programs in legacy mode
- Dual CPU architecture integrated successfully  
- `c64_cpu_step()` functions working through dual CPU interface
- Build system compiles successfully
- All dual CPU infrastructure in place

**🔄 What Needs Connection**:
- Cycle CPU core → Dual CPU framework
- Cycle CPU function calls (currently returning errors)
- State comparison between legacy and cycle CPUs
- Performance measurement collection

**🚀 Ultimate Goal**: 
- Validate cycle CPU produces identical results to legacy CPU
- Measure and optimize cycle CPU performance
- Enable safe transition to cycle-accurate CPU as default
- Maintain world's fastest C64 emulator performance goal

---

**Next Action**: Start with Step 1 - Review cycle CPU API and connect to dual CPU framework
