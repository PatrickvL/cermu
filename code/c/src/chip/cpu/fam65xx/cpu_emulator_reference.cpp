/*
 * Cycle-Accurate CPU Emulator Reference Implementation
 * 
 * This file demonstrates advanced C++ techniques for building a cycle-accurate,
 * multi-CPU emulator architecture. It's designed as a reference for incremental
 * refactoring of existing emulator projects.
 * 
 * Key Features:
 * - Trait-based CPU configuration (compile-time polymorphism)
 * - Packed bus state representation (pins: data, address, RW, RDY, SYNC, cycle_idx)
 * - Endian-aware register layout with explicit 8/16-bit access
 * - Scheduled bus access pattern (centralized in tick function)
 * - Callback-based instruction dispatch (addressing → operation)
 * - Generic data width support (8-bit 6502, 16-bit 65816)
 * - Zero overhead for disabled features (if constexpr optimization)
 * - Memory callbacks for CPU and DMA access (floating bus, I/O side effects)
 * 
 * Design Philosophy:
 * - Each cycle performs exactly one bus access
 * - Bus state flows through the entire system
 * - Handlers schedule the NEXT cycle's access
 * - Mode checks happen once per instruction (latched at opcode fetch)
 * - Template metaprogramming eliminates dead code at compile time
 */

#include <cstdint>
#include <cstring>

// ============================================================================
// ENDIANNESS DETECTION
// ============================================================================

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define CPU_BIG_ENDIAN 1
#else
    #define CPU_BIG_ENDIAN 0
#endif

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

// Bus state type - packs all pin states into a single 64-bit value
using bus_state_t = uint64_t;

// Memory access callback signature
// Returns data byte (may use bus_data for floating bits)
using MemAccessCallback = uint8_t (*)(void* ctx, uint16_t addr, 
                                      uint8_t bus_data, bool is_write);

// ============================================================================
// ENUMERATIONS
// ============================================================================

// Address register selector (only PC or AB used for bus access)
enum class AddrReg : uint8_t { 
    PC,  // Program Counter
    AB   // Address Bus register
};

// Access type for scheduled bus operations
enum class AccessType : uint8_t { 
    READ, 
    WRITE, 
    DUMMY_READ,   // Real bus cycle but data ignored
    DUMMY_WRITE   // Real bus cycle (RMW instructions)
};

// Operation type enumeration (example subset)
enum operation_t : uint8_t {
    OP_LDA = 0,
    OP_LDX = 1,
    OP_LDY = 2,
    OP_STA = 3,
    OP_STX = 4,
    OP_STY = 5,
    OP_ADC = 6,
    OP_SBC = 7,
    OP_AND = 8,
    OP_ORA = 9,
    OP_EOR = 10,
    OP_INC = 11,
    OP_DEC = 12,
    OP_CMP = 13,
    OP_TAX = 14,
    OP_NOP = 15,
    // ... up to 162 for full 6502 + illegals
};

// Addressing mode enumeration
enum addr_mode_t : uint8_t {
    AM_IMPLIED = 0,
    AM_IMMEDIATE = 1,
    AM_ZEROPAGE = 2,
    AM_ZEROPAGE_X = 3,
    AM_ZEROPAGE_Y = 4,
    AM_ABSOLUTE = 5,
    AM_ABSOLUTE_X = 6,
    AM_ABSOLUTE_Y = 7,
    AM_INDEXED_INDIRECT = 8,   // (zp,X)
    AM_INDIRECT_INDEXED = 9,   // (zp),Y
    AM_RELATIVE = 10,
    AM_INDIRECT = 11,
    AM_ACCUMULATOR = 12,
    AM_STACK_PUSH = 13,
    AM_STACK_PULL = 14,
};

// Opcode flags
enum opcode_flags_t : uint8_t {
    OF_NONE          = 0x0,
    OF_ILLEGAL_STORE = 0x1,  // Illegal store quirk on page cross
    OF_SKIP_PAGE     = 0x2,  // Can skip page cross penalty
    OF_RMW           = 0x4,  // Read-Modify-Write operation
    OF_PC_OR_AB      = 0x8   // First operation cycle: PC(1) or AB(0)
};

// ============================================================================
// CPU TRAIT EXAMPLES
// ============================================================================

// MOS 6502 traits
struct MOS6502_Traits {
    using data_t = uint8_t;
    static constexpr uint8_t data_width = 8;
    static constexpr uint8_t address_width = 16;
    static constexpr bool has_emulation_mode = false;
    
    // Pin bit positions (0 = disabled)
    static constexpr uint8_t rw_bit = 35;
    static constexpr uint8_t rdy_bit = 36;
    static constexpr uint8_t sync_bit = 37;
    
    static constexpr bool accurate_internal_cycles = true;
    static constexpr bool update_bus_lines = true;
};

// WDC 65C816 traits
struct WDC65816_Traits {
    using data_t = uint16_t;  // Can be 16-bit (but 8-bit bus!)
    static constexpr uint8_t data_width = 16;
    static constexpr uint8_t address_width = 24;
    static constexpr bool has_emulation_mode = true;
    
