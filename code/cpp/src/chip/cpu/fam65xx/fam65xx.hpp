#pragma once
/*
 * fam65xx.hpp - MOS 65xx Family CPU Emulator (Main Template Implementation)
 *
 * This header contains the main CPU template class implementation that builds
 upon
 * modular components for maintainability. It includes C types, processor
 traits,
 * and conditional mixins from separate headers to keep file sizes manageable.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * 1. Zero overhead abstractions - templates eliminate runtime costs
 * 2. Conditional features - only include data/code for enabled features
 * 3. Pin-accurate bus interface - maintains bus_state_t pins design
 * 4. Modular components - separate headers for logical groupings
 * 5. Single include point - just include this header for full functionality
 *
 * SUPPORTED PROCESSOR VARIANTS:
 * =============================
 * - MOS6502Tag:     Original NMOS 6502 with illegal opcodes
 * - MOS6510Tag:     C64/C128 variant with I/O port
 * - NES6502Tag:     NES variant (no BCD, no illegal opcodes)

 * - WDC65C02Tag:    CMOS 65C02 with enhanced instructions
 * - Rockwell65C02Tag: CMOS with bit manipulation instructions
 * - WDC65C816Tag:   16-bit extended processor
 *
 * USAGE:
 * ======
 * ```cpp
 * #include "fam65xx.hpp"
 *
 * // Create a C64 CPU instance
 * fam65xx::fam65xx_t<MOS6510Tag> c64_cpu;
 *
 * // Initialize and use with pin-based interface
 * bus_state_t pins = c64_cpu.init(&desc);
 * pins = c64_cpu.reset(pins);
 * pins = c64_cpu.tick(pins);
 * ```
 */

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

// Include instruction decoder for disassembly
extern "C" {
#include "fam65xx_decoder.h"
}

#include "fam65xx_mixins.hpp"
#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.h"

// ============================================================================
// C++ NAMESPACE - MAIN CPU TEMPLATE IMPLEMENTATION
// ============================================================================

namespace fam65xx {

// ============================================================================
// FORWARD DECLARATIONS FOR OPCODE TABLE GENERATION
// ============================================================================

// Forward declaration for opcode table generation function
constexpr std::array<opcode_info_t, 256>
generate_opcode_table_for_traits(const CPUTraits &traits);

// Include opcode table generation implementation first
#include "operations/opcode_tables.inc.hpp"

// Now define the template function
// Generate processor-specific opcode table at compile time
template <const CPUTraits &Traits>
constexpr std::array<opcode_info_t, 256> generate_opcode_table() {
  // This will use the generate_opcode_table_for_traits function from
  // opcode_tables.inc.hpp
  return generate_opcode_table_for_traits(Traits);
}

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template <const CPUTraits &Traits>
class fam65xx_t : public io_port_base_t<Traits>, public apu_base_t<Traits> {
// Include register declarations and accessors
#include "fam65xx_registers.inc.hpp"

  // CPUTraits-based feature detection helpers for operations files
  static constexpr bool has_apu() { return Traits.has_apu(); }
  static constexpr bool has_io_port() { return Traits.has_io_port(); }
  static constexpr bool has_nmos_bugs() { return Traits.is_nmos(); }

  static constexpr bool has_bcd() {
    return Traits.has(CPUCoreFlags::HAS_DECIMAL_MODE);
  }
  static constexpr bool has_bcd_extra_cycle() {
    return Traits.has(CPUCoreFlags::BCD_EXTRA_CYCLE);
  }
  static constexpr bool has_bcd_nmos_flags() {
    return Traits.has(CPUCoreFlags::BCD_NMOS_FLAGS);
  }
  static constexpr bool has_cmos() {
    return Traits.has(CPUCoreFlags::CMOS_BASE);
  }
  static constexpr bool has_illegal_opcodes() {
    return Traits.has(CPUCoreFlags::ILLEGAL_OPCODES);
  }
  static constexpr bool has_irq_line() {
    return !Traits.has(CPUCoreFlags::NO_IRQ_LINE);
  }
  static constexpr bool has_nmi_line() {
    return !Traits.has(CPUCoreFlags::NO_NMI_LINE);
  }
  static constexpr bool has_optimized_cycles() {
    return Traits.has(CPUCoreFlags::OPTIMIZED_CYCLES);
  }
  static constexpr bool has_rmw_dummy_write() {
    return Traits.has(CPUCoreFlags::RMW_DUMMY_WRITE);
  }
  static constexpr bool has_wide_registers() {
    return Traits.has(CPUCoreFlags::C816_16BIT);
  }
  static constexpr bool has_bit_manipulation() {
    return Traits.has(CPUCoreFlags::ROCKWELL_BITS);
  }

  // ========================================================================
  // DEBUG TRACING HELPERS
  // ========================================================================

  template <const bool trace_instructions = false>
  void print_instruction_trace() const {

    if constexpr (trace_instructions) {
      static int instruction_count = 0;
      if (instruction_count < 100) {
        uint16_t pc = this->get(REG_PC);
        uint16_t ab = this->get(REG_AB); // Get address bus from register
        uint8_t ir = this->get(REG_IR);  // Get instruction register

        // Use the current opcode_entry which is properly set during reset to
        // BRK This shows the logical instruction being executed (BRK during
        // reset) rather than whatever random data is in the IR register
        const char *opcode_name =
            fam65xx_get_opcode_name(this->opcode_entry.op_index);

        printf("[%03d] PC=$%04X AB=$%04X IR=$%02X (%s) ", instruction_count, pc,
               ab, ir, opcode_name);

        // Get instruction info if opcode is valid
        printf("A=$%02X X=$%02X Y=$%02X S=$%02X P=$%02X\n", this->get(REG_A),
               this->get(REG_X), this->get(REG_Y), this->get(REG_SPL),
               this->get(REG_P));

        instruction_count++;
        if (instruction_count == 100) {
          printf("=== Instruction trace complete (%d instructions) ===\n",
                 instruction_count);
        }
      }
    }
  }

  void trace(const char *format, ...) const {
    if constexpr (ENABLE_TRACING) {
      // Print indentation
      for (int i = 0; i < trace_indent; i++) {
        printf("  ");
      }

      // Print formatted message
      va_list args;
      va_start(args, format);
      vprintf(format, args);
      va_end(args);
      printf("\n");
    }
  }

  void trace_enter(const char *function_name) const {
    if constexpr (ENABLE_TRACING) {
      trace("→ %s", function_name);
      trace_indent++;
    }
  }

  void trace_exit(const char *function_name) const {
    if constexpr (ENABLE_TRACING) {
      trace_indent--;
      trace("← %s", function_name);
    }
  }

  void trace_registers(const char *context = "") const {
    if constexpr (ENABLE_TRACING) {
      if constexpr (has_wide_registers()) {
        trace("REGS %s: PBR=%02X:PC=%04X A16=%05X X16=%04X Y16=%04X P=%02X "
              "ZBR=%02X:S=%02X DBR=%02X(:AB=%04X DL=%02X)",
              context, this->get(REG_PBR), this->get(REG_PC),
              this->get(REG_A_16), this->get(REG_X_16), this->get(REG_Y_16),
              this->get(REG_P), this->get(REG_ZBR), this->get(REG_S),
              this->get(REG_DBR), this->get(REG_AB), this->get(REG_DL));
      } else {
        trace("REGS %s: PC=%04X A=%02X X=%02X Y=%02X P=%02X S=%02X (AB=%04X "
              "DL=%02X)",
              context, this->get(REG_PC), this->get(REG_A), this->get(REG_X),
              this->get(REG_Y), this->get(REG_P), this->get(REG_S),
              this->get(REG_AB), this->get(REG_DL));
      }
    }
  }

