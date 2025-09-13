# 65C02 CMOS Test Suite Setup

## Overview
The 65C02 is the CMOS version of the 6502 processor with significant enhancements and bug fixes. Testing 65C02 compatibility requires validation of CMOS-specific features, timing improvements, and new instructions.

## Key 65C02 Differences from NMOS 6502

### 1. New Instructions
- **BRA** - Branch Always (relative)
- **PHX/PHY** - Push X/Y register to stack
- **PLX/PLY** - Pull X/Y register from stack
- **STZ** - Store Zero (absolute, zero page, indexed)
- **TRB/TSB** - Test and Reset/Set Bits
- **WAI** - Wait for Interrupt
- **STP** - Stop processor
- **BIT** - Immediate and indexed addressing modes

### 2. Addressing Mode Enhancements
- **Absolute Indexed Indirect** - (addr,X)
- **Zero Page Indirect** - (zp)
- **JMP (addr,X)** - Indexed indirect jump

### 3. Bug Fixes
- **Decimal Mode**: N and Z flags work correctly in decimal mode
- **JMP ($xxFF)**: Page boundary crossing handled correctly
- **RMW Instructions**: No extra reads during read-modify-write
- **Interrupt Timing**: More precise interrupt handling

### 4. Hardware Enhancements
- **WAI/STP Instructions**: Low power modes
- **BE Pin**: Bus enable for DMA
- **RDY Line**: Affects all cycles, not just reads
- **More Robust**: Better noise immunity

## Test Categories for 65C02

### Core Functionality Tests
1. **New Instruction Validation**
   - BRA instruction and branch behavior
   - PHX/PHY/PLX/PLY stack operations
   - STZ instruction variants
   - TRB/TSB bit manipulation
   - WAI/STP processor control

2. **Enhanced Addressing Modes**
   - (addr,X) absolute indexed indirect
   - (zp) zero page indirect
   - JMP (addr,X) indexed indirect jump
   - BIT immediate and indexed modes

3. **Bug Fix Validation**
   - Decimal mode N/Z flag behavior
   - JMP indirect page boundary fix
   - RMW instruction cycle accuracy
   - Interrupt timing improvements

4. **Hardware Feature Tests**
   - WAI instruction interrupt wake-up
   - STP instruction halt behavior
   - BE pin bus control
   - Enhanced RDY line behavior

## Available 65C02 Test Sources

### 1. Klaus Dormann Extended Tests
**Already Available**: We have the 65C02 extended opcodes test from Klaus Dormann
- Location: `external/6502-tests/6502_65C02_functional_tests/bin_files/65C02_extended_opcodes_test.bin`
- Status: ✅ **ALREADY PASSING** (113 cycles)

### 2. Western Design Center Tests
**Source**: WDC provides official 65C02 validation tests
- New instruction functionality
- CMOS-specific timing validation
- Hardware feature verification

### 3. Community Test Suites
**Sources**: 
- Bruce Clark's 65C02 decimal mode tests
- Jeff Laughton's 65C02 timing tests
- Michal Kowalski's 65C02 validation suite

### 4. Custom CMOS Validation Tests
**Need to Create**:
- Decimal mode flag behavior comparison
- New addressing mode validation
- Hardware pin behavior tests

## Test Implementation Strategy

### 1. Extend Existing Klaus Framework
The Klaus test harness already supports 65C02 with the `--cmos` flag:
```bash
./fam65xx_cpp_klaus_test_runner --cmos -v
```

### 2. Create 65C02-Specific Test Harness
```cpp
class CMOS65C02TestHarness : public LorenzTestHarness {
private:
    std::unique_ptr<fam65xx<config_65c02>> cpu_;  // CMOS configuration
    
public:
    // 65C02-specific test methods
    bool test_new_instructions();
    bool test_enhanced_addressing();
    bool test_decimal_mode_fixes();
    bool test_hardware_features();
    
    // WAI/STP instruction testing
    bool test_wai_behavior();
    bool test_stp_behavior();
    
    // Bus control testing
    bool test_be_pin_control();
    bool test_enhanced_rdy();
};
```

### 3. Configuration-Based Testing
Use the existing CPU configuration system:
```cpp
// NMOS 6502 testing
using test_config_nmos = config_6502;

// CMOS 65C02 testing  
using test_config_cmos = config_65c02;

template<typename Config>
class ConfigurableTestHarness {
    std::unique_ptr<fam65xx<Config>> cpu_;
    
    // Test behavior varies based on configuration
    bool test_decimal_mode() {
        if constexpr (Config::has_cmos_fixes) {
            return test_cmos_decimal_mode();
        } else {
            return test_nmos_decimal_mode();
        }
    }
};
```

