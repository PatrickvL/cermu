// =============================================================================
// memory_bus.hpp — Umbrella include for the MemoryBus framework
// =============================================================================
//
// This header includes all components of the chip-select–aware memory bus:
//
//   memory_bus/config.hpp      — BusConfigConcept, feature probes, defaults
//   memory_bus/packing.hpp     — PackingTraits: chip-id types and sentinels
//   memory_bus/mmio.hpp        — MmioHandler: register-file callback record
//   memory_bus/viewer.hpp      — ViewerState, ModeSnapshot: per-viewer tables
//   memory_bus/masks.hpp       — DataBusMasks: partial-bus width masks
//   memory_bus/sub_tables.hpp  — IndexedSubTable, MaskedSubTable: sub-page dispatch
//   memory_bus/bus.hpp         — MemoryBus, BusView: main bus class
//   memory_bus/configs.hpp     — Pre-baked configurations and type aliases
//
// Include this single header for full MemoryBus access.  Individual component
// headers can be included directly for compilation firewall purposes.
//
// HARDWARE ACCURACY NOTES:
// ────────────────────────
// The MemoryBus models real hardware signal propagation:
// • All bus signals (ADDR, DATA, CS, R/W, clock edges, IRQ, etc.) are
//   propagated to all chips each clock cycle.
// • Each chip's tick() inspects the bus and reacts based on its select logic.
// • No explicit "bus master" abstraction; arbitration emerges from correct
//   sequencing of chip ticks and signal observation.
// • Address drivers (CPU, VIC-II, DMA) tick first to set up address lines.
// • Subsequent chips observe the bus and respond if selected.
// • This approach is hardware-faithful and composable across multiple systems.
//
// For detailed timing architecture, see docs/TIMING_ARCHITECTURE.md
//
// =============================================================================
#pragma once

#include "core/memory_bus/configs.hpp"