  void trace_memory_access(const char *op, uint16_t addr, uint8_t data) const {
    if constexpr (ENABLE_TRACING) {
      trace("MEM %s: [%04X] = %02X", op, addr, data);
    }
  }

  void trace_addressing_mode(const char *am_name) const {
    if constexpr (ENABLE_TRACING) {
      trace("AM %s: IR=0x%02X cycle=%d AB=%04X PC=%04X", am_name,
            this->get(REG_IR), cycle_index, this->get(REG_AB),
            this->get(REG_PC));
    }
  }

  void trace_operation(const char *function_name) const {
    if constexpr (ENABLE_TRACING) {
      trace("OP: %s (IR=0x%02X cycle=%d)", function_name, this->get(REG_IR),
            cycle_index);
    }
  }

  void trace_instruction(uint8_t opcode, const char *mnemonic) const {
    if constexpr (ENABLE_TRACING) {
      trace("EXEC: %02X %s (cycle %d)", opcode, mnemonic, cycle_index);
    }
  }

  /**
   * Template tooling function that returns true when 16-bit mode is required
   * for the specified register. Handles all processor types and modes.
   *
   * @tparam reg_type The register type (REG_A, REG_X, REG_Y, or memory
   * placeholder)
   * @return true if 16-bit mode should be used for this register
   */
  template <reg8_t reg_type> inline bool is_register_16bit() const {
    if constexpr (has_wide_registers()) {
      // 65C816: Check register-specific width flags
      if constexpr (reg_type == REG_A) {
        // Accumulator: M=0 means 16-bit (only in native mode)
        return this->is_accumulator_16bit();
      } else if constexpr (reg_type == REG_X || reg_type == REG_Y) {
        // Index registers: X=0 means 16-bit (only in native mode)
        return this->is_index_16bit();
      }
    }
    // Non-65C816 processors or memory operations: always 8-bit
    return false;
  }

  // Get bank byte for address construction (0 if banking doesn't apply)
  template <Addr addr_arg>
  inline uint8_t get_address_bank(Bank bank_arg) const {
    // Non-65C816 processors never use banking
    if constexpr (!has_wide_registers())
      return 0;

    // PC always uses PBR, even in emulation mode
    if constexpr (addr_arg == Addr::PC)
      return this->get(REG_PBR);

    // Emulation mode: non-PC addresses don't use banking (6502 compatibility)
    if (this->in_emulation_mode())
      return 0;

    // Native mode: SP uses ZBR, data addresses use provided bank (typically
    // DBR)
    if constexpr (addr_arg == Addr::SP) {
      return this->get(REG_ZBR);
    } else {
      return this->get(static_cast<reg8_t>(bank_arg));
    }
  }

  // ========================================================================
  // PHI2/PHI1 BUS SETUP FUNCTIONS (NO MEMORY ACCESS)
  // ========================================================================

  /*
   * REVOLUTIONARY ARCHITECTURE: CPU ONLY DRIVES BUS SIGNALS
   *
   * The CPU no longer performs memory access internally. Instead:
   *
   * PHI2 (Even cycles): CPU sets up bus signals only
   *   - Sets address on bus
   *   - Sets R/W signal
   *   - For writes: sets data on bus
   *   - NO memory callback invocation
   *   - External code performs actual memory operation
   *
   * PHI1 (Odd cycles): CPU samples bus data and performs internal operations
   *   - Reads data from bus pins (if read cycle)
   *   - Performs ALU operations
   *   - Updates internal registers
   *   - Transitions between addressing/operation handlers
   *
   * This matches real 6502 hardware where:
   * - PHI2 = address/control lines stable, memory access window
   * - PHI1 = internal CPU operations, data latching
   */

  /**
   * Core bus setup template - sets up address, R/W, and data (writes only)
   * NO MEMORY ACCESS - external code reads bus state and performs operation
   *
   * @tparam IsWrite true for write operations, false for reads
   * @tparam IsDummy true for dummy cycles (may be optimized out)
   * @tparam addr_arg Which address register to use (PC, AB, SP)
   * @tparam bank_arg Which bank register to use (65C816 only)
   */
  template <bool IsWrite, bool IsDummy, Addr addr_arg,
            Bank bank_arg = Bank::DBR>
  bus_state_t bus_setup(bus_state_t pins, uint8_t data = 0) {
    // Skip dummy cycles if not simulating internal timing
    if constexpr (IsDummy && !Traits.accurate_internal_cycles()) {
      return pins;
    }

    // Get address from the specified address register
    constexpr reg16_t addr_reg = static_cast<reg16_t>(addr_arg);
    uint32_t addr = this->get(addr_reg);

    // 65C816 banking: OR bank into high bits (optimizer eliminates for non-wide
    // CPUs)
    addr |= static_cast<uint32_t>(get_address_bank<addr_arg>(bank_arg)) << 16;

    // Update bus lines (for simulation/test environments)
    if constexpr (Traits.update_bus_lines()) {
      // For 65816: Split 24-bit address into 16-bit address + 8-bit bank
      if constexpr (has_wide_registers()) {
        if (!this->in_emulation_mode()) {
          // Native mode: Set lower 16 bits in address field and upper 8 bits in
          // bank field
          pins = FAM65XX_SET_ADDR(pins, addr & 0xFFFF);
          pins = FAM65XX_SET_BANK(pins, (addr >> 16) & 0xFF);
        } else {
          // Emulation mode: use address field only like 8-bit CPUs
          pins = FAM65XX_SET_ADDR(pins, addr & Traits.address_mask());
        }
      } else {
        // For 8/16-bit CPUs: use address field only (zero overhead)
        pins = FAM65XX_SET_ADDR(pins, addr & Traits.address_mask());
      }

      // Set R/W signal
      if constexpr (IsWrite) {
        pins &= ~FAM65XX_RW;                 // Clear RW for write
        pins = FAM65XX_SET_DATA(pins, data); // Output data for write
      } else {
        pins |= FAM65XX_RW; // Set RW for read
        // External code will put data on bus during memory access
      }
    }

    return pins; // Bus setup complete - NO MEMORY ACCESS
  }

  // ========================================================================
  // CONCRETE BUS SETUP WRAPPERS
  // ========================================================================

  /**
   * Set up bus for read cycle - NO MEMORY ACCESS
   * External code reads address/RW from pins and performs memory operation
   */
  template <Addr addr_reg, Bank bank_arg = Bank::DBR>
  inline bus_state_t bus_setup_read(bus_state_t pins) {
    return bus_setup<false, false, addr_reg, bank_arg>(pins);
  }

  /**
   * Set up bus for write cycle - NO MEMORY ACCESS
   * External code reads address/RW/data from pins and performs memory operation
   */
  template <Addr addr_reg, Bank bank_arg = Bank::DBR>
  inline bus_state_t bus_setup_write(bus_state_t pins, uint8_t data) {
    return bus_setup<true, false, addr_reg, bank_arg>(pins, data);
  }

  /**
   * Set up bus for dummy cycle - NO MEMORY ACCESS
   * May be optimized out if accurate_internal_cycles() is false
   */
  template <Addr addr_reg, Bank bank_arg = Bank::DBR>
  inline bus_state_t bus_setup_dummy(bus_state_t pins) {
    return bus_setup<false, true, addr_reg, bank_arg>(pins);
  }

  // ========================================================================
  // PHI1 DATA LOADING HELPERS
  // ========================================================================

  /**
   * Load register from bus data (PHI1 phase)
   * Single-line helper for clean read handling
   */
  inline void bus_load_reg(reg8_t data_reg, bus_state_t pins) {
    this->set(data_reg, FAM65XX_GET_DATA(pins));
  }

