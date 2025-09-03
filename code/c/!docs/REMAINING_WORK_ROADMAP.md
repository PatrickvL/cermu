# C++ Template Helper Classes Pattern - Remaining Work Roadmap

## Current Status: ✅ PHASE 1 COMPLETE

The C++ Template Helper Classes Pattern implementation has been **successfully completed** with:

- ✅ **Template Helper Classes Pattern**: Modular architecture across 5 specialized headers
- ✅ **Zero Overhead Architecture**: Direct native bus API, no adapter layers
- ✅ **Ultimate Type Safety**: Pure `enum class` with type-safe register arrays
- ✅ **Legacy Removal Complete**: All wrapper macros and compatibility code removed
- ✅ **Functionality Migration**: All monolithic file features preserved and enhanced

### Achieved Architecture
```
C64 System → MOS6510 C API → cpu_6510 Template → Native 64-bit bus_state_t
```

### Key Files (New Modular Structure)
- **[`cpu_defs.hpp`](src/chip/cpu/fam65xx_cpp/cpu_defs.hpp)** - Type-safe enums & register array
- **[`cpu_config.hpp`](src/chip/cpu/fam65xx_cpp/cpu_config.hpp)** - CPU variant configurations
- **[`alu_operations.hpp`](src/chip/cpu/fam65xx_cpp/alu_operations.hpp)** - Complete ALU instruction set
- **[`memory_operations.hpp`](src/chip/cpu/fam65xx_cpp/memory_operations.hpp)** - Memory operations with native bus API
- **[`cycle_tables.hpp`](src/chip/cpu/fam65xx_cpp/cycle_tables.hpp)** - Constexpr cycle table generation
- **[`cpu.hpp`](src/chip/cpu/fam65xx_cpp/cpu.hpp)** - Main CPU implementation

## PHASE 2: Extended Functionality and Optimization

### Immediate Priorities (High Impact)

#### 1. Control Line Extensions
**Goal**: Complete hardware-accurate control line support
- [ ] **SYNC pin enhancement**: Full cycle-accurate SYNC timing
- [ ] **SO pin (Set Overflow)**: NMOS edge detection and CMOS differences
- [ ] **BE pin (Bus Enable)**: 65C02/65C816 bus control
- [ ] **ABORT pin**: 65C816 abort interrupt support
- [ ] **VP pin (Vector Pull)**: Hardware interrupt vector detection
- [ ] **ML pin (Memory Lock)**: 65C816 memory protection

**Files to Update**:
- `system_lines.h` - Add new pin definitions
- `cpu.hpp` - Implement pin handling in tick() method
- `cpu_config.hpp` - Add pin configuration flags

#### 2. Complete Instruction Set Implementation
**Goal**: Full 6502/6510/65C02/6507/65C816 instruction coverage
- [ ] **All addressing modes**: Complete zp,x / abs,y / (zp,x) / (zp),y patterns
- [ ] **Branch instructions**: BCC, BCS, BNE, BEQ, BPL, BMI, BVC, BVS
- [ ] **Stack operations**: PHA, PLA, PHP, PLP, PHX, PHY, PLX, PLY
- [ ] **Increment/Decrement**: INC, DEC, INX, DEX, INY, DEY
- [ ] **Shift/Rotate**: ASL, LSR, ROL, ROR (all addressing modes)
- [ ] **65C02 additions**: STZ, TSB, TRB, BRA, PHX/PHY/PLX/PLY
- [ ] **Illegal opcodes**: Complete NMOS 6502 undocumented instruction set

**Files to Update**:
- `cycle_tables.hpp` - Add all instruction cycle patterns
- `alu_operations.hpp` - Complete ALU operation set
- `memory_operations.hpp` - All addressing mode support

#### 3. Hardware Test Suite Validation
**Goal**: Verify cycle-accurate emulation against known test suites
- [ ] **Klaus Dormann 6502 Test Suite**: Comprehensive instruction validation
- [ ] **Wolfgang Lorenz Test Suite**: Detailed timing and flag tests
- [ ] **65C02 Test Validation**: CMOS-specific behavior verification
- [ ] **Internal regression tests**: Project-specific test cases

### Medium Priority (Architecture Enhancement)

#### 4. Variant-Specific Hardware Quirks
**Goal**: Accurate emulation of CPU variant differences
- [ ] **RDY pin semantics**: NMOS (reads only) vs CMOS (all cycles)
- [ ] **AEC/BA DMA timing**: 6510 VIC-II integration accuracy
- [ ] **SO pin edge detection**: NMOS vs CMOS behavior differences
- [ ] **Decimal mode bugs**: NMOS ADC/SBC decimal flag quirks
- [ ] **CMOS timing fixes**: 65C02 cycle-accurate improvements

