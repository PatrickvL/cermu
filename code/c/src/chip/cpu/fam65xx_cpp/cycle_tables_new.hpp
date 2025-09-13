#ifndef CYCLE_TABLES_NEW_HPP
#define CYCLE_TABLES_NEW_HPP

// Unified header for the split cycle table system
// This file replaces the old monolithic cycle_tables.hpp

// Core types and constants
#include "cycle_types.hpp"

// Validation system
#include "cycle_validation.hpp"

// Addressing mode helpers
#include "cycle_addressing.hpp"

// Interrupt sequences
#include "cycle_interrupts.hpp"

// Instruction implementations
#include "cycle_instructions.hpp"

// Table generation and dispatch
#include "cycle_table_gen.hpp"

// Re-export the main class for backward compatibility
namespace fam65xx_cpp {

// Type alias for backward compatibility
template<typename BusConfig>
using CycleTablesNew = CycleTables<BusConfig>;

} // namespace fam65xx_cpp

#endif // CYCLE_TABLES_NEW_HPP