  /**
   * Get data from bus without storing (PHI1 phase)
   * Use when you need the value for calculations
   */
  inline uint8_t bus_get_data(bus_state_t pins) const {
    return FAM65XX_GET_DATA(pins);
  }

  // ========================================================================
  // I/O PORT AND APU HANDLING (INTERNAL TO CPU)
  // ========================================================================

  /**
   * Handle I/O port read (6510 only) - called during PHI1 after external memory
   * tick Returns true if address was I/O port (data placed in pins)
   */
  inline bool handle_io_port_read(bus_state_t &pins, uint32_t addr) {
    if constexpr (Traits.has_io_port()) {
      if (addr <= 0x0001) {
        const uint8_t io_data =
            (addr == 0x0000) ? this->read_io_port() : this->io_port.direction;
        pins = FAM65XX_SET_DATA(pins, io_data);
        return true;
      }
    }
    return false;
  }

  /**
   * Handle I/O port write (6510 only) - called during PHI1 after external
   * memory tick Returns true if address was I/O port
   */
  inline bool handle_io_port_write(uint32_t addr, uint8_t data) {
    if constexpr (Traits.has_io_port()) {
      if (addr <= 0x0001) {
        if (addr == 0x0000) {
          this->write_io_ddr(data);
        } else {
          this->write_io_data(data);
        }
        return true;
      }
    }
    return false;
  }

  /**
   * Handle APU register read (NES 6502 only) - called during PHI1 after
   * external memory tick Returns true if address was APU register (data placed
   * in pins)
   */
  inline bool handle_apu_read(bus_state_t &pins, uint32_t addr) {
    if constexpr (Traits.has_apu()) {
      uint8_t apu_data;
      if (this->read_apu_register(addr, apu_data)) {
        pins = FAM65XX_SET_DATA(pins, apu_data);
        return true;
      }
    }
    return false;
  }

  /**
   * Handle APU register write (NES 6502 only) - called during PHI1 after
   * external memory tick Returns true if address was APU register
   */
  inline bool handle_apu_write(uint32_t addr, uint8_t data) {
    if constexpr (Traits.has_apu()) {
      return this->write_apu_register(addr, data);
    }
    return false;
  }

  // ========================================================================
  // HARDWARE-ACCURATE BCD ARITHMETIC HELPERS (extracted to separate file)
  // ========================================================================

#include "fam65xx_arithmetic.inc.hpp"

  // ========================================================================
  // INTERNAL I/O PORT AND APU ACCESS (CALLED FROM PHI1 HANDLERS)
  // ========================================================================

  /**
   * Get address from pins for internal I/O handling
   * Used by PHI1 handlers to check if I/O port/APU needs handling
   */
  inline uint32_t get_address_from_pins(bus_state_t pins) const {
    uint32_t addr = FAM65XX_GET_ADDR(pins);
    if constexpr (has_wide_registers()) {
      if (!this->in_emulation_mode()) {
        // Native mode: combine address and bank
        addr |= static_cast<uint32_t>(FAM65XX_GET_BANK(pins)) << 16;
      }
    }
    return addr;
  }

  // ========================================================================
  // ADDRESSING AND PAGE CROSSING HELPERS
  // ========================================================================

  /**
   * Check if page boundary was crossed during addressing
   * Used for determining extra cycle penalties
   */
  inline bool page_crossed(uint16_t addr1, uint16_t addr2) const {
    return ((addr1 ^ addr2) & 0x0100) != 0;
  }

  // ========================================================================
  // RMW OPERATION HELPER FUNCTIONS
  // ========================================================================

  /**
   * Unified RMW (Read-Modify-Write) operation helper with constexpr wide
   * support Handles the complex cycle-accurate timing for RMW operations with:
   * - Constexpr wide register support (65C816) using data_t
   * - Automatic 8/16-bit accumulator handling via
   * get_accumulator()/set_accumulator()
   * - Automatic banking fallback (Bank::DBR default handles emulation mode)
   * - Hardware-accurate NMOS/CMOS dummy cycle behavior
   * - Lambda-based operation functions for flexibility
   */
  template <typename OperationFunc>
  bus_state_t rmw_operation_helper(bus_state_t pins,
                                   OperationFunc operation_func) {
    if (this->opcode_entry.flags & to_index(OF::RMW)) {
      // Memory mode - multi-cycle RMW operation (8-bit or 16-bit based on M
      // flag) Determine correct banking based on addressing mode For 65C816,
      // Direct Page operations use Bank 0 (ZBR) instead of DBR
      constexpr bool can_use_direct_page = has_wide_registers();
      const bool is_direct_page_addressing =
          (this->opcode_entry.am_index == to_index(AM::ZER) ||
           this->opcode_entry.am_index == to_index(AM::ZPX) ||
           this->opcode_entry.am_index == to_index(AM::ZPY));

      bool use_zero_bank = false;
      if constexpr (can_use_direct_page) {
        use_zero_bank = is_direct_page_addressing && !this->in_emulation_mode();
      }

      // Check if this is a 16-bit memory operation
      const bool is_16bit_memory = this->is_memory_16bit();

      if (is_16bit_memory) {
        // 16-bit memory RMW operation (6 cycles for 65C816)
        switch (this->cycle_index) {
        case 0:
          // Cycle 0: Read low byte from memory
          if (use_zero_bank) {
            pins = this->/*TODO_READ*/ phi2_read<Addr::AB, Bank::ZBR>(
                pins, REG_DL); // Direct Page uses Bank 0
          } else {
            pins = this->/*TODO_READ*/ phi2_read<Addr::AB, Bank::DBR>(
                pins, REG_DL); // Other modes use DBR
          }

          
          this->cycle_index++;
          return pins;

        case 1:
          // Cycle 1: Read high byte from memory (address + 1)
          this->inc(REG_ABL); // Increment address for high byte
          if (use_zero_bank) {
            pins = this->/*TODO_READ*/ phi2_read<Addr::AB, Bank::ZBR>(
                pins, REG_DPL); // Store high byte in DPL
          } else {
            pins = this->/*TODO_READ*/ phi2_read<Addr::AB, Bank::DBR>(
                pins, REG_DPL); // Store high byte in DPL
          }

          
          this->cycle_index++;
          return pins;

        case 2:
          // Cycle 2: Dummy cycle and perform 16-bit modification
          if (this->has_rmw_dummy_write()) {
            // NMOS: Dummy write of high byte
            if (use_zero_bank) {
              pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::ZBR>(
                  pins, this->get(REG_DPL));
            } else {
              pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::DBR>(
                  pins, this->get(REG_DPL));
            }
          } else {
            // CMOS: Dummy read instead of write
            if (use_zero_bank) {
              pins = this->bus_setup_dummy<Addr::AB, Bank::ZBR>(pins);
            } else {
              pins = this->bus_setup_dummy<Addr::AB, Bank::DBR>(pins);
            }
          }

          
          // Perform 16-bit operation
          data_t value = static_cast<data_t>(this->get(REG_DL)) |
                         (static_cast<data_t>(this->get(REG_DPL)) << 8);
          operation_func(value);
          this->set(REG_DL, static_cast<uint8_t>(value & 0xFF)); // Low byte
          this->set(REG_DPL,
                    static_cast<uint8_t>((value >> 8) & 0xFF)); // High byte
          this->cycle_index++;
          return pins;

        case 3:
          // Cycle 3: Write high byte back to memory (address + 1)
          
          if (use_zero_bank) {
            pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::ZBR>(
                pins, this->get(REG_DPL)); else {
              pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::DBR>(
                  pins, this->get(REG_DPL));
            }
            this->cycle_index++;
          }
          return pins;

        case 4:
          // Cycle 4: Write low byte back to memory (address)
          this->dec(REG_ABL); // Decrement address back to low byte
          
          if (use_zero_bank) {
            pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::ZBR>(
                pins, this->get(REG_DL)); else {
              pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::DBR>(
                  pins, this->get(REG_DL));
            }
            this->transition_to_fetch();
          }
          return pins;
        }
      } else {
        // 8-bit memory RMW operation (3 cycles)
        switch (this->cycle_index) {
        case 0:
          // Cycle 0: Read original value from memory
          if (use_zero_bank) {
            pins = this->/*TODO_READ*/ phi2_read<Addr::AB, Bank::ZBR>(
                pins, REG_DL); // Direct Page uses Bank 0
          } else {
            pins = this->/*TODO_READ*/ phi2_read<Addr::AB, Bank::DBR>(
                pins, REG_DL); // Other modes use DBR
          }

          
          this->cycle_index++;
          return pins;

        case 1:
          // Cycle 1: Dummy cycle and perform modification
          if (this->has_rmw_dummy_write()) {
            // NMOS: Dummy write of original value
            if (use_zero_bank) {
              pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::ZBR>(
                  pins, this->get(REG_DL)); // Direct Page uses Bank 0
            } else {
              pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::DBR>(
                  pins, this->get(REG_DL)); // Other modes use DBR
            }
          } else {
            // CMOS: Dummy read instead of write
            if (use_zero_bank) {
              pins = this->bus_setup_dummy<Addr::AB, Bank::ZBR>(
                  pins); // Direct Page uses Bank 0
            } else {
              pins = this->bus_setup_dummy<Addr::AB, Bank::DBR>(
                  pins); // Other modes use DBR
            }
          }

          
          // For 8-bit memory operations
          data_t value = static_cast<data_t>(this->get(REG_DL));
          operation_func(value);
          this->set(REG_DL, static_cast<uint8_t>(value & 0xFF));
          this->cycle_index++;
          return pins;

        case 2:
          // Cycle 2: Write modified value back to memory
          
          if (use_zero_bank) {
            pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::ZBR>(
                pins, this->get(REG_DL)); // Direct Page uses Bank 0 else {
              pins = this->/*TODO_WRITE*/ phi2_write<Addr::AB, Bank::DBR>(
                  pins, this->get(REG_DL)); // Other modes use DBR
            }
            this->transition_to_fetch();
          }
          return pins;
        }
      }
    } else {
      // Accumulator mode - single cycle operation with automatic 8/16-bit
      // handling
      pins = this->bus_setup_dummy<Addr::PC>(pins);
      
      data_t value = this->get_accumulator(); // Automatically handles
                                              // 8/16-bit based on M flag
      operation_func(value);
      this->set_accumulator(
          value); // Automatically handles 8/16-bit based on M flag
      this->transition_to_fetch();
    }
    return pins;
  }

// ========================================================================
// OPERATION IMPLEMENTATIONS (included via .inc.hpp files)
// ========================================================================

// Include all operation implementations
// These .inc.hpp files contain function definitions that will be compiled
// as part of this template class, allowing conditional compilation
// based on CPUTraits features

// Define template context guard for .inc.hpp files BEFORE including them
#define FAM65XX_TEMPLATE_CONTEXT

#include "operations/addressing_modes.inc.hpp" // Addressing mode handlers
#include "operations/arithmetic.inc.hpp"       // ADC, SBC, CMP operations
#include "operations/branches.inc.hpp"         // Branch operations
#include "operations/cmos.inc.hpp"             // 65C02 enhancements
#include "operations/control.inc.hpp"          // Control flow operations
#include "operations/flags.inc.hpp"            // Flag manipulation operations
#include "operations/illegal.inc.hpp"          // Illegal/undocumented opcodes
#include "operations/memory.inc.hpp"           // Load/store operations
#include "operations/rmw.inc.hpp"              // Read-modify-write operations
#include "operations/rockwell.inc.hpp"  // Rockwell 65C02 bit manipulation
#include "operations/stack.inc.hpp"     // Stack operations
#include "operations/transfers.inc.hpp" // Register transfer operations
#include "operations/wide.inc.hpp"      // 65C816 16-bit operations

// Undefine the guard after inclusion
#undef FAM65XX_TEMPLATE_CONTEXT