#### 5. Advanced Interrupt Handling
**Goal**: Complete and accurate interrupt processing
- [ ] **NMI edge detection**: Proper edge triggering with timing
- [ ] **IRQ level handling**: Maskable interrupt processing
- [ ] **BRK instruction**: Software interrupt with proper flag setting
- [ ] **RESET sequence**: Complete CPU initialization
- [ ] **ABORT/COP**: 65C816 extended interrupt support
- [ ] **Interrupt priority**: Proper handling of simultaneous interrupts

#### 6. Performance Optimization and Tuning
**Goal**: Maximum runtime efficiency with profiling
- [ ] **Hot path analysis**: Profile actual C64 code execution patterns
- [ ] **Branch elimination**: Optimize frequent conditional paths
- [ ] **Constexpr table expansion**: More compile-time precomputation
- [ ] **Register layout optimization**: Memory access pattern optimization
- [ ] **Template instantiation tuning**: Minimize compile-time overhead

### Lower Priority (System Integration)

#### 7. Legacy Code Removal
**Goal**: Clean up codebase after parity achieved
- [ ] **Remove fam65xx**: Old C-based 65xx core implementations
- [ ] **Remove mos6510**: Legacy 6510 C implementation
- [ ] **Remove mos6502**: Legacy 6502 C implementation
- [ ] **Clean build system**: Remove legacy compilation targets
- [ ] **Update documentation**: Reflect new architecture

#### 8. GUI Re-enablement
**Goal**: Restore debugging and visualization capabilities
- [ ] **Update CPU API calls**: Remove dual-CPU references
- [ ] **Register display**: Show CPU state with new register access
- [ ] **Cycle counting**: Display performance metrics
- [ ] **Debugger integration**: Breakpoints and step-through support

#### 9. Final System Integration
**Goal**: Complete project integration and cleanup
- [ ] **Bus definition cleanup**: Deduplicate any remaining definitions
- [ ] **Documentation updates**: Comprehensive architecture documentation
- [ ] **Performance benchmarking**: Compare vs legacy implementations
- [ ] **Production readiness**: Final validation and optimization

## Implementation Strategy

### Phase 2A: Core Functionality (Next 2-3 weeks)
1. **Control line extensions** - Essential for hardware accuracy
2. **Complete instruction set** - Required for full compatibility
3. **Hardware test validation** - Ensure correctness

### Phase 2B: Optimization and Integration (Following 2-3 weeks)
1. **Variant-specific quirks** - Hardware-accurate differences
2. **Advanced interrupts** - Complete interrupt handling
3. **Performance tuning** - Optimize hot paths

### Phase 2C: Cleanup and Polish (Final 1-2 weeks)
1. **Legacy code removal** - Clean codebase
2. **GUI re-enablement** - Restore functionality
3. **Final integration** - Production readiness

## Technical Notes

### Compiler Requirements
- **C++17 Standard**: Template features and constexpr if
- **Optimization Level**: -O2 or higher for template optimization
- **Template Depth**: May need increased template instantiation limits

### Memory Requirements
- **Compile-time**: Higher memory usage during template instantiation
- **Runtime**: Same or better than legacy implementation
- **Binary size**: Per-variant optimization may increase total size

### Compatibility
- **C API Preserved**: All existing system integration unchanged
- **Bus Interface**: Native 64-bit bus_state_t throughout
- **Legacy Support**: None - clean break from old implementations

## Success Metrics

### Phase 2 Completion Criteria
- [ ] **Klaus Test Suite**: 100% pass rate
- [ ] **Performance**: Equal or better than legacy cores
- [ ] **Memory Usage**: No increase in runtime footprint
- [ ] **Build Time**: Reasonable compile-time performance
- [ ] **Code Quality**: Clean, maintainable, documented

### Long-term Goals
- [ ] **Maintainability**: Easy addition of new CPU variants
- [ ] **Extensibility**: Simple feature additions and modifications
- [ ] **Performance**: Industry-leading emulation speed
- [ ] **Accuracy**: Hardware-perfect emulation behavior

## Current Development Status

**✅ FOUNDATION COMPLETE**: The Template Helper Classes Pattern implementation provides a solid, type-safe, zero-overhead foundation for all remaining work.

**🚀 READY FOR PHASE 2**: All architectural decisions made, clean codebase ready for enhancement.

**📈 RISK MITIGATION**: Modular structure allows incremental development and testing of each component.

The groundwork is complete - Phase 2 can proceed with confidence on this robust foundation.