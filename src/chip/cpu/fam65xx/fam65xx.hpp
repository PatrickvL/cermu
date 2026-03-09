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
//#include <type_traits>

// Include instruction decoder for disassembly (outside extern "C" to avoid template linkage issues)
#include "fam65xx_decoder.h"

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
// SHARED OPCODE TABLE (deduplicated across compatible CPU variants)
// ============================================================================
// The opcode table content depends only on instruction-set flags (CMOS_BASE,
// ROCKWELL_BITS, WAI_STP, C816_16BIT, CE02_EXTENDED, HUC6280_EXTENDED).
// Non-instruction-set flags (HAS_IO_PORT, NO_IRQ_LINE, HAS_DECIMAL_MODE, etc.)
// do NOT affect which opcodes appear in the table — those differences are
// handled at runtime by the operation handlers.
//
// By keying the table on only the relevant flags, all NMOS CPUs (MOS6502,
// MOS6510, MOS6507, CSG7501, RICOH_2A03) share a single static table instead
// of carrying 5 identical copies.

// Bitmask of core_flags bits that affect opcode table content
inline constexpr uint32_t OPCODE_TABLE_FLAGS_MASK =
    CPUCoreFlags::CMOS_BASE | CPUCoreFlags::ROCKWELL_BITS |
    CPUCoreFlags::WAI_STP | CPUCoreFlags::CE02_EXTENDED |
    CPUCoreFlags::C816_16BIT | CPUCoreFlags::HUC6280_EXTENDED;

constexpr uint32_t opcode_table_key(const CPUTraits &t) {
  return t.core_flags & OPCODE_TABLE_FLAGS_MASK;
}

// Shared table holder — one instantiation per unique key value.
// A canonical CPUTraits is synthesized with only the relevant flags so that
// generate_opcode_table_for_traits produces the correct table.
template <uint32_t Key> struct SharedOpcodeTable {
  static constexpr CPUTraits canonical_traits_{
      "",
      "",
      Key,
      16,
      0,
      BankingType::NONE,
      {SoundChip::NONE, DMAController::NONE, false}};
  static constexpr auto table = generate_opcode_table_for_traits(canonical_traits_);
};

// ============================================================================
// MAIN CPU TEMPLATE CLASS
// ============================================================================

template <const CPUTraits &Traits>
class fam65xx_t : public ChipBase, public io_port_base_t<Traits>, public apu_base_t<Traits> {
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

  // AEC pin detection: all 65xx CPUs with an I/O port (6510, 7501/8501, 8502)
  // also carry the AEC (Address Enable Control) pin for VIC-II / TED bus
  // arbitration.  Using the I/O-port flag avoids a non-dependent name lookup
  // on a specific traits constant that may not yet be visible at template
  // definition time.
  static constexpr bool has_aec_pin() {
    return Traits.has_io_port();
  }

  /// Minimum viable bus state for this CPU to function correctly.
  /// Returns bus_state_t with all CPU-required pull-up signals HIGH:
  ///  - RW (read mode), RDY (ready)
  ///  - IRQ (inactive, active-low) — unless NO_IRQ_LINE
  ///  - NMI (inactive, active-low) — unless NO_NMI_LINE
  ///  - AEC (CPU has bus) — only for CPUs with I/O port (6510/7501/8502)
  ///  - RES (inactive, active-low) — prevents continuous reset detection
  /// Systems should OR in additional system-specific signals (BA,
  /// CNT, FLAG, data bus pull-ups, etc.).
  static constexpr bus_state_t default_bus_state() {
    bus_state_t s = BUS_BIT(BUS_RW_BIT) | BUS_BIT(BUS_RDY_BIT) | BUS_BIT(BUS_RES_BIT);
    if constexpr (has_irq_line()) { s |= BUS_BIT(BUS_IRQ_BIT); }
    if constexpr (has_nmi_line()) { s |= BUS_BIT(BUS_NMI_BIT); }
    if constexpr (has_aec_pin()) { s |= BUS_BIT(BUS_AEC_BIT); }
    return s;
  }

  // Identity / addressing helpers — let consumers query the CPU type without
  // depending on CPUTraits directly.
  static constexpr uint32_t address_mask() { return Traits.address_mask(); }
  static constexpr const char* vendor()    { return Traits.vendor; }
  static constexpr const char* chip_id()   { return Traits.chip_id; }

  // ========================================================================
  // DEBUG TRACING STATE (must be declared before use in member functions)
  // ========================================================================

  /* Debug tracing state */
  static constexpr bool ENABLE_TRACING = false; /* Compile-time tracing flag - DISABLED FOR PERFORMANCE */
  mutable int trace_indent = 0; /* Current tracing indentation level */

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

        // NOTE: This trace uses placeholder operand values (0x00, 0x00)
        // For accurate operand display, tracing should be done at the system level
        // where memory can be accessed. See tools/test_disasm_trace.cpp for example
        // of proper implementation using c64_read_memory() helper.
        // TODO: Move tracing to system level or add memory callback parameter
        char disasm_buffer[32];
        fam65xx_disassemble_instruction(pc, this->opcode_entry, 0x00, 0x00,
                                       disasm_buffer, sizeof(disasm_buffer));

