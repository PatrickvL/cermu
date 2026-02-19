# Systematic Testing and Fixing Guide

## Current Status

The C64 emulator test framework is fully implemented with:
- **2,329 tests cataloged** across 9 categories
- Multi-protocol support (DEBUG_REGISTER, INFINITE_LOOP, BASIC_LOADER)
- Screenshot comparison with reference images
- Automatic hardware reconfiguration (PAL/NTSC)
- CPU initialization fixed (mos6510_init() now called)

## Systematic Testing Approach

### Phase 1: CPU Core Tests (Priority: CRITICAL)

**Goal**: Verify all CPU instructions work correctly

**Test Categories**:
1. `CPU/cpuport/*` - CPU port functionality
2. `CPU/cia1pb6/*` - CIA timer interactions
3. `CPU/cputiming/*` - Instruction timing
4. `CPU/interrupts/*` - Interrupt handling
5. `CPU/irq/*` - IRQ timing
6. `CPU/nmi/*` - NMI timing

**Expected Issues**:
- Instruction timing inaccuracies
- Flag handling bugs (especially V, N, Z, C flags)
- Addressing mode errors
- Stack pointer handling

**Testing Command**:
```bash
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-category CPU \
    --test-output cpu_results.json
```

**Analysis**:
```bash
# Extract failed tests
grep "TIMEOUT\|FAILED" cpu_results.json > cpu_failures.txt

# Count by subcategory
grep -o '"category":"CPU/[^"]*"' cpu_results.json | sort | uniq -c
```

### Phase 2: CIA Timer Tests (Priority: HIGH)

**Goal**: Fix timer and port functionality

**Test Categories**:
1. `CIA/shiftregister/*` - Shift register
2. `CIA/timer/*` - Timer A/B
3. `CIA/tod/*` - Time of Day clock

**Expected Issues**:
- Timer underflow not triggering correctly
- Timer chaining (A->B) broken
- TOD clock inaccurate
- Port direction register issues

**Testing Command**:
```bash
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-category CIA \
    --test-output cia_results.json
```

### Phase 3: VIC-II Display Tests (Priority: HIGH)

**Goal**: Fix display timing and sprite rendering

**Test Categories**:
1. `VICII/ba/*` - Bus Available signal
2. `VICII/fld/*` - Flexible Line Distance
3. `VICII/sprites/*` - Sprite rendering
4. `VICII/timing/*` - Display timing

**Expected Issues**:
- Raster timing off by cycles
- Badline timing incorrect
- Sprite priorities wrong
- Border timing issues

**Testing Command**:
```bash
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-category VICII \
    --test-output vicii_results.json
```

### Phase 4: Interrupt Tests (Priority: MEDIUM)

**Goal**: Verify interrupt timing and priority

**Test Categories**:
1. `interrupts/branchquirk/*` - Branch timing quirks
2. `interrupts/cia/*` - CIA interrupts
3. `interrupts/vicii/*` - VIC-II interrupts

### Phase 5: Comprehensive Testing (Priority: ONGOING)

**Goal**: Run all categories and track progress

**Command**:
```bash
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-output all_results.json

# Generate summary
./c64emu --test-results all_results.json --test-summary
```

## Debugging Workflow

### 1. Identify Failing Test
```bash
# Run specific test with verbose output
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-filter "CPU/cpuport/test1.prg" \
    --verbose
```

### 2. Analyze Test Behavior
- Check if PC changes (CPU executing?)
- Check if $D7FF is written (test completion?)
- Check cycle count (timeout vs actual failure?)
- Look for KERNAL entry (unexpected interrupt?)

### 3. Examine Test Source
```bash
# Many tests have assembly source
ls -la /home/patrick/Git/VICE-testprogs/CPU/cpuport/*.asm
```

### 4. Fix Root Cause
Common fix locations:
- **CPU instructions**: `code/cpp/src/chip/cpu/fam65xx/*.cpp`
- **CIA timers**: `code/cpp/src/chip/io/mos6526.cpp`
- **VIC-II**: `code/cpp/src/chip/video/vic_ii/*.cpp`
- **Interrupt handling**: `code/cpp/src/systems/c64/c64.cpp`

### 5. Verify Fix
```bash
# Re-run the specific test
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-filter "CPU/cpuport/test1.prg"

# Run related tests
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-filter "CPU/cpuport"
```

### 6. Regression Test
```bash
# Run full category to ensure no regressions
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-category CPU
```

## Common Issues and Solutions

### Issue: All Tests Timeout

**Symptom**: Tests run for 10M cycles without completion

**Causes**:
1. CPU not executing (check PC changes)
2. Test uses different protocol (not $D7FF)
3. Emulation too slow
4. Infinite loop in emulator code

**Solutions**:
- Enable verbose mode to see PC changes
- Check if test uses BASIC loader or infinite-loop protocol
- Profile emulator performance
- Add breakpoint detection

### Issue: Tests Fail with $D7FF = $FF

**Symptom**: Test writes $FF to debug register

**Causes**:
1. CPU instruction bug
2. Memory access issue
3. Banking problem
4. Timing issue

**Solutions**:
- Compare with VICE emulator behavior
- Add instruction tracing
- Check memory map configuration
- Verify cycle-accurate timing

### Issue: Screenshot Tests Fail

**Symptom**: Generated image doesn't match reference

**Causes**:
1. VIC-II timing wrong
2. Sprite rendering incorrect
3. Color palette mismatch
4. Border size different

**Solutions**:
- Visual comparison of images
- Check raster timing
- Verify sprite priorities
- Use border detection

## Performance Optimization

### Reduce Boot Time
The current boot sequence runs 1M cycles. For testing:
1. Skip boot sequence entirely (tests don't need BASIC ready)
2. Load test directly and set PC
3. Use test mode that bypasses normal initialization

### Parallel Testing
Run multiple test categories in parallel:
```bash
./c64emu --test-category CPU & 
./c64emu --test-category CIA &
./c64emu --test-category VICII &
wait
```

## Progress Tracking

### Create Baseline
```bash
# Run all tests and save baseline
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-output baseline_$(date +%Y%m%d).json
```

### Track Improvements
```bash
# After fixes, run again
./c64emu --test-run /home/patrick/Git/VICE-testprogs \
    --test-output current_$(date +%Y%m%d).json

# Compare results
diff baseline_*.json current_*.json
```

### Test Pass Rate Goals
- **Phase 1**: CPU tests: 80% pass rate
- **Phase 2**: CIA tests: 70% pass rate
- **Phase 3**: VIC-II tests: 60% pass rate
- **Phase 4**: All tests: 75% pass rate

## Next Steps

1. **Disable verbose boot logging** in main to speed up testing
2. **Run CPU category** to get baseline numbers
3. **Pick top 5 failing tests** and analyze each
4. **Fix identified issues** one at a time
5. **Re-run and track progress**
6. **Repeat** until satisfactory pass rate achieved

## Resources

- **VICE Testprogs README**: `/home/patrick/Git/VICE-testprogs/README`
- **Test Framework Docs**: `code/cpp/TEST_FRAMEWORK_STATUS.md`
- **C64 Architecture**: `code/cpp/!docs/`
- **MOS 6510 Datasheet**: (external)
- **VIC-II Programmer's Reference**: (external)