# External Dependencies

This folder contains external projects and dependencies used by the aiemuc C64 emulator.

## Structure

### `/6502-tests/`
Contains the Klaus Dormann 6502/65C02 functional test suite, which is used to verify the correctness of our MOS6510 CPU implementation.

#### `/6502-tests/6502_65C02_functional_tests/`
**Source**: Klaus Dormann's 6502 Functional Test Suite  
**Repository**: https://github.com/Klaus2m5/6502_65C02_functional_tests  
**License**: See `license.txt` in the folder  
**Purpose**: Comprehensive test suite for 6502/65C02 CPU instruction set

Contains:
- `6502_functional_test.a65` - Main functional test suite
- `6502_decimal_test.a65` - Decimal mode testing
- `6502_interrupt_test.a65` - Interrupt handling tests
- `65C02_extended_opcodes_test.a65c` - Extended 65C02 instruction tests
- `bin_files/` - Pre-assembled binary files and listings
- `as65_142.zip` - Assembler tools

## Usage

These external tests are integrated into our CMake build system and can be run using the test executables built from our codebase:

```bash
cd code/c
mkdir build && cd build
cmake ..
make
./bin/klaus_test
```

## Maintenance

When updating external dependencies:
1. Document the version/commit being used
2. Update any integration code in our test suite
3. Verify all tests still pass with the new version
