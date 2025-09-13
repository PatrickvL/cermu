#!/bin/bash

# Comprehensive CPU Test Runner for fam65xx_cpp
# Automated test runner for continuous validation of the CPU implementation

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}    fam65xx_cpp Comprehensive Test Suite${NC}"
echo -e "${BLUE}    Automated CPU Validation Runner${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to run a test and track results
run_test() {
    local test_name="$1"
    local test_command="$2"
    local description="$3"
    
    echo -e "${YELLOW}Running: ${test_name}${NC}"
    echo -e "Description: ${description}"
    echo "Command: ${test_command}"
    echo "----------------------------------------"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if eval "${test_command}"; then
        echo -e "${GREEN}✅ PASSED: ${test_name}${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}❌ FAILED: ${test_name}${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    echo ""
}

# Change to build directory
cd "$(dirname "$0")/build" || {
    echo -e "${RED}Error: Could not change to build directory${NC}"
    exit 1
}

# Ensure all test runners are built
echo -e "${BLUE}Building test runners...${NC}"
# Build only the working test runners, ignore legacy MOS6510 tests
make fam65xx_cpp_klaus_test_runner fam65xx_cpp_lorenz_test_runner fam65xx_cpp_65c02_test_runner -j4 || {
    echo -e "${RED}Error: Failed to build test runners${NC}"
    exit 1
}
echo -e "${GREEN}Build complete!${NC}"
echo ""

# 1. Klaus Dormann 6502 Functional Test
run_test "Klaus 6502 Functional" \
         "./fam65xx_cpp_klaus_test_runner -f" \
         "Klaus Dormann 6502 functional test validation"

# 2. Klaus Dormann 6502 All Tests  
run_test "Klaus 6502 Complete" \
         "./fam65xx_cpp_klaus_test_runner -a" \
         "Klaus Dormann complete 6502 test suite"

# 3. Wolfgang Lorenz Test Suite (if implemented)
if [ -f "./fam65xx_cpp_lorenz_test_runner" ]; then
    run_test "Lorenz Timing Tests" \
             "./fam65xx_cpp_lorenz_test_runner --quick" \
             "Wolfgang Lorenz cycle-accurate timing validation"
fi

# 4. 65C02 CMOS Feature Tests
run_test "65C02 CMOS Features" \
         "./fam65xx_cpp_65c02_test_runner -a" \
         "Comprehensive 65C02 CMOS feature validation"

# 5. 65C02 New Instructions
run_test "65C02 New Instructions" \
         "./fam65xx_cpp_65c02_test_runner bra_test && ./fam65xx_cpp_65c02_test_runner phx_phy_test" \
         "65C02 new instruction validation (BRA, PHX/PHY, etc.)"

# 6. 65C02 Enhanced Addressing
run_test "65C02 Enhanced Addressing" \
         "./fam65xx_cpp_65c02_test_runner zp_indirect && ./fam65xx_cpp_65c02_test_runner abs_idx_indirect" \
         "65C02 enhanced addressing mode validation"

# 7. 65C02 Hardware Features
run_test "65C02 Hardware Features" \
         "./fam65xx_cpp_65c02_test_runner wai_test && ./fam65xx_cpp_65c02_test_runner stp_test" \
         "65C02 hardware feature validation (WAI, STP, etc.)"

# 8. CMOS Bug Fixes
run_test "CMOS Bug Fixes" \
         "./fam65xx_cpp_65c02_test_runner decimal_cmos && ./fam65xx_cpp_65c02_test_runner jmp_indirect_fix" \
         "CMOS bug fix validation (decimal mode, JMP indirect, etc.)"

# 9. NMOS vs CMOS Comparison
run_test "NMOS vs CMOS Comparison" \
         "./fam65xx_cpp_65c02_test_runner -n nmos_vs_cmos" \
         "Behavioral difference validation between NMOS and CMOS"

# Performance benchmark (optional)
if [ "$1" = "--benchmark" ]; then
    echo -e "${BLUE}Running performance benchmarks...${NC}"
    run_test "Performance Benchmark" \
             "time ./fam65xx_cpp_klaus_test_runner -f" \
             "CPU performance benchmark using Klaus test"
fi

# Final results summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}    Test Results Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Total tests run: ${TOTAL_TESTS}"
echo -e "${GREEN}Tests passed: ${PASSED_TESTS}${NC}"
echo -e "${RED}Tests failed: ${FAILED_TESTS}${NC}"

if [ ${FAILED_TESTS} -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL TESTS PASSED! 🎉${NC}"
    echo -e "${GREEN}fam65xx_cpp CPU implementation is validated!${NC}"
    exit 0
else
    echo -e "${RED}⚠️ SOME TESTS FAILED ⚠️${NC}"
    echo -e "${YELLOW}Check individual test outputs above for details.${NC}"
    echo -e "${YELLOW}Review the comprehensive test results document:${NC}"
    echo -e "${YELLOW}  code/c/tests/65C02_TEST_COMPREHENSIVE_RESULTS.md${NC}"
    exit 1
fi