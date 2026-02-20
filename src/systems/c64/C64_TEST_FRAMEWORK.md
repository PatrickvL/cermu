# C64 Test Framework Documentation

## Overview

The C64 Test Framework provides automated testing capabilities for the C64 emulator using test programs from the VICE test suite. It discovers, catalogs, runs, and validates tests automatically, making it easy to ensure emulator accuracy and catch regressions.

## Features

- **Automatic test discovery**: Recursively scans VICE-testprogs directory for test programs
- **Multiple test types**: 
  - Exitcode tests (via debug register $D7FF)
  - Screenshot tests (visual comparison with reference images)
  - Interactive tests (marked for manual testing)
- **Hardware configuration filtering**: Filter tests by PAL/NTSC, CIA versions, SID versions, etc.
- **Batch execution**: Run hundreds of tests unattended
- **Failed test re-running**: Re-run only tests that failed in previous runs
- **Multiple output formats**: Text, JSON, and HTML reports
- **Detailed statistics**: Pass rates, timing information, failure analysis

## Test Types

### Exitcode Tests

Most tests use the "debug cartridge" register at $D7FF:
- Write `$00` to indicate success (test passed)
- Write `$FF` to indicate failure (test failed)  
- Test times out if no value is written within the cycle limit

The framework monitors this register and automatically determines test results.

### Screenshot Tests

Tests with reference images in a `references/` subdirectory are screenshot tests:
```
VICII/border/vborder.prg
VICII/border/references/vborder.prg.png
```

These tests run the program and compare the generated screen output against the reference image. (Note: Screenshot comparison is not yet fully implemented)

### Interactive Tests

Tests marked as interactive require user interaction and are skipped in batch mode.

## Directory Structure

```
VICE-testprogs/
├── CPU/              # CPU instruction tests
│   ├── kdormann/     # Klaus Dormann's 6502 functional tests
│   ├── bclark/       # Bruce Clark's tests
│   └── ...
├── CIA/              # CIA timer and interrupt tests
├── VICII/            # VIC-II graphics chip tests
│   ├── border/       # Border manipulation tests
│   ├── sprites/      # Sprite tests
│   └── ...
├── SID/              # SID sound chip tests
├── interrupts/       # Interrupt timing tests
├── C64/              # C64-specific tests
└── general/          # General/combined tests
```

## Usage

### Command-Line Tool

Basic usage:
```bash
./c64_test_runner --testprogs /path/to/VICE-testprogs
```

Filter by category:
```bash
./c64_test_runner --testprogs /path/to/VICE-testprogs --category CPU
```

Filter by path substring:
```bash
./c64_test_runner --testprogs /path/to/VICE-testprogs --filter kdormann
```

Specify hardware configuration:
```bash
./c64_test_runner --testprogs /path/to/VICE-testprogs --pal --cia-new
```

Re-run only failed tests:
```bash
./c64_test_runner --testprogs /path/to/VICE-testprogs --failed previous_results.json
```

Generate JSON output:
```bash
./c64_test_runner --testprogs /path/to/VICE-testprogs --json results.json
```

List tests without running:
```bash
./c64_test_runner --testprogs /path/to/VICE-testprogs --list
```

### Programmatic API

```cpp
#include "c64_test_framework.h"

// Create framework
c64_test::TestFramework framework("/path/to/VICE-testprogs");
framework.set_verbose(true);

// Scan for tests
framework.scan_tests();

// Create filter
c64_test::TestFilter filter;
filter.path_filter = "CPU";
filter.hardware = c64_test::HardwareConfig::VICII_PAL;
filter.skip_interactive = true;

// Get filtered tests
auto tests = framework.get_filtered_tests(filter);

// Create C64 system
c64_config_t config = {};
c64_config_init_defaults(&config);
C64SystemData* c64 = c64_system_create(&config);

// Run tests
auto results = framework.run_tests(tests, c64);

// Print statistics
framework.print_summary(results);

// Save results
framework.save_results("results.txt", results);
framework.save_results_json("results.json", results);

// Cleanup
c64_system_destroy(c64);
```

## Hardware Configuration

The framework supports filtering tests by hardware configuration:

### Video Standard
- `VICII_PAL` - PAL timing (985248 Hz)
- `VICII_NTSC` - NTSC timing (1022727 Hz)
- `VICII_NTSCOLD` - Old NTSC timing
- `VICII_DREAN` - DREAN timing (PAL-N)

### CIA Versions
- `CIA_OLD` - Old 6526 CIA (earlier timing)
- `CIA_NEW` - New 6526A CIA (later timing)

### SID Versions
- `SID_6581` - Old SID chip
- `SID_8580` - New SID chip

### VIC-II Versions
- `VICII_OLD` - Earlier VIC-II revision
- `VICII_NEW` - Later VIC-II revision

## Test Result Status

