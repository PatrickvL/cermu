// =============================================================================
// mmio.hpp — Register-file (MMIO) handler record
// =============================================================================
//
// A lightweight callback pair for memory-mapped I/O chips.  The MemoryBus
// dispatches reads and writes to these handlers when a page's chip table
// holds a kRegChipBase sentinel.
//
// Handlers use raw function pointers (not std::function) for zero overhead
// and trivial copyability.  The void* ctx pointer carries the chip instance.
//
// =============================================================================
#pragma once

#include "core/system_lines.h"   // bus_state_t


struct MmioHandler {
    using HandlerFn = bus_state_t (*)(void* ctx, bus_state_t bus) noexcept;

    void*     ctx      = nullptr;
    HandlerFn on_read  = nullptr;
    HandlerFn on_write = nullptr;

    [[nodiscard]] bool valid() const noexcept { return on_read || on_write; }
};
