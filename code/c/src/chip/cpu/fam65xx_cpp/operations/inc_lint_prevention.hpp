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

namespace fam65xx_cpp {
    
    // Forward declare any missing processor tags (avoid redefinition)
    #ifndef FAM65XX_MOCK_PROCESSOR_TAG
    #define FAM65XX_MOCK_PROCESSOR_TAG
    struct MockProcessorTag {};
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
      
      // Memory callback interface (matching real template)
      fam65xx_mem_read_t mem_read;
      fam65xx_mem_write_t mem_write;
      void* mem_user_data;
      
      // Handler lookup tables (matching real template)
      std::array<bus_state_t (fam65xx_t::*)(bus_state_t), OP_COUNT> operation_handlers;
      std::array<bus_state_t (fam65xx_t::*)(bus_state_t), AM_COUNT> addressing_mode_handlers;
      
      // =====================================================================
      // SURROGATE MEMBER FUNCTION STUBS
      // =====================================================================
      
      // Memory access functions
      inline bus_state_t phi2_read(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) { return pins; }
      inline bus_state_t phi2_write(bus_state_t pins, reg16_t addr_reg, reg8_t data_reg) { return pins; }
      inline bus_state_t phi2_read_operand(bus_state_t pins, reg8_t target_reg) { return pins; }
      
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
      
      // =====================================================================
      // OPCODE TABLE TEMPLATE FUNCTION DECLARATION
      // =====================================================================
      
      // Special handling for opcode_tables.inc.hpp template specializations
      template<typename ProcessorType>
      static constexpr std::array<opcode_info_t, 256> generate_opcode_table() {
        return std::array<opcode_info_t, 256>{};
      }
      
      // =====================================================================
      // THE ACTUAL .INC.HPP MEMBER FUNCTIONS WILL BE DECLARED HERE
      // This is where the linter will see the member functions in proper class scope
      // =====================================================================

#endif // FAM65XX_TEMPLATE_CONTEXT