- `PASSED` - Test completed successfully (exit code $00)
- `FAILED` - Test failed (exit code $FF)
- `TIMEOUT` - Test exceeded cycle limit without writing result
- `SKIPPED` - Test skipped due to filters or type
- `ERROR` - Error loading or running test
- `NOT_RUN` - Test has not been executed yet

## Output Formats

### Text Format

Human-readable format with test details and statistics:
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

### JSON Format

Machine-readable format for integration with other tools:
```json
{
  "results": [
    {
      "path": "CPU/kdormann/6502_functional_test.prg",
      "name": "6502_functional_test.prg",
      "category": "CPU",
      "status": "PASSED",
      "type": "exitcode",
      "cycles": 8543210,
      "time_ms": 125.3,
      "exit_code": 0,
      "message": "Test passed"
    }
  ],
  "summary": {
    "total": 150,
    "passed": 142,
    "failed": 5,
    "timeout": 2,
    "skipped": 1,
    "error": 0,
    "pass_rate": 94.7,
    "total_time_ms": 18234.5
  }
}
```

## Test Categories

### CPU Tests

Tests CPU instruction execution, addressing modes, timing, and edge cases:
- **kdormann**: Klaus Dormann's comprehensive 6502 functional tests
- **bclark**: Bruce Clark tests for decimal mode and other features
- **ane/lax/sha**: Illegal opcode tests
- **cpuport**: CPU port ($0000/$0001) behavior tests

### CIA Tests

Tests CIA timers, interrupts, ports, and timing:
- Timer A/B functionality and modes
- Interrupt generation and acknowledgment
- Port input/output
- Time-of-day clock
- Serial port

### VIC-II Tests

Tests graphics chip behavior:
- **border**: Border opening/closing
- **sprites**: Sprite display, priority, collisions
- **raster**: Raster interrupt timing
- **videomode**: Different video modes (hires, multicolor, bitmap)
- **timing**: Cycle-exact timing tests

### Interrupt Tests

Tests interrupt timing and behavior:
- IRQ and NMI timing
- Interrupt acknowledgment
- Branch quirks during interrupts
- CIA interrupt interaction

## Best Practices

### Running Tests

1. **Start with CPU tests**: These are fundamental and must pass first
   ```bash
   ./c64_test_runner --testprogs /path --category CPU
   ```

2. **Test one category at a time**: Easier to diagnose issues
   ```bash
   ./c64_test_runner --testprogs /path --category VICII
   ```

3. **Use appropriate hardware configs**: Match your emulator's capabilities
   ```bash
   ./c64_test_runner --testprogs /path --pal --cia-new
   ```

4. **Re-run failures**: After fixes, re-run only failed tests
   ```bash
   ./c64_test_runner --testprogs /path --failed results.json
   ```

### Interpreting Results

- **High pass rate (>95%)**: Emulator core is solid
- **Timeout failures**: Often indicate infinite loops or missing functionality
- **Consistent failures in one area**: Targeted bug in specific component
- **Random failures**: May indicate timing issues or race conditions

### Adding New Tests

1. Place `.prg` file in appropriate category directory
2. Add reference image in `references/` if visual test:
   ```
   VICII/mytest/mytest.prg
   VICII/mytest/references/mytest.prg.png
   ```
3. Add `readme.txt` documenting test purpose and expected behavior
4. Test will be automatically discovered on next scan

## Troubleshooting

### Test Timeouts

If tests timeout:
- Increase timeout in test descriptor (default 10M cycles)
- Check if test requires specific hardware configuration
- Verify test program is loaded correctly
- Check if CPU is executing instructions

### Failed Tests

If tests fail:
1. Run with `--verbose` to see detailed output
2. Check test's readme for requirements
3. Verify hardware configuration matches test needs
4. Compare with VICE behavior for reference
5. Use debugger to step through test execution

### Missing Tests

If tests aren't found:
- Verify `--testprogs` path is correct
- Check file permissions
- Ensure `.prg` files exist in scanned directories
- Use `--list` to see what tests were discovered

## Future Enhancements

Planned improvements:
- [ ] Full screenshot comparison implementation
- [ ] HTML report generation with visual test results  
- [ ] Performance regression testing
- [ ] Test execution history tracking
- [ ] Parallel test execution
- [ ] Integration with CI/CD systems
- [ ] Test coverage analysis
- [ ] Automated bisection for finding regressions

## References

- [VICE Test Programs](https://sourceforge.net/p/vice-emu/code/HEAD/tree/testprogs/)
- [VICE Test Bench Documentation](https://vice-emu.sourceforge.io/vice_13.html)
- [6502 Test Programs](http://visual6502.org/wiki/index.php?title=6502TestPrograms)
- [Klaus Dormann's Tests](https://github.com/Klaus2m5/6502_65C02_functional_tests)