    static constexpr uint8_t rw_bit = 35;
    static constexpr uint8_t rdy_bit = 36;
    static constexpr uint8_t sync_bit = 0;  // No SYNC on 65816
    
    static constexpr bool accurate_internal_cycles = true;
    static constexpr bool update_bus_lines = true;
};

// MOS 6507 traits (Atari 2600 - only 13 address lines)
struct MOS6507_Traits {
    using data_t = uint8_t;
    static constexpr uint8_t data_width = 8;
    static constexpr uint8_t address_width = 13;
    static constexpr bool has_emulation_mode = false;
    
    static constexpr uint8_t rw_bit = 35;
    static constexpr uint8_t rdy_bit = 0;   // No RDY pin
    static constexpr uint8_t sync_bit = 37;
    
    static constexpr bool accurate_internal_cycles = true;
    static constexpr bool update_bus_lines = true;
};

// ============================================================================
// OPCODE INFO STRUCTURE
// ============================================================================

// Packed opcode metadata (16 bits total)
typedef struct {
    uint16_t op_index : 8;   // Operation index (0-162)
    uint16_t am_index : 4;   // Addressing mode index (0-14)
    uint16_t flags    : 4;   // Opcode flags
} opcode_info_t;

// ============================================================================
// MEMORY HANDLERS STRUCTURE
// ============================================================================

struct MemoryHandlers {
    MemAccessCallback cpu_cb;
    void* cpu_ctx;
    MemAccessCallback dma_cb;
    void* dma_ctx;
};

// ============================================================================
// BUS STATE CONFIGURATION
// ============================================================================

template<typename CPUTraits>
struct BusConfig {
    // Fixed layout:
    // Bits 0-7:   Data byte (always present)
    // Bits 8-31:  Address (CPU-dependent width, up to 24 bits)
    // Bits 32-34: Cycle index (3 bits, 0-7, always present)
    // Bits 35+:   Optional pins (RW, RDY, SYNC based on traits)
    
    static constexpr uint64_t DATA_MASK = 0x00000000000000FFULL;
    
    static constexpr uint8_t ADDR_SHIFT = 8;
    static constexpr uint8_t ADDR_WIDTH = CPUTraits::address_width;
    static constexpr uint64_t ADDR_MASK = ((1ULL << ADDR_WIDTH) - 1) << ADDR_SHIFT;
    
    static constexpr uint8_t CYCLE_IDX_SHIFT = 32;
    static constexpr uint64_t CYCLE_IDX_MASK = 0x0000000700000000ULL;
    
    // Pin bit positions from traits (0 = disabled)
    static constexpr uint8_t RW_BIT = CPUTraits::rw_bit;
    static constexpr uint8_t RDY_BIT = CPUTraits::rdy_bit;
    static constexpr uint8_t SYNC_BIT = CPUTraits::sync_bit;
    
    // Computed flags (0 when bit position is 0)
    static constexpr uint64_t RW_FLAG = RW_BIT ? (1ULL << RW_BIT) : 0;
    static constexpr uint64_t RDY_FLAG = RDY_BIT ? (1ULL << RDY_BIT) : 0;
    static constexpr uint64_t SYNC_FLAG = SYNC_BIT ? (1ULL << SYNC_BIT) : 0;
    
    // Enable flags
    static constexpr bool has_rw = (RW_BIT != 0);
    static constexpr bool has_rdy = (RDY_BIT != 0);
    static constexpr bool has_sync = (SYNC_BIT != 0);
};

// ============================================================================
// BUS STATE HELPER FUNCTIONS
// ============================================================================

template<typename Config>
class BusState {
public:
    // Data access - always zero shift
    static uint8_t get_data(bus_state_t state) {
        return (uint8_t)state;
    }
    
    static bus_state_t set_data(bus_state_t state, uint8_t data) {
        return (state & ~Config::DATA_MASK) | data;
    }
    
    // Address access - single shift
    static uint32_t get_addr(bus_state_t state) {
        return (state >> Config::ADDR_SHIFT) & ((1ULL << Config::ADDR_WIDTH) - 1);
    }
    
    static bus_state_t set_addr(bus_state_t state, uint32_t addr) {
        uint32_t masked = addr & ((1ULL << Config::ADDR_WIDTH) - 1);
        return (state & ~Config::ADDR_MASK) | ((uint64_t)masked << Config::ADDR_SHIFT);
    }
    
    // Cycle index - always present, fixed position
    static uint8_t get_cycle_idx(bus_state_t state) {
        return (state >> Config::CYCLE_IDX_SHIFT) & 0x7;
    }
    
    static bus_state_t set_cycle_idx(bus_state_t state, uint8_t idx) {
        return (state & ~Config::CYCLE_IDX_MASK) | 
               ((uint64_t)(idx & 0x7) << Config::CYCLE_IDX_SHIFT);
    }
    
    static bus_state_t reset_cycle_idx(bus_state_t state) {
        return state & ~Config::CYCLE_IDX_MASK;
    }
    