  // ========================================================================
  // EMULATION HELPER FUNCTIONS AND DECLARATIONS
  // ========================================================================

  // Hardware-accurate interrupt detection with priority-order processing
  bool process_interrupt_detection(bus_state_t pins) {
    // Load state into registers to reduce memory accesses
    uint32_t shift_reg = this->interrupt_shift_register;

    // Shift and clear separators (prevent cross-over) in one operation
    shift_reg = (shift_reg << 1) & ~INT_SEPARATOR_MASK;

    // Extract interrupt pins (bits 32-34) and invert (active low)
    // After shift: bit 0=RES, bit 1=IRQ, bit 2=NMI
    uint32_t int_pins = (~pins) >> BUS_RES_BIT;

    // Pin bit offsets after extraction
    constexpr uint8_t IRQ_OFFSET = BUS_IRQ_BIT - BUS_RES_BIT; // 1
    constexpr uint8_t NMI_OFFSET = BUS_NMI_BIT - BUS_RES_BIT; // 2

    // Sample IRQ (extracted bit 1 -> shift_reg bit 0) - only if IRQ line exists
    if constexpr (has_irq_line()) {
      shift_reg |= (int_pins >> IRQ_OFFSET) & (1 << INT_IRQ_START_BIT);
    }

    // NMI edge detection (extracted bit 2) - only if NMI line exists
    if constexpr (has_nmi_line()) {
      uint8_t nmi_current = (int_pins >> NMI_OFFSET) & 0x1;
      shift_reg |=
          (-(this->nmi_prev & !nmi_current)) & (1 << INT_NMI_START_BIT);
      this->nmi_prev = nmi_current;
    }

    // Sample RESET (extracted bit 0 -> shift_reg bit 8)
    shift_reg |= (int_pins & 0x1) << INT_RESET_START_BIT;

    // Store updated state
    this->interrupt_shift_register = shift_reg;

    // Check for completed interrupt sequences in priority order (highest first)
    // Set active_interrupt to highest priority interrupt detected

    // Check RESET first (highest priority)
    if ((shift_reg & INT_RESET_MASK) == INT_RESET_MASK) {
      this->active_interrupt = FAM65XX_INT_RESET;
      return true;
    }

    // Check ABORT (65C816 only, second highest)
    if constexpr (has_wide_registers()) {
      if (!this->in_emulation_mode()) {
        // Native mode: ABORT detection logic would go here when implemented
        // For now, ABORT is not connected to hardware pins
      }
      // Emulation mode: ABORT is not available (fallback for wide registers)
    }

    // Check NMI (third highest priority)
    if constexpr (has_nmi_line()) {
      if ((shift_reg & INT_NMI_MASK) == INT_NMI_MASK) {
        this->active_interrupt = FAM65XX_INT_NMI;
        return true;
      }
    }

    // Check COP (65C816 software interrupt, fourth priority)
    // COP is triggered by software, not hardware pins - handled elsewhere

    // Check IRQ (fifth priority, maskable by I flag)
    if constexpr (has_irq_line()) {
      if ((shift_reg & INT_IRQ_MASK) == INT_IRQ_MASK &&
          !(this->get(REG_P) & FLAG_I)) {
        this->active_interrupt = FAM65XX_INT_IRQ;
        return true;
      }
    }

    // Check BRK (software interrupt, sixth priority)
    // BRK is triggered by software, not hardware pins - handled in op_brk

    // No interrupt detected
    return false;
  }

  // Template-dependent function pointer type
  using InstructionHandler = bus_state_t (fam65xx_t<Traits>::*)(bus_state_t);

