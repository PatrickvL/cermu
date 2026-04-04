#pragma once
// =============================================================================
// session.hpp — Session: owns named systems and inter-system connections
// =============================================================================
//
// A Session is a runtime container that holds one or more emulated systems
// and the connections between them.  It provides a focused-system concept
// for the GUI layer and delegates lifecycle calls (reset, power_on) to all
// managed systems.
//
// SessionGUI will eventually own a Session instead of a bare System*.
// Until that migration happens, Session is additive and standalone.
//
// =============================================================================

#include "core/system.hpp"
#include <memory>
#include <string>
#include <vector>

// =============================================================================
// Session
// =============================================================================

class Session {
public:
    Session() = default;
    ~Session() = default;

    // Non-copyable, movable
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = default;
    Session& operator=(Session&&) = default;

    // ── System management ───────────────────────────────────────────────

    /// Add a named system.  Returns a non-owning pointer to it.
    System* add_system(std::string name, std::unique_ptr<System> system);

    /// Number of systems in this session.
    [[nodiscard]] size_t system_count() const { return systems_.size(); }

    /// Access the primary (first) system, or nullptr if empty.
    [[nodiscard]] System*       primary_system();
    [[nodiscard]] const System* primary_system() const;

    /// Access the currently focused system, or nullptr if empty.
    [[nodiscard]] System*       focused_system();
    [[nodiscard]] const System* focused_system() const;

    /// Get/set the focused system index.
    [[nodiscard]] size_t focused_system_index() const { return focused_index_; }
    void set_focused_system(size_t index);

    // ── Lifecycle ───────────────────────────────────────────────────────

    /// Reset all systems and reconnect all connections.
    void reset();

private:
    struct Entry {
        std::string             name;
        std::unique_ptr<System> system;
    };
    std::vector<Entry>  systems_;
    size_t              focused_index_ = 0;
};
