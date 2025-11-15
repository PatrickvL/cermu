/*
 * inc_lint_prevention.hpp - Comprehensive Lint Prevention for .inc.hpp Files
 * 
 * This header provides a complete standalone analysis environment for .inc.hpp
 * template implementation files. It creates a surrogate class context that
 * matches the real template class structure, allowing linters and IDEs to
 * analyze member functions correctly without template compilation errors.
 *
 * KEY DESIGN PRINCIPLES:
 * ======================
 * 1. Only active during standalone analysis (when FAM65XX_TEMPLATE_CONTEXT is not defined)
 * 2. Includes actual project headers to get real type definitions
 * 3. Creates surrogate class context that mimics the real template class
 * 4. Provides all essential constants, types, and member functions as stubs
 * 5. Suppresses compiler warnings that are irrelevant for lint analysis
 *
 * USAGE:
 * ======
 * At the start of each .inc.hpp file:
 *   #include "inc_lint_prevention.hpp"
 *   
 * At the end of each .inc.hpp file:
 *   #include "inc_lint_prevention_footer.hpp"
 */

#pragma once

// =============================================================================
// STANDALONE LINT ANALYSIS ENVIRONMENT
// =============================================================================

#ifndef FAM65XX_TEMPLATE_CONTEXT
  // This entire section only applies when analyzing .inc.hpp files standalone
  // When included in the real template, FAM65XX_TEMPLATE_CONTEXT will be defined

  // =========================================================================
  // COMPILER-SPECIFIC DIAGNOSTIC SUPPRESSION
  // =========================================================================
  
  #if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Winvalid-use-of-this"
    #pragma clang diagnostic ignored "-Wundeclared-identifier"
    #pragma clang diagnostic ignored "-Wunknown-type-name"
    #pragma clang diagnostic ignored "-Wunused-parameter"
    #pragma clang diagnostic ignored "-Wunused-variable"
    #pragma clang diagnostic ignored "-Wunused-function"
  #elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wunused-function"
  #elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4101) // unreferenced local variable
    #pragma warning(disable: 4102) // unreferenced label
    #pragma warning(disable: 4189) // local variable initialized but not referenced
  #endif

  // =========================================================================
  // INCLUDE REAL PROJECT HEADERS FOR ACCURATE TYPE DEFINITIONS
  // =========================================================================
  
// Include essential headers BEFORE entering namespace to avoid conflicts
#include <cstdint>
#include <type_traits>
#include <array>
#include <cstdio>      // For printf, fflush
#include <cstring>     // For memset, memcpy

// Include actual project type definitions
#include "../fam65xx_types.h"
#include "../fam65xx_processor_traits.hpp"

// =========================================================================
// SURROGATE NAMESPACE AND CLASS CONTEXT
// =========================================================================

namespace fam65xx {
    
    // Forward declare any missing processor tags (avoid redefinition)
    #ifndef FAM65XX_MOCK_PROCESSOR_TAG
    #define FAM65XX_MOCK_PROCESSOR_TAG
    
    struct MockProcessorTag {
        constexpr bool has(uint32_t flag) const { return false; }
        constexpr bool has_io_port() const { return false; }
    };
    #endif
    
    // Create surrogate template class that matches real template structure
    // This provides the correct class scope context for member function analysis
    template<typename ProcessorTag = MockProcessorTag>
    class fam65xx_t {
    public:
      // =====================================================================
      // SURROGATE CLASS MEMBERS (matching real template class)
      // =====================================================================
      
      // CPU register state (matching real template)
      union {
          uint8_t reg8[16];        
          uint16_t reg16[8];  
      };
      
      // Execution state (matching real template)
      opcode_info_t opcode_entry;
      bus_state_t (fam65xx_t::*current_handler)(bus_state_t);
      uint8_t cycle_index;
      
      // Surrogate processor traits object
      static constexpr MockProcessorTag Traits{};
      
      // Memory callback interface (matching real template)
      fam65xx_mem_read_t mem_read;
      fam65xx_mem_write_t mem_write;
      void* mem_user_data;
      
      // Handler lookup tables (matching real template)
      std::array<bus_state_t (fam65xx_t::*)(bus_state_t), to_index(OP::COUNT)> operation_handlers;
      std::array<bus_state_t (fam65xx_t::*)(bus_state_t), to_index(AM::COUNT)> addressing_mode_handlers;
      
