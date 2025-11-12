#!/bin/bash

# test_all_cores.sh - Comprehensive 65xx Processor Testing Script
# 
# This script provides comprehensive testing capabilities for all supported
# 65xx processor variants with trait-based implementations.
#
# Features:
# - Test single opcode across all processor cores
# - Test all opcodes for all cores (full regression)
# - Individual core testing
# - Performance benchmarking
# - Detailed failure analysis

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_RUNNER="./fam65xx_processor_tests_runner"
PROCESSOR_TESTS_DIR="tests/processor_tests"

# Supported processors with their test directories
declare -a PROCESSORS=("6502" "nes6502" "wdc65c02" "rockwell65c02" "synertek65c02" "65816")
declare -A PROCESSOR_NAMES=(
    ["6502"]="MOS 6502"
    ["nes6502"]="NES 6502 (Ricoh 2A03/2A07)"
    ["wdc65c02"]="WDC 65C02 (W65C02S)"
    ["rockwell65c02"]="Rockwell 65C02"
    ["synertek65c02"]="Synertek 65C02"
    ["65816"]="WDC 65C816 (Emulation Mode)"
)

# Statistics tracking
declare -A TOTAL_TESTS=()
declare -A PASSED_TESTS=()
declare -A FAILED_TESTS=()
declare -A EXECUTION_TIMES=()

# Utility functions
log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

log_header() {
    echo -e "${PURPLE}$1${NC}"
}

log_separator() {
    echo -e "${CYAN}=============================================${NC}"
}

# Build test runner (always build to ensure latest changes)
build_test_runner() {
    log_info "Building test runner with latest changes..."
    
    # Check if make is available and Makefile exists
    if command -v make >/dev/null 2>&1 && [[ -f "Makefile" ]]; then
        log_info "Using make to build test runner..."
        make fam65xx_processor_tests_runner
        if [[ $? -eq 0 ]]; then
            log_success "Test runner built successfully via make"
            return
        else
            log_warning "Make failed, falling back to direct compilation..."
        fi
    fi
    
    # Fallback to direct g++ compilation
    log_info "Compiling test runner directly..."
    g++ -std=c++17 -O2 -pthread \
        -I src/chip/cpu/fam65xx \
        -I src/core \
        -I tests \
        tests/fam65xx_processor_tests_runner.cpp \
        tests/json_parser.cpp \
        -o fam65xx_processor_tests_runner
    
    if [[ $? -eq 0 ]]; then
        log_success "Test runner built successfully"
    else
        log_error "Failed to build test runner"
        exit 1
    fi
}

# Check prerequisites and build
check_prerequisites() {
    # Always build to ensure we have the latest code
    build_test_runner
    
    # Verify the executable exists and is executable
    if [[ ! -f "$TEST_RUNNER" ]]; then
        log_error "Test runner still not found after build: $TEST_RUNNER"
        exit 1
    fi
    
    if [[ ! -x "$TEST_RUNNER" ]]; then
        log_info "Making test runner executable..."
        chmod +x "$TEST_RUNNER"
    fi
}

# Get available opcodes for a processor
get_opcodes() {
    local processor=$1
    local test_dir="$PROCESSOR_TESTS_DIR/$processor/v1"
    
    # Special case for 65C816: use emulation-only tests
    if [[ "$processor" == "65816" ]]; then
        test_dir="$PROCESSOR_TESTS_DIR/$processor/v1_emulation_only"
    fi
    
    if [[ ! -d "$test_dir" ]]; then
        log_error "Test directory not found: $test_dir"
        return 1
    fi
    
    # List all JSON files and extract opcode numbers (remove .json extension and .e extension for 65816)
    if [[ "$processor" == "65816" ]]; then
        find "$test_dir" -name "*.e.json" -type f | \
            sed 's/.*\/\([^/]*\)\.e\.json$/\1/' | \
            sort -n
    else
        find "$test_dir" -name "*.json" -type f | \
            sed 's/.*\/\([^/]*\)\.json$/\1/' | \
            sort -n
    fi
}

