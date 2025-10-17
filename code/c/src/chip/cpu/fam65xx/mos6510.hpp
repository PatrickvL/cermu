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
#include "../../../core/chip.h"
#include <cstdlib>
#include <cstring>

#ifdef __cplusplus

namespace fam65xx_cpu {

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

// Forward declare the type alias
using MOS6510 = fam65xx_t;
typedef fam65xx_t MOS6510; // C compatibility

// MOS 6510 with Enhanced I/O Port Management and Callback System
class MOS6510WithIOPort {
private:
    MOS6510 cpu;
    IOPortState io_port;
    
    // Callback system for external decoupling
    mos6510_io_callbacks_t callbacks = {nullptr, nullptr, nullptr, nullptr};
    uint32_t io_access_count = 0;
    
public:
    // CPU interface delegation using C functions
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) { return fam65xx_init(&cpu, desc); }
    bus_state_t reset(bus_state_t pins) {
        io_port = IOPortState(); // Reset I/O port
        return fam65xx_reset(&cpu, pins);
    }
    bus_state_t bootstrap(bus_state_t pins) { return fam65xx_bootstrap(&cpu, pins); }
    bus_state_t tick(bus_state_t pins) { return fam65xx_tick(&cpu, pins); }
    bool opdone() const { return fam65xx_opdone(const_cast<fam65xx_t*>(&cpu)); }
    
    // Register access using C functions
    void set_a(uint8_t v) { fam65xx_set_a(&cpu, v); }
    void set_x(uint8_t v) { fam65xx_set_x(&cpu, v); }
    void set_y(uint8_t v) { fam65xx_set_y(&cpu, v); }
    void set_s(uint8_t v) { fam65xx_set_s(&cpu, v); }
    void set_p(uint8_t v) { fam65xx_set_p(&cpu, v); }
    void set_pc(uint16_t v) { fam65xx_set_pc(&cpu, v); }
    
    uint8_t a() const { return fam65xx_a(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t x() const { return fam65xx_x(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t y() const { return fam65xx_y(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t s() const { return fam65xx_s(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t p() const { return fam65xx_p(const_cast<fam65xx_t*>(&cpu)); }
    uint16_t pc() const { return fam65xx_pc(const_cast<fam65xx_t*>(&cpu)); }
    
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
// Convenient creation function for basic MOS6510
inline fam65xx_t create() {
    fam65xx_t cpu;
    fam65xx_init(&cpu, nullptr);
    return cpu;
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
    
    fam65xx_t cpu;
    fam65xx_init(&cpu, &desc);
    return cpu;
}

} // namespace fam65xx_cpu

// Global type aliases for convenience
using mos6510_t = fam65xx_t;
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

// ============================================================================
// MOS6510 Chip API - System Integration (MIGRATED FROM mos6510/ folder)
// ============================================================================

// MOS6510 CPU chip structure - zero-overhead wrapper
typedef struct {
    fam65xx_t cpu_impl;
    void (*io_callback)(uint16_t addr, uint8_t data, bool write);
} mos6510_chip_t;

// Create/destroy CPU instance
inline mos6510_chip_t* mos6510_chip_create_impl(void) {
    mos6510_chip_t* chip = (mos6510_chip_t*)malloc(sizeof(mos6510_chip_t));
    if (chip) {
        memset(chip, 0, sizeof(mos6510_chip_t));
    }
    return chip;
}

inline void mos6510_destroy(mos6510_chip_t* cpu) {
    if (cpu) {
        free(cpu);
    }
}

// Initialize CPU with optional IO callback
inline void mos6510_chip_init(mos6510_chip_t* cpu,
                       void (*io_callback)(uint16_t addr, uint8_t data, bool write)) {
    if (cpu) {
        cpu->io_callback = io_callback;
        fam65xx_init(&cpu->cpu_impl, nullptr);
    }
}

// Main CPU tick - direct bus interface
inline bus_state_t mos6510_chip_tick(mos6510_chip_t* cpu, bus_state_t bus_state) {
    if (!cpu) return 0;
    return fam65xx_tick(&cpu->cpu_impl, bus_state);
}

// Debug interface
inline uint16_t mos6510_get_pc(mos6510_chip_t* cpu) {
    return cpu ? fam65xx_pc(&cpu->cpu_impl) : 0;
}

inline uint8_t mos6510_get_a(mos6510_chip_t* cpu) {
    return cpu ? fam65xx_a(&cpu->cpu_impl) : 0;
}

inline uint8_t mos6510_get_x(mos6510_chip_t* cpu) {
    return cpu ? fam65xx_x(&cpu->cpu_impl) : 0;
}

inline uint8_t mos6510_get_y(mos6510_chip_t* cpu) {
    return cpu ? fam65xx_y(&cpu->cpu_impl) : 0;
}

inline uint8_t mos6510_get_s(mos6510_chip_t* cpu) {
    return cpu ? fam65xx_s(&cpu->cpu_impl) : 0;
}

inline uint8_t mos6510_get_p(mos6510_chip_t* cpu) {
    return cpu ? fam65xx_p(&cpu->cpu_impl) : 0;
}

// System integration functions
inline bus_state_t mos6510_tick_chip(void* chip, bus_state_t bus_state) {
    return mos6510_chip_tick((mos6510_chip_t*)chip, bus_state);
}

// Chip descriptor for system registration
static void* mos6510_chip_create(void* desc) {
    (void)desc; // Unused parameter
    return mos6510_chip_create_impl();
}

static void mos6510_chip_destroy(void* chip) {
    mos6510_destroy((mos6510_chip_t*)chip);
}

// Chip descriptor - commented out until chip_descriptor_t is available
/*
static chip_descriptor_t mos6510_descriptor = {
    .description = "MOS6510 CPU (Unified Implementation)",
    .create = mos6510_chip_create,
    .destroy = mos6510_chip_destroy,
    .bus_attach = NULL, // No special bus attachment needed
    .bank_change = NULL, // No banking change needed
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = NULL, // No GUI debug window implemented yet
    .render_settings_window = NULL // No GUI settings window implemented yet
#endif
};
*/

#ifdef __cplusplus
}
#endif