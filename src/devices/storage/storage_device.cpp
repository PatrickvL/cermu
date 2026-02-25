/**
 * storage_device.cpp - StorageDevice base class implementation
 *
 * Provides the shared fliplist and media-state logic used by all
 * concrete storage peripherals.
 */

#include "storage_device.h"
#include <cstdio>

// ============================================================================
// Media State
// ============================================================================

void StorageDevice::eject_media() {
    media_path_.clear();
    media_loaded_ = false;
}

// ============================================================================
// Fliplist
// ============================================================================

void StorageDevice::fliplist_add(const char* filepath) {
    std::string path(filepath);

    // Avoid duplicates
    for (const auto& entry : fliplist_) {
        if (entry == path) return;
    }
    fliplist_.push_back(std::move(path));

    // If this is the currently loaded media, update index
    if (media_loaded_ && media_path_ == filepath) {
        fliplist_index_ = static_cast<int>(fliplist_.size()) - 1;
    }
    printf("StorageDevice: Fliplist add '%s' (total: %zu)\n", filepath,
           fliplist_.size());
}

void StorageDevice::fliplist_remove(int index) {
    if (index < 0 || index >= static_cast<int>(fliplist_.size())) return;

    fliplist_.erase(fliplist_.begin() + index);

    // Adjust current index
    if (fliplist_.empty()) {
        fliplist_index_ = -1;
    } else if (fliplist_index_ >= static_cast<int>(fliplist_.size())) {
        fliplist_index_ = static_cast<int>(fliplist_.size()) - 1;
    }
}

void StorageDevice::fliplist_clear() {
    fliplist_.clear();
    fliplist_index_ = -1;
}

bool StorageDevice::flip_next() {
    if (fliplist_.empty()) return false;

    fliplist_index_++;
    if (fliplist_index_ >= static_cast<int>(fliplist_.size())) {
        fliplist_index_ = 0;  // Wrap around
    }

    return swap_media(fliplist_[fliplist_index_].c_str());
}

bool StorageDevice::flip_prev() {
    if (fliplist_.empty()) return false;

    fliplist_index_--;
    if (fliplist_index_ < 0) {
        fliplist_index_ = static_cast<int>(fliplist_.size()) - 1;  // Wrap
    }

    return swap_media(fliplist_[fliplist_index_].c_str());
}