# Test single opcode across all processors
test_single_opcode() {
    local opcode=$1
    local quiet_mode=${2:-""}
    
    log_header "🔍 TESTING OPCODE 0x$(printf '%02X' $((16#$opcode))) ACROSS ALL PROCESSORS"
    log_separator
    
    local overall_success=true
    local opcode_hex=$(printf '%02X' $((16#$opcode)))
    local opcode_lower=$(echo "$opcode" | tr '[:upper:]' '[:lower:]')
    
    for processor in "${PROCESSORS[@]}"; do
        local test_file="$PROCESSOR_TESTS_DIR/$processor/v1/$opcode_lower.json"
        
        # Special case for 65C816: use emulation-only tests
        if [[ "$processor" == "65816" ]]; then
            test_file="$PROCESSOR_TESTS_DIR/$processor/v1_emulation_only/$opcode_lower.e.json"
        fi
        
        if [[ ! -f "$test_file" ]]; then
            log_warning "${PROCESSOR_NAMES[$processor]}: Opcode 0x$opcode_hex not available"
            continue
        fi
        
        log_info "Testing ${PROCESSOR_NAMES[$processor]} (opcode 0x$opcode_hex)..."
        
        local start_time=$(date +%s%N)
        
        if $TEST_RUNNER $quiet_mode "$test_file" > /tmp/test_output_${processor}_${opcode}.log 2>&1; then
            local end_time=$(date +%s%N)
            local duration=$(( (end_time - start_time) / 1000000 )) # Convert to milliseconds
            
            # Extract test statistics from output
            local passed=$(grep "Tests passed:" /tmp/test_output_${processor}_${opcode}.log | awk '{print $3}')
            local failed=$(grep "Tests failed:" /tmp/test_output_${processor}_${opcode}.log | awk '{print $3}')
            local performance=$(grep "Performance:" /tmp/test_output_${processor}_${opcode}.log | awk '{print $2}')
            
            log_success "${PROCESSOR_NAMES[$processor]}: PASSED (${passed} tests, ${performance} tests/sec, ${duration}ms)"
        else
            log_error "${PROCESSOR_NAMES[$processor]}: FAILED"
            if [[ "$quiet_mode" != "-q" ]]; then
                echo "--- Failure Details ---"
                tail -20 /tmp/test_output_${processor}_${opcode}.log
                echo "--- End Details ---"
            fi
            overall_success=false
        fi
    done
    
    log_separator
    if $overall_success; then
        log_success "🎉 OPCODE 0x$opcode_hex: ALL PROCESSORS PASSED"
    else
        log_error "💥 OPCODE 0x$opcode_hex: SOME PROCESSORS FAILED"
        return 1
    fi
}

# Test single processor with all its opcodes
test_single_processor() {
    local processor=$1
    local quick_mode=${2:-false}
    local quiet_mode=""
    
    if [[ "$quick_mode" == "true" ]]; then
        quiet_mode="-q"
    fi
    
    log_header "🖥️  TESTING ALL OPCODES FOR ${PROCESSOR_NAMES[$processor]}"
    log_separator
    
    local test_dir="$PROCESSOR_TESTS_DIR/$processor/v1"
    local opcodes=($(get_opcodes "$processor"))
    local total_opcodes=${#opcodes[@]}
    local passed_opcodes=0
    local failed_opcodes=0
    
    log_info "Found $total_opcodes opcodes for ${PROCESSOR_NAMES[$processor]}"
    
    local overall_start_time=$(date +%s%N)
    
    for opcode in "${opcodes[@]}"; do
        local opcode_lower=$(echo "$opcode" | tr '[:upper:]' '[:lower:]')
        local test_file="$test_dir/$opcode_lower.json"
        local opcode_hex=$(printf '%02X' $((16#$opcode)))
        
        # Special case for 65C816: use emulation-only tests
        if [[ "$processor" == "65816" ]]; then
            test_file="$test_dir/$opcode_lower.e.json"
        fi
        
        if [[ "$quick_mode" != "true" ]]; then
            log_info "Testing opcode 0x$opcode_hex..."
        fi
        
        if $TEST_RUNNER $quiet_mode "$test_file" > /tmp/test_output_${processor}_${opcode}.log 2>&1; then
            ((passed_opcodes++))
            if [[ "$quick_mode" != "true" ]]; then
                log_success "Opcode 0x$opcode_hex: PASSED"
            fi
        else
            ((failed_opcodes++))
            log_error "Opcode 0x$opcode_hex: FAILED"
            if [[ "$quiet_mode" != "-q" ]]; then
                echo "--- Failure Details ---"
                tail -10 /tmp/test_output_${processor}_${opcode}.log
                echo "--- End Details ---"
            fi
        fi
    done
    
    local overall_end_time=$(date +%s%N)
    local total_duration=$(( (overall_end_time - overall_start_time) / 1000000 )) # Convert to milliseconds
    
    # Store statistics
    TOTAL_TESTS[$processor]=$total_opcodes
    PASSED_TESTS[$processor]=$passed_opcodes
    FAILED_TESTS[$processor]=$failed_opcodes
    EXECUTION_TIMES[$processor]=$total_duration
    
    log_separator
    log_header "📊 ${PROCESSOR_NAMES[$processor]} RESULTS"
    echo "Total opcodes: $total_opcodes"
    echo "Passed: $passed_opcodes"
    echo "Failed: $failed_opcodes"
    
    if [[ $total_opcodes -gt 0 ]]; then
        local pass_rate=$(( passed_opcodes * 100 / total_opcodes ))
        echo "Pass rate: ${pass_rate}%"
    fi
    
    echo "Execution time: ${total_duration}ms"
    
    if [[ $failed_opcodes -eq 0 ]]; then
        log_success "🎉 ${PROCESSOR_NAMES[$processor]}: ALL OPCODES PASSED"
        return 0
    else
        log_error "💥 ${PROCESSOR_NAMES[$processor]}: $failed_opcodes OPCODES FAILED"
        return 1
    fi
}

# Test all processors with all opcodes (full regression)
test_all_processors() {
    local quick_mode=${1:-false}
    
    log_header "🚀 FULL REGRESSION: TESTING ALL PROCESSORS WITH ALL OPCODES"
    log_separator
    
    local overall_success=true
    local global_start_time=$(date +%s%N)
    
    for processor in "${PROCESSORS[@]}"; do
        echo
        if ! test_single_processor "$processor" "$quick_mode"; then
            overall_success=false
        fi
    done
    
    local global_end_time=$(date +%s%N)
    local global_duration=$(( (global_end_time - global_start_time) / 1000000 )) # Convert to milliseconds
    
    # Print comprehensive summary
    echo
    log_header "📈 COMPREHENSIVE TEST SUMMARY"
    log_separator
    
    local total_all_tests=0
    local total_all_passed=0
    local total_all_failed=0
    
    printf "%-25s %8s %8s %8s %10s %8s\n" "Processor" "Total" "Passed" "Failed" "Pass Rate" "Time(ms)"
    printf "%-25s %8s %8s %8s %10s %8s\n" "-------------------------" "--------" "--------" "--------" "----------" "--------"
    
    for processor in "${PROCESSORS[@]}"; do
        local total=${TOTAL_TESTS[$processor]:-0}
        local passed=${PASSED_TESTS[$processor]:-0}
        local failed=${FAILED_TESTS[$processor]:-0}
        local time=${EXECUTION_TIMES[$processor]:-0}
        local pass_rate="N/A"
        
        if [[ $total -gt 0 ]]; then
            pass_rate=$(( passed * 100 / total ))"%"
        fi
        
        printf "%-25s %8d %8d %8d %10s %8d\n" "${PROCESSOR_NAMES[$processor]}" "$total" "$passed" "$failed" "$pass_rate" "$time"
        
        total_all_tests=$((total_all_tests + total))
        total_all_passed=$((total_all_passed + passed))
        total_all_failed=$((total_all_failed + failed))
    done
    
    printf "%-25s %8s %8s %8s %10s %8s\n" "-------------------------" "--------" "--------" "--------" "----------" "--------"
    
    local overall_pass_rate="N/A"
    if [[ $total_all_tests -gt 0 ]]; then
        overall_pass_rate=$(( total_all_passed * 100 / total_all_tests ))"%"
    fi
    
    printf "%-25s %8d %8d %8d %10s %8d\n" "TOTAL" "$total_all_tests" "$total_all_passed" "$total_all_failed" "$overall_pass_rate" "$global_duration"
    
    log_separator
    
    if $overall_success; then
        log_success "🎉 FULL REGRESSION: ALL PROCESSORS PASSED ALL TESTS"
        echo "Total tests executed: $total_all_tests"
        echo "Global execution time: ${global_duration}ms"
        local tests_per_second=$(( total_all_tests * 1000 / global_duration ))
        echo "Average performance: ${tests_per_second} tests/second"
    else
        log_error "💥 FULL REGRESSION: SOME TESTS FAILED"
        echo "Failed tests: $total_all_failed out of $total_all_tests"
        return 1
    fi
}

# Show usage information
show_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo
    echo "Commands:"
    echo "  opcode <HEX>     Test single opcode across all processors"
    echo "  processor <NAME> Test all opcodes for single processor"
    echo "  all              Test all opcodes for all processors (full regression)"
    echo "  list-opcodes     List available opcodes for each processor"
    echo "  help             Show this help message"
    echo
    echo "Processors: ${PROCESSORS[*]}"
    echo
    echo "Options:"
    echo "  -q, --quiet      Quiet mode (less verbose output)"
    echo "  --quick          Quick mode for processor/all commands"
    echo
    echo "Examples:"
    echo "  $0 opcode A9              # Test opcode 0xA9 (LDA immediate) on all processors"
    echo "  $0 processor wdc65c02     # Test all opcodes for WDC 65C02"
    echo "  $0 all --quick            # Full regression test (quick mode)"
    echo "  $0 list-opcodes           # Show available opcodes for each processor"
}

# List available opcodes for each processor
list_opcodes() {
    log_header "📋 AVAILABLE OPCODES BY PROCESSOR"
    log_separator
    
    for processor in "${PROCESSORS[@]}"; do
        echo
        log_info "${PROCESSOR_NAMES[$processor]}:"
        local opcodes=($(get_opcodes "$processor"))
        local count=${#opcodes[@]}
        
        if [[ $count -eq 0 ]]; then
            log_warning "No opcodes found"
            continue
        fi
        
        echo "Count: $count opcodes"
        echo -n "Opcodes: "
        
        local line_length=0
        for opcode in "${opcodes[@]}"; do
            local opcode_hex=$(printf '0x%02X' $((16#$opcode)))
            echo -n "$opcode_hex "
            line_length=$((line_length + 5))
            
            # Line wrap every 10 opcodes
            if [[ $((line_length % 50)) -eq 0 ]]; then
                echo
                echo -n "         "
                line_length=9  # Account for indentation
            fi
        done
        echo
    done
}

# Main script execution
main() {
    cd "$SCRIPT_DIR"
    
    # Parse command line arguments
    local command=""
    local quiet_mode=""
    local quick_mode=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -q|--quiet)
                quiet_mode="-q"
                shift
                ;;
            --quick)
                quick_mode=true
                shift
                ;;
            help|--help|-h)
                show_usage
                exit 0
                ;;
            opcode|processor|all|list-opcodes)
                command=$1
                shift
                break
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    if [[ -z "$command" ]]; then
        show_usage
        exit 1
    fi
    
    check_prerequisites
    
    case $command in
        opcode)
            if [[ $# -lt 1 ]]; then
                log_error "Opcode value required"
                echo "Usage: $0 opcode <HEX>"
                exit 1
            fi
            test_single_opcode "$1" "$quiet_mode"
            ;;
        processor)
            if [[ $# -lt 1 ]]; then
                log_error "Processor name required"
                echo "Usage: $0 processor <NAME>"
                echo "Available processors: ${PROCESSORS[*]}"
                exit 1
            fi
            
            local processor=$1
            if [[ ! " ${PROCESSORS[@]} " =~ " ${processor} " ]]; then
                log_error "Unknown processor: $processor"
                echo "Available processors: ${PROCESSORS[*]}"
                exit 1
            fi
            
            test_single_processor "$processor" "$quick_mode"
            ;;
        all)
            test_all_processors "$quick_mode"
            ;;
        list-opcodes)
            list_opcodes
            ;;
        *)
            log_error "Unknown command: $command"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"