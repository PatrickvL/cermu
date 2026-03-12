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
// =============================================================================
#pragma once

#include "core/memory_bus/configs.hpp"