    static bus_state_t inc_cycle_idx(bus_state_t state) {
        return state + (1ULL << Config::CYCLE_IDX_SHIFT);
    }
    
    // Pin accessors
    static bool is_rdy_high(bus_state_t state) {
        if constexpr (Config::has_rdy) {
            return state & Config::RDY_FLAG;
        }
        return true;
    }
    
    static bus_state_t set_rdy(bus_state_t state, bool high) {
        if constexpr (Config::has_rdy) {
            return high ? (state | Config::RDY_FLAG) : (state & ~Config::RDY_FLAG);
        }
        return state;
    }
    
    static bool is_sync_high(bus_state_t state) {
        if constexpr (Config::has_sync) {
            return state & Config::SYNC_FLAG;
        }
        return false;
    }
    
    static bus_state_t set_sync(bus_state_t state, bool high) {
        if constexpr (Config::has_sync) {
            return high ? (state | Config::SYNC_FLAG) : (state & ~Config::SYNC_FLAG);
        }
        return state;
    }
};

// ============================================================================
// REGISTER LAYOUT
// ============================================================================

// 8-bit register indices (endian-aware alignment)
enum class Reg8 : uint8_t {
    A = 0,
    X = 1,
    Y = 2,
    P = 3,    // Status register
#if CPU_BIG_ENDIAN
    PCH = 4, PCL = 5,
    SPH = 6, SPL = 7,
    ABH = 8, ABL = 9,
#else
    PCL = 4, PCH = 5,
    SPL = 6, SPH = 7,
    ABL = 8, ABH = 9,
#endif
    DL = 10,  // Data Latch
    DH = 11,  // Data High (for 16-bit CPUs)
    IR = 12,  // Instruction Register
    COUNT = 13
};

// 16-bit register indices (point to lower byte of pair)
enum class Reg16 : uint8_t {
    PC = static_cast<uint8_t>(Reg8::PCL) / 2,
    SP = static_cast<uint8_t>(Reg8::SPL) / 2,
    AB = static_cast<uint8_t>(Reg8::ABL) / 2,
    DL = static_cast<uint8_t>(Reg8::DL) / 2,
};

// Status register flags
constexpr uint8_t FLAG_C = 0x01;  // Carry
constexpr uint8_t FLAG_Z = 0x02;  // Zero
constexpr uint8_t FLAG_I = 0x04;  // Interrupt Disable
constexpr uint8_t FLAG_D = 0x08;  // Decimal Mode
constexpr uint8_t FLAG_B = 0x10;  // Break (6502)
constexpr uint8_t FLAG_X = 0x10;  // Index Register Size (65816)
constexpr uint8_t FLAG_M = 0x20;  // Memory/Accumulator Size (65816)
constexpr uint8_t FLAG_V = 0x40;  // Overflow
constexpr uint8_t FLAG_N = 0x80;  // Negative

// ============================================================================
// CPU CLASS TEMPLATE
// ============================================================================

template<typename CPUTraits>
class CPU {
public:
    using Config = BusConfig<CPUTraits>;
    using Bus = BusState<Config>;
    using data_t = typename CPUTraits::data_t;
    using CycleHandler = bus_state_t (CPU::*)(bus_state_t);
    
    static constexpr uint8_t data_width = CPUTraits::data_width;
    static constexpr bool has_emulation_mode = CPUTraits::has_emulation_mode;
    
private:
    // ========================================================================
    // CPU STATE
    // ========================================================================
    
    uint8_t regs[static_cast<uint8_t>(Reg8::COUNT)];
    
    // Scheduled access for next cycle
    struct {
        AddrReg addr_reg;
        AccessType type;
        uint8_t data;
    } next_access;
    
    // Current instruction state
    opcode_info_t current_opcode_info;
    CycleHandler current_handler;
    
    // Memory handlers
    MemoryHandlers mem;
    
    // Emulation mode flag (65816 only)
    bool emulation_mode = true;
    
    // ========================================================================
    // REGISTER ACCESS - EXPLICIT WIDTH
    // ========================================================================
    
    // 8-bit access (always 8-bit regardless of data_t)
    inline uint8_t get8(Reg8 idx) const {
        return regs[static_cast<uint8_t>(idx)];
    }
    
    inline void set8(Reg8 idx, uint8_t value) {
        regs[static_cast<uint8_t>(idx)] = value;
    }
    
    // 16-bit access (always 16-bit regardless of data_t)
    inline uint16_t get16(Reg16 idx) const {
        uint8_t byte_idx = static_cast<uint8_t>(idx) * 2;
        return *(const uint16_t*)&regs[byte_idx];
    }
    
    inline void set16(Reg16 idx, uint16_t value) {
        uint8_t byte_idx = static_cast<uint8_t>(idx) * 2;
        *(uint16_t*)&regs[byte_idx] = value;
    }
    
    inline void inc16(Reg16 idx) {
        uint8_t byte_idx = static_cast<uint8_t>(idx) * 2;
        (*(uint16_t*)&regs[byte_idx])++;
    }
    
