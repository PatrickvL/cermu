# C64 Emulator Testing Guide

This guide explains how to use the automated test framework to validate your C64 emulator against the VICE test suite.

## Quick Start

### Prerequisites

1. **Build the test runner**:
   ```bash
   cd code/cpp
   mkdir -p build && cd build
   cmake ..
   make c64_test_runner
   ```

2. **Get VICE test programs**:
   ```bash
   git clone https://github.com/VICE-Team/testprogs.git /path/to/VICE-testprogs
   ```

### Running Tests

**Easy way** - Use the convenience script:
```bash
cd code/cpp/tools
./run_tests.sh
```

**Direct way** - Use the test runner directly:
```bash
./build/bin/c64_test_runner --testprogs /path/to/VICE-testprogs
```

> **Note**: Tests now run with **automatic hardware reconfiguration** by default. The framework automatically creates appropriate C64/VIC20 systems (PAL/NTSC) based on each test's requirements. This ensures tests run with the correct hardware configuration without manual intervention.

## Common Test Scenarios

### Run All Tests
```bash
./run_tests.sh
```

### Run CPU Tests Only
```bash
./run_tests.sh --category CPU
```

### Run Specific Test Suite
```bash
./run_tests.sh --filter kdormann
```

### Run VIC-II Tests
```bash
./run_tests.sh --category VICII
```
This automatically runs tests with their required hardware (PAL/NTSC).

### Run VIC20 Tests
```bash
./run_tests.sh --category VIC20
```

### Re-run Failed Tests
After fixing bugs, re-run only the tests that previously failed:
```bash
./run_tests.sh --rerun-failed
```

## Test Categories

### CPU Tests (`--category CPU`)
Tests CPU instruction execution, addressing modes, and timing.

**Key test suites**:
- `kdormann` - Klaus Dormann's comprehensive 6502 functional tests (must pass!)
- `bclark` - Bruce Clark's decimal mode tests
- `ane/lax/sha` - Illegal opcode tests
- `cpuport` - CPU port behavior

**Expected results**: Should achieve >99% pass rate. Any failures indicate serious CPU bugs.

### CIA Tests (`--category CIA`)
Tests CIA timers, interrupts, and I/O ports.

**Key test suites**:
- Timer A/B functionality
- Interrupt generation and acknowledgment
- Port operations
- Time-of-day clock

**Expected results**: >95% pass rate. Timing-sensitive tests may fail on inaccurate timing.

### VIC-II Tests (`--category VICII`)
Tests graphics chip behavior and timing.

**Key test suites**:
- `border` - Border manipulation
- `sprites` - Sprite display and collisions
- `raster` - Raster interrupt timing
- `videomode` - Graphics modes

**Expected results**: 80-95% pass rate. Many tests are very timing-sensitive.

### Interrupt Tests (`--category interrupts`)
Tests IRQ/NMI timing and behavior.

**Expected results**: >90% pass rate. Critical for game compatibility.

## Understanding Test Results

### Exit Codes

The test runner returns:
- `0` - All tests passed
- `1` - Some tests failed, timed out, or had errors

### Result Files

After running tests, you'll find:
```
test_results/
├── results_YYYYMMDD_HHMMSS.txt    # Human-readable report
├── results_YYYYMMDD_HHMMSS.json   # Machine-readable data
├── latest_results.txt             # Symlink to latest text report
└── latest_results.json            # Symlink to latest JSON report
```

### Interpreting Results

**Text report format**:
```
Test: CPU/kdormann/6502_functional_test.prg
  Status: PASSED
  Type: exitcode
  Cycles: 8543210
  Time: 125.3 ms

Summary:
  Total: 150
  Passed: 142 (94.7%)
  Failed: 5
  Timeout: 2
  Skipped: 1
```

**Pass rate guidelines**:
- **>95%** - Excellent, emulator is highly accurate
- **90-95%** - Good, minor issues to fix
- **80-90%** - Acceptable, several bugs present
- **<80%** - Needs significant work

## Common Issues and Solutions

### Tests Timeout

**Symptom**: Many tests show `TIMEOUT` status

**Possible causes**:
1. CPU not executing instructions correctly
2. Missing or broken functionality
3. Infinite loop in test or emulator

**Solutions**:
- Run with `--verbose` to see detailed output
- Start with CPU tests first
- Check if specific instructions are implemented
- Use a debugger to step through test execution

### Tests Fail Immediately

**Symptom**: Tests fail within first few cycles

**Possible causes**:
1. Incorrect memory initialization
2. Wrong start address
3. ROM/RAM mapping issues

**Solutions**:
- Verify PRG file loads correctly
- Check memory map configuration
- Ensure CPU starts at correct address

### Inconsistent Results

**Symptom**: Same test passes/fails randomly

**Possible causes**:
1. Timing issues
2. Uninitialized variables
3. Race conditions

**Solutions**:
- Check cycle-accurate timing
- Review interrupt handling
- Verify deterministic behavior

## Advanced Usage

### Automatic Hardware Reconfiguration (Default)

By default, the test framework automatically reconfigures hardware per test:
```bash
# This automatically handles PAL/NTSC switching
./run_tests.sh --category VICII
```

The framework:
- Detects each test's hardware requirements (PAL/NTSC, CIA old/new, etc.)
- Creates appropriate system configurations automatically
- Minimizes system recreations for performance
- Supports mixed-hardware test batches

### Filter by Multiple Criteria

Combine filters for precise test selection:
```bash
./run_tests.sh --category VICII --filter border
```

### Legacy Mode (Single Hardware Configuration)

