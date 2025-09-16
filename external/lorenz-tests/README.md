# Wolfgang Lorenz 6502 Test Suite

## Overview
This directory contains the Wolfgang Lorenz test suite for comprehensive 6502/6510 CPU validation.
The tests focus on cycle-accurate timing, processor flag edge cases, and illegal opcode behavior.

## Test Files
The following test binaries should be placed in this directory:

### Core Instruction Tests
- `ladx` - LDA/LDX addressing mode tests
- `sbcb` - SBC decimal mode edge cases  
- `cpxy` - CPX/CPY comparison tests
- `cmpx` - CMP instruction comprehensive testing

### Timing and Edge Case Tests
- `branchwrap` - Branch page boundary behavior
- `mmufetch` - Memory fetch cycle timing
- `trap` - Illegal opcode behavior
- `flag` - Processor status flag edge cases

### Hardware Integration Tests
- `cia1tab` - CIA timer A interaction
- `cia2tab` - CIA timer B behavior
- `irq` - IRQ timing validation
- `nmi` - NMI edge detection timing

## Acquisition
To obtain the test files:

1. **From VICE Source**: 
   ```bash
   git clone https://github.com/VICE-Team/svn-mirror.git
   cp svn-mirror/vice/testprogs/CPU/* ./
   ```

2. **From VICE Binary**:
   - Download VICE emulator binary distribution
   - Extract testprogs from installation directory
   - Copy CPU test files to this location

3. **From Archive**:
   - Download testprogs.tar.gz from VICE project
   - Extract CPU-specific test files

## Usage
Once test files are available, run using:
```bash
cd code/c/tests
./fam65xx_cpp_lorenz_test_runner
```

## Expected Format
Each test file:
- Binary executable format
- Load address: $0801 (standard C64 BASIC start)
- Test completion: RTS instruction or specific end address
- Results: Final CPU state validation against expected values