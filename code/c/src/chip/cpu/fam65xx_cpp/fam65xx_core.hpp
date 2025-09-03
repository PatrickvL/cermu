#pragma once

#include <cstdint>

namespace fam65xx {

// Forward declarations
class CPU6502Core;

// CPU Variant types - these match the C interface expectations
enum class CPUVariant {
    MOS6502,
    MOS6510, 
    MOS6507,
    MOS65C02,
    MOS65C816
};

// CPU state structure (compatible with C interface)
struct CPURegisters {
    uint16_t pc;        // Program counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X index register
    uint8_t y;          // Y index register
    uint8_t sp;         // Stack pointer
    uint8_t flags;      // Processor status flags
};

// Memory bus interface (64-bit address space for future expansion)
struct BusInterface {
    void* context;
    uint8_t (*read)(void* context, uint64_t address);
    void (*write)(void* context, uint64_t address, uint8_t value);
};

// IO port callback interface (for 6510 banking)
struct IOInterface {
    void* context;
    void (*port_changed)(void* context, uint8_t port, uint8_t value);
};

// Control lines interface
struct ControlInterface {
    void* context;
    uint64_t (*get_lines)(void* context);
    void (*set_lines)(void* context, uint64_t lines);
};

// Main CPU core class
class CPU6502Core {
public:
    // Construction/Destruction
    static CPU6502Core* create(CPUVariant variant = CPUVariant::MOS6502);
    void destroy();

    // Core control
    void reset();
    bool step();  // Execute one instruction, return false if error
    void set_pc(uint16_t pc);

    // Register access
    CPURegisters get_registers() const;
    void set_registers(const CPURegisters& regs);

    // Interface attachment
    void attach_bus(const BusInterface& bus);
    void attach_io(const IOInterface& io);     // Optional, for 6510 banking
    void attach_control(const ControlInterface& ctrl); // Optional

    // Cycle counting
    uint64_t get_cycle_count() const;
    void reset_cycle_count();

private:
    // Private implementation details
    CPU6502Core(CPUVariant variant);
    ~CPU6502Core();
    
    class Impl;
    Impl* pImpl;
    
    // Non-copyable
    CPU6502Core(const CPU6502Core&) = delete;
    CPU6502Core& operator=(const CPU6502Core&) = delete;
};

} // namespace fam65xx