# ProcessorTests Integration for MOS6510 CPU Validation

This directory contains a comprehensive test suite that uses **TomHarte's ProcessorTests** as ground truth for validating the MOS6510 CPU implementation.

## 🎯 Overview

The fam65xx ProcessorTests runner provides **definitive, hardware-verified validation** of your MOS6510 CPU using authoritative test data from the ProcessorTests repository, eliminating any guesswork about correct CPU behavior.

## 📦 Files

- **`fam65xx_processor_tests_runner.cpp`** - Main test runner using ProcessorTests JSON data
- **`json_parser.c/.h`** - Custom JSON parser (no external dependencies)
- **`CMakeLists.txt`** - Build configuration with ProcessorTests integration

## 🚀 Quick Setup

### 1. Add ProcessorTests as Git Submodule

```bash
# From the tests directory
cd code/c/tests/
git submodule add https://github.com/TomHarte/ProcessorTests.git processor_tests
git submodule update --init --recursive
```

### 2. Build and Run

```bash
# Build the test suite
mkdir build && cd build
cmake ..
cmake --build . --config Debug

# Run ProcessorTests validation (thousands of tests)
./fam65xx_processor_tests_runner ../processor_tests/6502/v1/

# Or run specific test file
./fam65xx_processor_tests_runner -v ../processor_tests/6502/v1/00.json
```

### 3. Using Build Scripts

```cmd
REM Windows
build.bat

REM Linux/macOS  
./build.sh
```

## 📊 Test Types

### ProcessorTests (Primary Validation)
- **Source**: TomHarte/ProcessorTests repository
- **Authority**: Hardware-verified against real 6502/6510
- **Coverage**: All 256 opcodes including illegal instructions
- **Format**: JSON test vectors with initial/final CPU states
- **Accuracy**: Cycle-perfect timing validation

### Comprehensive Tests (Secondary)
- **Source**: Custom test suite in this repository
- **Purpose**: Additional validation and edge cases
- **Coverage**: Specific scenarios and instruction combinations

## 🔧 Usage Examples

### Run All ProcessorTests
```bash
./fam65xx_processor_tests_runner processor_tests/6502/v1/
```

### Run Single Test File (Verbose)
```bash
./fam65xx_processor_tests_runner -v processor_tests/6502/v1/00.json
```

### Run Only Comprehensive Tests
```bash
./test_mos6510_comprehensive
```

### Run All Tests via CTest
```bash
ctest --verbose -C Debug
```

## 📋 ProcessorTests JSON Format

Each test contains:
```json
{
  "name": "adc immediate",
  "initial": {
    "pc": 4096,
    "s": 255,
    "a": 0,
    "x": 0,
    "y": 0,
    "p": 32,
    "ram": [[4096, [105, 1]]]
  },
  "final": {
    "pc": 4098,
    "s": 255,
    "a": 1,
    "x": 0,
    "y": 0,
    "p": 32,
    "ram": [[4096, [105, 1]]],
    "cycles": 2
  }
}
```

## 🎯 Expected Results

### Successful Validation
```
=== ProcessorTests Runner for MOS6510 CPU ===
Using ground truth data from TomHarte/ProcessorTests
Test path: processor_tests/6502/v1/

Processing file: processor_tests/6502/v1/00.json
Processing file: processor_tests/6502/v1/01.json
...

=== TEST SUMMARY ===
Total tests run: 8792
Tests passed: 8792
Tests failed: 0

ALL TESTS PASSED - MOS6510 CPU matches ProcessorTests ground truth!
```

### Failed Validation
```
FAIL adc immediate: A - expected 0x01, got 0x00
FAIL adc immediate: Cycles - expected 2, got 3 (raw: 4)

=== TEST SUMMARY ===
Total tests run: 8792
Tests passed: 8791
Tests failed: 1

SOME TESTS FAILED - CPU implementation differs from ground truth
```

## 🔍 Troubleshooting

### ProcessorTests Not Found
```
ERROR: Could not open directory: processor_tests/6502/v1/
```
**Solution**: Ensure ProcessorTests submodule is initialized:
```bash
git submodule update --init --recursive
```

### Cycle Count Mismatches
If all cycle counts are consistently off by 1:
- Check intercept mechanism overhead adjustment in `fam65xx_processor_tests_runner.cpp`
- Modify the cycle adjustment logic if needed

### Memory Access Errors
- Verify memory interface implementation in your MOS6510
- Check bus operations are correctly attached

## 🎯 Benefits

### ProcessorTests Advantages
- ✅ **Hardware-verified** - tested against real 6502/6510 chips
- ✅ **Industry standard** - used by major emulator projects
- ✅ **Comprehensive** - covers all 256 opcodes
- ✅ **Accurate timing** - cycle-perfect validation
- ✅ **No assumptions** - eliminates guesswork

### Integration Benefits
- ✅ **Authoritative validation** - definitive correctness proof
- ✅ **Continuous validation** - run tests after any CPU changes
- ✅ **Cross-platform** - works on Windows, Linux, macOS
- ✅ **No dependencies** - custom JSON parser included

## 📚 References

- **ProcessorTests Repository**: https://github.com/TomHarte/ProcessorTests
- **6502 Reference**: http://www.6502.org/
- **Visual6502**: http://visual6502.org/
- **MOS6510 Documentation**: Commodore 64 Programmer's Reference Guide

## 🚨 Important Notes

1. **Ground Truth**: ProcessorTests data is considered authoritative
2. **Cycle Adjustment**: Tests account for intercept mechanism overhead
3. **Platform Support**: Windows and Unix directory traversal included
4. **Memory Management**: All test data is loaded into 64KB test memory
5. **Error Reporting**: Detailed failure information for debugging

This validation framework ensures your MOS6510 CPU implementation is **cycle-accurate and hardware-compatible**!