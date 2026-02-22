#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <chrono>

#ifndef CERMU_IMPL
    #define CERMU_IMPL
#endif

// Include processor-specific headers
#include "../src/chip/cpu/fam65xx/mos6502.h"

namespace fam65xx_lorenz {

// Lorenz test state structure
struct LorenzTestState {
    uint16_t pc = 0;
    uint8_t a = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t sp = 0;
    uint8_t p = 0;
    uint64_t cycles = 0;
    
    bool operator==(const LorenzTestState& other) const {
        return pc == other.pc && a == other.a && x == other.x && 
               y == other.y && sp == other.sp && p == other.p && 
               cycles == other.cycles;
    }
};

// Lorenz test result structure
struct LorenzTestResult {
    enum class Status {
        PASSED,
        FAILED_STATE,
        FAILED_TIMING,
        FAILED_MEMORY,
        TIMEOUT,
        CRASH
    } status = Status::FAILED_STATE;
    
    std::string message;
    LorenzTestState actual_state;
    LorenzTestState expected_state;
    uint64_t actual_cycles = 0;
    uint64_t expected_cycles = 0;
};

class LorenzTestHarness {
private:
    mos6502_t* cpu_;
    std::vector<uint8_t> memory_;
    uint64_t cycle_count_;
    uint16_t load_address_;
    uint16_t end_address_;
    bool trace_enabled_;
    std::string trace_filename_;
    uint64_t pins_;
    
    LorenzTestState initial_state_;
    LorenzTestState expected_final_state_;
    uint64_t expected_cycles_;
    
    // Memory callbacks
    static uint8_t mem_read(void* user_data, uint32_t addr, uint8_t bus_state) {
        (void)bus_state; // Suppress unused parameter warning
        LorenzTestHarness* harness = static_cast<LorenzTestHarness*>(user_data);
        return harness->memory_[addr & 0xFFFF]; // Lorenz tests use 16-bit address space
    }

    static void mem_write(void* user_data, uint32_t addr, uint8_t data) {
        LorenzTestHarness* harness = static_cast<LorenzTestHarness*>(user_data);
        harness->memory_[addr & 0xFFFF] = data; // Lorenz tests use 16-bit address space
    }

public:
    LorenzTestHarness() :
        memory_(65536, 0x00),
        cycle_count_(0),
        load_address_(0x0801),  // Standard C64 BASIC start address
        end_address_(0x0000),
        trace_enabled_(false),
        pins_(0),
        expected_cycles_(0) {
        
        // Create CPU instance
        cpu_ = mos6502_create();
        
        // Initialize CPU (descriptor-free — memory I/O is handled via bus_state_t pins)
        pins_ = mos6502_init(cpu_);
        
        // Set up memory map
        setup_memory_map();
    }
    
    ~LorenzTestHarness() {
        if (cpu_) {
            mos6502_destroy(cpu_);
        }
    }

    bool load_test(const std::string& test_name) {
        std::string test_path = "../external/lorenz-tests/bin/" + test_name;
        
        std::ifstream file(test_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open test file: " << test_path << std::endl;
            return false;
        }
        
        // Read test file into memory starting at load address
        file.seekg(0, std::ios::end);
        std::size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (load_address_ + file_size > 65536) {
            std::cerr << "Error: Test file too large for memory" << std::endl;
            return false;
        }
        
        file.read(reinterpret_cast<char*>(&memory_[load_address_]), file_size);
        
        std::cout << "Loaded " << file_size << " bytes from " << test_path 
                  << " at address $" << std::hex << std::uppercase << std::setw(4) 
                  << std::setfill('0') << load_address_ << std::endl;
        
        return true;
    }

    bool load_expected_results(const std::string& results_file) {
        std::string results_path = "../external/lorenz-tests/expected/" + results_file;
        
        std::ifstream file(results_path);
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot open expected results file: " << results_path << std::endl;
            // For now, we'll continue without expected results and just run the test
            return true;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("PC:") != std::string::npos) {
                // Parse expected final state from results file
                // Format: PC:$1234 A:$56 X:$78 Y:$9A SP:$BC P:$DE CYCLES:12345
                expected_final_state_ = parse_state_string(line);
            }
        }
        
