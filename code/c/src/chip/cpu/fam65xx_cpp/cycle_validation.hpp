#ifndef CYCLE_VALIDATION_HPP
#define CYCLE_VALIDATION_HPP

#include "cycle_types.hpp"
#include <cstdio>

namespace fam65xx_cpp {

// Compile-time validation - always enabled with enhanced debugging info
namespace cycle_validation {
    // Template-based validation helpers that show opcode and cycle in error messages
    template<int opcode, int cycle>
    constexpr void trigger_multiple_sync_error() {
        static_assert(opcode < 0, "MULTIPLE_SYNC_FLAGS_DETECTED - Check cycle implementations for duplicate SYNC placements. Opcode and cycle are visible in template parameters above.");
    }
    
    template<int opcode>
    constexpr void trigger_missing_sync_error() {
        static_assert(opcode < 0, "NO_SYNC_FLAG_FOUND - Every opcode must have exactly one SYNC flag in its final cycle. Opcode is visible in template parameter above.");
    }
}

// Enhanced validation macros that provide specific opcode/cycle information in compiler errors
#define VALIDATE_MULTIPLE_SYNC_TEMPLATE(opcode_const, cycle_const) \
    cycle_validation::trigger_multiple_sync_error<opcode_const, cycle_const>()

#define VALIDATE_MISSING_SYNC_TEMPLATE(opcode_const) \
    cycle_validation::trigger_missing_sync_error<opcode_const>()

// Fallback validation macros for runtime variables
#define VALIDATE_MULTIPLE_SYNC(opcode_var, cycle_var) \
    cycle_validation::trigger_multiple_sync_error<255, MAX_CYCLES>()

#define VALIDATE_MISSING_SYNC(opcode_var) \
    cycle_validation::trigger_missing_sync_error<255>()

} // namespace fam65xx_cpp

#endif // CYCLE_VALIDATION_HPP