    inline void dec16(Reg16 idx) {
        uint8_t byte_idx = static_cast<uint8_t>(idx) * 2;
        (*(uint16_t*)&regs[byte_idx])--;
    }
    
    // ========================================================================
    // REGISTER ACCESS - DATA WIDTH AWARE
    // ========================================================================
    
    inline data_t get(Reg8 idx) const {
        if constexpr (data_width == 8) {
            return get8(idx);
        } else {
            // 16-bit: may need to check mode
            return get8(idx);  // Most operations use 8-bit explicitly
        }
    }
    
    inline void set(Reg8 idx, data_t value) {
        if constexpr (data_width == 8) {
            set8(idx, value);
        } else {
            set8(idx, value);
        }
    }
    
    // ========================================================================
    // MODE CHECKING (65816 EMULATION MODE)
    // ========================================================================
    
    inline bool is_8bit_mode() const {
        if constexpr (has_emulation_mode) {
            return emulation_mode || (get8(Reg8::P) & FLAG_M);
        }
        return data_width == 8;
    }
    
    inline bool is_16bit_mode() const {
        if constexpr (has_emulation_mode) {
            return !emulation_mode && !(get8(Reg8::P) & FLAG_M);
        }
        return data_width == 16;
    }
    
    // ========================================================================
    // HELPER: CHECK IF CPU HAS BUS
    // ========================================================================
    
    static inline bool cpu_has_bus(bus_state_t pins) {
        if constexpr (Config::has_rdy) {
            return Bus::is_rdy_high(pins);
        }
        return true;
    }
    
    // ========================================================================
    // HELPER: LOAD BUS DATA INTO REGISTER
    // ========================================================================
    
    inline void load(Reg8 reg, bus_state_t pins) {
        set8(reg, Bus::get_data(pins));
    }
    
    // ========================================================================
    // FLAG OPERATIONS
    // ========================================================================
    
    inline void set_flag(uint8_t flag, bool value) {
        if (value) {
            set8(Reg8::P, get8(Reg8::P) | flag);
        } else {
            set8(Reg8::P, get8(Reg8::P) & ~flag);
        }
    }
    
    inline void set_nz_flags(data_t value) {
        if constexpr (data_width == 8) {
            set_flag(FLAG_N, value & 0x80);
            set_flag(FLAG_Z, value == 0);
        } else {
            if (is_8bit_mode()) {
                set_flag(FLAG_N, value & 0x80);
                set_flag(FLAG_Z, (value & 0xFF) == 0);
            } else {
                set_flag(FLAG_N, value & 0x8000);
                set_flag(FLAG_Z, value == 0);
            }
        }
    }
    
    // ========================================================================
    // ARITHMETIC OPERATIONS
    // ========================================================================
    
    inline void do_adc(data_t operand) {
        data_t a = get8(Reg8::A);
        uint8_t carry = (get8(Reg8::P) & FLAG_C) ? 1 : 0;
        
        if constexpr (data_width == 8) {
            uint16_t result = a + operand + carry;
            set8(Reg8::A, (uint8_t)result);
            
            set_flag(FLAG_C, result > 0xFF);
            set_flag(FLAG_V, ((a ^ result) & (operand ^ result) & 0x80) != 0);
            set_nz_flags((uint8_t)result);
        } else {
            if (is_8bit_mode()) {
                uint16_t result = (a & 0xFF) + (operand & 0xFF) + carry;
                set8(Reg8::A, (uint8_t)result);
                
                set_flag(FLAG_C, result > 0xFF);
                set_flag(FLAG_V, (((a & 0xFF) ^ result) & ((operand & 0xFF) ^ result) & 0x80) != 0);
                set_nz_flags((uint8_t)result);
            } else {
                uint32_t result = a + operand + carry;
                set16(Reg16::PC, (uint16_t)result);  // Should be A register
                
                set_flag(FLAG_C, result > 0xFFFF);
                set_flag(FLAG_V, ((a ^ result) & (operand ^ result) & 0x8000) != 0);
                set_nz_flags((uint16_t)result);
            }
        }
    }
    
    inline void do_compare(data_t reg_value, data_t operand) {
        if constexpr (data_width == 8) {
            uint16_t result = reg_value - operand;
            set_flag(FLAG_C, reg_value >= operand);
            set_nz_flags((uint8_t)result);
        } else {
            if (is_8bit_mode()) {
                uint16_t result = (reg_value & 0xFF) - (operand & 0xFF);
                set_flag(FLAG_C, (reg_value & 0xFF) >= (operand & 0xFF));
                set_nz_flags((uint8_t)result);
            } else {
                uint32_t result = reg_value - operand;
                set_flag(FLAG_C, reg_value >= operand);
                set_nz_flags((uint16_t)result);
            }
        }
    }
    
    // ========================================================================
    // SCHEDULE ACCESS FOR NEXT CYCLE
    // ========================================================================
    
    void schedule_read(AddrReg reg) {
        next_access.addr_reg = reg;
        next_access.type = AccessType::READ;
    }
    
