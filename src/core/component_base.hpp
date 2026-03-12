#pragma once
/**
 * component_base.h — Abstract root for all board-level components
 *
 * ChipBase (emulated chips) and Port (physical jacks) both derive
 * from this so that Board can hold a flat std::vector<ComponentBase*> without
 * knowing concrete types.
 *
 * The interface is deliberately minimal:
 *   - name()   — human-readable label for display and debugging
 *   - reset()  — opt-in power-on / hardware reset
 */

class ComponentBase {
public:
    virtual ~ComponentBase() = default;

    /// Human-readable component name (never nullptr).
    virtual const char* name() const = 0;

    /// Reset to power-on state.  Default is a no-op; chips with internal
    /// state override this to clear registers, timers, etc.
    virtual void reset() {}

protected:
    ComponentBase() = default;

    // Movable but non-copyable — components are identity objects with unique
    // board positions, but may be transferred (e.g. MemoryChipBase move ops).
    ComponentBase(ComponentBase&&) noexcept = default;
    ComponentBase& operator=(ComponentBase&&) noexcept = default;
    ComponentBase(const ComponentBase&) = delete;
    ComponentBase& operator=(const ComponentBase&) = delete;
};
