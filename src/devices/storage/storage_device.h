#pragma once
/**
 * storage_device.h - Abstract base for media-based peripheral devices
 *
 * Sits between PeripheralDevice and concrete storage implementations
 * (Drive1541Device, Datasette1530Device, etc.).  Provides the common
 * infrastructure that every media-swappable device needs:
 *
 *   - Media loaded/path tracking
 *   - Eject semantics
 *   - Activity indicator (drive LED, motor spin, …)
 *   - GUI file-dialog request flag
 *   - Disc/tape fliplist for quick media cycling
 */

#include "../../core/connector.h"
#include <cstdint>
#include <string>
#include <vector>

class StorageDevice : public PeripheralDevice {
public:
    ~StorageDevice() override = default;

    // --- Media state ---------------------------------------------------

    /// Is any media currently loaded/inserted?
    bool is_media_loaded() const { return media_loaded_; }

    /// Filesystem path of the currently loaded media image.
    const std::string& get_media_path() const { return media_path_; }

    /// Eject the current media — override to add device-specific cleanup.
    virtual void eject_media();

    // --- Activity indicator --------------------------------------------

    /// Returns true while the device is performing I/O (drive LED on,
    /// tape motor running, …).  Overridden by concrete devices.
    bool has_activity() const override { return false; }

    // --- File Dialog Request (GUI communication) -----------------------

    /// True when the device UI requested a file dialog (e.g. "Insert…").
    bool wants_file_dialog() const { return wants_file_dialog_; }

    /// Clear the file dialog request flag (called by GUI after opening).
    void clear_file_dialog_request() { wants_file_dialog_ = false; }

    // --- Media Fliplist ------------------------------------------------
    // A pre-loaded list of media images that can be cycled with a single
    // keypress — like VICE's "attach next disc in fliplist".

    /// Add a media path to the fliplist (avoids duplicates).
    void fliplist_add(const char* filepath);

    /// Remove a media entry from the fliplist by index.
    void fliplist_remove(int index);

    /// Clear the entire fliplist.
    void fliplist_clear();

    /// Swap to the next media in the fliplist.  Wraps around.
    /// Calls swap_media() internally — override that in the concrete device.
    bool flip_next();

    /// Swap to the previous media in the fliplist.  Wraps around.
    bool flip_prev();

    /// Get the current fliplist.
    const std::vector<std::string>& get_fliplist() const { return fliplist_; }

    /// Get the current fliplist index (-1 if empty or no match).
    int get_fliplist_index() const { return fliplist_index_; }

protected:
    // --- Override point for concrete devices ---------------------------

    /// Replace the currently loaded media without resetting device state.
    /// Concrete devices implement this to read their specific format.
    /// Returns true on success.
    virtual bool swap_media(const char* filepath) = 0;

    // --- State accessible by derived classes ---------------------------

    bool        media_loaded_  = false;
    std::string media_path_;
    bool        wants_file_dialog_ = false;

    // Fliplist
    std::vector<std::string> fliplist_;
    int                      fliplist_index_ = -1;
};