  // Lookup tables for handlers (initialized during init)
  std::array<InstructionHandler, to_index(OP::COUNT)> operation_handlers;
  std::array<InstructionHandler, to_index(AM::COUNT)> addressing_mode_handlers;

  inline InstructionHandler get_instruction_handler() {
    // Validate op_index bounds
    if (this->opcode_entry.op_index >= to_index(OP::COUNT)) {
      printf("ERROR: op_index %d >= COUNT %d\n", this->opcode_entry.op_index,
             to_index(OP::COUNT));
      return &fam65xx_t::op_nop; // Safe fallback
    }

    // Validate am_index bounds
    if (this->opcode_entry.am_index >= to_index(AM::COUNT)) {
      printf("ERROR: am_index %d >= COUNT %d\n", this->opcode_entry.am_index,
             to_index(AM::COUNT));
      return &fam65xx_t::op_nop; // Safe fallback
    }

    // For addressing modes that need address calculation, start with addressing
    // mode handler
    if (this->opcode_entry.am_index > to_index(AM::IMM)) {
      InstructionHandler am_handler =
          addressing_mode_handlers[this->opcode_entry.am_index];

      // VALIDATION: Check for null addressing mode handler
      if (am_handler == nullptr) {
        printf("ERROR: NULL addressing mode handler for am_index=%d\n",
               this->opcode_entry.am_index);
        return &fam65xx_t::op_nop; // Safe fallback
      }

      return am_handler;
    }

    // Get operation handler
    InstructionHandler op_handler =
        this->operation_handlers[this->opcode_entry.op_index];

    // VALIDATION: Check for null operation handler
    if (op_handler == nullptr) {
      printf("ERROR: NULL operation handler for op_index=%d\n",
             this->opcode_entry.op_index);
      return &fam65xx_t::op_nop; // Safe fallback
    }

    return op_handler;
  }

  void transition_to_opcode(const opcode_info_t entry) {
    this->opcode_entry = entry;
    this->cycle_index = 0;
    // Set up first instruction cycle handler
    this->current_handler = this->get_instruction_handler();
  }

  // Helper method for calling current handler with proper member function
  // pointer syntax
  inline bus_state_t call_current_handler(bus_state_t pins) {
    return (this->*current_handler)(pins);
  }

  // Instruction fetch and decode
  bus_state_t fetch_opcode(bus_state_t pins) {
    // Read opcode from PC (using program banking PBR for 65C816)
    pins = this->/*TODO_READ*/ phi2_read<Addr::PC>(pins, REG_IR);
    this->set(REG_AB, this->get(REG_PC));
    this->inc(REG_PC);
    // Set SYNC signal for opcode fetch
    pins |= FAM65XX_SYNC;
    // Decode opcode and set up instruction
    uint8_t opcode = this->get(REG_IR);

    opcode_info_t entry = get_opcode_info(opcode);
    this->transition_to_opcode(entry);

    return pins;
  }

  // Transition to next instruction fetch (public for bootstrap function)
  void transition_to_fetch() {
    this->current_handler = &fam65xx_t::fetch_opcode;
    this->cycle_index = 0;
  }

  void transition_to_operation() {
    this->cycle_index = 0;
    this->current_handler =
        this->operation_handlers[this->opcode_entry.op_index];
  }

  // ========================================================================
  // PROCESSOR-SPECIFIC INITIALIZATION
  // ========================================================================

  void init_conditional_features() {
    // Initialize I/O port if present
    if constexpr (has_io_port()) {
      this->init_io_port();
    }

    // Initialize APU if present
    if constexpr (has_apu()) {
      this->init_apu();
    }

    // Register initialization is handled by register mixin
  }