        printf("[%03d] PC=$%04X AB=$%04X IR=$%02X (%s) ", instruction_count, pc,
               ab, ir, disasm_buffer);

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
              "DL=%02X IR=%02X)",
              context, this->get(REG_PC), this->get(REG_A), this->get(REG_X),
              this->get(REG_Y), this->get(REG_P), this->get(REG_S),
              this->get(REG_AB), this->get(REG_DL), this->get(REG_IR));
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
            this->get(REG_IR), this->half_cycle, this->get(REG_AB),
            this->get(REG_PC));
    }
  }

  void trace_operation(const char *function_name) const {
    if constexpr (ENABLE_TRACING) {
      trace("OP: %s (IR=0x%02X cycle=%d)", function_name, this->get(REG_IR),
            this->half_cycle);
    }
  }

  void trace_instruction(uint8_t opcode, const char *mnemonic) const {
    if constexpr (ENABLE_TRACING) {
      trace("EXEC: %02X %s (cycle %d)", opcode, mnemonic, this->half_cycle);
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

    // Stack always uses bank 0 (both emulation and native mode)
    // In emulation mode: SP is 8-bit constrained to page 1 (0x0100-0x01FF)
    // In native mode: SP is 16-bit and can address anywhere in bank 0
    if constexpr (addr_arg == Addr::SP) {
      return 0;
    }

    // For data addresses (Addr::AB):
    // Use the provided bank register (DBR or ZBR)
    // Note: In emulation mode, DBR cannot be changed via instructions,
    // but its current value is still used for address calculation
    return this->get(static_cast<reg8_t>(bank_arg));
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
    uint16_t addr;
    // Special handling for stack pointer - use get_sp() for correct behavior
    if constexpr (addr_arg == Addr::SP) {
      addr = this->get_sp();
    } else {
      // Get address from the specified address register
      constexpr reg16_t addr_reg = static_cast<reg16_t>(addr_arg);
      // Non-SP addresses: Use register directly
      addr = this->get(addr_reg);
    }

    // For 8/16-bit CPUs: use address field
    FAM65XX_SET_ADDR(pins, addr & Traits.address_mask());

    // Update bus lines - always enabled for external bus-based memory access
    // For 65816: Split 24-bit address into 16-bit address + 8-bit bank
    if constexpr (has_wide_registers()) {
      // 65C816: Always set both address (16-bit) and bank (8-bit) fields
      // In emulation mode, PC uses PBR while data/stack use bank 0
      // In native mode, all addresses use their respective bank registers
      // 65C816 banking: OR bank into high bits (optimizer eliminates for non-wide CPUs)
      // For Direct Page (Bank::ZBR): address wraps within bank 0 (0x000000-0x00FFFF)
      const uint8_t bank = get_address_bank<addr_arg>(bank_arg);

      FAM65XX_SET_BANK(pins, bank);
    }

    // Set R/W signal
    if constexpr (IsWrite) {
      pins &= ~FAM65XX_RW;                 // Clear RW for write
      FAM65XX_SET_DATA(pins, data); // Output data for write
    } else {
      pins |= FAM65XX_RW; // Set RW for read
      // External code will put data on bus during memory access
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
   * Set up bus for write cycle with register enum - NO MEMORY ACCESS
   * Overload that accepts reg8_t enum, retrieves value, and forwards to original
   */
  template <Addr addr_reg, Bank bank_arg = Bank::DBR>
  inline bus_state_t bus_setup_write(bus_state_t pins, reg8_t data_reg) {
    return bus_setup_write<addr_reg, bank_arg>(pins, this->get(data_reg));
  }

  /**
   * Set up bus for dummy cycle - NO MEMORY ACCESS
   * Used for internal CPU operations and timing penalties
   */
  template <Addr addr_reg, Bank bank_arg = Bank::DBR>
  inline bus_state_t bus_setup_dummy(bus_state_t pins) {
    return bus_setup<false, true, addr_reg, bank_arg>(pins);
  }

  /**
   * Unified operand read helper for PHI2 phase
   * Handles immediate vs memory mode addressing automatically
   *
   * For immediate mode: reads from PC (operand follows opcode)
   * For memory modes: reads from AB (effective address calculated)
   *
   * For 65C816: Zero-page/Direct-page modes always use Bank 0 (ZBR)
   * regardless of what's stored in REG_ABH, to prevent corruption of
   * banking information by operand high bytes in 16-bit operations.
   *
   * This is a runtime check and cannot be avoided because immediate and
   * memory addressing modes are fundamentally different operations at the
   * hardware level (operand in instruction stream vs operand at address).
   */
  inline bus_state_t bus_setup_read_operand(bus_state_t pins) {
    const uint8_t am = this->opcode_entry.am_index;
    
    // Immediate mode: operand at PC
    if (am == to_index(AM::IMM)) {
      return this->bus_setup_read<Addr::PC>(pins);
    }
    
    // For 65C816: Direct Page modes use bank 0 (ZBR)
    // But ONLY for non-indirect modes: ZER(2), ZPX(3), ZPY(4)
    // ZPI(5) is indirect - the pointer is in DP but target address uses DBR!
    if constexpr (has_wide_registers()) {
      if (am <= to_index(AM::ZPY)) {
        return this->bus_setup_read<Addr::AB, Bank::ZBR>(pins);
      }
    }
    
    // All other memory modes use default banking (DBR)
    // This includes ZPI(5) and all absolute/indexed modes
    return this->bus_setup_read<Addr::AB>(pins);
  }

  // ========================================================================
  // PHI1 DATA LOADING HELPERS
  // ========================================================================

  /**
   * Get data from bus without storing (PHI1 phase)
   * Use when you need the value for calculations
   */
  inline uint8_t bus_get_data(bus_state_t pins) const {
    return FAM65XX_GET_DATA(pins);
  }

  /**
   * Load register from bus data (PHI1 phase)
   * Single-line helper for clean read handling
   */
  inline void bus_load_reg(reg8_t data_reg, bus_state_t pins) {
    this->set(data_reg, bus_get_data(pins));
  }

  /**
   * Get data from bus and increment PC if immediate mode (PHI1 phase)
   * Unified helper for immediate mode operand handling
   */
  inline uint8_t bus_get_operand(bus_state_t pins) {
    const uint8_t am = this->opcode_entry.am_index;
    
    // Immediate mode: increment PC
    if (am == to_index(AM::IMM)) {
      this->inc(REG_PC);
    } else if constexpr (has_wide_registers()) {
      // Memory modes for 65C816: increment address for next byte
      // Direct Page modes (ZER, ZPX, ZPY): Always increment only ABL
      // These read from Direct Page which is always in bank 0
      // ZPI is NOT included - it's indirect so target can be anywhere
      if (am <= to_index(AM::ZPY)) {
        this->inc(REG_ABL);
      } else {
        this->inc(REG_AB);
      }
    }
    return this->bus_get_data(pins);
  }

  /**
   * Load register from bus and increment PC if immediate mode (PHI1 phase)
   * Unified helper for immediate mode operand handling
   */
  inline void bus_load_operand(reg8_t data_reg, bus_state_t pins) {
    const uint8_t operand = this->bus_get_operand(pins);
    // Load data from bus into register
    this->set(data_reg, operand);
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
      // PROCESSOR_TESTS mode: Skip I/O port interception (compile-time check for zero overhead)
      #ifdef PROCESSOR_TESTS
        return false; // Not handled - allow normal memory access for test harness
      #else
        if (addr <= 0x0001) {
          // $0000 = DDR (Data Direction Register)
          // $0001 = Data Port (effective I/O value combining data, direction, and input)
          const uint8_t io_data =
              (addr == 0x0000) ? *this->port.ddr : this->read_io_port();
          FAM65XX_SET_DATA(pins, io_data);
          return true;
        }
      #endif
    }
    return false;
  }

  /**
   * Handle I/O port write (6510 only) - called during PHI1 after external
   * memory tick Returns true if address was I/O port
   */
  inline bool handle_io_port_write(uint32_t addr, uint8_t data) {
    if constexpr (Traits.has_io_port()) {
      // PROCESSOR_TESTS mode: Skip I/O port interception (compile-time check for zero overhead)
      #ifdef PROCESSOR_TESTS
        return false; // Not handled - allow normal memory access for test harness
      #else
        if (addr <= 0x0001) {
          if (addr == 0x0000) {
            this->write_io_ddr(data);
          } else {
            this->write_io_data(data);
          }
          return true;
        }
      #endif
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
      // PROCESSOR_TESTS mode: Skip APU interception (compile-time check for zero overhead)
      #ifdef PROCESSOR_TESTS
        return false; // Not handled - allow normal memory access for test harness
      #else
        uint8_t apu_data;
        if (this->read_apu_register(addr, apu_data)) {
          FAM65XX_SET_DATA(pins, apu_data);
          return true;
        }
      #endif
    }
    return false;
  }

  /**
   * Handle APU register write (NES 6502 only) - called during PHI1 after
   * external memory tick Returns true if address was APU register
   */
  inline bool handle_apu_write(uint32_t addr, uint8_t data) {
    if constexpr (Traits.has_apu()) {
      // PROCESSOR_TESTS mode: Skip APU interception (compile-time check for zero overhead)
      #ifdef PROCESSOR_TESTS
        return false; // Not handled - allow normal memory access for test harness
      #else
        return this->write_apu_register(addr, data);
      #endif
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
  /**
   * Internal template helper for RMW operations with compile-time bank selection
   * Eliminates runtime banking checks by using template parameter
   */
  template <Bank BankArg, typename OperationFunc>
  bus_state_t rmw_operation_helper_impl(bus_state_t pins,
                                        OperationFunc operation_func) {
    // Check if this is a 16-bit memory operation
    const bool is_16bit_memory = this->is_memory_16bit();

    if (is_16bit_memory) {
      // 16-bit memory RMW operation (6 cycles for 65C816)
      switch (this->half_cycle) {
      case 0:
        // PHI2: Set up bus for reading low byte
        pins = this->bus_setup_read<Addr::AB, BankArg>(pins);
        return pins;
      case 1:
        // PHI1: Load low byte from bus
        this->bus_load_reg(REG_DL, pins);
        this->inc(REG_ABL); // Increment address for high byte
        this->half_cycle++;
        return pins;

      case 2:
        // PHI2: Set up bus for reading high byte
        pins = this->bus_setup_read<Addr::AB, BankArg>(pins);
        return pins;
      case 3:
        // PHI1: Load high byte and perform dummy cycle setup
        this->bus_load_reg(REG_SBR, pins); // Note : Uses REG_SBR as temporary storage (allowed gievn its limited scope)
        this->half_cycle++;
        return pins;

      case 4:
        // PHI2: Dummy cycle
        if (this->has_rmw_dummy_write()) {
          // NMOS: Dummy write of high byte
          pins = this->bus_setup_write<Addr::AB, BankArg>(pins, REG_SBR);
        } else {
          // CMOS: Dummy read instead of write
          pins = this->bus_setup_dummy<Addr::AB, BankArg>(pins);
        }
        return pins;
      case 5: {
        // PHI1: Perform 16-bit operation
        uint16_t value16 = static_cast<uint16_t>(this->get(REG_DL)) |
                           (static_cast<uint16_t>(this->get(REG_SBR)) << 8);
        data_t value = static_cast<data_t>(value16);
        operation_func(value);
        value16 = static_cast<uint16_t>(value);
        this->set(REG_DL, static_cast<uint8_t>(value16 & 0xFF)); // Low byte
        this->set(REG_SBR, // Note : Uses REG_SBR as temporary storage (allowed gievn its limited scope)
                  static_cast<uint8_t>((value16 >> 8) & 0xFF)); // High byte
        this->half_cycle++;
        return pins;
      }

      case 6:
        // Cycle 6 PHI2: Write high byte back to memory (address + 1)
        pins = this->bus_setup_write<Addr::AB, BankArg>(pins, REG_SBR);
        return pins;
      case 7:
        // Cycle 7 PHI1: Decrement address
        this->dec(REG_ABL); // Decrement address back to low byte
        this->half_cycle++;
        return pins;

      case 8:
        // Cycle 8 PHI2: Write low byte back to memory (address)
        pins = this->bus_setup_write<Addr::AB, BankArg>(pins, REG_DL);
        return pins;
      case 9:
        // Cycle 9 PHI1: Complete operation
        this->transition_to_fetch();
        return pins;
      }
    } else {
      // 8-bit memory RMW operation (PHI2/PHI1 split)
      switch (this->half_cycle) {
      case 0:
        // PHI2: Set up bus for reading original value
        pins = this->bus_setup_read<Addr::AB, BankArg>(pins);
        return pins;
      case 1:
        // PHI1: Load original value from bus
        this->bus_load_reg(REG_DL, pins);
        this->half_cycle++;
        return pins;

      case 2:
        // PHI2: Dummy cycle
        if (this->has_rmw_dummy_write()) {
          // NMOS: Dummy write of original value
          pins = this->bus_setup_write<Addr::AB, BankArg>(pins, REG_DL);
        } else {
          // CMOS: Dummy read instead of write
          pins = this->bus_setup_dummy<Addr::AB, BankArg>(pins);
        }
        return pins;
      case 3: {
        // PHI1: Perform modification
        data_t value = static_cast<data_t>(this->get(REG_DL));
        operation_func(value);
        this->set(REG_DL, static_cast<uint8_t>(value & 0xFF));
        this->half_cycle++;
        return pins;
      }

      case 4:
        // PHI2: Write modified value back to memory
        pins = this->bus_setup_write<Addr::AB, BankArg>(pins, REG_DL);
        return pins;
      case 5:
        // Cycle 5 PHI1: Complete operation
        this->transition_to_fetch();
        return pins;
      }
    } // end 8-bit memory RMW

    return pins;
  }

  /**
   * Public RMW operation helper - dispatches to optimized template variant
   * Single runtime check at function entry, then zero-overhead execution
   */
  template <typename OperationFunc>
  bus_state_t rmw_operation_helper(bus_state_t pins,
                                   OperationFunc operation_func) {
    if (this->opcode_entry.is_rmw()) {
      // Memory mode - determine banking once at function entry
      // For 65C816, Direct Page operations ALWAYS use Bank 0 (ZBR)
      // This is true in both emulation and native modes
      constexpr bool can_use_direct_page = has_wide_registers();
      if constexpr (can_use_direct_page) {
        // Check if this is a Direct Page addressing mode
        // Direct Page modes: ZER(2), ZPX(3), ZPY(4) - these access Bank 0 directly
        // ZPI is EXCLUDED - it's indirect, so RMW happens at target (uses DBR)
        const uint8_t am = this->opcode_entry.am_index;
        const bool is_direct_page_addressing =
          (am >= to_index(AM::ZER) && am <= to_index(AM::ZPY));
        
        // Dispatch to template variant - compiler optimizes away all banking checks
        if (is_direct_page_addressing) {
          // Direct Page always uses Bank 0 (ZBR) in both emulation and native modes
          return this->rmw_operation_helper_impl<Bank::ZBR>(pins, operation_func);
        } else {
          // Other addressing modes use Data Bank Register (DBR)
          return this->rmw_operation_helper_impl<Bank::DBR>(pins, operation_func);
        }
      } else {
        // Non-65C816: always use DBR (which is ignored anyway)
        return this->rmw_operation_helper_impl<Bank::DBR>(pins, operation_func);
      }
    } else {
      // Accumulator mode - two-phase operation (PHI2 + PHI1)
      switch (this->half_cycle) {
      case 0: {
        // Cycle 0 PHI2: Set up dummy read from PC
        pins = this->bus_setup_dummy<Addr::PC>(pins);
        return pins;
      }
      case 1: {
        // Cycle 1 PHI1: Perform operation on accumulator
        data_t value = this->get_accumulator(); // Automatically handles 8/16-bit based on M flag
        operation_func(value);
        this->set_accumulator(value); // Automatically handles 8/16-bit based on M flag
        this->transition_to_fetch();
        return pins;
      }
      }
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

  /**
   * Sample NMI pin using post-bus-dispatch pin state.
   *
   * The end of PHI2 and start of PHI1 are the same clock edge.  In our
   * half-cycle model, bus dispatch happens between PHI2 and PHI1, so
   * calling this method after dispatch is equivalent to sampling at the
   * end of PHI2 (= start of the next PHI1).
   *
   * Edge detection: compares nmi_prev (previous sample) to the current
   * pin state.  A falling edge sets nmi_edge_latch.  The latch persists
   * until NMI is serviced (vector fetch clears it), matching the 6502's
   * internal edge-detect flip-flop.  A brief NMI pulse that returns HIGH
   * within one cycle is caught — once the latch is set, pin state doesn't
   * matter.  IRQ by contrast is level-sensitive (shift register based).
   *
   * The 1-cycle-before-acting delay is inherent in the pipeline:
   *   Post-dispatch N   : /NMI falls → edge_latch SET
   *   PHI2 N+1          : process_interrupt_detection sees edge_latch
   *                        → NMI serviceable (hijack at instruction boundary)
   *
   * Every system tick loop MUST call this method once per CPU cycle,
   * after bus dispatch and before PHI1.  process_interrupt_detection()
   * does NOT perform inline edge detection — it only reads the latch
   * state established here.
   */
  inline void sample_nmi_pin(bus_state_t pins) {
    if constexpr (has_nmi_line()) {
      uint32_t int_pins = (~pins) >> BUS_RES_BIT;
      constexpr uint8_t NMI_OFFSET = BUS_NMI_BIT - BUS_RES_BIT;
      uint8_t nmi_current = (int_pins >> NMI_OFFSET) & 0x1;

      // Detect FALLING edge: 0→1 in inverted domain (pin went HIGH→LOW)
      if ((!this->nmi_prev) & nmi_current) {
        this->nmi_edge_latch = 1;
      }
      this->nmi_prev = nmi_current;
    }
  }

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

    // Sample IRQ (extracted bit 1 -> shift_reg bit 4) - only if IRQ line exists
    if constexpr (has_irq_line()) {
      shift_reg |= ((int_pins >> IRQ_OFFSET) & 0x1) << INT_IRQ_START_BIT;
    }

    // NMI edge detection — only if NMI line exists
    //
    // The 6502 uses an internal edge-detect flip-flop for NMI:
    //   - SET on the falling edge of /NMI (HIGH→LOW)
    //   - CLEARED when the NMI vector is fetched (service acknowledgement)
    //   - Persists regardless of subsequent pin state — once latched, the
    //     pin can return HIGH and the NMI will still fire.
    //
    // This is nmi_edge_latch.  It IS the NMI pending state.
    //
    // The 6502 samples IRQ/NMI one cycle before acting on it.  The sample
    // taken at the end of cycle N determines whether an interrupt sequence
    // begins after cycle N+1 (the last cycle of the current instruction).
    // "After N+1" means the interrupt starts at N+2 — the instruction at
    // N+1 executes normally first.  Total latency = 2 cycles from sample.
    //
    // In our model this is achieved with a 2-stage pipeline:
    //
    //   Stage 1 — Edge latch (in sample_nmi_pin(), called by the system
    //     tick loop after bus dispatch):
    //     End of cycle N: sample_nmi_pin() detects falling edge →
    //                     nmi_edge_latch SET
    //
    //   Stage 2 — Serviceability (1-cycle delay, in this function):
    //     PHI2 of N+1: process_interrupt_detection reads the latch and
    //     sets nmi_output_latch_ = 1 (not yet serviceable this cycle)
    //     PHI2 of N+2: nmi_output_latch_ is true → NMI serviceable → hijack
    //
    // IRQ is level-sensitive: sampled directly each cycle via the shift
    // register, no latch.  A pulse shorter than one cycle can be missed.
    //
    // A brief NMI pulse (low and back high within one cycle) IS caught,
    // because the edge detector latches the transition.  Once latched,
    // the NMI fires regardless of the current pin state.
    bool nmi_serviceable = false;

    if constexpr (has_nmi_line()) {
      // Capture the output latch from the PREVIOUS cycle.
      // This is the 1-cycle-before-acting delay.
      nmi_serviceable = this->nmi_output_latch_;

      // Feed current NMI pin state into shift register for debug visibility
      shift_reg |= ((int_pins >> NMI_OFFSET) & 0x1) << INT_NMI_START_BIT;

      // Update output latch for NEXT cycle's serviceability check.
      // sample_nmi_pin() already ran edge detection and set nmi_edge_latch.
      this->nmi_output_latch_ = (this->nmi_edge_latch != 0);
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
    // nmi_edge_latch is the hardware's internal flip-flop.  Once set by
    // a falling edge it persists until the NMI vector is fetched.  The
    // nmi_serviceable flag (computed above) incorporates the 1-cycle
    // pipeline delay so we don't act on an edge detected this very cycle.
    if constexpr (has_nmi_line()) {
      if (nmi_serviceable) {
        this->active_interrupt = FAM65XX_INT_NMI;
        return true;
      }
    }

    // Check COP (65C816 software interrupt, fourth priority)
    // COP is triggered by software, not hardware pins - handled elsewhere

    // Check IRQ (fifth priority, maskable by I flag)
    // CRITICAL: IRQ detection (sampling) happens regardless of I flag state
    // The shift register continuously samples IRQ line every cycle for glitch rejection
    // The I flag check happens LATER at hijacking time (instruction boundary)
    // This matches hardware: shift register samples continuously, I flag only controls servicing
    if constexpr (has_irq_line()) {
      if ((shift_reg & INT_IRQ_MASK) == INT_IRQ_MASK) {
        // IRQ pattern detected - shift register full
        // I flag will be checked at hijacking time (instruction fetch boundary)
        this->active_interrupt = FAM65XX_INT_IRQ;
        return true;
      }
    }

    // Check BRK (software interrupt, sixth priority)
    // BRK is triggered by software, not hardware pins - handled in op_brk

    // No interrupt detected
    return false;
  }

  // ========================================================================
  // INTERRUPT HIJACKING HELPER FUNCTIONS
  // ========================================================================

  /**
   * Check if interrupt can be hijacked at current execution point
   * Returns true only if at fetch boundary AND no interrupt already active
   */
  inline bool can_hijack_for_interrupt() const {
    // Must be at fetch boundary (start of instruction)
    // This is sufficient anti-nesting protection: during interrupt servicing
    // the CPU runs op_brk (not fetch_opcode), so this naturally returns false.
    // NOTE: Do NOT check active_interrupt here - process_interrupt_detection()
    // has already set it, so checking == NONE would always fail.
    if (this->current_handler != &fam65xx_t::fetch_opcode || this->half_cycle != 0)
      return false;

    // Branch fixup cycle suppression: on the 6502, a taken branch that
    // doesn't cross a page has a fixup cycle (T2) that does NOT poll
    // interrupts.  If an interrupt was already pending at the penultimate
    // cycle (T1), it IS serviced.  Only interrupts that first become
    // pending at T2 are suppressed until the next instruction.
    if (this->branch_irq_suppression_) {
      // Check if IRQ pin was asserted at the branch's penultimate cycle.
      // Check if IRQ was detectable at the branch's penultimate cycle using
      // a 2-bit shift register check (not 3-bit).  The snapshot is taken 1
      // cycle before the fetch boundary, so it has 1 fewer accumulated sample
      // than the main detection path.  2 consecutive LOW samples at the poll
      // means IRQ was pending before the fixup → service it now.
      if constexpr (has_irq_line()) {
        constexpr uint32_t TWO_BIT_IRQ = 3u << INT_IRQ_START_BIT;
        if ((this->branch_poll_shift_reg_ & TWO_BIT_IRQ) == TWO_BIT_IRQ)
          return true;  // IRQ was detectable at penultimate → service it
      }
      // Check if NMI was already pending at the branch's penultimate cycle
      if constexpr (has_nmi_line()) {
        if (this->branch_nmi_pending_at_poll_)
          return true;  // NMI was already pending — service it
      }
      return false;  // Interrupt first detected during fixup — suppress
    }

    return true;
  }

  /**
   * Check if detected interrupt should be serviced
   * Non-maskable interrupts (NMI, RESET) always serviced
   * Maskable IRQ only serviced if I flag is clear
   */
  inline bool should_service_interrupt(interrupt_t detected_int) const {
    // Non-maskable interrupts always serviced
    if (detected_int != FAM65XX_INT_IRQ) {
      return true;
    }
    
    // IRQ: only service if I flag was clear at the START of the
    // most recently executed instruction.  This implements the 6502's
    // 1-instruction pipeline delay for CLI/SEI/PLP/RTI — the effect
    // of an I flag change doesn't mask/unmask IRQs until the next
    // instruction boundary.
    return !(this->irq_i_flag_sample_);
  }

  // Template-dependent function pointer type
  using InstructionHandler = bus_state_t (fam65xx_t<Traits>::*)(bus_state_t);

  // Lookup tables for handlers (initialized during init)
  std::array<InstructionHandler, to_index(OP::COUNT)> operation_handlers;
  std::array<InstructionHandler, to_index(AM::COUNT)> addressing_mode_handlers;

  inline InstructionHandler get_instruction_handler() {
    if constexpr (ENABLE_TRACING) {
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
    }

    // For addressing modes that need address calculation, start with addressing mode handler
    // AM::NON and AM::IMM go directly to operation (no address calculation needed)
    if (this->opcode_entry.am_index > to_index(AM::IMM)) {
        // COMPILE-TIME SAFETY: addressing_mode_handlers is fully initialized in init_opcode_table()
        // Every valid addressing mode has a handler assigned, so this is always safe
        InstructionHandler handler = addressing_mode_handlers[this->opcode_entry.am_index];
        if (handler == nullptr) {
            // Fallback to NOP if no handler is assigned (should never happen in practice)
            return &fam65xx_t::op_nop;
        }
        return handler;
    }

    // Get operation handler
    // COMPILE-TIME SAFETY: operation_handlers is fully initialized in init_opcode_table()
    // Every valid opcode has a handler assigned, so this is always safe
    return this->operation_handlers[this->opcode_entry.op_index];
  }

  inline bus_state_t transition_to_opcode(bus_state_t pins, const opcode_info_t entry) {
    this->opcode_entry = entry;
    // Track whether this is a software BRK (for B flag in pushed P register).
    // Must be set here (at dispatch time) because NMI vector hijacking can
    // override active_interrupt before op_brk case 1 gets to check it.
    this->brk_is_software_ = (entry.op_index == to_index(OP::BRK));
    // Set up first instruction cycle handler
    this->current_handler = this->get_instruction_handler();
    
    // For OPTIMIZED_CYCLES: Check if this is a single-cycle implicit operation
    // that can execute immediately during fetch's PHI1 phase
    if constexpr (has_optimized_cycles()) {
      // Use new helper method that checks both AM::NON and OPTIMIZED_CYCLE flag
      if (entry.is_optimized_cycle()) {
        // Set half_cycle to 1 to skip dummy bus setup (case 0)
        // and execute operation directly (case 1)
        this->half_cycle = 1;
        // Execute the operation immediately in this PHI1 phase
        return this->call_current_handler(pins);
      }
    }
    
    // Default path: normal execution starting at cycle 0
    this->half_cycle = 0;
    return pins;
  }

  // Helper method for calling current handler with proper member function
  // pointer syntax
  inline bus_state_t call_current_handler(bus_state_t pins) {
    return (this->*current_handler)(pins);
  }

  // Instruction fetch and decode - PHI2/PHI1 split pattern
  bus_state_t fetch_opcode(bus_state_t pins) {
    switch (this->half_cycle) {
    case 0:
      // PHI2: Set up bus for opcode read from PC - ZERO SIDE EFFECTS
      pins = this->bus_setup_read<Addr::PC>(pins);
      // Set SYNC signal for opcode fetch (hardware-accurate timing)
      pins |= FAM65XX_SYNC;
      return pins;
    case 1: {
      // PHI1: ALL side effects happen here (safe from RDY retry)

      // DEFERRED INTERRUPT HIJACK: If an interrupt was detected at PHI2,
      // the pending flag was set and fetch_opcode case 0 ran normally
      // (the T0 dummy opcode read).  Now redirect to op_brk for T1+.
      // DON'T increment PC: hardware interrupts preserve the return
      // address so the interrupted instruction re-executes after RTI.
      if (this->interrupt_hijack_pending_) {
        this->interrupt_hijack_pending_ = false;
        this->branch_irq_suppression_ = false;
        this->current_handler = &fam65xx_t::op_brk;
        this->half_cycle = 0;
        pins &= ~FAM65XX_SYNC;
        return pins;
      }

      // Clear branch IRQ suppression after one boundary has passed.
      // The suppression prevented hijack at this boundary's PHI2;
      // the next boundary will allow normal interrupt detection.
      this->branch_irq_suppression_ = false;

      // Copy PC to AB and increment PC
      this->set(REG_AB, this->get(REG_PC));
      this->inc(REG_PC);
      // Sample opcode from bus and decode
      uint8_t opcode = this->bus_get_data(pins);
      this->set(REG_IR, opcode);
      // Clear SYNC signal after opcode fetch completes (hardware-accurate timing)
      pins &= ~FAM65XX_SYNC;
      // Transition resets half_cycle to 0 for new instructions except
      // for OPTIMIZED_CYCLES, which executes the operation immediately
      // with half_cycle set to 1 (which skips the initial dummy cycle)
      opcode_info_t entry = get_opcode_info(opcode);
      pins = this->transition_to_opcode(pins, entry);
      return pins;
    }
    }

    return pins; // Should never reach here
  }

  // Transition to next instruction fetch (public for bootstrap function)
  inline void transition_to_fetch() {
    if constexpr (ENABLE_TRACING) {
      trace("transition_to_fetch() called - resetting to fetch mode");
    }
    this->current_handler = &fam65xx_t::fetch_opcode;
    this->half_cycle = 0;
  }

  inline void transition_to_operation() {
    this->half_cycle = 0;
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

  /// Reset processor-specific peripherals to power-up state.
  /// Unlike init_conditional_features(), this preserves existing instances
  /// (no new allocations) and avoids invalidating external pointers.
  void reset_conditional_features() {
    // Reset I/O port if present
    if constexpr (has_io_port()) {
      this->init_io_port();
    }

    // Reset APU if present (does not recreate the instance)
    if constexpr (has_apu()) {
      this->reset_apu();
    }
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
        nullptr); // Default to nullptr (safe for AM_NON and AM_IMM)

    // Addressing modes
    addressing_mode_handlers[to_index(AM::NON)] =
        nullptr; // No handler needed (implicit/accumulator/relative)
    addressing_mode_handlers[to_index(AM::IMM)] =
        nullptr; // No handler needed - bus_setup_read_operand() handles this

    // Addressing modes
    addressing_mode_handlers[to_index(AM::ABS)] = &fam65xx_t::am_abs;
    addressing_mode_handlers[to_index(AM::ABX)] = &fam65xx_t::am_abx;
    addressing_mode_handlers[to_index(AM::ABY)] = &fam65xx_t::am_aby;
    addressing_mode_handlers[to_index(AM::IND)] = &fam65xx_t::am_ind;
    addressing_mode_handlers[to_index(AM::INX)] = &fam65xx_t::am_inx;
    addressing_mode_handlers[to_index(AM::INY)] = &fam65xx_t::am_iny;
    // 6502/6510 Zero Page addressing modes
    // Note: 65C816 will overwrite these with Direct Page handlers below
    addressing_mode_handlers[to_index(AM::ZER)] =
        &fam65xx_t::am_zp; // Zero Page (replaced by Direct Page in 65C816)
    addressing_mode_handlers[to_index(AM::ZPX)] =
        &fam65xx_t::am_zpx;  // Zero Page,X (replaced by Direct Page,X in 65C816)
    addressing_mode_handlers[to_index(AM::ZPY)] =
        &fam65xx_t::am_zpy;  // Zero Page,Y (replaced by Direct Page,Y in 65C816)

    // Rockwell 65C02 addressing modes
    if constexpr (Traits.has(fam65xx::detail::CPUCoreFlags::ROCKWELL_BITS)) {
      addressing_mode_handlers[to_index(AM::ZPR)] =
          &fam65xx_t::am_zpr; // Zero Page Relative - BBR/BBS $nn,$offset
    }

    // CMOS processors
    if constexpr (Traits.has(fam65xx::detail::CPUCoreFlags::CMOS_BASE)) {
      addressing_mode_handlers[to_index(AM::ZPI)] =
          &fam65xx_t::am_zpi;  // Zero Page Indirect
      addressing_mode_handlers[to_index(AM::ABI)] =
          &fam65xx_t::am_abi; // Absolute Indexed Indirect (abs,X) - JMP ($nnnn,X) - ALL 65C02
    }

    // WDC 65C816 modifications (16-bit enhanced instructions)
    if constexpr (Traits.has(fam65xx::detail::CPUCoreFlags::C816_16BIT)) {
      // Initialize 65C816 exclusive addressing modes
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

      // Overwrite 65C816 replacement addressing modes
      // CRITICAL: 65C816 uses Direct Page addressing even in emulation mode
      // ZER, ZPX, ZPY are aliases for DP, DPX, DPY - same enum values
      addressing_mode_handlers[to_index(AM::ZER)] =
          &fam65xx_t::am_dp; // Direct/Zero Page - handles D register offset
      addressing_mode_handlers[to_index(AM::ZPX)] =
          &fam65xx_t::am_dpx; // Direct/Zero Page,X - handles D register offset
      addressing_mode_handlers[to_index(AM::ZPY)] =
          &fam65xx_t::am_dpy; // Direct/Zero Page,Y - handles D register offset
      addressing_mode_handlers[to_index(AM::ZPI)] =
          &fam65xx_t::am_dpi; // Direct/Zero Page Indirect - handles D register offset
    }
  }

  opcode_info_t get_opcode_info(uint8_t opcode) const {
    // Use shared table keyed by instruction-set-relevant flags only.
    // All NMOS variants (6502/6510/6507/7501/2A03) share one table.
    return SharedOpcodeTable<opcode_table_key(Traits)>::table[opcode];
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
      // Use REG_P_16 to handle FLAG_E (bit 8) directly
      if (mode) {
        this->set(REG_P_16, this->get(REG_P_16) | FLAG_E);
      } else {
        this->set(REG_P_16, this->get(REG_P_16) & ~FLAG_E);
      }
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

  /**
   * Hardware RESET — enter the 7-cycle reset sequence.
   *
   * Resets internal CPU state (interrupt pipeline, WAI/STP, conditional
   * features) then routes the processor into the BRK/interrupt handler
   * via the deferred-hijack mechanism.  The first 7 ticks after reset()
   * execute the hardware-accurate RESET sequence:
   *
   *   T0  fetch_opcode (dummy opcode read — byte discarded)
   *   T1  op_brk case 0+1 (dummy operand read)
   *   T2  op_brk case 2+3 (suppressed push PCH, dec S)
   *   T3  op_brk case 4+5 (suppressed push PCL, dec S)
   *   T4  op_brk case 6+7 (suppressed push P,   dec S, set I)
   *   T5  op_brk case 8+9 (read vector low  — $FFFC)
   *   T6  op_brk case 10+11 (read vector high — $FFFD, load PC)
   *
   * The vector words are fetched through the normal bus, so the system's
   * memory map services the reads — no manual load_reset_vector() needed.
   *
   * A, X, Y, and P (except I) are preserved across reset; only power-on
   * randomises them.  The stack pointer decrements by 3 (the three
   * suppressed pushes) and the I flag is set — both handled by op_brk.
   */
  bus_state_t reset(bus_state_t pins) {
    // Reset interrupt state
    this->nmi_prev = 0;
    this->nmi_edge_latch = 0;
    this->nmi_output_latch_ = false;
    this->interrupt_shift_register = 0;
    this->irq_i_flag_sample_ = 1;  // Reset sets I flag; pipeline starts masked
    this->brk_is_software_ = 0;     // Hardware interrupt, not software BRK
    this->interrupt_hijack_pending_ = true; // Deferred hijack → fetch T0 then op_brk
    this->branch_irq_suppression_ = false;
    this->branch_poll_shift_reg_ = 0;
    this->branch_nmi_pending_at_poll_ = false;

    // Reset 65C02 extended state
    this->wait_for_interrupt = false;
    this->stopped = false;

    // Reset processor-specific features (preserves existing APU instance)
    this->reset_conditional_features();

    // Route through the standard interrupt/BRK sequence.
    // fetch_opcode T0 runs one dummy read, then the deferred hijack
    // redirects to op_brk which executes the RESET sequence with writes
    // suppressed and reads the vector from $FFFC/$FFFD via the bus.
    this->active_interrupt = FAM65XX_INT_RESET;
    this->transition_to_fetch();

    return pins;
  }

  /**
   * Load the reset vector into PC and AB directly (no bus sequence).
   *
   * Used by test harnesses that call bootstrap() and need to set an
   * arbitrary start address without running the 7-cycle reset sequence.
   *
   * @param reset_vector  The 16-bit address to load into PC and AB.
   */
  void load_reset_vector(uint16_t reset_vector) {
    this->set(REG_PC, reset_vector);
    this->set(REG_AB, reset_vector);
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
    this->nmi_prev = 0; /* NMI line inactive (inverted convention: 0=pin HIGH) */
    this->nmi_edge_latch = 0;
    this->nmi_output_latch_ = false;

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
   * Inline helper for RDY signal checking - optimized to avoid code duplication
   * Returns true if CPU should halt due to RDY low during read cycle
   *
   * Per MOS 6510 datasheet: "RDY is ignored during write accesses"
   * - READ + RDY low = HALT (return true)
   * - WRITE + RDY low = CONTINUE (return false, RDY ignored)
   * - RDY high = CONTINUE (return false)
   */
  inline bool should_halt_for_rdy(bus_state_t pins) const {
    if (!FAM65XX_GET_RDY(pins)) {
      // RDY low (BA low) - VIC-II will need bus in 3 cycles
      const bool is_read = (pins & FAM65XX_RW) != 0;
      
      if (is_read) {
        // READ cycle with RDY low: CPU must HALT
        if constexpr (ENABLE_TRACING) {
          trace("RDY low during READ - CPU halted");
        }
        return true;
      }
      
      // WRITE cycle: RDY ignored (up to 3 consecutive writes allowed)
      if constexpr (ENABLE_TRACING) {
        trace("RDY low during WRITE - CPU continues (RDY ignored on writes)");
      }
    }
    return false;
  }

  /**
   * Template tick function - compile-time phase selection
   *
   * @tparam phase PHI2 for bus setup, PHI1 for internal operations
   * @param pins Current bus state
   * @return Updated bus state
   */
  template <Phase phase> bus_state_t tick(bus_state_t pins) {
    // Check bus availability signals FIRST - applies to BOTH PHI2 and PHI1
    // When CPU is halted by VIC-II, it stays halted for the complete clock cycle
    // Two signals control bus access (6510 only):
    // 1. AEC: When LOW, VIC-II owns bus completely - CPU fully tri-stated
    // 2. RDY (connected to BA): Stops CPU on READS, ignored on WRITES (up to 3)
    
    if constexpr (has_aec_pin()) {
      // Check AEC first - when LOW, bus is completely unavailable
      if (!FAM65XX_GET_AEC(pins)) {
        // AEC low - VIC-II owns bus, CPU address lines tri-stated
        // CPU must halt for BOTH PHI2 and PHI1 phases
        if constexpr (ENABLE_TRACING) {
          trace("AEC low - VIC-II owns bus, CPU tri-stated and halted");
        }
        return pins; // Don't proceed with either phase - complete halt
      }
    }
    
    if constexpr (phase == Phase::PHI2) {
      // PHI2: Bus setup phase
      if constexpr (ENABLE_TRACING) {
        trace_enter("tick<PHI2>");
        trace_registers("before PHI2");
      }
       
      // Hardware-accurate interrupt detection
#ifndef PROCESSOR_TESTS
      // PROCESSOR_TESTS mode: Disable interrupt hijacking for clean instruction testing
      // Production mode: Always allow interrupt hijacking for accurate emulation
      
      // Save active_interrupt before detection - process_interrupt_detection may
      // overwrite it even when we're mid-instruction (e.g., during op_brk execution).
      // We must restore it if we can't actually hijack.
      InterruptType prev_interrupt = this->active_interrupt;
      
      if (this->process_interrupt_detection(pins)) {
        // RESET is special - immediate return
        if (this->active_interrupt == FAM65XX_INT_RESET) {
          return reset(pins);
        }
        
        // Can we hijack at this point? (fetch boundary + not nested)
        if (this->can_hijack_for_interrupt()) {
          // Should we service this specific interrupt? (I flag check for IRQ)
          if (this->should_service_interrupt(this->active_interrupt)) {
            // DEFERRED HIJACK: Don't redirect handler yet.
            // Let fetch_opcode case 0 run its PHI2 bus read — this IS
            // the T0 dummy opcode fetch that real 6502 hardware performs.
            // The actual redirect to op_brk happens at PHI1 in
            // fetch_opcode case 1.  This gives hardware interrupts the
            // correct 7-cycle count:
            //   T0: fetch_opcode case 0+1 (dummy opcode fetch, byte discarded)
            //   T1-T6: op_brk cases 0-11 (push PC/P, read vector, jump)
            //
            // With immediate hijack, T0 is skipped → only 6 cycles,
            // causing IRQ to fire 1 cycle too early (branch_delays_irq).
            this->interrupt_hijack_pending_ = true;
            this->brk_is_software_ = 0;  // Hardware interrupt, not software BRK
            
            // Clear shift register to prevent immediate re-trigger
            this->interrupt_shift_register = 0;
            // Clear NMI edge latch and reset edge detection only when NMI is
            // being serviced.  For IRQ, do NOT touch nmi_prev — if NMI went
            // LOW at the same cycle as IRQ detection, the pending edge must
            // survive so sample_nmi_pin can latch it and op_brk case 7 can
            // hijack the vector from IRQ to NMI.
            if constexpr (has_nmi_line()) {
              if (this->active_interrupt == FAM65XX_INT_NMI) {
                // Reset NMI edge detection using INVERTED convention:
                // Pin HIGH (inactive) → inverted = 0, Pin LOW (asserted) → inverted = 1
                this->nmi_prev = (pins & FAM65XX_NMI) ? 0 : 1;
                this->nmi_edge_latch = 0;
                this->nmi_output_latch_ = false;
              }
            }
            // DON'T change current_handler — fetch_opcode will run T0
          } else {
            // I flag set - don't service IRQ, keep it pending in shift register
            // Restore active_interrupt to what it was before detection
            this->active_interrupt = prev_interrupt;
          }
        } else {
          // Can't hijack (mid-instruction) — restore active_interrupt.
          // NMI vector hijacking during BRK/IRQ is handled separately by the
          // direct nmi_output_latch_ check at op_brk case 7 (the vector-
          // determination cycle), so we don't need to persist NMI here.
          this->active_interrupt = prev_interrupt;
        }
      }
#endif

      // Call handler to set up bus (sees current even half_cycle)
      // COMPILE-TIME SAFETY: current_handler is always assigned by get_instruction_handler()
      // which is guaranteed to return a valid handler pointer
      // CRITICAL: PHI2 handlers must have ZERO side effects - only bus setup!
      pins = this->call_current_handler(pins);

      // Check RDY signal AFTER bus setup (now we know if it's read or write)
      // AEC was already checked above (applies to both phases)
      if (this->should_halt_for_rdy(pins)) {
        // HALT: Do NOT increment half_cycle - retry this PHI2 setup next tick
        return pins;
      }

      // PHI2 increments half_cycle AFTER handler execution (and RDY check)
      // so PHI1 sees the next odd cycle number
      // This allows PHI2 (even) and PHI1 (odd) to execute different code
      half_cycle++;

      if constexpr (ENABLE_TRACING) {
        trace_registers("after PHI2");
        trace_exit("tick<PHI2>");
      }

    } else { // Phase::PHI1
      // PHI1: Internal operation phase
      if constexpr (ENABLE_TRACING) {
        trace_enter("tick<PHI1>");
        trace_registers("before PHI1");
      }

      // Check RDY signal at START of PHI1 as well
      // The RDY check must happen in BOTH PHI2 and PHI1 because:
      // - PHI2 sets up the bus and checks RDY, halting if needed
      // - Memory tick happens unconditionally (external to CPU)
      // - PHI1 must re-check RDY using the bus state from PHI2
      // Without this check, CPU would incorrectly proceed with PHI1 internal
      // operations even when it was halted in the preceding PHI2 tick
      if (this->should_halt_for_rdy(pins)) {
        // HALT: Do NOT proceed with PHI1 - return without calling handler
        this->bus_snapshot_ = pins;
        return pins;
      }

      // Snapshot the I flag at the START of every PHI1, BEFORE any
      // instruction logic executes.  This is used by the interrupt
      // masking check at the next instruction boundary (fetch PHI2).
      //
      // Why every PHI1, not just at instruction start (fetch)?
      //   - For CLI/SEI (2-cycle): the save at the penultimate (cycle 1)
      //     PHI1 captures I BEFORE CLI clears it, giving the correct
      //     1-instruction delay.
      //   - For RTI (6-cycle): I is restored from the stack in cycle 4.
      //     The save at cycle 5 PHI1 captures the RESTORED value,
      //     so RTI takes effect immediately — no spurious delay.
      this->irq_i_flag_sample_ = (this->get(REG_P) & FLAG_I);

      // Handle I/O port and APU memory accesses BEFORE calling handler
      // These functions intercept memory operations for internal CPU features
      // Note: When PROCESSOR_TESTS is defined, these handlers return false immediately
      // for zero runtime overhead (compile-time optimization)
      const uint32_t addr = this->get_address_from_pins(pins);
      const bool is_write = !(pins & FAM65XX_RW);
      
      if (is_write) {
        const uint8_t data = FAM65XX_GET_DATA(pins);
        // Handle I/O port writes (6510 only)
        if (!this->handle_io_port_write(addr, data)) {
          // Handle APU writes (NES 6502 only)
          this->handle_apu_write(addr, data);
        }
      } else {
        // Handle I/O port reads (6510 only)
        if (!this->handle_io_port_read(pins, addr)) {
          // Handle APU reads (NES 6502 only)
          this->handle_apu_read(pins, addr);
        }
      }

      // Call handler to perform internal operations
      // Handler will increment half_cycle by 1 (odd → even)
      // COMPILE-TIME SAFETY: current_handler is always assigned by get_instruction_handler()
      // which is guaranteed to return a valid handler pointer
      pins = this->call_current_handler(pins);

      // Clock APU if present
      if constexpr (has_apu()) {
        pins = this->clock_apu(pins);
      }

      // Print instruction trace for debugging
      if constexpr (ENABLE_TRACING) {
        this->print_instruction_trace();
        trace_registers("after PHI1");
        trace_exit("tick<PHI1>");
      }
      this->bus_snapshot_ = pins;
    }

    return pins;
  }


  bool opdone() const {
    bool is_done = (this->current_handler == &fam65xx_t::fetch_opcode);
    if constexpr (ENABLE_TRACING) {
      trace("opdone() returning %s (handler=%s)",
            is_done ? "true" : "false",
            is_done ? "fetch_opcode" : "operation");
    }
    return is_done;
  }

  // ========================================================================
  // CONSTRUCTOR AND INITIALIZATION
  // ========================================================================

  bus_state_t init() {
    // Initialize processor-specific features
    this->init_conditional_features();

    // Return initial pin state
    bus_state_t pins = 0;
    return pins;
  }

  fam65xx_t()
    : ChipBase(ChipInfo{Traits.get_chip_id(), Traits.get_vendor()})
  {
    category_ = "CPU";  
    // Initialize CPU state to zero
    opcode_entry = {};
    current_handler = &fam65xx_t::fetch_opcode; // Always initialize to valid handler
    half_cycle = 0;
    active_interrupt = FAM65XX_INT_NONE;
    nmi_prev = 0;
    nmi_edge_latch = 0;
    nmi_output_latch_ = false;
    interrupt_shift_register = 0;
    interrupt_hijack_pending_ = false;
    branch_irq_suppression_ = false;
    branch_poll_shift_reg_ = 0;
    branch_nmi_pending_at_poll_ = false;
    wait_for_interrupt = false;
    stopped = false;
    trace_indent = 0;

    // Initialize operation and addressing mode handlers
    this->init_opcode_table();

    // Initialize conditional features
    this->init_conditional_features();

    // Initialize registers
    this->init_registers();

#ifdef CERMU_HAS_CHIP_DEBUG
    register_debug_fields();
#endif
  }

  ~fam65xx_t() override {
    // Cleanup conditional features
    if constexpr (has_apu()) {
      this->destroy_apu();
    }
  }

  // ========================================================================
  // ChipBase VIRTUAL METHOD IMPLEMENTATIONS
  // ========================================================================

#ifdef CERMU_HAS_GUI
  bool has_settings_content() const override { return true; }

  // Declared here, defined in fam65xx_gui.cpp with explicit instantiations
  void render_settings_content() override;
  ChipLayout* create_chip_layout() const override;
  std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
  const char* get_layout_chip_name() const override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
  void register_debug_fields() {
    using CPU = const fam65xx_t;
    auto& r = debug_registry_;

    // ---- CPU Registers ----
    r.category("CPU Registers");
    r.value("A", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_A); }, 8);
    r.value("X", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_X); }, 8);
    r.value("Y", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_Y); }, 8);
    r.address("SP", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_SP); }, 16);
    r.address("PC", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_PC); }, 16);
    r.flag_string("P", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_P); },
        "NVuBDIZC", "nvubdizc", 8);

    if constexpr (Traits.has(CPUCoreFlags::C816_16BIT)) {
      r.address("Direct Page", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_D); }, 16);
      r.value("Data Bank", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_DBR); }, 8);
      r.value("Program Bank", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_PBR); }, 8);
    }

    // ---- Internal State ----
    r.category("Internal State");
    r.value("IR", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_IR); }, 8);
    r.value("Data Latch", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_DL); }, 8);
    r.address("Address Bus", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->get(REG_AB); },
        Traits.address_bits);
    r.value("Half Cycle", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->half_cycle; }, 8);
    r.flag("Op Done", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->opdone(); });

    // ---- Interrupt State ----
    r.category("Interrupt State", false);
    r.flag("IRQ Disabled", +[](const ChipBase* c) -> uint32_t { return (static_cast<CPU*>(c)->get(REG_P) & FLAG_I) ? 1u : 0u; });

    if constexpr (Traits.has(CPUCoreFlags::CMOS_BASE)) {
      r.flag("Wait for IRQ", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->wait_for_interrupt; });
      r.flag("Stopped", +[](const ChipBase* c) -> uint32_t { return static_cast<CPU*>(c)->stopped; });
    }
  }
