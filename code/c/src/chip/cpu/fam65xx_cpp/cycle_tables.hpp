#ifndef CYCLE_TABLES_HPP
#define CYCLE_TABLES_HPP

// Unified header for the split cycle table system
// This file includes all split cycle table components for easier maintenance
// while preserving the original API for backward compatibility

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

#endif // CYCLE_TABLES_HPP