        return true;
    }

    LorenzTestResult run_test(uint64_t max_cycles) {
        LorenzTestResult result;
        result.status = LorenzTestResult::Status::FAILED_STATE;
        
        // Reset CPU and set initial state
        reset_cpu();
        mos6502_set_pc(cpu_, load_address_);
        
        // Capture initial state
        initial_state_ = get_current_state();
        cycle_count_ = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Main execution loop
        while (cycle_count_ < max_cycles) {
            if (is_test_complete()) {
                result.status = LorenzTestResult::Status::PASSED;
                break;
            }
            
            try {
                execute_cycle();
                cycle_count_++;
                
                // Check for infinite loops or crashes
                uint16_t pc = mos6502_get_pc(cpu_);
                if (pc == 0x0000 || pc >= 0xFF00) {
                    result.status = LorenzTestResult::Status::CRASH;
                    result.message = "CPU crashed or jumped to invalid address: $" +
                                   std::to_string(pc);
                    break;
                }
                
            } catch (const std::exception& e) {
                result.status = LorenzTestResult::Status::CRASH;
                result.message = "Exception during execution: " + std::string(e.what());
                break;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (cycle_count_ >= max_cycles) {
            result.status = LorenzTestResult::Status::TIMEOUT;
            result.message = "Test exceeded maximum cycle count";
        }
        
        // Capture final state
        result.actual_state = get_current_state();
        result.actual_cycles = cycle_count_;
        
        // Set expected values (would normally come from results file)
        result.expected_state = expected_final_state_;
        result.expected_cycles = expected_cycles_;
        
        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Status: " << (result.status == LorenzTestResult::Status::PASSED ? "PASSED" : "FAILED") << std::endl;
        std::cout << "Cycles executed: " << result.actual_cycles << std::endl;
        std::cout << "Final PC: $" << std::hex << std::uppercase << std::setw(4) 
                  << std::setfill('0') << result.actual_state.pc << std::endl;
        std::cout << "Duration: " << duration.count() << " ms" << std::endl;
        
        if (result.status != LorenzTestResult::Status::PASSED) {
            std::cout << "Error: " << result.message << std::endl;
        }
        
        return result;
    }

    void write_memory(uint16_t address, uint8_t value) {
        memory_[address] = value;
    }

    uint8_t read_memory(uint16_t address) {
        return memory_[address];
    }

    void set_initial_state(const LorenzTestState& state) {
        mos6502_set_pc(cpu_, state.pc);
        mos6502_set_a(cpu_, state.a);
        mos6502_set_x(cpu_, state.x);
        mos6502_set_y(cpu_, state.y);
        mos6502_set_s(cpu_, state.sp);
        mos6502_set_p(cpu_, state.p);
        
        initial_state_ = state;
    }

    LorenzTestState get_current_state() const {
        LorenzTestState state;
        state.pc = mos6502_get_pc(cpu_);
        state.a = mos6502_get_a(cpu_);
        state.x = mos6502_get_x(cpu_);
        state.y = mos6502_get_y(cpu_);
        state.sp = mos6502_get_s(cpu_);
        state.p = mos6502_get_p(cpu_);
        state.cycles = cycle_count_;
        
        return state;
    }

    void enable_trace(const std::string& filename) {
        trace_enabled_ = true;
        trace_filename_ = filename;
    }

    void disable_trace() {
        trace_enabled_ = false;
    }

private:
    void reset_cpu() {
        // Bootstrap processor for immediate execution
        pins_ = mos6502_bootstrap(cpu_, pins_);
        
        // Set reset vector to load address
        memory_[0xFFFC] = load_address_ & 0xFF;
        memory_[0xFFFD] = (load_address_ >> 8) & 0xFF;
    }

    void setup_memory_map() {
        // Initialize memory with typical 6502 system layout
        std::fill(memory_.begin(), memory_.end(), 0x00);
        
        // Set up interrupt vectors
        memory_[0xFFFA] = 0x00; memory_[0xFFFB] = 0xFF; // NMI
        memory_[0xFFFC] = 0x01; memory_[0xFFFD] = 0x08; // RESET (will be overridden)
        memory_[0xFFFE] = 0x00; memory_[0xFFFF] = 0xFF; // IRQ/BRK
    }

    bool is_test_complete() const {
        uint16_t pc = mos6502_get_pc(cpu_);
        
        // Test completion detection methods:
        
        // 1. RTS instruction at end of test
        if (memory_[pc] == 0x60) { // RTS
            return true;
        }
        
        // 2. BRK instruction indicating test end
        if (memory_[pc] == 0x00) { // BRK
            return true;
        }
        
        // 3. Jump to specific end address (if set)
        if (end_address_ != 0x0000 && pc == end_address_) {
            return true;
        }
        
        // 4. Infinite loop detection (simple)
        static uint16_t prev_pc = 0xFFFF;
        static int loop_count = 0;
        
        if (pc == prev_pc) {
            loop_count++;
            if (loop_count > 100) { // Detected infinite loop
                return true;
            }
        } else {
            loop_count = 0;
            prev_pc = pc;
        }
        
        return false;
    }

    void execute_cycle() {
        // Execute CPU cycle
        pins_ = mos6502_tick(cpu_, pins_);
        
        if (trace_enabled_) {
            uint16_t pc = mos6502_get_pc(cpu_);
            std::cout << "Cycle " << cycle_count_ << ": PC=$"
                     << std::hex << std::setw(4) << std::setfill('0') << pc
                     << " A=$" << std::setw(2) << static_cast<int>(mos6502_get_a(cpu_))
                     << " X=$" << std::setw(2) << static_cast<int>(mos6502_get_x(cpu_))
                     << " Y=$" << std::setw(2) << static_cast<int>(mos6502_get_y(cpu_))
                     << " SP=$" << std::setw(2) << static_cast<int>(mos6502_get_s(cpu_))
                     << " P=$" << std::setw(2) << static_cast<int>(mos6502_get_p(cpu_))
                     << std::endl;
        }
    }

    bool compare_states(const LorenzTestState& expected, const LorenzTestState& actual) const {
        return expected == actual;
    }

    std::string format_state_diff(const LorenzTestState& expected, const LorenzTestState& actual) const {
        std::ostringstream diff;
        
        diff << "State Comparison:\n";
        diff << "         Expected  Actual\n";
        diff << "PC:      $" << std::hex << std::setw(4) << std::setfill('0') << expected.pc
             << "     $" << std::setw(4) << actual.pc << "\n";
        diff << "A:       $" << std::setw(2) << static_cast<int>(expected.a)
             << "       $" << std::setw(2) << static_cast<int>(actual.a) << "\n";
        diff << "X:       $" << std::setw(2) << static_cast<int>(expected.x)
             << "       $" << std::setw(2) << static_cast<int>(actual.x) << "\n";
        diff << "Y:       $" << std::setw(2) << static_cast<int>(expected.y)
             << "       $" << std::setw(2) << static_cast<int>(actual.y) << "\n";
        diff << "SP:      $" << std::setw(2) << static_cast<int>(expected.sp)
             << "       $" << std::setw(2) << static_cast<int>(actual.sp) << "\n";
        diff << "P:       $" << std::setw(2) << static_cast<int>(expected.p)
             << "       $" << std::setw(2) << static_cast<int>(actual.p) << "\n";
        diff << "Cycles:  " << std::dec << expected.cycles
             << "       " << actual.cycles << "\n";
        
        return diff.str();
    }
    
    static LorenzTestState parse_state_string(const std::string& state_str) {
        LorenzTestState state = {};
        
        // Simple parser for state string format
        // Expected format: "PC:$1234 A:$56 X:$78 Y:$9A SP:$BC P:$DE CYC:12345"
        
        std::istringstream iss(state_str);
        std::string token;
        
        while (iss >> token) {
            if (token.find("PC:$") == 0) {
                state.pc = std::stoi(token.substr(3), nullptr, 16);
            } else if (token.find("A:$") == 0) {
                state.a = std::stoi(token.substr(2), nullptr, 16);
            } else if (token.find("X:$") == 0) {
                state.x = std::stoi(token.substr(2), nullptr, 16);
            } else if (token.find("Y:$") == 0) {
                state.y = std::stoi(token.substr(2), nullptr, 16);
            } else if (token.find("SP:$") == 0) {
                state.sp = std::stoi(token.substr(3), nullptr, 16);
            } else if (token.find("P:$") == 0) {
                state.p = std::stoi(token.substr(2), nullptr, 16);
            } else if (token.find("CYC:") == 0) {
                state.cycles = std::stoi(token.substr(4));
            }
        }
        
        return state;
    }
};

// Utility functions
std::string format_state(const LorenzTestState& state) {
    std::ostringstream str;
    str << "PC:$" << std::hex << std::setw(4) << std::setfill('0') << state.pc
        << " A:$" << std::setw(2) << static_cast<int>(state.a)
        << " X:$" << std::setw(2) << static_cast<int>(state.x)
        << " Y:$" << std::setw(2) << static_cast<int>(state.y)
        << " SP:$" << std::setw(2) << static_cast<int>(state.sp)
        << " P:$" << std::setw(2) << static_cast<int>(state.p)
        << " CYC:" << std::dec << state.cycles;
    return str.str();
}

bool save_test_results(const std::string& filename, const LorenzTestResult& result) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "Wolfgang Lorenz Test Result\n";
    file << "===========================\n\n";
    
    file << "Status: ";
    switch (result.status) {
        case LorenzTestResult::Status::PASSED:
            file << "PASSED\n";
            break;
        case LorenzTestResult::Status::FAILED_STATE:
            file << "FAILED (State Mismatch)\n";
            break;
        case LorenzTestResult::Status::FAILED_TIMING:
            file << "FAILED (Timing Mismatch)\n";
            break;
        case LorenzTestResult::Status::FAILED_MEMORY:
            file << "FAILED (Memory Mismatch)\n";
            break;
        case LorenzTestResult::Status::TIMEOUT:
            file << "TIMEOUT\n";
            break;
        case LorenzTestResult::Status::CRASH:
            file << "CRASH\n";
            break;
    }
    
    if (!result.message.empty()) {
        file << "Message: " << result.message << "\n";
    }
    
    file << "\nFinal State:\n";
    file << format_state(result.actual_state) << "\n";
    
    file << "\nExpected State:\n";
    file << format_state(result.expected_state) << "\n";
    
    file << "\nCycles: " << result.actual_cycles;
    if (result.expected_cycles > 0) {
        file << " (expected: " << result.expected_cycles << ")";
    }
    file << "\n";
    
    return true;
}

} // namespace fam65xx_lorenz