    void schedule_write(AddrReg reg, uint8_t data) {
        next_access.addr_reg = reg;
        next_access.type = AccessType::WRITE;
        next_access.data = data;
    }
    
    void schedule_dummy_read(AddrReg reg) {
        next_access.addr_reg = reg;
        next_access.type = AccessType::DUMMY_READ;
    }
    
    void schedule_dummy_write(AddrReg reg, uint8_t data) {
        next_access.addr_reg = reg;
        next_access.type = AccessType::DUMMY_WRITE;
        next_access.data = data;
    }
    
    // ========================================================================
    // LOW-LEVEL BUS ACCESS
    // ========================================================================
    
    template<bool IsWrite, bool IsDummy>
    bus_state_t phi2_access(bus_state_t pins, uint32_t addr, uint8_t data = 0) {
        // RDY check - DMA device has bus
        if constexpr (Config::has_rdy) {
            if (!Bus::is_rdy_high(pins)) {
                uint32_t dma_addr = Bus::get_addr(pins);
                uint8_t bus_data = Bus::get_data(pins);
                uint8_t dma_data = mem.dma_cb(mem.dma_ctx, dma_addr, bus_data, false);
                return Bus::set_data(pins, dma_data);
            }
        }
        
        // Skip dummy cycles if not simulating
        if constexpr (IsDummy && !CPUTraits::accurate_internal_cycles) {
            return pins;
        }
        
        // Update bus lines
        if constexpr (CPUTraits::update_bus_lines) {
            pins = Bus::set_addr(pins, addr);
            
            if constexpr (Config::has_rw) {
                if constexpr (IsWrite) {
                    pins &= ~Config::RW_FLAG;
                } else {
                    pins |= Config::RW_FLAG;
                }
            }
        }
        
        uint8_t bus_data = Bus::get_data(pins);
        
        if constexpr (!IsWrite) {
            uint8_t read_data = mem.cpu_cb(mem.cpu_ctx, addr, bus_data, false);
            
            if constexpr (!IsDummy) {
                pins = Bus::set_data(pins, read_data);
            }
        } else {
            uint8_t result = mem.cpu_cb(mem.cpu_ctx, addr, data, true);
            pins = Bus::set_data(pins, result);
        }
        
        return pins;
    }
    
    // ========================================================================
    // CONCRETE BUS ACCESS WRAPPERS
    // ========================================================================
    
    inline bus_state_t read(bus_state_t pins, uint32_t addr) {
        return phi2_access<false, false>(pins, addr);
    }
    
    inline bus_state_t write(bus_state_t pins, uint32_t addr, uint8_t data) {
        return phi2_access<true, false>(pins, addr, data);
    }
    
    inline bus_state_t dummy_read(bus_state_t pins, uint32_t addr) {
        return phi2_access<false, true>(pins, addr);
    }
    
    inline bus_state_t dummy_write(bus_state_t pins, uint32_t addr, uint8_t data) {
        return phi2_access<true, true>(pins, addr, data);
    }
    
    // ========================================================================
    // TRANSITION HELPERS
    // ========================================================================
    
    bus_state_t transition_to_operation(bus_state_t pins) {
        pins = Bus::reset_cycle_idx(pins);
        current_handler = operation_handlers[current_opcode_info.op_index];
        
        // Schedule first operation access based on flags
        AddrReg addr_reg = (current_opcode_info.flags & OF_PC_OR_AB) 
                           ? AddrReg::PC 
                           : AddrReg::AB;
        
        schedule_read(addr_reg);
        
        return pins;
    }
    
    bus_state_t transition_to_fetch(bus_state_t pins) {
        pins = Bus::reset_cycle_idx(pins);
        schedule_read(AddrReg::PC);
        current_handler = &CPU::handle_opcode_fetch;
        return pins;
    }
    
    // ========================================================================
    // OPCODE FETCH HANDLER
    // ========================================================================
    
    bus_state_t handle_opcode_fetch(bus_state_t pins) {
        // Load opcode into IR
        uint8_t opcode = Bus::get_data(pins);
        set8(Reg8::IR, opcode);
        inc16(Reg16::PC);
        
        // Cache opcode info
        current_opcode_info = opcode_table[opcode];
        
        // Determine first cycle handler
        if (current_opcode_info.am_index == 0) {
            // No addressing mode - go directly to operation
            current_handler = operation_handlers[current_opcode_info.op_index];
            schedule_dummy_read(AddrReg::PC);
        } else {
            // Has addressing mode
            current_handler = addressing_handlers[current_opcode_info.am_index];
            schedule_read(AddrReg::PC);
        }
        
        return Bus::inc_cycle_idx(pins);
    }
    
    // ========================================================================
    // ADDRESSING MODE HANDLERS
    // ========================================================================
    
    bus_state_t addr_immediate(bus_state_t pins) {
        // Immediate value already on bus from PC read
        return transition_to_operation(pins);
    }
    
