# MOS6510 CPU Test Suite

This directory contains comprehensive tests for the MOS6510 CPU implementation.

## Test Files

- `test_mos6510_comprehensive.c` - Comprehensive test suite covering all 256 opcodes
- `CMakeLists.txt` - CMake build system for compiling and running tests
- `build.bat` / `build.sh` - Automated build and test scripts

## Quick Start (Recommended)

### Windows
```cmd
cd code\c\tests
build.bat
```

### Linux/macOS
```bash
cd code/c/tests
chmod +x build.sh
./build.sh
```

This will automatically:
1. Create a build directory
2. Configure CMake
3. Build the tests
4. Run all tests with verbose output

## Manual CMake Usage

### Step-by-step CMake build

```bash
# IMPORTANT: You must create and use a build directory
cd code/c/tests

# Create build directory
mkdir build
cd build

# Configure CMake (from inside build directory)
cmake ..

# Build tests
cmake --build .

# Run tests
ctest --verbose
```

### Common CMake Commands

```bash
# From build directory:

# Run specific tests
ctest -R basic_test --verbose
ctest -R comprehensive_test --verbose

# Build and run custom targets
cmake --build . --target run_basic_test
cmake --build . --target run_comprehensive_test
cmake --build . --target run_all_tests

# Clean and rebuild
cmake --build . --target clean
cmake --build .
```

## Build Options

```bash
# Debug build (from build directory)
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Specify compiler
cmake -DCMAKE_C_COMPILER=gcc ..
cmake -DCMAKE_C_COMPILER=clang ..
```

## Troubleshooting

### "could not load cache" Error
This error occurs when you run `cmake --build .` from the wrong directory. 

**Solution:** Always run CMake commands from the build directory:
```bash
cd code/c/tests
mkdir build
cd build          # <-- IMPORTANT: Must be in build directory
cmake ..          # <-- Configure from build dir
cmake --build .   # <-- Build from build dir
```

### Missing Source Files
If you get compilation errors about missing files, ensure the source structure is correct:
```
code/c/
├── src/
│   ├── chip/cpu/mos6510/
│   │   ├── mos6510.c
│   │   ├── mos6510.h
│   │   └── ...
│   └── core/
│       └── chip.c
└── tests/
    ├── test_mos6510_comprehensive.c
    ├── CMakeLists.txt
    └── build/          # <-- Created by mkdir build
```

## Test Coverage

The comprehensive test suite validates:

### **All 256 Opcodes**
- Legal instructions (151 opcodes)
- Illegal/undocumented instructions (105 opcodes)
- Cycle-accurate timing for each instruction

### **Register Operations**
- Accumulator (A)
- Index registers (X, Y)
- Stack pointer (SP)
- Program counter (PC)
- Processor status (P) flags

### **Status Flags**
- **N (Negative)** - Set when bit 7 of result is 1
- **Z (Zero)** - Set when result is 0
- **C (Carry)** - Set on arithmetic carry/borrow
- **V (Overflow)** - Set on signed arithmetic overflow
- **I (Interrupt Disable)** - Controls IRQ response
- **D (Decimal Mode)** - Enables BCD arithmetic
- **B (Break)** - Distinguishes BRK from IRQ

### **Addressing Modes**
- Immediate (#$nn)
- Zero Page ($nn)
- Zero Page,X ($nn,X)
- Zero Page,Y ($nn,Y)
- Absolute ($nnnn)
- Absolute,X ($nnnn,X)
- Absolute,Y ($nnnn,Y)
- Indirect (JMP only) ($nnnn)
- Indexed Indirect ($nn,X)
- Indirect Indexed ($nn),Y

### **Instruction Categories**
- **Load/Store** - LDA, LDX, LDY, STA, STX, STY
- **Arithmetic** - ADC, SBC (binary and decimal mode)
- **Logic** - AND, ORA, EOR
- **Shift/Rotate** - ASL, LSR, ROL, ROR
- **Increment/Decrement** - INC, DEC, INX, INY, DEX, DEY
- **Compare** - CMP, CPX, CPY
- **Branch** - BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS
- **Jump/Call** - JMP, JSR, RTS, RTI
- **Stack** - PHA, PLA, PHP, PLP
- **Transfer** - TAX, TAY, TXA, TYA, TSX, TXS
- **Flag Control** - CLC, SEC, CLI, SEI, CLD, SED, CLV
- **System** - BRK, NOP

### **Illegal Instructions**
- **LAX** - Load A and X
- **SAX** - Store A AND X
- **DCP** - Decrement and Compare
- **ISC** - Increment and Subtract with Carry
- **SLO** - Shift Left and OR
- **RLA** - Rotate Left and AND
- **SRE** - Shift Right and EOR
- **RRA** - Rotate Right and Add
- And many more undocumented opcodes

### **Cycle Accuracy**
- Validates exact cycle counts for each instruction
- Tests page boundary crossing penalties
- Verifies read-modify-write instruction timing
- Confirms branch instruction timing variations

## Expected Output

```
=== MOS6510 Comprehensive Test Suite ===
Testing all 256 opcodes for correct register states, flags, and cycle counts

=== Testing Basic Instructions ===
PASS NOP
PASS LDA #$42
PASS LDA $50
...

=== TEST SUMMARY ===
Total tests run: 45
Tests passed: 45
Tests failed: 0

ALL TESTS PASSED - MOS6510 CPU is cycle-accurate!
```

## Integration with IDEs

### Visual Studio Code
1. Install CMake Tools extension
2. Open the tests directory
3. Use Ctrl+Shift+P → "CMake: Configure"
4. Use Ctrl+Shift+P → "CMake: Build"
5. Use Ctrl+Shift+P → "CMake: Run Tests"

### CLion
1. Open the tests directory as a CMake project
2. Use the built-in test runner
3. Right-click CMakeLists.txt → "Reload CMake Project"

## Debugging Failed Tests

When a test fails, detailed error messages show:
- Expected vs actual register values
- Expected vs actual flag states
- Expected vs actual memory contents
- Expected vs actual cycle counts

Example failure output:
```
FAIL ADC #$05: A register - expected 0x15, got 0x16
FAIL ADC #$05: Cycle count - expected 2, got 3
```

This comprehensive test suite ensures the MOS6510 CPU implementation is fully compatible with the original hardware and ready for C64 emulation.