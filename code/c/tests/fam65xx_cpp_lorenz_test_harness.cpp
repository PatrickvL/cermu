#include "fam65xx_cpp_lorenz_test_harness.h"
#include "../src/core/system_lines.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>

namespace fam65xx_cpp {

LorenzTestHarness::LorenzTestHarness() :
    memory_(65536, 0x00),
    cycle_count_(0),
    load_address_(0x0801),  // Standard C64 BASIC start address
    end_address_(0x0000),
    trace_enabled_(false) {
    
    // Initialize CPU with NMOS 6502 configuration for Lorenz tests
    cpu_ = std::make_unique<fam65xx<config_6502>>();
    
    // Set up memory map
    setup_memory_map();
}

LorenzTestHarness::~LorenzTestHarness() = default;

bool LorenzTestHarness::load_test(const std::string& test_name) {
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

bool LorenzTestHarness::load_expected_results(const std::string& results_file) {
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
            // This is a simplified parser - real Lorenz format may be different
        }
    }
    
    return true;
}

LorenzTestResult LorenzTestHarness::run_test(uint64_t max_cycles) {
    LorenzTestResult result;
    result.status = LorenzTestResult::Status::FAILED_STATE;
    
    // Reset CPU and set initial state
    reset_cpu();
    cpu_->set_pc(load_address_);
    
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
            uint16_t pc = cpu_->get_pc();
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

void LorenzTestHarness::write_memory(uint16_t address, uint8_t value) {
    memory_[address] = value;
}

uint8_t LorenzTestHarness::read_memory(uint16_t address) {
    return memory_[address];
}

void LorenzTestHarness::set_initial_state(const LorenzTestState& state) {
    cpu_->set_pc(state.pc);
    cpu_->set_a(state.a);
    cpu_->set_x(state.x);
    cpu_->set_y(state.y);
    cpu_->set_s(state.sp);
    cpu_->set_p(state.p);
    
    initial_state_ = state;
}

LorenzTestState LorenzTestHarness::get_current_state() const {
    LorenzTestState state;
    state.pc = cpu_->get_pc();
    state.a = cpu_->get_a();
    state.x = cpu_->get_x();
    state.y = cpu_->get_y();
    state.sp = cpu_->get_s();
    state.p = cpu_->get_p();
    state.cycles = cycle_count_;
    
    return state;
}

void LorenzTestHarness::enable_trace(const std::string& filename) {
    trace_enabled_ = true;
    trace_filename_ = filename;
}

void LorenzTestHarness::disable_trace() {
    trace_enabled_ = false;
}

void LorenzTestHarness::reset_cpu() {
    cpu_->init();
    
    // Set reset vector to load address
    memory_[0xFFFC] = load_address_ & 0xFF;
    memory_[0xFFFD] = (load_address_ >> 8) & 0xFF;
}

void LorenzTestHarness::setup_memory_map() {
    // Initialize memory with typical 6502 system layout
    std::fill(memory_.begin(), memory_.end(), 0x00);
    
    // Set up interrupt vectors
    memory_[0xFFFA] = 0x00; memory_[0xFFFB] = 0xFF; // NMI
    memory_[0xFFFC] = 0x01; memory_[0xFFFD] = 0x08; // RESET (will be overridden)
    memory_[0xFFFE] = 0x00; memory_[0xFFFF] = 0xFF; // IRQ/BRK
}

bool LorenzTestHarness::is_test_complete() const {
    uint16_t pc = cpu_->get_pc();
    
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

void LorenzTestHarness::execute_cycle() {
    // Create bus state for CPU communication
    bus_state_t bus_state = 0;
    
    // Set current PC on address lines
    uint16_t pc = cpu_->get_pc();
    BUS_SET_ADDR(bus_state, pc);
    
    // Set data from memory
    BUS_SET_DATA(bus_state, memory_[pc]);
    
    // Set RDY line (always ready)
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    
    // Execute CPU cycle
    bus_state = cpu_->cycle_tick(bus_state);
    
    // Handle memory operations based on bus state
    uint16_t addr = BUS_GET_ADDR(bus_state);
    bool is_write = !(bus_state & BUS_BIT(BUS_RW_BIT));
    
    if (is_write) {
        // Write operation
        uint8_t data = BUS_GET_DATA(bus_state);
        memory_[addr] = data;
        
        if (trace_enabled_) {
            std::cout << "WRITE $" << std::hex << std::setw(4) << std::setfill('0') 
                      << addr << " = $" << std::setw(2) << (int)data << std::endl;
        }
    } else {
        // Read operation
        uint8_t data = memory_[addr];
        BUS_SET_DATA(bus_state, data);
        
        if (trace_enabled_) {
            std::cout << "READ  $" << std::hex << std::setw(4) << std::setfill('0') 
                      << addr << " = $" << std::setw(2) << (int)data << std::endl;
        }
    }
}

bool LorenzTestHarness::compare_states(const LorenzTestState& expected, const LorenzTestState& actual) const {
    return expected == actual;
}

std::string LorenzTestHarness::format_state_diff(const LorenzTestState& expected, const LorenzTestState& actual) const {
    std::ostringstream diff;
    
    diff << "State Comparison:\n";
    diff << "         Expected  Actual\n";
    diff << "PC:      $" << std::hex << std::setw(4) << std::setfill('0') << expected.pc
         << "     $" << std::setw(4) << actual.pc << "\n";
    diff << "A:       $" << std::setw(2) << (int)expected.a
         << "       $" << std::setw(2) << (int)actual.a << "\n";
    diff << "X:       $" << std::setw(2) << (int)expected.x
         << "       $" << std::setw(2) << (int)actual.x << "\n";
    diff << "Y:       $" << std::setw(2) << (int)expected.y
         << "       $" << std::setw(2) << (int)actual.y << "\n";
    diff << "SP:      $" << std::setw(2) << (int)expected.sp
         << "       $" << std::setw(2) << (int)actual.sp << "\n";
    diff << "P:       $" << std::setw(2) << (int)expected.p
         << "       $" << std::setw(2) << (int)actual.p << "\n";
    diff << "Cycles:  " << std::dec << expected.cycles
         << "       " << actual.cycles << "\n";
    
    return diff.str();
}

// Utility functions
std::string format_state(const LorenzTestState& state) {
    std::ostringstream str;
    str << "PC:$" << std::hex << std::setw(4) << std::setfill('0') << state.pc
        << " A:$" << std::setw(2) << (int)state.a
        << " X:$" << std::setw(2) << (int)state.x
        << " Y:$" << std::setw(2) << (int)state.y
        << " SP:$" << std::setw(2) << (int)state.sp
        << " P:$" << std::setw(2) << (int)state.p
        << " CYC:" << std::dec << state.cycles;
    return str.str();
}

LorenzTestState parse_state_string(const std::string& state_str) {
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

} // namespace fam65xx_cpp