      // =====================================================================
      // SURROGATE MEMBER FUNCTION STUBS
      // =====================================================================
      
      // Register access functions
      inline uint8_t get(reg8_t reg) const { return 0; }
      inline uint16_t get(reg16_t reg) const { return 0; }
      inline void set(reg8_t reg, uint8_t value) {}
      inline void set(reg16_t reg, uint16_t value) {}
      inline void inc(reg8_t reg) {}
      inline void inc(reg16_t reg) {}
      inline void dec(reg8_t reg) {}
      inline void dec(reg16_t reg) {}
      
      // Memory access functions
      template<bool store_in_register = true>
      inline bus_state_t phi2_read_template(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) { return pins; }
      template<Addr addr_arg, Bank bank_arg = Bank::DBR>
      inline bus_state_t phi2_read(bus_state_t pins, reg8_t data_reg) { return pins; }
      template<Addr addr_arg, Bank bank_arg = Bank::DBR>
      inline bus_state_t phi2_write(bus_state_t pins, uint8_t data) { return pins; }
      inline bus_state_t phi2_read_operand(bus_state_t pins, reg8_t target_reg) { return pins; }
      template<Addr addr_arg, Bank bank_arg = Bank::DBR>
      inline bus_state_t phi2_dummy_read(bus_state_t pins) { return pins; }
      inline void load(reg8_t data_reg, bus_state_t pins) {}
      
      // Flag manipulation functions
      inline void set_flag(uint8_t flag_mask) {}
      inline void clear_flag(uint8_t flag_mask) {}
      inline void update_flags(uint8_t clear_mask, uint8_t set_mask) {}
      inline void update_flag(uint8_t flag_mask, bool condition) {}
      inline void update_nz_flags(uint8_t value) {}
      
      // Arithmetic operation helpers
      inline void perform_adc(uint8_t operand) {}
      inline void perform_sbc(uint8_t operand) {}
      inline void perform_compare(uint8_t reg_value, uint8_t operand) {}
      
      // Address calculation helpers
      inline bool page_crossed(uint16_t addr1, uint16_t addr2) const { return false; }
      inline bool should_complete_write_cycle(bus_state_t pins) { return true; }
      
      // Control flow functions
      inline void transition_to_operation() {}
      inline void transition_to_fetch() {}
      
      // RMW operation helper is implemented in rmw.inc.hpp - no stub needed
      
      // Additional helper functions that might be missing
      inline uint8_t calc_nz_flags(uint8_t value) const { return 0; }
      inline uint8_t calc_z_flag(uint8_t value) const { return 0; }
      
      // 65C816 compatibility functions
      inline bool get_emulation_mode() const { return true; }
      inline uint16_t get_x_register() const { return 0; }
      inline uint16_t get_y_register() const { return 0; }
      
      // Processor trait functions (static constexpr)
      static constexpr bool has_cmos() { return false; }
      static constexpr bool has_wide_registers() { return false; }
      static constexpr bool has_illegal_opcodes() { return false; }
      static constexpr bool has_bcd() { return false; }
      static constexpr bool has_bcd_extra_cycle() { return false; }
      static constexpr bool has_apu() { return false; }
      static constexpr bool has_rmw_dummy_write() { return false; }
      
      // RMW operation helper - forward declaration
      template<typename OperationFunc>
      bus_state_t rmw_operation_helper(bus_state_t pins, OperationFunc operation_func) { return pins; }
      
      // =====================================================================
      // OPCODE TABLE TEMPLATE FUNCTION DECLARATION
      // =====================================================================
      
      // Special handling for opcode_tables.inc.hpp template specializations
      // Note: Template function removed to avoid lint parsing errors
      
      // =====================================================================
      // THE ACTUAL .INC.HPP MEMBER FUNCTIONS WILL BE DECLARED HERE
      // This is where the linter will see the member functions in proper class scope
      // =====================================================================
      
      // Note: Class intentionally left open - will be closed by inc_lint_prevention_footer.hpp
      // The VS Code language server may show "expected ';' after class" but this is by design
      
#endif // FAM65XX_TEMPLATE_CONTEXT
