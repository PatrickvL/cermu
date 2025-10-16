#pragma once
/*
 * mos6510.hpp - MOS 6510 Microprocessor Emulator
 *
 * Enhanced 6502 with I/O port for the Commodore 64.
 * Used in: Commodore 64, Commodore 128 (in 64 mode).
 *
 * Features:
 * - All MOS 6502 features (illegal opcodes, NMOS bugs)
 * - Memory-mapped I/O port at addresses $0000 and $0001
 * - $0000 = Data Direction Register (DDR)
 * - $0001 = I/O Port Data (controls cassette, serial, CHAREN, HIRAM, LORAM)
 * - Compatible with 6502 software
 */

// Include tables for template-based opcode generation
#ifndef AIEMUC_IMPL
    #define AIEMUC_IMPL
#endif

// Include the main modular header (which now includes everything)
#include "fam65xx.hpp"

// Define internal processor-specific opcode table for MOS 6510
DEFINE_PROCESSOR_OPCODE_TABLE_INTERNAL(fam65xx_variants::MOS6510Tag)

#ifdef __cplusplus

namespace fam65xx_cpu {

// MOS 6510 CPU class - 6502 with I/O port
using MOS6510 = fam65xx_variants::CPU<fam65xx_variants::MOS6510Tag>;

// I/O Port Callback System for External Decoupling
struct mos6510_io_callbacks_t {
    // Called when CPU writes to I/O port direction register ($00)
    void (*on_direction_write)(void* user_data, uint8_t value);
    
    // Called when CPU writes to I/O port data register ($01)
    void (*on_data_write)(void* user_data, uint8_t value, uint8_t direction_mask);
    
    // Called when CPU reads from I/O port data register ($01)
    // Should return the current port state (external pins + internal data)
    uint8_t (*on_data_read)(void* user_data, uint8_t internal_data, uint8_t direction_mask);
    
    // User-provided context pointer
    void* user_data;
};

// I/O Port bit definitions for Commodore 64
enum IOPortBits {
    LORAM = 0x01,     // RAM/ROM at $A000-$BFFF and $E000-$FFFF
    HIRAM = 0x02,     // RAM/ROM at $A000-$BFFF and $E000-$FFFF
    CHAREN = 0x04,    // Character ROM at $D000-$DFFF
    CASS_DATA = 0x08, // Cassette data output
    CASS_SWITCH = 0x10, // Cassette switch sense
    CASS_MOTOR = 0x20,  // Cassette motor control
    UNUSED6 = 0x40,     // Unused
    UNUSED7 = 0x80      // Unused
};

// I/O Port state structure
struct IOPortState {
    uint8_t ddr;   // Data Direction Register ($0000)
    uint8_t data;  // Port Data Register ($0001)
    uint8_t external; // External input state
    
    IOPortState() : ddr(0x00), data(0x37), external(0xFF) {}
};

// MOS 6510 with Enhanced I/O Port Management and Callback System
class MOS6510WithIOPort {
private:
    MOS6510 cpu;
    IOPortState io_port;
    
    // Callback system for external decoupling
    mos6510_io_callbacks_t callbacks = {nullptr, nullptr, nullptr, nullptr};
    uint32_t io_access_count = 0;
    
public:
    // CPU interface delegation
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) { return cpu.init(desc); }
    bus_state_t reset(bus_state_t pins) { 
        io_port = IOPortState(); // Reset I/O port
        return cpu.reset(pins); 
    }
    bus_state_t bootstrap(bus_state_t pins) { return cpu.bootstrap(pins); }
    bus_state_t tick(bus_state_t pins) { return cpu.tick(pins); }
    bool opdone() const { return cpu.opdone(); }
    
    // Register access
    void set_a(uint8_t v) { cpu.set_a(v); }
    void set_x(uint8_t v) { cpu.set_x(v); }
    void set_y(uint8_t v) { cpu.set_y(v); }
    void set_s(uint8_t v) { cpu.set_s(v); }
    void set_p(uint8_t v) { cpu.set_p(v); }
    void set_pc(uint16_t v) { cpu.set_pc(v); }
    
    uint8_t a() const { return cpu.a(); }
    uint8_t x() const { return cpu.x(); }
    uint8_t y() const { return cpu.y(); }
    uint8_t s() const { return cpu.s(); }
    uint8_t p() const { return cpu.p(); }
    uint16_t pc() const { return cpu.pc(); }
    
    // I/O Port Callback System
    void set_io_callbacks(const mos6510_io_callbacks_t& cb) {
        callbacks = cb;
    }
    
    void clear_io_callbacks() {
        callbacks = {nullptr, nullptr, nullptr, nullptr};
    }
    
    // I/O Port access with callback integration
    void set_io_ddr(uint8_t value) {
        io_port.ddr = value;
        io_access_count++;
        
        // Notify external system of direction change
        if (callbacks.on_direction_write) {
            callbacks.on_direction_write(callbacks.user_data, value);
        }
    }
    
    void set_io_data(uint8_t value) {
        io_port.data = value;
        io_access_count++;
        
        // Notify external system of data write
        if (callbacks.on_data_write) {
            callbacks.on_data_write(callbacks.user_data, value, io_port.ddr);
        }
    }
    
    void set_io_external(uint8_t value) { io_port.external = value; }
    
    uint8_t get_io_ddr() const { return io_port.ddr; }
    
    uint8_t get_io_data() const {
        // If callback is available, get external pin state
        if (callbacks.on_data_read) {
            return callbacks.on_data_read(callbacks.user_data, io_port.data, io_port.ddr);
        }
        
        // Fallback: Return (output & ddr) | (external & ~ddr)
        return (io_port.data & io_port.ddr) | (io_port.external & ~io_port.ddr);
    }
    
