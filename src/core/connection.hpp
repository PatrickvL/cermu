#pragma once
// =============================================================================
// connection.hpp — Inter-board / inter-system connector wiring
// =============================================================================
//
// Connection is the abstract base for any link between two Ports.
// DirectConnection is the simplest implementation: it registers bidirectional
// SignalChangeCallbacks so each port's device-output changes propagate to the
// other via the existing open-collector AND mechanism.
//
// Future subclasses:
//   ChannelConnection  — cross-thread lock-free queue (for secondary boards)
//   IPCConnection      — cross-process shared memory
//   NetworkConnection  — cross-machine sockets
//
// =============================================================================

#include "core/port.hpp"   // Port

// =============================================================================
// Connection — abstract base
// =============================================================================

class Connection {
public:
    virtual ~Connection() = default;

    /// Wire two ports together.  The concrete class decides the mechanism.
    virtual void connect(Port* a, Port* b) = 0;

    /// Tear down the link.  Safe to call if not connected.
    virtual void disconnect() = 0;

    /// True after a successful connect() and before disconnect().
    [[nodiscard]] virtual bool is_connected() const = 0;
};

// =============================================================================
// DirectConnection — same-thread bidirectional bridge
// =============================================================================
//
// Registers SignalChangeCallbacks on both ports.  When port A's combined
// signal state changes (system or device output), port B sees it through
// its device-signal input, and vice versa.  This mirrors the physical
// wired-AND behaviour of a cable connecting two jacks.
//

class DirectConnection : public Connection {
public:
    ~DirectConnection() override { disconnect(); }

    void connect(Port* a, Port* b) override;
    void disconnect() override;

    [[nodiscard]] bool is_connected() const override { return a_ != nullptr && b_ != nullptr; }

    [[nodiscard]] Port* port_a() const { return a_; }
    [[nodiscard]] Port* port_b() const { return b_; }

private:
    Port* a_ = nullptr;
    Port* b_ = nullptr;
};
