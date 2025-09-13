# Wolfgang Lorenz Test Suite Setup

## Overview
The Wolfgang Lorenz test suite is the gold standard for 6502/6510 CPU validation, focusing on:
- Exact cycle timing validation
- Processor flag behavior in edge cases  
- Illegal opcode behavior on NMOS variants
- Bus behavior and timing-critical operations
- Memory read/write patterns during instruction execution

## Test Suite Components

### Core Tests (NMOS 6502/6510)
- **ladx** - LDA/LDX instruction tests with various addressing modes
- **sbcb** - SBC instruction with decimal mode edge cases
- **cpxy** - CPX/CPY comparison instruction tests
- **cmpx** - CMP instruction comprehensive testing
- **branchwrap** - Branch instruction page boundary behavior
- **mmufetch** - Memory management and fetch cycle testing
- **trap** - Illegal opcode and JAM instruction behavior

### Timing-Critical Tests  
- **cia1tab** - CIA timer interaction timing
- **cia2tab** - CIA timer B behavior
- **irq** - IRQ timing and acknowledgment 
- **nmi** - NMI edge detection and timing
- **cnto2** - Counter/timer overflow behavior

### Flag Behavior Tests
- **flag** - Processor status flag edge cases
- **decimal** - Decimal mode N/Z flag bugs (NMOS vs CMOS)
- **overflow** - V flag behavior in edge cases

## Repository Information
**Original Source**: Wolfgang Lorenz (VICE team member)
**Mirror Repository**: https://github.com/Klaus2m5/6502_65C02_functional_tests
**Alternative Source**: VICE emulator test suite
**Format**: Binary test files with expected register states

## File Structure
```
external/lorenz-tests/
├── README.md
├── bin/
│   ├── ladx        - LDA/LDX tests
│   ├── sbcb        - SBC decimal tests  
│   ├── cpxy        - CPX/CPY tests
│   ├── cmpx        - CMP tests
│   ├── branchwrap  - Branch timing
│   ├── mmufetch    - Memory fetch timing
│   ├── trap        - Illegal opcodes
│   ├── cia1tab     - Timer tests
│   ├── cia2tab     - Timer B tests
│   ├── irq         - IRQ timing
│   ├── nmi         - NMI timing
│   └── flag        - Flag behavior
└── expected/
    ├── ladx.exp
    ├── sbcb.exp
    └── ... (expected results for each test)
```

## Test Format
Each Lorenz test:
1. Loads at specific memory address (usually $0801)
2. Initializes CPU state to known values
3. Executes test sequence
4. Ends with specific register states and memory contents
5. Requires exact cycle counting and timing validation

## Integration Plan
1. Download test binaries from VICE or mirror repository
2. Create lorenz_test_harness.cpp for test execution
3. Implement cycle-accurate state comparison
4. Add timing validation for each instruction
5. Create automated test runner for all Lorenz tests

## Expected Challenges
- **Exact Timing**: Lorenz tests require cycle-perfect execution
- **Illegal Opcodes**: NMOS-specific illegal instruction behavior
- **Flag Edge Cases**: Subtle processor flag behavior differences
- **Memory Timing**: Bus read/write patterns during instructions
- **Decimal Mode Bugs**: NMOS 6502 decimal mode N/Z flag bugs

## Success Criteria
- All Lorenz tests pass with exact cycle counts
- Correct processor state after each test
- Proper illegal opcode handling for NMOS variants
- Accurate timing for page boundary crossings
- Correct decimal mode flag behavior for variant type