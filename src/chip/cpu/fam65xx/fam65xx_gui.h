#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations for FAM65XX CPU family debug windows
// These are C-compatible wrappers around the C++ template implementation
void fam65xx_render_debug_content(void *chip);
void fam65xx_render_settings_content(void *chip);
void fam65xx_render_layout_content(void *chip);

// Bus state update function (also needs C linkage)
void fam65xx_update_bus_state(void *chip, uint64_t bus_state);

#ifdef __cplusplus
}
#endif

#ifdef IMGUI_VERSION
#include "../../gui/chip_visualization.h"
#include "fam65xx_processor_traits.hpp"
#include <memory>
#include <unordered_map>

// Forward declarations
namespace fam65xx {
// Forward declare processor tag types instead of template parameters
struct MOS6502;
struct MOS6510;
struct NES6502;
struct Rockwell65C02;
struct WDC65C02;
struct WDC65C816;

template <const CPUTraits &Traits> class fam65xx_t;
class CPUGUIRenderer;

// C++ template interface for registering CPU instances with the GUI system
// Register a CPU instance for GUI rendering
template <const CPUTraits &Traits>
void register_cpu_for_gui(fam65xx_t<Traits> *cpu);

// Unregister a CPU instance
void unregister_cpu_from_gui(void *cpu);

// Non-template registration functions for specific CPU types
void register_mos6502_for_gui(void *cpu);
void register_mos6510_for_gui(void *cpu);
void register_csg7501_for_gui(void *cpu);
void register_nes6502_for_gui(void *cpu);
void register_rockwell65c02_for_gui(void *cpu);
void register_wdc65c816_for_gui(void *cpu);

// Non-template function for rendering CPU content (no window framing)
void render_cpu_debug_content_impl(void *cpu, const char *cpu_name);
void render_cpu_settings_content_impl(void *cpu, const char *cpu_name);

// Update bus state for CPU visualization
void fam65xx_update_bus_state(void *chip, bus_state_t bus_state);
} // namespace fam65xx

#endif // IMGUI_VERSION
