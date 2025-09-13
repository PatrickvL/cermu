#pragma once

#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace fam65xx_cpp {

// Wolfgang Lorenz test state structure
struct LorenzTestState {
    uint16_t pc;        // Program counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X register
    uint8_t y;          // Y register
    uint8_t sp;         // Stack pointer
    uint8_t p;          // Processor status
    uint64_t cycles;    // Cycle count
    
    // Memory state for validation
    std::vector<std::pair<uint16_t, uint8_t>> memory_changes;
    
    bool operator==(const LorenzTestState& other) const {
        return pc == other.pc && a == other.a && x == other.x && 
               y == other.y && sp == other.sp && p == other.p;
    }
};

// Test result structure
struct LorenzTestResult {
    enum class Status {
        PASSED,
        FAILED_STATE,
        FAILED_TIMING,
        FAILED_MEMORY,
        TIMEOUT,
        CRASH
    };
    
    Status status;
    std::string message;
    LorenzTestState expected_state;
    LorenzTestState actual_state;
    uint64_t expected_cycles;
    uint64_t actual_cycles;
};

class LorenzTestHarness {
public:
    LorenzTestHarness();
    ~LorenzTestHarness();
    
    // Load test ROM and expected results
    bool load_test(const std::string& test_name);
    bool load_expected_results(const std::string& results_file);
    
    // Execute test with cycle-accurate timing
    LorenzTestResult run_test(uint64_t max_cycles = 100000);
    
    // Memory interface for CPU
    void write_memory(uint16_t address, uint8_t value);
    uint8_t read_memory(uint16_t address);
    
    // CPU state access
    void set_initial_state(const LorenzTestState& state);
    LorenzTestState get_current_state() const;
    
    // Debugging support
    void enable_trace(const std::string& filename);
    void disable_trace();
    
private:
    // CPU instance - using NMOS 6502 configuration for Lorenz tests
    std::unique_ptr<fam65xx<config_6502>> cpu_;
    
    // Memory system
    std::vector<uint8_t> memory_;
    
    // Test state
    LorenzTestState initial_state_;
    LorenzTestState expected_final_state_;
    uint64_t expected_cycles_;
    
    // Test execution
    uint64_t cycle_count_;
    uint16_t load_address_;
    uint16_t end_address_;
    
    // Tracing
    bool trace_enabled_;
    std::string trace_filename_;
    
    // Helper methods
    void reset_cpu();
    void setup_memory_map();
    bool is_test_complete() const;
    void capture_memory_changes();
    
    // CPU cycle execution
    void execute_cycle();
    
    // State comparison
    bool compare_states(const LorenzTestState& expected, const LorenzTestState& actual) const;
    std::string format_state_diff(const LorenzTestState& expected, const LorenzTestState& actual) const;
};

// Utility functions
std::string format_state(const LorenzTestState& state);
LorenzTestState parse_state_string(const std::string& state_str);
bool save_test_results(const std::string& filename, const LorenzTestResult& result);

} // namespace fam65xx_cpp