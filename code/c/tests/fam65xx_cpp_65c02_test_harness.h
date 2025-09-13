#pragma once

#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace fam65xx_cpp {

// 65C02 CMOS test state structure
struct CMOS65C02TestState {
    uint16_t pc;        // Program counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X register
    uint8_t y;          // Y register
    uint8_t sp;         // Stack pointer
    uint8_t p;          // Processor status
    uint64_t cycles;    // Cycle count
    
    // CMOS-specific state
    bool wai_active;    // WAI instruction state
    bool stp_active;    // STP instruction state
    bool be_enabled;    // Bus enable state
    
    // Memory state for validation
    std::vector<std::pair<uint16_t, uint8_t>> memory_changes;
    
    bool operator==(const CMOS65C02TestState& other) const {
        return pc == other.pc && a == other.a && x == other.x && 
               y == other.y && sp == other.sp && p == other.p;
    }
};

// 65C02 test result structure
struct CMOS65C02TestResult {
    enum class Status {
        PASSED,
        FAILED_STATE,
        FAILED_TIMING,
        FAILED_MEMORY,
        FAILED_CMOS_FEATURE,
        TIMEOUT,
        CRASH
    };
    
    Status status;
    std::string message;
    CMOS65C02TestState expected_state;
    CMOS65C02TestState actual_state;
    uint64_t expected_cycles;
    uint64_t actual_cycles;
    
    // CMOS-specific validation results
    bool decimal_mode_correct;
    bool new_instructions_working;
    bool enhanced_addressing_working;
    bool hardware_features_working;
};

class CMOS65C02TestHarness {
public:
    CMOS65C02TestHarness();
    ~CMOS65C02TestHarness();
    
    // Load test ROM and expected results
    bool load_test(const std::string& test_name);
    bool load_expected_results(const std::string& results_file);
    
    // Execute test with CMOS-specific validation
    CMOS65C02TestResult run_test(uint64_t max_cycles = 100000);
    
    // CMOS-specific test methods
    bool test_new_instructions();
    bool test_enhanced_addressing_modes();
    bool test_decimal_mode_fixes();
    bool test_hardware_features();
    bool test_timing_improvements();
    
    // Individual instruction tests
    bool test_bra_instruction();
    bool test_phx_phy_instructions();
    bool test_plx_ply_instructions();
    bool test_stz_instruction();
    bool test_trb_tsb_instructions();
    bool test_wai_instruction();
    bool test_stp_instruction();
    bool test_bit_enhancements();
    
    // Addressing mode tests
    bool test_zp_indirect();
    bool test_abs_indexed_indirect();
    bool test_jmp_abs_indexed_indirect();
    
    // Bug fix validation
    bool test_decimal_mode_nz_flags();
    bool test_jmp_indirect_page_boundary();
    bool test_rmw_instruction_fixes();
    
    // Hardware feature tests
    bool test_wai_behavior();
    bool test_stp_behavior();
    bool test_be_pin_control();
    bool test_enhanced_rdy_behavior();
    
    // Memory interface for CPU
    void write_memory(uint16_t address, uint8_t value);
    uint8_t read_memory(uint16_t address);
    
    // CPU state access
    void set_initial_state(const CMOS65C02TestState& state);
    CMOS65C02TestState get_current_state() const;
    
    // Debugging support
    void enable_trace(const std::string& filename);
    void disable_trace();
    
    // Comparative testing (NMOS vs CMOS)
    struct ComparisonResult {
        bool behavior_differs;
        std::string difference_description;
        CMOS65C02TestState nmos_result;
        CMOS65C02TestState cmos_result;
    };
    
    ComparisonResult compare_with_nmos(const std::string& test_code);
    
private:
    // CPU instances for comparison
    std::unique_ptr<fam65xx<config_65c02>> cmos_cpu_;     // CMOS 65C02
    std::unique_ptr<fam65xx<config_6502>> nmos_cpu_;      // NMOS 6502
    
    // Memory system
    std::vector<uint8_t> memory_;
    
    // Test state
    CMOS65C02TestState initial_state_;
    CMOS65C02TestState expected_final_state_;
    uint64_t expected_cycles_;
    
    // Test execution
    uint64_t cycle_count_;
    uint16_t load_address_;
    uint16_t end_address_;
    
    // CMOS-specific state tracking
    bool wai_executed_;
    bool stp_executed_;
    bool be_pin_tested_;
    
    // Tracing
    bool trace_enabled_;
    std::string trace_filename_;
    
    // Helper methods
    void reset_cpu();
    void reset_both_cpus();
    void setup_memory_map();
    bool is_test_complete() const;
    void capture_memory_changes();
    
    // CPU cycle execution
    void execute_cycle();
    void execute_comparative_cycle();
    
    // State comparison
    bool compare_states(const CMOS65C02TestState& expected, 
                       const CMOS65C02TestState& actual) const;
    std::string format_state_diff(const CMOS65C02TestState& expected, 
                                 const CMOS65C02TestState& actual) const;
    
    // CMOS-specific validation helpers
    bool validate_decimal_mode_behavior(uint8_t operation, uint8_t operand1, uint8_t operand2);
    bool validate_new_instruction(uint8_t opcode, const std::vector<uint8_t>& operands);
    bool validate_addressing_mode(uint8_t opcode, const std::string& mode_name);
    bool validate_hardware_feature(const std::string& feature_name);
    
    // Test program generators
    std::vector<uint8_t> generate_bra_test();
    std::vector<uint8_t> generate_stack_test();
    std::vector<uint8_t> generate_stz_test();
    std::vector<uint8_t> generate_bit_manipulation_test();
    std::vector<uint8_t> generate_decimal_mode_test();
    std::vector<uint8_t> generate_addressing_mode_test();
    
    // Execution helpers
    void load_test_program(const std::vector<uint8_t>& program);
    bool execute_until_completion(uint64_t max_cycles);
    void simulate_interrupt(uint16_t vector);
    void simulate_hardware_pin(const std::string& pin_name, bool state);
};

// Utility functions for 65C02 testing
std::string format_cmos_state(const CMOS65C02TestState& state);
CMOS65C02TestState parse_cmos_state_string(const std::string& state_str);
bool save_cmos_test_results(const std::string& filename, const CMOS65C02TestResult& result);

// CMOS feature detection helpers
bool is_65c02_instruction(uint8_t opcode);
bool is_enhanced_addressing_mode(uint8_t opcode);
bool requires_cmos_behavior(uint8_t opcode);

// Test program validation
bool validate_65c02_test_program(const std::vector<uint8_t>& program);
std::vector<std::string> analyze_65c02_features_used(const std::vector<uint8_t>& program);

} // namespace fam65xx_cpp