### 4. Comparative Testing Framework
```cpp
class ComparativeTestSuite {
    NMOS6502TestHarness nmos_harness_;
    CMOS65C02TestHarness cmos_harness_;
    
public:
    // Compare NMOS vs CMOS behavior
    TestResult compare_decimal_mode_behavior();
    TestResult compare_jmp_indirect_behavior();
    TestResult compare_interrupt_timing();
    
    // Validate CMOS-only features
    TestResult test_cmos_exclusive_features();
};
```

## Test File Organization

### Directory Structure
```
external/65c02-tests/
├── README.md
├── bin/
│   ├── bra_test.prg           # BRA instruction tests
│   ├── phx_phy_test.prg       # Stack instruction tests
│   ├── stz_test.prg           # Store zero tests
│   ├── trb_tsb_test.prg       # Bit manipulation tests
│   ├── wai_stp_test.prg       # Processor control tests
│   ├── decimal_cmos_test.prg  # CMOS decimal mode tests
│   ├── jmp_indirect_fix.prg   # Page boundary fix test
│   └── addressing_modes.prg   # New addressing modes
├── expected/
│   ├── bra_test.exp
│   ├── phx_phy_test.exp
│   └── ... (expected results)
└── source/
    ├── bra_test.asm
    ├── phx_phy_test.asm
    └── ... (assembly sources)
```

### Test Binary Format
Each test follows the standard format:
- Load address: `$0801` (C64 BASIC start)
- Test execution with proper initialization
- Success/failure indication via specific end state
- Compatible with existing test harness

## Implementation Plan

### Phase 1: Test Collection and Setup
1. ✅ Klaus Dormann 65C02 tests (already available and passing)
2. Download additional 65C02 test suites
3. Create test directory structure
4. Set up 65C02-specific test harness

### Phase 2: CMOS Configuration Testing
1. Validate 65C02 CPU configuration in fam65xx_cpp
2. Test CMOS-specific features (WAI, STP, new addressing modes)
3. Verify bug fixes (decimal mode, JMP indirect)
4. Hardware feature validation (BE pin, enhanced RDY)

### Phase 3: Comparative Validation
1. NMOS vs CMOS behavior comparison
2. Decimal mode flag behavior verification
3. Timing difference validation
4. New instruction comprehensive testing

### Phase 4: Integration
1. Add 65C02 tests to automated test suite
2. CMake integration for 65C02 test runner
3. Continuous integration setup
4. Documentation and reporting

## Expected Test Results

### Current Status
- ✅ **Klaus 65C02 Extended Opcodes**: PASSING (113 cycles)
- ⏳ **Additional 65C02 Tests**: Pending setup
- ⏳ **CMOS Feature Tests**: Pending implementation
- ⏳ **Comparative Tests**: Pending development

### Success Criteria
1. **All 65C02 new instructions** execute correctly
2. **Enhanced addressing modes** work properly
3. **CMOS bug fixes** are implemented correctly
4. **Hardware features** (WAI, STP, BE) function as specified
5. **Decimal mode** behaves correctly (CMOS vs NMOS)
6. **Timing accuracy** matches real 65C02 hardware

## CPU Configuration Validation

### Verify 65C02 Configuration
The fam65xx_cpp already has 65C02 support via `config_65c02`:
```cpp
using config_65c02 = cpu_config<
    fam65xx_cpp::CpuVariant::CMOS_65C02,  // CMOS variant
    true,   // decimal_mode
    false,  // io_ports  
    true,   // sync_pin
    true,   // so_pin
    false,  // aec_pin
    true,   // be_pin (CMOS feature)
    false,  // abort_pin
    true,   // vp_pin
    true,   // ml_pin
    false,  // bank_pins
    false,  // illegal_opcodes (CMOS has none)
    true,   // cmos_fixes (key difference)
    16,     // address_lines
    true,   // rdy_affects_writes (CMOS enhancement)
    true,   // wai_instruction (CMOS feature)
    true    // stp_instruction (CMOS feature)
>;
```

### Key Configuration Differences
- `has_cmos_fixes = true` - Enables decimal mode and JMP fixes
- `rdy_affects_writes = true` - CMOS RDY behavior
- `wai_instruction = true` - WAI support
- `stp_instruction = true` - STP support
- `has_be_pin = true` - Bus enable pin
- `illegal_opcodes = false` - No illegal opcodes in CMOS

## Next Steps

1. **Create 65C02 test directory structure**
2. **Download additional 65C02 test suites**
3. **Implement 65C02-specific test harness**
4. **Validate CMOS configuration in fam65xx_cpp**
5. **Run comprehensive 65C02 test suite**
6. **Document results and any required fixes**

This comprehensive 65C02 testing approach ensures the fam65xx_cpp implementation correctly handles both NMOS 6502 and CMOS 65C02 variants with full compatibility and feature support.