  void init_opcode_table() {
    // Initialize operation handler lookup table with NOP as safe default
    operation_handlers.fill(&fam65xx_t::op_nop);

    // Sorted alphabetically for easier maintenance
    operation_handlers[to_index(OP::ADC)] = &fam65xx_t::op_adc;
    operation_handlers[to_index(OP::ANC)] = &fam65xx_t::op_anc;
    operation_handlers[to_index(OP::AND)] = &fam65xx_t::op_and;
    operation_handlers[to_index(OP::ARR)] = &fam65xx_t::op_arr;
    operation_handlers[to_index(OP::ASL)] = &fam65xx_t::op_asl;
    operation_handlers[to_index(OP::ASR)] = &fam65xx_t::op_asr;
    operation_handlers[to_index(OP::BBR0)] = &fam65xx_t::op_bbr0;
    operation_handlers[to_index(OP::BBR1)] = &fam65xx_t::op_bbr1;
    operation_handlers[to_index(OP::BBR2)] = &fam65xx_t::op_bbr2;
    operation_handlers[to_index(OP::BBR3)] = &fam65xx_t::op_bbr3;
    operation_handlers[to_index(OP::BBR4)] = &fam65xx_t::op_bbr4;
    operation_handlers[to_index(OP::BBR5)] = &fam65xx_t::op_bbr5;
    operation_handlers[to_index(OP::BBR6)] = &fam65xx_t::op_bbr6;
    operation_handlers[to_index(OP::BBR7)] = &fam65xx_t::op_bbr7;
    operation_handlers[to_index(OP::BBS0)] = &fam65xx_t::op_bbs0;
    operation_handlers[to_index(OP::BBS1)] = &fam65xx_t::op_bbs1;
    operation_handlers[to_index(OP::BBS2)] = &fam65xx_t::op_bbs2;
    operation_handlers[to_index(OP::BBS3)] = &fam65xx_t::op_bbs3;
    operation_handlers[to_index(OP::BBS4)] = &fam65xx_t::op_bbs4;
    operation_handlers[to_index(OP::BBS5)] = &fam65xx_t::op_bbs5;
    operation_handlers[to_index(OP::BBS6)] = &fam65xx_t::op_bbs6;
    operation_handlers[to_index(OP::BBS7)] = &fam65xx_t::op_bbs7;
    operation_handlers[to_index(OP::BCC)] = &fam65xx_t::op_bcc;
    operation_handlers[to_index(OP::BCS)] = &fam65xx_t::op_bcs;
    operation_handlers[to_index(OP::BEQ)] = &fam65xx_t::op_beq;
    operation_handlers[to_index(OP::BIT)] = &fam65xx_t::op_bit;
    operation_handlers[to_index(OP::BMI)] = &fam65xx_t::op_bmi;
    operation_handlers[to_index(OP::BNE)] = &fam65xx_t::op_bne;
    operation_handlers[to_index(OP::BPL)] = &fam65xx_t::op_bpl;
    operation_handlers[to_index(OP::BRA)] = &fam65xx_t::op_bra;
    operation_handlers[to_index(OP::BRK)] = &fam65xx_t::op_brk;
    operation_handlers[to_index(OP::BVC)] = &fam65xx_t::op_bvc;
    operation_handlers[to_index(OP::BVS)] = &fam65xx_t::op_bvs;
    operation_handlers[to_index(OP::CLC)] = &fam65xx_t::op_clc;
    operation_handlers[to_index(OP::CLD)] = &fam65xx_t::op_cld;
    operation_handlers[to_index(OP::CLI)] = &fam65xx_t::op_cli;
    operation_handlers[to_index(OP::CLV)] = &fam65xx_t::op_clv;
    operation_handlers[to_index(OP::CMP)] = &fam65xx_t::op_cmp;
    operation_handlers[to_index(OP::COP)] = &fam65xx_t::op_cop;
    operation_handlers[to_index(OP::CPX)] = &fam65xx_t::op_cpx;
    operation_handlers[to_index(OP::CPY)] = &fam65xx_t::op_cpy;
    operation_handlers[to_index(OP::DCP)] = &fam65xx_t::op_dcp;
    operation_handlers[to_index(OP::DEC)] = &fam65xx_t::op_dec;
    operation_handlers[to_index(OP::DEX)] = &fam65xx_t::op_dex;
    operation_handlers[to_index(OP::DEY)] = &fam65xx_t::op_dey;
    operation_handlers[to_index(OP::EOR)] = &fam65xx_t::op_eor;
    operation_handlers[to_index(OP::INC)] = &fam65xx_t::op_inc;
    operation_handlers[to_index(OP::INX)] = &fam65xx_t::op_inx;
    operation_handlers[to_index(OP::INY)] = &fam65xx_t::op_iny;
    operation_handlers[to_index(OP::ISC)] = &fam65xx_t::op_isc;
    operation_handlers[to_index(OP::JAM)] = &fam65xx_t::op_jam;
    operation_handlers[to_index(OP::JML)] = &fam65xx_t::op_jml;
    operation_handlers[to_index(OP::JMP)] = &fam65xx_t::op_jmp;
    operation_handlers[to_index(OP::JSL)] = &fam65xx_t::op_jsl;
    operation_handlers[to_index(OP::JSR)] = &fam65xx_t::op_jsr;
    operation_handlers[to_index(OP::LAS)] = &fam65xx_t::op_las;
    operation_handlers[to_index(OP::LAX)] = &fam65xx_t::op_lax;
    operation_handlers[to_index(OP::LDA)] = &fam65xx_t::op_lda;
    operation_handlers[to_index(OP::LDX)] = &fam65xx_t::op_ldx;
    operation_handlers[to_index(OP::LDY)] = &fam65xx_t::op_ldy;
    operation_handlers[to_index(OP::LSR)] = &fam65xx_t::op_lsr;
    operation_handlers[to_index(OP::MVN)] = &fam65xx_t::op_mvn;
    operation_handlers[to_index(OP::MVP)] = &fam65xx_t::op_mvp;
    operation_handlers[to_index(OP::NOP)] = &fam65xx_t::op_nop;
    operation_handlers[to_index(OP::ORA)] = &fam65xx_t::op_ora;
    operation_handlers[to_index(OP::PEA)] = &fam65xx_t::op_pea;
    operation_handlers[to_index(OP::PEI)] = &fam65xx_t::op_pei;
    operation_handlers[to_index(OP::PER)] = &fam65xx_t::op_per;
    operation_handlers[to_index(OP::PHA)] = &fam65xx_t::op_pha;
    operation_handlers[to_index(OP::PHB)] = &fam65xx_t::op_phb;
    operation_handlers[to_index(OP::PHD)] = &fam65xx_t::op_phd;
    operation_handlers[to_index(OP::PHK)] = &fam65xx_t::op_phk;
    operation_handlers[to_index(OP::PHP)] = &fam65xx_t::op_php;
    operation_handlers[to_index(OP::PHX)] = &fam65xx_t::op_phx;
    operation_handlers[to_index(OP::PHY)] = &fam65xx_t::op_phy;
    operation_handlers[to_index(OP::PLA)] = &fam65xx_t::op_pla;
    operation_handlers[to_index(OP::PLB)] = &fam65xx_t::op_plb;
    operation_handlers[to_index(OP::PLD)] = &fam65xx_t::op_pld;
    operation_handlers[to_index(OP::PLP)] = &fam65xx_t::op_plp;
    operation_handlers[to_index(OP::PLX)] = &fam65xx_t::op_plx;
    operation_handlers[to_index(OP::PLY)] = &fam65xx_t::op_ply;
    operation_handlers[to_index(OP::REP)] = &fam65xx_t::op_rep;
    operation_handlers[to_index(OP::RLA)] = &fam65xx_t::op_rla;
    operation_handlers[to_index(OP::RMB0)] = &fam65xx_t::op_rmb0;
    operation_handlers[to_index(OP::RMB1)] = &fam65xx_t::op_rmb1;
    operation_handlers[to_index(OP::RMB2)] = &fam65xx_t::op_rmb2;
    operation_handlers[to_index(OP::RMB3)] = &fam65xx_t::op_rmb3;
    operation_handlers[to_index(OP::RMB4)] = &fam65xx_t::op_rmb4;
    operation_handlers[to_index(OP::RMB5)] = &fam65xx_t::op_rmb5;
    operation_handlers[to_index(OP::RMB6)] = &fam65xx_t::op_rmb6;
    operation_handlers[to_index(OP::RMB7)] = &fam65xx_t::op_rmb7;
    operation_handlers[to_index(OP::ROL)] = &fam65xx_t::op_rol;
    operation_handlers[to_index(OP::ROR)] = &fam65xx_t::op_ror;
    operation_handlers[to_index(OP::RRA)] = &fam65xx_t::op_rra;
    operation_handlers[to_index(OP::RTI)] = &fam65xx_t::op_rti;
    operation_handlers[to_index(OP::RTL)] = &fam65xx_t::op_rtl;
    operation_handlers[to_index(OP::RTS)] = &fam65xx_t::op_rts;
    operation_handlers[to_index(OP::SAX)] = &fam65xx_t::op_sax;
    operation_handlers[to_index(OP::SBC)] = &fam65xx_t::op_sbc;
    operation_handlers[to_index(OP::SBX)] = &fam65xx_t::op_sbx;
    operation_handlers[to_index(OP::SEC)] = &fam65xx_t::op_sec;
    operation_handlers[to_index(OP::SED)] = &fam65xx_t::op_sed;
    operation_handlers[to_index(OP::SEI)] = &fam65xx_t::op_sei;
    operation_handlers[to_index(OP::SEP)] = &fam65xx_t::op_sep;
    operation_handlers[to_index(OP::SHA)] = &fam65xx_t::op_sha;
    operation_handlers[to_index(OP::SHS)] = &fam65xx_t::op_shs;
    operation_handlers[to_index(OP::SHX)] = &fam65xx_t::op_shx;
    operation_handlers[to_index(OP::SHY)] = &fam65xx_t::op_shy;
    operation_handlers[to_index(OP::SLO)] = &fam65xx_t::op_slo;
    operation_handlers[to_index(OP::SMB0)] = &fam65xx_t::op_smb0;
    operation_handlers[to_index(OP::SMB1)] = &fam65xx_t::op_smb1;
    operation_handlers[to_index(OP::SMB2)] = &fam65xx_t::op_smb2;
    operation_handlers[to_index(OP::SMB3)] = &fam65xx_t::op_smb3;
    operation_handlers[to_index(OP::SMB4)] = &fam65xx_t::op_smb4;
    operation_handlers[to_index(OP::SMB5)] = &fam65xx_t::op_smb5;
    operation_handlers[to_index(OP::SMB6)] = &fam65xx_t::op_smb6;
    operation_handlers[to_index(OP::SMB7)] = &fam65xx_t::op_smb7;
    operation_handlers[to_index(OP::SRE)] = &fam65xx_t::op_sre;
    operation_handlers[to_index(OP::STA)] = &fam65xx_t::op_sta;
    operation_handlers[to_index(OP::STP)] = &fam65xx_t::op_stp;
    operation_handlers[to_index(OP::STX)] = &fam65xx_t::op_stx;
    operation_handlers[to_index(OP::STY)] = &fam65xx_t::op_sty;
    operation_handlers[to_index(OP::STZ)] = &fam65xx_t::op_stz;
    operation_handlers[to_index(OP::TAX)] = &fam65xx_t::op_tax;
    operation_handlers[to_index(OP::TAY)] = &fam65xx_t::op_tay;
    operation_handlers[to_index(OP::TRB)] = &fam65xx_t::op_trb;
    operation_handlers[to_index(OP::TSB)] = &fam65xx_t::op_tsb;
    operation_handlers[to_index(OP::TSX)] = &fam65xx_t::op_tsx;
    operation_handlers[to_index(OP::TXA)] = &fam65xx_t::op_txa;
    operation_handlers[to_index(OP::TXS)] = &fam65xx_t::op_txs;
    operation_handlers[to_index(OP::TYA)] = &fam65xx_t::op_tya;
    operation_handlers[to_index(OP::WAI)] = &fam65xx_t::op_wai;
    operation_handlers[to_index(OP::WDM)] = &fam65xx_t::op_wdm;
    operation_handlers[to_index(OP::XAA)] = &fam65xx_t::op_xaa;
    operation_handlers[to_index(OP::XBA)] = &fam65xx_t::op_xba;
    operation_handlers[to_index(OP::XCE)] = &fam65xx_t::op_xce;

    // Initialize addressing mode handler lookup table
    addressing_mode_handlers.fill(
        nullptr); // Default to nullptr (safe for AM_NON/AM_IMM)

    // Addressing modes (no handler needed)
    addressing_mode_handlers[to_index(AM::NON)] =
        nullptr; // No handler needed (implicit/accumulator/relative)
    addressing_mode_handlers[to_index(AM::IMM)] =
        nullptr; // No handler (handled directly in operations)

    // Addressing modes
    addressing_mode_handlers[to_index(AM::ABS)] = &fam65xx_t::am_abs;
    addressing_mode_handlers[to_index(AM::ABX)] = &fam65xx_t::am_abx;
    addressing_mode_handlers[to_index(AM::ABY)] = &fam65xx_t::am_aby;
    addressing_mode_handlers[to_index(AM::IND)] = &fam65xx_t::am_ind;
    addressing_mode_handlers[to_index(AM::INX)] = &fam65xx_t::am_inx;
    addressing_mode_handlers[to_index(AM::INY)] = &fam65xx_t::am_iny;

    // Rockwell 65C02 addressing modes
    addressing_mode_handlers[to_index(AM::ZPR)] =
        &fam65xx_t::am_zpr; // Zero Page Relative - BBR/BBS $nn,$offset

    // 6502/6510 and 65C816 addressing modes (with pre-65C816 compatibility)
    addressing_mode_handlers[to_index(AM::DP)] =
        &fam65xx_t::am_dp; // Direct/Zero Page (aliassed to AM::ZER, am_zp)
                           // implementation for pre-65C816 compatibility
    addressing_mode_handlers[to_index(AM::DPX)] =
        &fam65xx_t::am_dpx; // Direct/Zero Page,X (aliassed to AM::ZPX, am_zpx)
                            // implementation for pre-65C816 compatibility
    addressing_mode_handlers[to_index(AM::DPY)] =
        &fam65xx_t::am_dpy; // Direct/Zero Page,Y (aliassed to AM::ZPY, am_zpy)
                            // implementation for pre-65C816 compatibility

    // 65C02 and 65C816 addressing modes
    addressing_mode_handlers[to_index(AM::DPI)] =
        &fam65xx_t::am_dpi; // Direct/Zero Page Indirect (maps to AM::ZPI,
                            // am_zpi) implementation for pre-65C816
                            // compatibility

    // Initialize 65C816 exclusive addressing modes (some native map to
    // emulation modes)
    addressing_mode_handlers[to_index(AM::ABI)] =
        &fam65xx_t::am_abi; // Absolute Indexed Indirect (abs,X) - JMP/JSR
                            // ($nnnn,X)
    addressing_mode_handlers[to_index(AM::ABL)] =
        &fam65xx_t::am_abl; // Absolute Long
    addressing_mode_handlers[to_index(AM::ABLX)] =
        &fam65xx_t::am_ablx; // Absolute Long,X
    addressing_mode_handlers[to_index(AM::DPIL)] =
        &fam65xx_t::am_dpil; // Direct Page Indirect Long
    addressing_mode_handlers[to_index(AM::DPILY)] =
        &fam65xx_t::am_dpily; // Direct Page Indirect Long,Y
    addressing_mode_handlers[to_index(AM::SR)] =
        &fam65xx_t::am_sr; // Stack Relative
    addressing_mode_handlers[to_index(AM::SRI)] =
        &fam65xx_t::am_sri; // Stack Relative Indirect Indexed
  }

