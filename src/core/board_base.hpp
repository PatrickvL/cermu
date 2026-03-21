#pragma once
// =============================================================================
// board_base.hpp — Non-templated base for all boards
// =============================================================================
//
// BoardBase is the abstract base class for Board<Spec> and concrete board
// subclasses (e.g. VIC20Board).  It owns:
//
//   - A non-owning component index into the chips and ports that the
//     derived Board<Spec> owns, enabling type-erased iteration from
//     System and SessionGUI.
//
//   - Port storage (vector<unique_ptr<Port>>) — physical connector jacks
//     soldered onto the board.  Template-independent, so it lives here
//     rather than in the templated Board<Spec>.
//
// Inherits ComponentBase so that boards themselves can participate in the
// component hierarchy (e.g. a Drive1541Board attached as a peripheral).
//

#include "core/component_base.hpp"

class System;  // Forward declaration for parent back-reference

#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

// Forward declarations — full definitions in port.hpp.
class Port;
struct PortDefinition;

class BoardBase : public ComponentBase {
public:
    ~BoardBase() override;  // defined in board_base.cpp for unique_ptr<Port>

    // ── ComponentBase overrides ──────────────────────────────────────────

    // Default name — concrete boards should override with a meaningful label.
    const char* name() const override { return "Board"; }

    // Reset all registered components.  Concrete boards may override to add
    // post-reset fixups (e.g. re-wiring callbacks, restoring banking state).
    void reset() override {
        for (auto* c : components_)
            c->reset();
    }

    // ── Parent system (set by System::register_board) ────────────────────
    System* system() const { return system_; }
    void set_system(System* s) { system_ = s; }

    // ── Board lifecycle ──────────────────────────────────────────────────

    /// Optional power-on hook.  Called once before the first tick() when the
    /// board is powered up (cold start).  Default is a no-op.
    virtual void power_on() {}

    /// Per-cycle (or per-machine-cycle) tick.  Default is a no-op.
    /// Concrete boards override this to run the tick loop (CPU + chips).
    virtual void tick() {}

    // ── Component registry ───────────────────────────────────────────────
    //
    // Non-owning index of all chips and ports on this board.  Populated
    // by Board<Spec>::register_board_components() after chip creation and
    // port construction.
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

    // ── Port ownership ───────────────────────────────────────────────────
    //
    // Physical connector jacks on this board.  Ports are template-independent,
    // so ownership lives here in the non-templated base rather than in
    // Board<Spec>.  Ports created via add_port() are automatically included
    // in register_board_components().
    //

    /// Add a port from a definition (creates the Port internally).
    /// Returns the port index.
    int add_port(const PortDefinition& def, int port_number = 0);

    /// Add a pre-constructed port (takes ownership).  Returns the port index.
    int add_port(std::unique_ptr<Port> port);

    /// Get a port by index (nullptr if out of range).
    [[nodiscard]] Port* get_port(int index);
    [[nodiscard]] const Port* get_port(int index) const;

    /// Get all ports on this board.
    [[nodiscard]] const std::vector<std::unique_ptr<Port>>& get_ports() const {
        return ports_;
    }

    /// Number of ports on this board.
    [[nodiscard]] int port_count() const {
        return static_cast<int>(ports_.size());
    }

    /// Remove all ports from this board.
    void clear_ports();

    /// Register a component in the board's component index (non-owning).
    /// Public so that ChipSet::register_extras() can add extra value-typed
    /// chips from outside the class hierarchy.
    void register_component(ComponentBase* c) {
        if (c) components_.push_back(c);
    }

protected:
    void clear_components() {
        components_.clear();
    }

    std::vector<std::unique_ptr<Port>> ports_;
    System*        system_     = nullptr;  // Non-owning back-reference to parent system

private:
    std::vector<ComponentBase*> components_;  // non-owning; lifetime in Board<Spec>
};