    bus_state_t addr_absolute(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        
        switch (cycle) {
            case 0:  // Read address low
                load(Reg8::ABL, pins);
                inc16(Reg16::PC);
                schedule_read(AddrReg::PC);
                return Bus::inc_cycle_idx(pins);
                
            case 1:  // Read address high
                load(Reg8::ABH, pins);
                inc16(Reg16::PC);
                return transition_to_operation(pins);
        }
        
        return pins;
    }
    
    bus_state_t addr_absolute_x(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        
        switch (cycle) {
            case 0:  // Read base address low
                load(Reg8::ABL, pins);
                inc16(Reg16::PC);
                schedule_read(AddrReg::PC);
                return Bus::inc_cycle_idx(pins);
                
            case 1:  // Read base address high
                load(Reg8::ABH, pins);
                inc16(Reg16::PC);
                
                // Calculate effective address
                uint16_t base = get16(Reg16::AB);
                uint16_t addr = base + get8(Reg8::X);
                bool page_cross = (addr & 0xFF00) != (base & 0xFF00);
                
                if (page_cross) {
                    if (current_opcode_info.flags & OF_ILLEGAL_STORE) {
                        // Use wrong address (illegal opcode quirk)
                        set16(Reg16::AB, (base & 0xFF00) | (addr & 0xFF));
                        return transition_to_operation(pins);
                    }
                    
                    // Dummy read from wrong page
                    set16(Reg16::AB, (base & 0xFF00) | (addr & 0xFF));
                    schedule_read(AddrReg::AB);
                    return Bus::inc_cycle_idx(pins);
                } else {
                    set16(Reg16::AB, addr);
                    return transition_to_operation(pins);
                }
                
            case 2:  // Fix address after page cross
                uint16_t base = get16(Reg16::AB);
                uint16_t addr = (base & 0x00FF) + get8(Reg8::X) + ((base & 0xFF00) ? 0x0100 : 0);
                set16(Reg16::AB, addr);
                return transition_to_operation(pins);
        }
        
        return pins;
    }
    
    bus_state_t addr_indexed_indirect(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        
        switch (cycle) {
            case 0:  // Read zero page base
                load(Reg8::DL, pins);
                inc16(Reg16::PC);
                schedule_dummy_read(AddrReg::PC);
                return Bus::inc_cycle_idx(pins);
                
            case 1:  // Dummy read, add X
                uint8_t zp_addr = get8(Reg8::DL) + get8(Reg8::X);
                set8(Reg8::ABL, zp_addr);
                set8(Reg8::ABH, 0);
                schedule_read(AddrReg::AB);
                return Bus::inc_cycle_idx(pins);
                
            case 2:  // Read pointer low
                load(Reg8::DL, pins);
                inc16(Reg16::AB);
                schedule_read(AddrReg::AB);
                return Bus::inc_cycle_idx(pins);
                
            case 3:  // Read pointer high
                load(Reg8::ABH, pins);
                set8(Reg8::ABL, get8(Reg8::DL));
                return transition_to_operation(pins);
        }
        
        return pins;
    }
    
    // ========================================================================
    // OPERATION HANDLERS
    // ========================================================================
    
    bus_state_t op_LDA(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        
        switch (cycle) {
            case 0:  // Read data byte
                set8(Reg8::A, Bus::get_data(pins));
                
                if constexpr (data_width == 16) {
                    if (!is_8bit_mode()) {
                        // 16-bit: need high byte
                        inc16(Reg16::AB);
                        schedule_read(AddrReg::AB);
                        return Bus::inc_cycle_idx(pins);
                    }
                }
                
                // 8-bit: done
                set_nz_flags(get8(Reg8::A));
                return transition_to_fetch(pins);
                
            case 1:  // Read high byte (16-bit CPUs only)
                if constexpr (data_width == 16) {
                    set8(Reg8::DH, Bus::get_data(pins));
                    set_nz_flags(get16(Reg16::PC));  // Should be A register
                    return transition_to_fetch(pins);
                }
        }
        
        return pins;
    }
    
    bus_state_t op_STA(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        
        switch (cycle) {
            case 0:  // Write low byte (already happened)
                if constexpr (data_width == 16) {
                    if (!is_8bit_mode()) {
                        // 16-bit: write high byte
                        inc16(Reg16::AB);
                        schedule_write(AddrReg::AB, get8(Reg8::DH));
                        return Bus::inc_cycle_idx(pins);
                    }
                }
                
                // 8-bit: done
                return transition_to_fetch(pins);
                
            case 1:  // 16-bit only: high byte written
                if constexpr (data_width == 16) {
                    return transition_to_fetch(pins);
                }
        }
        
        return pins;
    }
    
    bus_state_t op_ADC(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        
        switch (cycle) {
            case 0:  // Read operand low byte
                set8(Reg8::DL, Bus::get_data(pins));
                
                if constexpr (data_width == 16) {
                    if (!is_8bit_mode()) {
                        // 16-bit: need high byte
                        inc16(Reg16::AB);
                        schedule_read(AddrReg::AB);
                        return Bus::inc_cycle_idx(pins);
                    }
                }
                
                // 8-bit: perform operation
                do_adc(get8(Reg8::DL));
                return transition_to_fetch(pins);
                
            case 1:  // 16-bit only: read high byte
                if constexpr (data_width == 16) {
                    set8(Reg8::DH, Bus::get_data(pins));
                    do_adc(get16(Reg16::DL));
                    return transition_to_fetch(pins);
                }
        }
        
        return pins;
    }
    
