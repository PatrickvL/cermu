// =============================================================================
// connection.cpp — Connection implementations
// =============================================================================

#include "core/connection.hpp"
#include <cstdio>

// ============================================================================
// DirectConnection
// ============================================================================

void DirectConnection::connect(Port* a, Port* b) {
    if (!a || !b) {
        printf("DirectConnection: cannot connect null port(s)\n");
        return;
    }

    // Tear down any existing link first
    disconnect();

    a_ = a;
    b_ = b;

    // When port A's combined signal state changes, propagate to B as device input.
    // The port's existing wired-AND mechanism handles the merging.
    a_->set_signal_change_callback([this](Port* /*port*/, uint32_t combined_state) {
        if (b_)
            b_->write_system_signals(0xFFFFFFFF, combined_state);
    });

    b_->set_signal_change_callback([this](Port* /*port*/, uint32_t combined_state) {
        if (a_)
            a_->write_system_signals(0xFFFFFFFF, combined_state);
    });

    printf("DirectConnection: %s <-> %s linked\n",
           a_->get_name(), b_->get_name());
}

void DirectConnection::disconnect() {
    if (a_) {
        a_->set_signal_change_callback(nullptr);
    }
    if (b_) {
        b_->set_signal_change_callback(nullptr);
    }

    if (a_ && b_) {
        printf("DirectConnection: %s <-> %s unlinked\n",
               a_->get_name(), b_->get_name());
    }

    a_ = nullptr;
    b_ = nullptr;
}
