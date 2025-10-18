/*
 * fam65xx_unified_opcode_tables.cpp - Implementation for Unified Opcode Table System
 *
 * This file provides the runtime implementations for the unified opcode table system,
 * including table initialization and validation functions.
 */

#define AIEMUC_IMPL
#include "fam65xx_unified_opcode_tables.hpp"
#include <cstring>  // For memcpy

namespace fam65xx_unified_tables {

#ifdef AIEMUC_IMPL

// ============================================================================
// TABLE STORAGE AND INITIALIZATION
// ============================================================================

namespace tables {
    // Runtime-initialized tables
    std::array<opcode_info_t, 256> mos6502_table;
    std::array<opcode_info_t, 256> mos6510_table;
    std::array<opcode_info_t, 256> wdc65c02_table;
    std::array<opcode_info_t, 256> rockwell65c02_table;
    std::array<opcode_info_t, 256> wdc65c816_table;
    
    // Initialize all tables
    void initialize_tables() {
        // Generate MOS 6502 table
        mos6502_table = generate_opcode_table<fam65xx_core::MOS6502Tag>();
        
        // Generate MOS 6510 table
        mos6510_table = generate_opcode_table<fam65xx_core::MOS6510Tag>();
        
        // Generate WDC 65C02 table
        wdc65c02_table = generate_opcode_table<fam65xx_core::WDC65C02Tag>();
        
        // Generate Rockwell 65C02 table
        rockwell65c02_table = generate_opcode_table<fam65xx_core::Rockwell65C02Tag>();
        
        // Generate WDC 65C816 table
        wdc65c816_table = generate_opcode_table<fam65xx_core::WDC65C816Tag>();
    }
}

#endif // AIEMUC_IMPL

} // namespace fam65xx_unified_tables

// ============================================================================
// PROCESSOR-SPECIFIC TABLE IMPLEMENTATIONS (for fam65xx_tables.hpp)
// ============================================================================

namespace fam65xx_variants {

#ifdef AIEMUC_IMPL

// Static arrays that get initialized at startup
static opcode_info_t mos6502_table_storage[256];
static opcode_info_t mos6510_table_storage[256];
static opcode_info_t wdc65c02_table_storage[256];
static opcode_info_t rockwell65c02_table_storage[256];
static opcode_info_t wdc65c816_table_storage[256];

// Exposed arrays
const opcode_info_t mos6502_opcode_table[256] = {};  // Will be initialized via init function
const opcode_info_t mos6510_opcode_table[256] = {};
const opcode_info_t wdc65c02_opcode_table[256] = {};
const opcode_info_t rockwell65c02_opcode_table[256] = {};
const opcode_info_t wdc65c816_opcode_table[256] = {};

// Initialization function to populate the tables
static bool tables_initialized = false;

void initialize_processor_tables() {
    if (tables_initialized) return;
    
    // Generate each processor's table using the template function from unified tables
    for (int i = 0; i < 256; ++i) {
        uint8_t opcode = static_cast<uint8_t>(i);
        mos6502_table_storage[i] = fam65xx_unified_tables::get_processor_opcode_entry<fam65xx_core::MOS6502Tag>(opcode);
        mos6510_table_storage[i] = fam65xx_unified_tables::get_processor_opcode_entry<fam65xx_core::MOS6510Tag>(opcode);
        wdc65c02_table_storage[i] = fam65xx_unified_tables::get_processor_opcode_entry<fam65xx_core::WDC65C02Tag>(opcode);
        rockwell65c02_table_storage[i] = fam65xx_unified_tables::get_processor_opcode_entry<fam65xx_core::Rockwell65C02Tag>(opcode);
        wdc65c816_table_storage[i] = fam65xx_unified_tables::get_processor_opcode_entry<fam65xx_core::WDC65C816Tag>(opcode);
    }
    
    // Copy to the exposed arrays (const_cast is safe here since we're initializing)
    memcpy(const_cast<opcode_info_t*>(mos6502_opcode_table), mos6502_table_storage, sizeof(mos6502_opcode_table));
    memcpy(const_cast<opcode_info_t*>(mos6510_opcode_table), mos6510_table_storage, sizeof(mos6510_opcode_table));
    memcpy(const_cast<opcode_info_t*>(wdc65c02_opcode_table), wdc65c02_table_storage, sizeof(wdc65c02_opcode_table));
    memcpy(const_cast<opcode_info_t*>(rockwell65c02_opcode_table), rockwell65c02_table_storage, sizeof(rockwell65c02_opcode_table));
    memcpy(const_cast<opcode_info_t*>(wdc65c816_opcode_table), wdc65c816_table_storage, sizeof(wdc65c816_opcode_table));
    
    tables_initialized = true;
}

// Auto-initialize tables at startup
static struct TableInitializer {
    TableInitializer() { 
        initialize_processor_tables();
        fam65xx_unified_tables::tables::initialize_tables();
    }
} table_initializer;

#endif // AIEMUC_IMPL

} // namespace fam65xx_variants