For compatibility or specific testing needs, disable auto-configuration:
```bash
# Run all tests with a single PAL system
./run_tests.sh --legacy-mode --pal --category CIA

# Run all tests with a single NTSC system
./run_tests.sh --legacy-mode --ntsc
```

**When to use legacy mode:**
- Debugging hardware-specific issues
- Performance testing with consistent configuration
- Comparing behavior across hardware variants

### Batch Testing with CI/CD

Use JSON output for automated testing:
```bash
./build/c64_test_runner \
  --testprogs /path/to/VICE-testprogs \
  --json results.json \
  --category CPU

# Check results programmatically
python3 -c "import json; \
  data = json.load(open('results.json')); \
  exit(0 if data['summary']['failed'] == 0 else 1)"
```

### Progressive Testing Strategy

1. **Start with CPU tests** - Foundation must be solid
   ```bash
   ./run_tests.sh --category CPU
   ```

2. **Add CIA tests** - Essential for timing
   ```bash
   ./run_tests.sh --category CIA
   ```

3. **Test interrupts** - Critical for compatibility
   ```bash
   ./run_tests.sh --category interrupts
   ```

4. **Add VIC-II tests** - Graphics accuracy
   ```bash
   ./run_tests.sh --category VICII
   ```

5. **Run full suite** - Complete validation
   ```bash
   ./run_tests.sh
   ```

## Test Development

### Adding New Tests

1. Place `.prg` file in appropriate directory under VICE-testprogs
2. For visual tests, add reference image:
   ```
   VICII/mytest/mytest.prg
   VICII/mytest/references/mytest.prg.png
   ```
3. Add `readme.txt` documenting the test
4. Test will be automatically discovered

### Test Requirements

Tests should:
- Write `$00` to `$D7FF` on success
- Write `$FF` to `$D7FF` on failure
- Complete within reasonable time (< 10M cycles)
- Document requirements in readme
- Include reference output if visual

## Continuous Integration

### GitHub Actions Example

```yaml
name: C64 Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Checkout VICE testprogs
        run: git clone https://github.com/VICE-Team/testprogs.git
      
      - name: Build
        run: |
          cd code/cpp
          mkdir build && cd build
          cmake ..
          make c64_test_runner
      
      - name: Run CPU Tests
        run: |
          cd code/cpp/tools
          ./run_tests.sh --category CPU
      
      - name: Upload Results
        if: always()
        uses: actions/upload-artifact@v2
        with:
          name: test-results
          path: code/cpp/tools/test_results/
```

## Performance Benchmarking

Track test execution performance over time:
```bash
# Run tests and track timing
./run_tests.sh --category CPU > results.txt

# Extract timing data
grep "Total time:" results.txt
```

## Troubleshooting

### Test Runner Not Found

```bash
# Build the test runner
cd code/cpp/build
cmake ..
make c64_test_runner
```

### VICE-testprogs Not Found

```bash
# Clone the repository
git clone https://github.com/VICE-Team/testprogs.git ~/VICE-testprogs

# Set environment variable
export TESTPROGS_PATH=~/VICE-testprogs
./run_tests.sh
```

### Permission Denied

```bash
# Make script executable
chmod +x code/cpp/tools/run_tests.sh
```

### Tests Running with Wrong Hardware

If you suspect tests are using incorrect hardware:

```bash
# Run with verbose output to see hardware configurations
./run_tests.sh --verbose --filter "test_name"

# Check if auto-config is detecting requirements correctly
./run_tests.sh --list --filter "test_name"
```

The framework automatically detects hardware requirements from:
- Test file paths (e.g., "pal", "ntsc", "newcia")
- Test directory structure
- VICE test metadata

## Hardware Configuration Details

### Automatic Detection

The framework detects hardware requirements from test paths:
- **PAL tests**: Paths containing "pal", "6569"
- **NTSC tests**: Paths containing "ntsc", "6567", "ntscold"
- **CIA old**: Paths containing "oldcia", "cia1"
- **CIA new**: Paths containing "newcia", "cia2"
- **VIC20**: Tests in "VIC20" category

### Supported Systems

Currently supported system configurations:
- **C64 PAL**: MOS6569 VIC-II @ 985,248 Hz
- **C64 NTSC**: MOS6567 VIC-II @ 1,022,727 Hz
- **C64 NTSC-old**: Early NTSC revision
- **VIC20**: MOS6560/6561 (stub implementation)

### Future Hardware Support

Planned additions:
- CIA timing variants (old 6526 vs new 6526A)
- SID chip revisions (6581 vs 8580)
- VIC-II sub-variants (R1 vs R3)
- Drean (PAL-N) systems

See [`HARDWARE_RECONFIGURATION.md`](../HARDWARE_RECONFIGURATION.md) for implementation details.

## Getting Help

- Review [`C64_TEST_FRAMEWORK.md`](../src/systems/c64/C64_TEST_FRAMEWORK.md) for API documentation
- Read [`HARDWARE_RECONFIGURATION.md`](../HARDWARE_RECONFIGURATION.md) for hardware configuration details
- Check VICE test program README files for test-specific details
- Examine test source code for expected behavior
- Compare with VICE emulator for reference implementation

## Resources

- [VICE Test Programs Repository](https://github.com/VICE-Team/testprogs)
- [VICE Emulator Documentation](https://vice-emu.sourceforge.io/)
- [6502 Test Programs Wiki](http://visual6502.org/wiki/index.php?title=6502TestPrograms)
- [Klaus Dormann's Tests](https://github.com/Klaus2m5/6502_65C02_functional_tests)
- [C64 Wiki](https://www.c64-wiki.com/)