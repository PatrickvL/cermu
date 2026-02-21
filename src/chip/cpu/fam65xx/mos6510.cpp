/*
 * mos6510.cpp - C API Wrapper Implementation for MOS 6510 CPU
 */

#include "mos6510.h"
#include "fam65xx.hpp"
#include "fam65xx_gui.h"

using namespace fam65xx;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using mos6510_cpu_t = fam65xx::mos6510_cpu_impl_t;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<mos6510_cpu_t *>(ptr)

// ============================================================================
// MOS 6510 (C64/C128) IMPLEMENTATION
// ============================================================================

mos6510_t *mos6510_create(void) {
  mos6510_t *cpu = reinterpret_cast<mos6510_t *>(new mos6510_cpu_t());
#ifdef IMGUI_VERSION
  fam65xx::register_mos6510_for_gui(cpu);
#endif
  return cpu;
}

void mos6510_destroy(mos6510_t *cpu) {
  // Unregister from GUI system before destroying
#ifdef IMGUI_VERSION
  fam65xx::unregister_cpu_from_gui(cpu);
#endif
  delete CPU_CAST(cpu);
}

bus_state_t mos6510_init(mos6510_t *cpu, const mos6510_desc_t *desc) {
  // Initialize base CPU with the chip descriptor
  bus_state_t pins = CPU_CAST(cpu)->init(&desc->base);

  // Initialize 6510-specific I/O port state
  auto *cpu_impl = CPU_CAST(cpu);
  if constexpr (MOS6510.has_io_port()) {
    // Initialize I/O port with default C64 state
    cpu_impl->init_io_port();

    // Set descriptor and chip instance for bank_change callback
    // The mixin will call descriptor->bank_change() when banking bits change
    cpu_impl->descriptor = &mos6510_descriptor;
    cpu_impl->chip_instance = cpu;
  }

  return pins;
}

bus_state_t mos6510_reset(mos6510_t *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->reset(pins);
}

bus_state_t mos6510_bootstrap(mos6510_t *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->bootstrap(pins);
}

bool mos6510_opdone(mos6510_t *cpu) { return CPU_CAST(cpu)->opdone(); }

// Register getters
uint8_t mos6510_get_a(mos6510_t *cpu) { return CPU_CAST(cpu)->get(REG_A); }

uint8_t mos6510_get_x(mos6510_t *cpu) { return CPU_CAST(cpu)->get(REG_X); }

uint8_t mos6510_get_y(mos6510_t *cpu) { return CPU_CAST(cpu)->get(REG_Y); }

uint8_t mos6510_get_s(mos6510_t *cpu) { return CPU_CAST(cpu)->get(REG_SPL); }

uint8_t mos6510_get_p(mos6510_t *cpu) { return CPU_CAST(cpu)->get(REG_P); }

uint16_t mos6510_get_pc(mos6510_t *cpu) { return CPU_CAST(cpu)->get(REG_PC); }

uint8_t mos6510_get_ir(mos6510_t *cpu) { return CPU_CAST(cpu)->get(REG_IR); }

// Get current opcode entry (for disassembly)
opcode_info_t mos6510_get_opcode_entry(mos6510_t *cpu) {
  return CPU_CAST(cpu)->opcode_entry;
}

// Look up opcode entry from opcode table
opcode_info_t mos6510_lookup_opcode(uint8_t opcode) {
  static const auto table = fam65xx::generate_opcode_table_for_traits(MOS6510);
  return table[opcode];
}

// Register setters
void mos6510_set_a(mos6510_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_A, value);
}

void mos6510_set_x(mos6510_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_X, value);
}

void mos6510_set_y(mos6510_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_Y, value);
}

void mos6510_set_s(mos6510_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_SPL, value);
}

void mos6510_set_p(mos6510_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_P, value);
}
void mos6510_set_pc(mos6510_t *cpu, uint16_t value) {
  CPU_CAST(cpu)->set(REG_PC, value);
}

void mos6510_set_ab(mos6510_t *cpu, uint16_t value) {
  CPU_CAST(cpu)->set(REG_AB, value);
}

void mos6510_transition_to_fetch(mos6510_t *cpu) {
  CPU_CAST(cpu)->transition_to_fetch();
}

// I/O Port access (6510-specific)
uint8_t mos6510_get_io_ddr(mos6510_t *cpu) {
  return CPU_CAST(cpu)->io_port.direction;
}

uint8_t mos6510_get_io_data(mos6510_t *cpu) {
  return CPU_CAST(cpu)->io_port.data;
}

uint8_t mos6510_get_io_input(mos6510_t *cpu) {
  return CPU_CAST(cpu)->io_port.input;
}

void mos6510_set_io_input(mos6510_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->io_port.input = value;
}

void mos6510_set_bank_change_context(mos6510_t *cpu, void* context) {
  CPU_CAST(cpu)->chip_instance = context;
}

// ============================================================================
// CHIP DESCRIPTOR AND INTERFACE
// ============================================================================

// Chip-compatible tick function
bus_state_t mos6510_tick_phi2(void *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->tick<mos6510_cpu_t::Phase::PHI2>(pins);
}

bus_state_t mos6510_tick_phi1(void *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->tick<mos6510_cpu_t::Phase::PHI1>(pins);
}

// Chip descriptor for system registration
chip_descriptor_t mos6510_descriptor = {
    .description = "MOS 6510 CPU (C64/C128)",
    .create = [](chip_descriptor_t *desc) -> void * {
      return mos6510_create();
    },
    .destroy =
        [](void *cpu) { mos6510_destroy(reinterpret_cast<mos6510_t *>(cpu)); },
    .bus_attach = nullptr,
    .bank_change = nullptr // Will be set by the system (e.g., C64) if banking callbacks are needed
};