    bus_state_t op_INC(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        uint8_t result8;
        uint16_t result16;
        
        switch (cycle) {
            case 0:  // Read value low byte
                set8(Reg8::DL, Bus::get_data(pins));
                
                if constexpr (data_width == 16) {
                    if (!is_8bit_mode()) {
                        // 16-bit: read high byte
                        inc16(Reg16::AB);
                        schedule_read(AddrReg::AB);
                        return Bus::inc_cycle_idx(pins);
                    }
                }
                
                // 8-bit: dummy write
                schedule_dummy_write(AddrReg::AB, get8(Reg8::DL));
                return Bus::inc_cycle_idx(pins);
                
            case 1:
                if constexpr (data_width == 16) {
                    if (!is_8bit_mode()) {
                        // 16-bit: read high byte
                        set8(Reg8::DH, Bus::get_data(pins));
                        dec16(Reg16::AB);
                        schedule_dummy_write(AddrReg::AB, get8(Reg8::DL));
                        return Bus::inc_cycle_idx(pins);
                    }
                }
                
                // 8-bit: calculate and write result
                result8 = get8(Reg8::DL) + 1;
                set_nz_flags(result8);
                schedule_write(AddrReg::AB, result8);
                return Bus::inc_cycle_idx(pins);
                
            case 2:
                if constexpr (data_width == 16) {
                    if (!is_8bit_mode()) {
                        // 16-bit: dummy write high
                        inc16(Reg16::AB);
                        schedule_dummy_write(AddrReg::AB, get8(Reg8::DH));
                        return Bus::inc_cycle_idx(pins);
                    }
                }
                
                // 8-bit: done
                return transition_to_fetch(pins);
                
            case 3:  // 16-bit only: write low byte
                if constexpr (data_width == 16) {
                    result16 = get16(Reg16::DL) + 1;
                    set_nz_flags(result16);
                    dec16(Reg16::AB);
                    schedule_write(AddrReg::AB, (uint8_t)result16);
                    return Bus::inc_cycle_idx(pins);
                }
                break;
                
            case 4:  // 16-bit only: write high byte
                if constexpr (data_width == 16) {
                    result16 = get16(Reg16::DL) + 1;
                    inc16(Reg16::AB);
                    schedule_write(AddrReg::AB, (uint8_t)(result16 >> 8));
                    return Bus::inc_cycle_idx(pins);
                }
                break;
                
            case 5:  // 16-bit only: done
                if constexpr (data_width == 16) {
                    return transition_to_fetch(pins);
                }
        }
        
        return pins;
    }
    
    bus_state_t op_CMP(bus_state_t pins) {
        uint8_t cycle = Bus::get_cycle_idx(pins);
        
        switch (cycle) {
            case 0:
                load(Reg8::DL, pins);
                
                if constexpr (data_width == 16) {
                    if (!is_8bit_mode()) {
                        inc16(Reg16::AB);
                        schedule_read(AddrReg::AB);
                        return Bus::inc_cycle_idx(pins);
                    }
                }
                
                do_compare(get8(Reg8::A), get8(Reg8::DL));
                return transition_to_fetch(pins);
                
            case 1:
                if constexpr (data_width == 16) {
                    set8(Reg8::DH, Bus::get_data(pins));
                    do_compare(get16(Reg16::PC), get16(Reg16::DL));  // Should be A
                    return transition_to_fetch(pins);
                }
        }
        
        return pins;
    }
    
    bus_state_t op_TAX(bus_state_t pins) {
        // Dummy read already performed
        set8(Reg8::X, get8(Reg8::A));
        set_nz_flags(get8(Reg8::X));
        return transition_to_fetch(pins);
    }
    
    bus_state_t op_NOP(bus_state_t pins) {
        // Dummy read already performed
        return transition_to_fetch(pins);
    }
    
    // ========================================================================
    // HANDLER TABLES (static)
    // ========================================================================
    
    static constexpr CycleHandler operation_handlers[256] = {
        &CPU::op_LDA,    // 0
        &CPU::op_STA,    // 3
        &CPU::op_ADC,    // 6
        &CPU::op_INC,    // 11
        &CPU::op_CMP,    // 13
        &CPU::op_TAX,    // 14
        &CPU::op_NOP,    // 15
        // ... fill out all 163 operations
    };
    
    static constexpr CycleHandler addressing_handlers[15] = {
        nullptr,                           // 0 - Implied
        &CPU::addr_immediate,              // 1
        nullptr,                           // 2 - Zero page (similar to absolute)
        nullptr,                           // 3 - Zero page,X
        nullptr,                           // 4 - Zero page,Y
        &CPU::addr_absolute,               // 5
        &CPU::addr_absolute_x,             // 6
        nullptr,                           // 7 - Absolute,Y
        &CPU::addr_indexed_indirect,       // 8
        nullptr,                           // 9 - Indirect indexed
        nullptr,                           // 10 - Relative
        nullptr,                           // 11 - Indirect
        nullptr,                           // 12 - Accumulator
        nullptr,                           // 13 - Stack push
        nullptr,                           // 14 - Stack pull
    };
    
