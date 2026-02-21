/*
 * mos7501.cpp - C API Wrapper Implementation for MOS 7501/8501 CPU
 *
 * The MOS 7501 is the CPU used in the Commodore 16 and Plus/4 computers.
 * It shares the 6502 NMOS core with the 6510 but has:
 *   - No NMI line (CSG7501 traits: NO_NMI_LINE flag)
 *   - Different I/O port mask: 0x5F (pins 0-4 and 6)
 */

#include "mos7501.h"
#include "fam65xx.hpp"
#include "fam65xx_gui.h"

using namespace fam65xx;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using mos7501_cpu_t = fam65xx::fam65xx_t<CSG7501>;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<mos7501_cpu_t *>(ptr)

// ============================================================================
// MOS 7501 (C16/Plus4) IMPLEMENTATION
// ============================================================================

mos7501_t *mos7501_create(void) {
  mos7501_t *cpu = reinterpret_cast<mos7501_t *>(new mos7501_cpu_t());
#ifdef IMGUI_VERSION
  fam65xx::register_csg7501_for_gui(cpu);
#endif
  return cpu;
}

void mos7501_destroy(mos7501_t *cpu) {
#ifdef IMGUI_VERSION
  fam65xx::unregister_cpu_from_gui(cpu);
#endif
  delete CPU_CAST(cpu);
}

bus_state_t mos7501_init(mos7501_t *cpu, const mos7501_desc_t *desc) {
  bus_state_t pins = CPU_CAST(cpu)->init(&desc->base);

  auto *cpu_impl = CPU_CAST(cpu);
  if constexpr (CSG7501.has_io_port()) {
    cpu_impl->init_io_port();
    // bank_change_fn + bank_change_ctx are set by the system via
    // mos7501_set_bank_change() after init — no default wiring here.
  }

  return pins;
}

bus_state_t mos7501_reset(mos7501_t *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->reset(pins);
}

bus_state_t mos7501_bootstrap(mos7501_t *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->bootstrap(pins);
}

bool mos7501_opdone(mos7501_t *cpu) { return CPU_CAST(cpu)->opdone(); }

// Register getters
uint8_t mos7501_get_a(mos7501_t *cpu) { return CPU_CAST(cpu)->get(REG_A); }
uint8_t mos7501_get_x(mos7501_t *cpu) { return CPU_CAST(cpu)->get(REG_X); }
uint8_t mos7501_get_y(mos7501_t *cpu) { return CPU_CAST(cpu)->get(REG_Y); }
uint8_t mos7501_get_s(mos7501_t *cpu) { return CPU_CAST(cpu)->get(REG_SPL); }
uint8_t mos7501_get_p(mos7501_t *cpu) { return CPU_CAST(cpu)->get(REG_P); }
uint16_t mos7501_get_pc(mos7501_t *cpu) { return CPU_CAST(cpu)->get(REG_PC); }
uint8_t mos7501_get_ir(mos7501_t *cpu) { return CPU_CAST(cpu)->get(REG_IR); }

// Get current opcode entry (for disassembly)
opcode_info_t mos7501_get_opcode_entry(mos7501_t *cpu) {
  return CPU_CAST(cpu)->opcode_entry;
}

opcode_info_t mos7501_lookup_opcode(uint8_t opcode) {
  static const auto table = fam65xx::generate_opcode_table_for_traits(CSG7501);
  return table[opcode];
}

// Register setters
void mos7501_set_a(mos7501_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_A, value);
}
void mos7501_set_x(mos7501_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_X, value);
}
void mos7501_set_y(mos7501_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_Y, value);
}
void mos7501_set_s(mos7501_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_SPL, value);
}
void mos7501_set_p(mos7501_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_P, value);
}
void mos7501_set_pc(mos7501_t *cpu, uint16_t value) {
  CPU_CAST(cpu)->set(REG_PC, value);
}
void mos7501_set_ab(mos7501_t *cpu, uint16_t value) {
  CPU_CAST(cpu)->set(REG_AB, value);
}

void mos7501_transition_to_fetch(mos7501_t *cpu) {
  CPU_CAST(cpu)->transition_to_fetch();
}

// I/O Port access
uint8_t mos7501_get_io_ddr(mos7501_t *cpu) {
  return CPU_CAST(cpu)->io_port.direction;
}
uint8_t mos7501_get_io_data(mos7501_t *cpu) {
  return CPU_CAST(cpu)->io_port.data;
}
uint8_t mos7501_get_io_input(mos7501_t *cpu) {
  return CPU_CAST(cpu)->io_port.input;
}
void mos7501_set_io_input(mos7501_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->io_port.input = value;
}

void mos7501_set_bank_change(mos7501_t *cpu,
                             void(*fn)(void*, uint8_t),
                             void* context) {
  CPU_CAST(cpu)->bank_change_fn = fn;
  CPU_CAST(cpu)->bank_change_ctx = context;
}

// ============================================================================
// CHIP DESCRIPTOR AND INTERFACE
// ============================================================================

bus_state_t mos7501_tick_phi2(void *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->tick<mos7501_cpu_t::Phase::PHI2>(pins);
}

bus_state_t mos7501_tick_phi1(void *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->tick<mos7501_cpu_t::Phase::PHI1>(pins);
}

chip_descriptor_t mos7501_descriptor = {
    .description = "MOS 7501 CPU (C16/Plus4)",
    .create = [](chip_descriptor_t *desc) -> void * {
      return mos7501_create();
    },
    .destroy =
        [](void *cpu) { mos7501_destroy(reinterpret_cast<mos7501_t *>(cpu)); },
    .bus_attach = nullptr
};