    uint8_t get_io_external() const { return io_port.external; }
    
    // I/O access statistics
    uint32_t get_io_access_count() const { return io_access_count; }
    void reset_io_access_count() { io_access_count = 0; }
    
    // Memory-mapped I/O port access handlers (called by memory system)
    void write_io_register(uint16_t addr, uint8_t value) {
        if (addr == 0x0000) {
            set_io_ddr(value);
        } else if (addr == 0x0001) {
            set_io_data(value);
        }
    }
    
    uint8_t read_io_register(uint16_t addr) {
        if (addr == 0x0000) {
            return get_io_ddr();
        } else if (addr == 0x0001) {
            io_access_count++; // Count reads too
            return get_io_data();
        }
        return 0xFF; // Invalid I/O address
    }
    
    // Memory banking helpers for C64
    bool is_basic_rom_enabled() const { 
        return (get_io_data() & (LORAM | HIRAM)) == (LORAM | HIRAM); 
    }
    bool is_kernal_rom_enabled() const { 
        return (get_io_data() & HIRAM) != 0; 
    }
    bool is_char_rom_enabled() const { 
        return (get_io_data() & CHAREN) == 0; 
    }
    bool is_io_enabled() const { 
        return (get_io_data() & CHAREN) != 0; 
    }
    
    // Direct CPU access
    MOS6510* get_cpu() { return &cpu; }
    const MOS6510* get_cpu() const { return &cpu; }
};

// Convenient creation functions
inline MOS6510 create() {
    return MOS6510{};
}

inline MOS6510WithIOPort create_with_io_port() {
    return MOS6510WithIOPort{};
}

// Initialization with memory callbacks
inline MOS6510 create_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    MOS6510 cpu;
    cpu.init(&desc);
    return cpu;
}

} // namespace fam65xx_cpu

// Global type aliases for convenience
using mos6510_t = fam65xx_cpu::MOS6510;
using mos6510_with_io_t = fam65xx_cpu::MOS6510WithIOPort;

#endif // __cplusplus

// C compatibility wrapper functions
#ifdef __cplusplus
extern "C" {
#endif

// C API for MOS 6510 - wraps the template implementation
typedef struct {
    fam65xx_t impl;
    uint8_t io_ddr;      // I/O Data Direction Register
    uint8_t io_data;     // I/O Port Data
    uint8_t io_external; // External input state
} mos6510_c_t;

// Initialize MOS 6510 CPU
inline uint64_t mos6510_init(mos6510_c_t* cpu, const fam65xx_desc_t* desc) {
    cpu->io_ddr = 0x00;
    cpu->io_data = 0x37;    // Default C64 startup state
    cpu->io_external = 0xFF;
    return fam65xx_init(&cpu->impl, desc);
}

// Reset MOS 6510 CPU
inline uint64_t mos6510_reset(mos6510_c_t* cpu, uint64_t pins) {
    cpu->io_ddr = 0x00;
    cpu->io_data = 0x37;
    cpu->io_external = 0xFF;
    return fam65xx_reset(&cpu->impl, pins);
}

// Execute one tick
inline uint64_t mos6510_tick(mos6510_c_t* cpu, uint64_t pins) {
    return fam65xx_tick(&cpu->impl, pins);
}

// Check if operation is done
inline bool mos6510_opdone(mos6510_c_t* cpu) {
    return fam65xx_opdone(&cpu->impl);
}

// Register access functions
inline uint8_t mos6510_a(mos6510_c_t* cpu) { return fam65xx_a(&cpu->impl); }
inline uint8_t mos6510_x(mos6510_c_t* cpu) { return fam65xx_x(&cpu->impl); }
inline uint8_t mos6510_y(mos6510_c_t* cpu) { return fam65xx_y(&cpu->impl); }
inline uint8_t mos6510_s(mos6510_c_t* cpu) { return fam65xx_s(&cpu->impl); }
inline uint8_t mos6510_p(mos6510_c_t* cpu) { return fam65xx_p(&cpu->impl); }
inline uint16_t mos6510_pc(mos6510_c_t* cpu) { return fam65xx_pc(&cpu->impl); }

inline void mos6510_set_a(mos6510_c_t* cpu, uint8_t v) { fam65xx_set_a(&cpu->impl, v); }
inline void mos6510_set_x(mos6510_c_t* cpu, uint8_t v) { fam65xx_set_x(&cpu->impl, v); }
inline void mos6510_set_y(mos6510_c_t* cpu, uint8_t v) { fam65xx_set_y(&cpu->impl, v); }
inline void mos6510_set_s(mos6510_c_t* cpu, uint8_t v) { fam65xx_set_s(&cpu->impl, v); }
inline void mos6510_set_p(mos6510_c_t* cpu, uint8_t v) { fam65xx_set_p(&cpu->impl, v); }
inline void mos6510_set_pc(mos6510_c_t* cpu, uint16_t v) { fam65xx_set_pc(&cpu->impl, v); }

// I/O Port functions
inline void mos6510_set_io_ddr(mos6510_c_t* cpu, uint8_t v) { cpu->io_ddr = v; }
inline void mos6510_set_io_data(mos6510_c_t* cpu, uint8_t v) { cpu->io_data = v; }
inline void mos6510_set_io_external(mos6510_c_t* cpu, uint8_t v) { cpu->io_external = v; }

inline uint8_t mos6510_get_io_ddr(mos6510_c_t* cpu) { return cpu->io_ddr; }
inline uint8_t mos6510_get_io_data(mos6510_c_t* cpu) { 
    return (cpu->io_data & cpu->io_ddr) | (cpu->io_external & ~cpu->io_ddr);
}
inline uint8_t mos6510_get_io_external(mos6510_c_t* cpu) { return cpu->io_external; }

#ifdef __cplusplus
}
#endif