    static constexpr opcode_info_t opcode_table[256] = {
        // Example entries
        { .op_index = 0, .am_index = 1, .flags = OF_PC_OR_AB },  // 0xA9: LDA #imm
        { .op_index = 0, .am_index = 5, .flags = OF_NONE },      // 0xAD: LDA abs
        { .op_index = 0, .am_index = 6, .flags = OF_NONE },      // 0xBD: LDA abs,X
        // ... fill out all 256 opcodes
    };
    
public:
    // ========================================================================
    // PUBLIC INTERFACE
    // ========================================================================
    
    CPU(MemoryHandlers& mem_handlers) : mem(mem_handlers) {
        std::memset(regs, 0, sizeof(regs));
        current_handler = &CPU::handle_opcode_fetch;
        schedule_read(AddrReg::PC);
    }
    
    bus_state_t tick(bus_state_t pins) {
        // Perform scheduled access
        uint32_t addr = (next_access.addr_reg == AddrReg::PC) 
                        ? get16(Reg16::PC) 
                        : get16(Reg16::AB);
        
        switch (next_access.type) {
            case AccessType::READ:
                pins = read(pins, addr);
                break;
            case AccessType::WRITE:
                pins = write(pins, addr, next_access.data);
                break;
            case AccessType::DUMMY_READ:
                pins = dummy_read(pins, addr);
                break;
            case AccessType::DUMMY_WRITE:
                pins = dummy_write(pins, addr, next_access.data);
                break;
        }
        
        if (!cpu_has_bus(pins)) return pins;
        
        // Call current handler
        return (this->*current_handler)(pins);
    }
    
    void reset(bus_state_t& pins) {
        // Reset vector fetch, etc.
        pins = Bus::reset_cycle_idx(pins);
        set16(Reg16::PC, 0xFFFC);  // Reset vector
        current_handler = &CPU::handle_opcode_fetch;
        schedule_read(AddrReg::PC);
    }
};

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

int main() {
    // Example: Create a 6502 CPU instance
    
    // Memory callbacks (simplified)
    uint8_t ram[65536] = {0};
    
    auto cpu_mem_cb = [](void* ctx, uint16_t addr, uint8_t bus_data, bool is_write) -> uint8_t {
        uint8_t* ram = (uint8_t*)ctx;
        if (is_write) {
            ram[addr] = bus_data;
            return bus_data;
        }
        return ram[addr];
    };
    
    auto dma_mem_cb = [](void* ctx, uint16_t addr, uint8_t bus_data, bool is_write) -> uint8_t {
        // VIC-II DMA access
        uint8_t* ram = (uint8_t*)ctx;
        return ram[addr];
    };
    
    MemoryHandlers mem = {
        .cpu_cb = cpu_mem_cb,
        .cpu_ctx = ram,
        .dma_cb = dma_mem_cb,
        .dma_ctx = ram
    };
    
    CPU<MOS6502_Traits> cpu(mem);
    
    // Initialize bus state
    bus_state_t pins = 0;
    
    // Run cycles
    for (int i = 0; i < 1000; i++) {
        pins = cpu.tick(pins);
    }
    
    return 0;
}

/*
 * INCREMENTAL REFACTORING GUIDE
 * ==============================
 * 
 * This reference shows best practices. You can adopt these patterns incrementally:
 * 
 * 1. START: Packed bus state
 *    - Replace separate pin variables with single uint64_t
 *    - Use BusConfig and BusState helpers
 *    - Benefits: Cache-friendly, easy to pass around
 * 
 * 2. ADD: Trait-based configuration
 *    - Extract CPU-specific constants to trait structs
 *    - Use if constexpr for conditional compilation
 *    - Benefits: Single codebase for multiple CPUs, zero overhead
 * 
 * 3. REFACTOR: Scheduled bus access
 *    - Move bus operations to centralized tick()
 *    - Handlers schedule next cycle's access
 *    - Benefits: Clearer control flow, easier to debug
 * 
 * 4. SEPARATE: Addressing modes from operations
 *    - Split instruction handlers into addressing + operation
 *    - Use callback-based dispatch
 *    - Benefits: Reusable addressing modes, cleaner code
 * 
 * 5. OPTIMIZE: Register layout
 *    - Align register pairs for endian-independent access
 *    - Provide explicit 8/16-bit accessors
 *    - Benefits: Fast 16-bit operations, portable
 * 
 * 6. EXTEND: Multi-width support
 *    - Add data_t typedef and mode checking
 *    - Use if constexpr for 8/16-bit paths
 *    - Benefits: Support 65816 with same codebase
 * 
 * Each step is independent and brings immediate benefits!
 */
