// =============================================================================
// session.cpp — Session implementation
// =============================================================================

#include "core/cermu.hpp"
#include "core/session.hpp"
#include <cstdio>

// ============================================================================
// System management
// ============================================================================

System* Session::add_system(std::string name, std::unique_ptr<System> system) {
    if (!system) {
        log_info("Session: WARNING — null system '%s' ignored\n", name.c_str());
        return nullptr;
    }

    System* ptr = system.get();
    ptr->set_session(this);
    systems_.push_back({std::move(name), std::move(system)});
    return ptr;
}

System* Session::primary_system() {
    return systems_.empty() ? nullptr : systems_.front().system.get();
}

const System* Session::primary_system() const {
    return systems_.empty() ? nullptr : systems_.front().system.get();
}

System* Session::focused_system() {
    if (focused_index_ >= systems_.size()) return nullptr;
    return systems_[focused_index_].system.get();
}

const System* Session::focused_system() const {
    if (focused_index_ >= systems_.size()) return nullptr;
    return systems_[focused_index_].system.get();
}

void Session::set_focused_system(size_t index) {
    if (index < systems_.size())
        focused_index_ = index;
}

// ============================================================================
// Lifecycle
// ============================================================================

void Session::reset() {
    for (auto& entry : systems_)
        entry.system->reset();
}
