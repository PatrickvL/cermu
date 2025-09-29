# AGENT DEVELOPMENT RULES

## 🚫 **STRICT PROHIBITION - NO SEPARATE DEBUG/TEST PROGRAMS**

**RULE #1: UNIFIED TESTING ONLY**
- **DO NOT** create separate debug programs, test harnesses, or standalone test files
- **DO NOT** create individual `.cpp` test files for specific opcodes or instructions
- **DO NOT** create custom JSON test files

**RULE #2: USE THE UNIFIED PROCESSOR TESTS RUNNER**
- **ALWAYS** use the existing `mos6502_processor_tests_runner.cpp` as the single testing and debugging tool
- **ALWAYS** use existing ProcessorTests JSON files from the standard test suite
- **ALWAYS** use the enhanced debugging command line arguments:
  - `--debug-cycles` for detailed cycle-by-cycle debugging
  - `--opcode 0xXX` for filtering to specific opcodes
  - `--test-index N` for running individual test cases
  - `-v` for verbose output

**RULE #3: SYSTEMATIC OPCODE PROCESSING**
- Use the unified runner to systematically test opcodes 0x00-0xFF
- Debug failures by examining the detailed cycle output from the unified runner
- Fix issues in the implementation files (`mos6502_optimized.hpp`, `mos6502_cycle_table.hpp`)
- Verify fixes using the same unified runner

**RULE #4: DEBUGGING WORKFLOW**
1. Run: `./mos6502_processor_tests_runner --debug-cycles --opcode 0xXX existing_test.json`
2. Analyze the detailed cycle-by-cycle output
3. Fix issues in the implementation files
4. Recompile and test again with the same command
5. Move to the next opcode only after achieving 100% pass rate

**VIOLATION CONSEQUENCES:**
Creating separate debug/test programs is **STRICTLY FORBIDDEN** and wastes development time.

## ✅ **CORRECT APPROACH EXAMPLES**

```bash
# Debug BRK instruction (0x00)
./mos6502_processor_tests_runner --debug-cycles --opcode 0x00 test_file.json

# Debug ORA instruction (0x01) 
./mos6502_processor_tests_runner --debug-cycles --opcode 0x01 test_file.json

# Run specific test case
./mos6502_processor_tests_runner --test-index 0 test_file.json

# Process all tests for a directory
./mos6502_processor_tests_runner --debug-cycles processor_tests/6502/v1/
```

**REMEMBER: ONE UNIFIED TOOL FOR ALL TESTING AND DEBUGGING**

## **RULE #5: CYCLE-DRIVEN IMPLEMENTATION**
- **CRITICAL**: Avoid opcode-specific comparisons (`if (current_opcode == 0x00)`) in mos6502_optimized.hpp
- Instead, rely on cycle definition patterns and addressing modes to determine behavior
- Use AddressGroup, BusDriverGroup, and RegTargetGroup to encode instruction semantics
- Let the cycle table drive all instruction behavior rather than hardcoded opcode checks
- Example: Use `cycle.get_address() == AddressGroup::IRQ_VECTOR_LOW` instead of `current_opcode == 0x00`

## **RULE #6: HARDWARE-ACCURATE VECTOR ADDRESSING**
- Use proper AddressGroup modes (IRQ_VECTOR_LOW/HIGH, etc.) for interrupt vectors
- Model PLA behavior where vector addresses are generated during φ1 phase
- Avoid hardcoded vector addresses in address calculation logic

## **RULE #7: CYCLE TABLE COMPLETENESS**
- Process all 256 opcodes systematically from 0x00-0xFF
- Include documented, undocumented, and illegal opcodes
- Use template conditions for CPU variant differences (NMOS/CMOS)
## **RULE #8: CODE OPTIMIZATION AND REUSABILITY**
- **NO DUPLICATE FUNCTIONALITY**: Never generate duplicate code blocks or similar logic
- **USE INLINE HELPER FUNCTIONS**: Extract common patterns into `inline` helper functions
- **BRANCHLESS CODE**: Prefer branchless implementations using bitwise operations and conditional expressions
- **AVOID REPEATED ARRAY LOOKUPS**: Cache array/table lookups in local variables when accessed multiple times
- **TEMPLATE METAPROGRAMMING**: Use `constexpr` and template specialization for compile-time optimizations

## **RULE #9: PERFORMANCE-CRITICAL PATTERNS**
- Replace `if/else` chains with bitwise operations where possible
- Use lookup tables only when absolutely necessary and cache results
- Prefer `constexpr` computations over runtime calculations
- Minimize function call overhead with `inline` functions
- Use template specialization for variant-specific optimizations

## **RULE #10: HARDWARE ACCURACY REQUIREMENT**
- **ALL MOS6502-RELATED DESIGN AND IMPLEMENTATION CHOICES MUST MIMIC ACTUAL HARDWARE**
- Follow the schematics and behavior documented in the Visual 6502 project
- Implement register operations, addressing modes, and timing exactly as the real hardware
- Use proper φ1/φ2 clock separation matching actual MOS6502 timing
- All cycle timings must match hardware-verified behavior from ProcessorTests
