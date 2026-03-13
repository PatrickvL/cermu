// =============================================================================
// board_base.cpp — BoardBase implementation
// =============================================================================

#include "core/board_base.hpp"

#include "core/port.hpp"

// Destructor must be defined here (not in the header) because
// unique_ptr<Port> needs Port's complete type for destruction.
BoardBase::~BoardBase() = default;

// ── Port ownership ───────────────────────────────────────────────────────────

int BoardBase::add_port(const PortDefinition& def, int port_number) {
    int index = static_cast<int>(ports_.size());
    ports_.push_back(std::make_unique<Port>(def, port_number));
    return index;
}

int BoardBase::add_port(std::unique_ptr<Port> port) {
    int index = static_cast<int>(ports_.size());
    ports_.push_back(std::move(port));
    return index;
}

Port* BoardBase::get_port(int index) {
    if (index >= 0 && index < static_cast<int>(ports_.size()))
        return ports_[index].get();
    return nullptr;
}

const Port* BoardBase::get_port(int index) const {
    if (index >= 0 && index < static_cast<int>(ports_.size()))
        return ports_[index].get();
    return nullptr;
}

void BoardBase::clear_ports() {
    ports_.clear();
}