  opcode_info_t get_opcode_info(uint8_t opcode) const {
    return generate_opcode_table<Traits>()[opcode];
  }

public:
  // ========================================================================
  // EMULATION MODE AND REGISTER WIDTH DETECTION (moved from mixins)
  // ========================================================================

  /**
   * Set emulation mode (65C816 specific)
   * Uses constexpr wide check for compile-time optimization
   */
  inline void set_emulation_mode(bool mode) {
    if constexpr (has_wide_registers()) {
      this->update_flag(FLAG_E, mode);
    }
    // Non-65C816 processors: no-op (always in emulation mode)
  }

  /**
   * Check if CPU is in emulation mode (65C816 specific)
   * Uses constexpr wide check for compile-time optimization
   */
  inline bool in_emulation_mode() const {
    if constexpr (has_wide_registers()) {
      return (this->get(REG_P_16) & FLAG_E) != 0;
    } else {
      // Non-65C816 processors are always in "emulation mode" (6502
      // compatibility)
      return true;
    }
  }

  /**
   * Check if accumulator is in 16-bit mode
   * Uses constexpr wide check for compile-time optimization
   */
  inline bool is_accumulator_16bit() const {
    if constexpr (has_wide_registers()) {
      // 16-bit when BOTH emulation=0 AND M=0
      return !(this->get(REG_P_16) & (FLAG_E | FLAG_M));
    } else {
      // Non-65C816 processors: always 8-bit
      return false;
    }
  }

  /**
   * Check if memory operations are in 16-bit mode
   * Uses constexpr wide check for compile-time optimization
   */
  inline bool is_memory_16bit() const {
    if constexpr (has_wide_registers()) {
      // Memory operations are 16-bit when BOTH emulation=0 AND M=0 (same as
      // accumulator)
      return !(this->get(REG_P_16) & (FLAG_E | FLAG_M));
    } else {
      // Non-65C816 processors: always 8-bit
      return false;
    }
  }

  /**
   * Check if index registers are in 16-bit mode
   * Uses constexpr wide check for compile-time optimization
   */
  inline bool is_index_16bit() const {
    if constexpr (has_wide_registers()) {
      // 16-bit when BOTH emulation=0 AND X=0
      return !(this->get(REG_P_16) & (FLAG_E | FLAG_X));
    } else {
      // Non-65C816 processors: always 8-bit
      return false;
    }
  }

  bus_state_t reset(bus_state_t pins) {
    // Reset registers properly (including emulation mode for 65C816)
    this->init_registers();

    // Reset interrupt state
    this->nmi_prev = 0;
    this->interrupt_shift_register = 0;

    // Reset 65C02 extended state
    this->wait_for_interrupt = false;
    this->stopped = false;

    // Reset processor-specific features
    this->init_conditional_features();

    // Use unified interrupt handler for vector loading
    // Skip stack operations (cycles 0-3) and jump to vector loading (cycles
    // 4-5)
    this->active_interrupt = FAM65XX_INT_RESET;

    // Set up opcode_entry for BRK (opcode $00) so tracing shows correct
    // instruction
    this->opcode_entry = get_opcode_info(0x00); // = {OP_BRK, AM_NON, OF_NONE};
    this->current_handler = &fam65xx_t::op_brk;
    this->cycle_index = 4;                      // Jump to vector loading phase
    this->set(REG_AB, this->get_vector_addr()); // Do the same memory setup as
                                                // preceding op_brk cycle 3

    return pins;
  }

