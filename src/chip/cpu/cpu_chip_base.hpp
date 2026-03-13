#pragma once

#include "core/chip.hpp"

// ============================================================================
// CPU CHIP BASE — intermediate base for all CPU/processor chips
// ============================================================================
//
// Shared foundation for CPU chips (6502 family, Z80 family, 6809, ...).
// Sets category_ = "CPU" so subtypes don't need to.

class CpuChipBase : public ChipBase {
public:
    CpuChipBase() { category_ = "CPU"; }
    explicit CpuChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "CPU"; }

    // ── Generic CPU lifecycle ───────────────────────────────────────────
    // Concrete CPU families (fam65xx_t, z80_t, ...) override these.
    // Systems can call through CpuChipBase* for init/reset without
    // downcasting to the concrete type.  tick() stays non-virtual —
    // the vtable cost matters in the per-cycle hot path.

    /// Initialize CPU state.  Returns the initial bus pin state.
    virtual bus_state_t init() = 0;

    /// Pin-based reset.  Returns updated bus pin state.
    /// Default pins = 0 is safe for callers that don't need to preserve
    /// existing bus signals (the next tick overwrites them anyway).
    virtual bus_state_t reset(bus_state_t pins = 0) = 0;
};
