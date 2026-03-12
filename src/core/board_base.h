#pragma once
// =============================================================================
// board_base.h — Non-templated base for all boards
// =============================================================================
//
// BoardBase is the abstract base class for Board<Spec> and concrete board
// subclasses (e.g. VIC20Board).  It owns a non-owning component index into
// the chips and connectors that the derived Board<Spec> owns, enabling
// type-erased iteration from System and SessionGUI.
//
// Inherits ComponentBase so that boards themselves can participate in the
// component hierarchy (e.g. a Drive1541Board attached as a peripheral).
//

#include "component_base.h"

#include <cstring>
#include <span>
#include <string_view>
#include <vector>

class BoardBase : public ComponentBase {
public:
    // ── ComponentBase overrides ──────────────────────────────────────────

    // Default name — concrete boards should override with a meaningful label.
    const char* name() const override { return "Board"; }

    // Reset all registered components.  Concrete boards may override to add
    // post-reset fixups (e.g. re-wiring callbacks, restoring banking state).
    void reset() override {
        for (auto* c : components_)
            c->reset();
    }

    // ── Board lifecycle ──────────────────────────────────────────────────

    /// Optional power-on hook.  Called once before the first tick() when the
    /// board is powered up (cold start).  Default is a no-op.
    virtual void power_on() {}

    /// Per-cycle (or per-machine-cycle) tick.  Default is a no-op.
    /// Concrete boards override this to run the tick loop (CPU + chips).
    virtual void tick() {}

    // ── Component registry ───────────────────────────────────────────────
    //
    // Non-owning index of all chips and connectors on this board.  Populated
    // by Board<Spec>::register_board_components() after chip creation and
    // connector construction.
    //

    [[nodiscard]] std::span<ComponentBase* const> components() const {
        return components_;
    }

    [[nodiscard]] ComponentBase* find_component(std::string_view comp_name) const {
        for (auto* c : components_)
            if (c->name() && comp_name == c->name()) return c;
        return nullptr;
    }

    template<typename T>
    [[nodiscard]] T* find_component() const {
        for (auto* c : components_)
            if (auto* t = dynamic_cast<T*>(c)) return t;
        return nullptr;
    }

    template<typename T>
    [[nodiscard]] std::vector<T*> find_components() const {
        std::vector<T*> result;
        for (auto* c : components_)
            if (auto* t = dynamic_cast<T*>(c)) result.push_back(t);
        return result;
    }

protected:
    void register_component(ComponentBase* c) {
        if (c) components_.push_back(c);
    }

    void clear_components() {
        components_.clear();
    }

private:
    std::vector<ComponentBase*> components_;  // non-owning; lifetime in Board<Spec>
};