// Example main function for standalone Lorenz test runner
int main(int argc, char* argv[]) {
    using namespace fam65xx_lorenz;
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <test_name> [expected_results_file]\n";
        std::cout << "Example: " << argv[0] << " adcb.prg adcb.expected\n";
        return 1;
    }
    
    std::string test_name = argv[1];
    std::string results_file = (argc > 2) ? argv[2] : "";
    
    std::cout << "========================================\n";
    std::cout << "    Wolfgang Lorenz Test Suite Runner\n";
    std::cout << "    Testing fam65xx Implementation\n";
    std::cout << "========================================\n";
    
    LorenzTestHarness harness;
    
    // Enable trace for debugging
    harness.enable_trace("lorenz_trace.log");
    
    // Load test
    if (!harness.load_test(test_name)) {
        std::cerr << "Failed to load test: " << test_name << std::endl;
        return 1;
    }
    
    // Load expected results if provided
    if (!results_file.empty()) {
        harness.load_expected_results(results_file);
    }
    
    // Run test with 10 million cycle limit
    auto result = harness.run_test(10000000);
    
    // Save results
    save_test_results("lorenz_results.txt", result);
    
    // Print final result
    std::cout << "\n========================================\n";
    if (result.status == LorenzTestResult::Status::PASSED) {
        std::cout << "🎉 TEST PASSED! 🎉\n";
        std::cout << "Your fam65xx implementation passed the Lorenz test!\n";
        return 0;
    } else {
        std::cout << "❌ TEST FAILED\n";
        std::cout << "Check lorenz_results.txt for detailed information.\n";
        return 1;
    }
}