  bus_state_t bootstrap(bus_state_t pins) {
    // Bootstrap CPU for immediate execution (test runner compatibility)
    // Ported from original fam65xx implementation

    /* Set up for immediate instruction execution without RESET sequence */
    pins |= FAM65XX_RDY; /* Ensure RDY is high for execution */
    pins |= FAM65XX_RW;  /* Ensure RW is set as default state */
    pins |= FAM65XX_IRQ; /* IRQ line high (inactive) */
    pins |= FAM65XX_NMI; /* NMI line high (inactive) */
    pins |= FAM65XX_RES; /* RESET line high (inactive) */

    /* Clear any interrupt flags that might have been set */
    this->active_interrupt = FAM65XX_INT_NONE;

    /* CRITICAL: Reset interrupt shift register to prevent false triggers */
    this->interrupt_shift_register =
        0x00000000;     /* No interrupt activity detected yet */
    this->nmi_prev = 1; /* NMI line starts high (inactive) for edge detection */

    /* Set up for instruction fetch - CPU ready to execute next instruction */
    this->transition_to_fetch();

    return pins;
  }

  // ========================================================================
  // TEMPLATE-BASED PHI2/PHI1 TICK FUNCTIONS
  // ========================================================================

  /**
   * Phase enum for template parameter - zero runtime overhead
   */
  enum class Phase { PHI2, PHI1 };

  /**
   * Template tick function - compile-time phase selection
   *
   * @tparam phase PHI2 for bus setup, PHI1 for internal operations
   * @param pins Current bus state
   * @return Updated bus state
   */
  template <Phase phase> bus_state_t tick(bus_state_t pins) {
    if constexpr (phase == Phase::PHI2) {
      // PHI2: Bus setup phase
      trace_enter("tick<PHI2>");
      trace_registers("before PHI2");

      // SYNC pin management - asserted during opcode fetch
      if (this->current_handler == &fam65xx_t::fetch_opcode &&
          this->cycle_index == 0) {
        pins |= FAM65XX_SYNC;
        trace("SYNC asserted (opcode fetch)");
      } else {
        pins &= ~FAM65XX_SYNC;
      }

      // Hardware-accurate interrupt detection
      if (this->process_interrupt_detection(pins)) {
        if (this->active_interrupt == FAM65XX_INT_RESET) {
          return reset(pins);
        } else if (this->current_handler == &fam65xx_t::fetch_opcode &&
                   this->cycle_index == 0) {
          this->current_handler = &fam65xx_t::op_brk;
        }
      }

      // Check RDY signal - CENTRALIZED CHECK (KEEP THIS!)
      if (!FAM65XX_GET_RDY(pins)) {
        // RDY low - external DMA active
        // DO NOT call handler, DO NOT increment
        trace("RDY low - DMA active, skipping handler");
        trace_exit("tick<PHI2>");
        return pins;
      }

      // Call handler to set up bus
      if (this->current_handler != nullptr) {
        pins = this->call_current_handler(pins);
      } else {
        trace("No handler - starting fetch_opcode");
        pins = this->fetch_opcode(pins);
      }

      // tick<PHI2> increments from even to odd
      cycle_index++;

      trace_registers("after PHI2");
      trace_exit("tick<PHI2>");

    } else { // Phase::PHI1
      // PHI1: Internal operation phase
      trace_enter("tick<PHI1>");
      trace_registers("before PHI1");

      // Call handler to perform internal operations
      // Handler will increment cycle_index by 1 (odd → even)
      if (this->current_handler != nullptr) {
        pins = this->call_current_handler(pins);
      }

      // Clock APU if present
      if constexpr (has_apu()) {
        pins = this->clock_apu(pins);
      }

      // Print instruction trace for debugging
      this->print_instruction_trace();

      trace_registers("after PHI1");
      trace_exit("tick<PHI1>");
    }

    return pins;
  }

  /**
   * Legacy tick() function for backward compatibility
   *
   * DEPRECATED: Use tick<Phase::PHI2>() and tick<Phase::PHI1>() instead
   * This function is maintained temporarily for test compatibility
   */
  bus_state_t tick(bus_state_t pins) {
    // Simulate combined PHI2+PHI1 cycle
    // This is NOT hardware-accurate but maintains test compatibility

    trace_enter("tick (legacy)");

    // PHI2 phase
    pins = tick<Phase::PHI2>(pins);

    // PHI1 phase (only if not waiting for RDY)
    
    pins = tick<Phase::PHI1>(pins);

    trace_exit("tick (legacy)");
    return pins;
  }

  bool opdone() const {
    return this->current_handler == &fam65xx_t::fetch_opcode;
  }

  // ========================================================================
  // CONSTRUCTOR AND INITIALIZATION
  // ========================================================================

  bus_state_t init(const chip_descriptor_t * /*desc*/) {
    // Initialize processor-specific features
    this->init_conditional_features();

    // Return initial pin state
    bus_state_t pins = 0;
    return pins;
  }

  fam65xx_t() {
    // Initialize CPU state to zero
    opcode_entry = {};
    current_handler = nullptr;
    cycle_index = 0;
    active_interrupt = FAM65XX_INT_NONE;
    nmi_prev = 0;
    interrupt_shift_register = 0;
    wait_for_interrupt = false;
    stopped = false;
    trace_indent = 0;

    // Initialize operation and addressing mode handlers
    this->init_opcode_table();

    // Initialize conditional features
    this->init_conditional_features();

    // Initialize registers
    this->init_registers();
  }

  ~fam65xx_t() {
    // Cleanup conditional features
    if constexpr (has_apu()) {
      this->destroy_apu();
    }
  }

  // ========================================================================
  // CPU STATE
  // ========================================================================

  /* Current execution state */
  opcode_info_t opcode_entry; /* Cached opcode entry (copied once) */
  bus_state_t (fam65xx_t::*current_handler)(
      bus_state_t);    /* Current instruction handler */
  uint8_t cycle_index; /* Current cycle within instruction */

  /* Interrupt state - hardware-accurate shift register system */
  interrupt_t active_interrupt; /* Currently active interrupt (enum serves as
                                   vector index) */
  uint8_t nmi_prev;             /* Previous NMI line state for edge detection */
  uint32_t interrupt_shift_register; /* Combined shift register for all
                                        interrupt types */

  /* 65C02 extended state */
  bool wait_for_interrupt; /* WAI instruction state */
  bool stopped;            /* STP instruction state */

  /* Debug tracing state */
  static constexpr bool ENABLE_TRACING = false; /* Compile-time tracing flag */
  mutable int trace_indent; /* Current tracing indentation level */
};

// ============================================================================
// MSVC COMPATIBILITY TYPE ALIASES
// ============================================================================

// Create concrete type aliases for MSVC template compatibility
// MSVC has stricter requirements for non-type template parameters
using mos6502_cpu_impl_t = fam65xx_t<MOS6502>;
using mos6510_cpu_impl_t = fam65xx_t<MOS6510>;
using nes6502_cpu_impl_t = fam65xx_t<RICOH_2A03>;
using wdc65c02_cpu_impl_t = fam65xx_t<WDC_65C02_EARLY>;
using rockwell65c02_cpu_impl_t = fam65xx_t<ROCKWELL_R65C02>;
using wdc65c816_cpu_impl_t = fam65xx_t<WDC_65C816>;

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS (required for MSVC compatibility)
// ============================================================================

// Explicit template instantiations for all CPU variants
// This ensures MSVC can properly resolve template parameters
template class fam65xx_t<MOS6502>;
template class fam65xx_t<MOS6510>;
template class fam65xx_t<RICOH_2A03>;
template class fam65xx_t<WDC_65C02_EARLY>;
template class fam65xx_t<ROCKWELL_R65C02>;
template class fam65xx_t<WDC_65C816>;

// ============================================================================
// OPCODE TABLE GENERATION (processor-specific specializations were included
// above)
// ============================================================================

} // namespace fam65xx