#endif

public:

  // ========================================================================
  // CPU STATE
  // ========================================================================

  /* Current execution state */
  opcode_info_t opcode_entry; /* Cached opcode entry (copied once) */
  bus_state_t (fam65xx_t::*current_handler)(
      bus_state_t);    /* Current instruction handler */
  uint8_t half_cycle; /* Current cycle within instruction */

  /* Interrupt state - hardware-accurate shift register system */
  interrupt_t active_interrupt; /* Currently active interrupt (enum serves as
                                   vector index) */
  uint8_t nmi_prev;             /* Previous NMI line state for edge detection */
  uint8_t nmi_edge_latch;       /* Internal edge-detect flip-flop: set on /NMI falling edge,
                                   cleared when NMI vector is fetched.  Persists regardless of
                                   subsequent pin state — this IS the NMI pending flag. */
  bool nmi_output_latch_;        /* 1-cycle pipeline output: reflects nmi_edge_latch from the
                                   PREVIOUS cycle.  NMI is serviceable when this is true.
                                   Implements the "sample at N, act after N+1" delay. */
  uint32_t interrupt_shift_register; /* Combined shift register for all
                                        interrupt types */
  uint8_t irq_i_flag_sample_;  /* I flag snapshot from the START of the current
                                  instruction (taken in fetch_opcode PHI1).
                                  Used for IRQ masking instead of the live I
                                  flag to implement the 6502's 1-instruction
                                  pipeline delay — CLI/SEI/PLP/RTI effects on
                                  interrupt masking are deferred until the
                                  NEXT instruction boundary. */
  uint8_t brk_is_software_;     /* Set in op_brk case 1 when the instruction is
                                  a software BRK (active_interrupt was NONE).
                                  Used for the B flag decision in the pushed P
                                  register.  Separate from active_interrupt so
                                  that NMI vector hijacking can change the vector
                                  without losing the B flag information. */
  bool interrupt_hijack_pending_; /* Deferred interrupt hijack flag.
                                    Set at PHI2 when an interrupt is detected at
                                    the fetch boundary; the actual redirect to
                                    op_brk happens at PHI1 in fetch_opcode case 1.
                                    This lets the dummy opcode read (T0) occur
                                    first, matching real 6502 timing where both
                                    software BRK and hardware interrupts take 7
                                    cycles. */
  bool branch_irq_suppression_;   /* Set when a taken branch without page cross
                                    completes.  Suppresses interrupt hijacking
                                    at the next fetch boundary UNLESS the
                                    interrupt was already detectable at the
                                    branch's penultimate cycle (2-bit shift
                                    register snapshot).  Models the 6502 quirk
                                    where the fixup cycle doesn't poll.
                                    Cleared at the first fetch_opcode PHI1. */
  uint32_t branch_poll_shift_reg_; /* Shift register snapshot at the branch's
                                     penultimate cycle (taken path, case 1 PHI1).
                                     Used with a 2-bit check (not 3-bit) to
                                     detect if IRQ was already pending. */
  bool branch_nmi_pending_at_poll_; /* NMI output latch at the branch's
                                      penultimate cycle. */

  /* 65C02 extended state */
  bool wait_for_interrupt; /* WAI instruction state */
  bool stopped;            /* STP instruction state */
};

// ============================================================================
// CONCRETE CPU TYPE ALIASES
// ============================================================================

// ============================================================================
// Type aliases, trait constants, and pin layouts are defined in per-CPU
// wrapper headers (mos6502.h, mos6510.h, etc.) and re-exported outside
// the fam65xx namespace.  Include the headers you need directly.
// ============================================================================

} // namespace fam65xx
