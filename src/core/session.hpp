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
#include "core/connection.hpp"
#include <memory>
#include <string>
#include <string_view>
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

    /// Look up a system by name.  Returns nullptr if not found.
    [[nodiscard]] System*       find_system(std::string_view name);
    [[nodiscard]] const System* find_system(std::string_view name) const;

    // ── Connection management ───────────────────────────────────────────

    /// Add a connection.  The session takes ownership.
    void add_connection(std::unique_ptr<Connection> connection);

    /// Access all connections.
    [[nodiscard]] const std::vector<std::unique_ptr<Connection>>& connections() const {
        return connections_;
    }

    // ── Lifecycle ───────────────────────────────────────────────────────

    /// Reset all systems and reconnect all connections.
    void reset();

private:
    struct Entry {
        std::string             name;
        std::unique_ptr<System> system;
    };

    std::vector<Entry>                       systems_;
    std::vector<std::unique_ptr<Connection>> connections_;
    size_t                                   focused_index_ = 0;
};
