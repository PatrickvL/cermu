#pragma once

#include "gui/emulator_host.hpp"
#include "gui/system_selection_dialog.hpp"
#include "core/system.hpp"
#include <memory>
#include <string>

class Drive1541Device;  // Forward declaration for drive file dialog

/**
 * SessionGUI - Owns an System and implements EmulatorHost hooks
 *
 * Routes input queue events to the focused system, manages system switching,
 * drag-and-drop file loading, and display scaling. Systems extend menus
 * via their callback methods.
 *
 * Generic host infrastructure (threading, audio, framebuffer,
 * frame pacing, FPS tracking) lives in the EmulatorHost base class.
 */
class SessionGUI : public EmulatorHost {
private:
    std::unique_ptr<System> system_;

    // System selection dialog
    SystemSelectionDialog system_selection_dialog_;
    
    // Last selected file path for file dialog
    std::string last_file_path_;
    
    // Pending drive insert — set when a 1541 drive requests a file dialog
    Drive1541Device* pending_drive_insert_ = nullptr;
    
    // Pending file to load after system selection (from command line)
    std::string pending_file_path_;

    // Filepath received via SDL drag-and-drop, processed next frame
    std::string pending_drop_path_;
    
public:
    /**
     * Constructor - takes ownership of the system (can be nullptr to show selection dialog)
     */
    explicit SessionGUI(std::unique_ptr<System> system, const char* pending_file = nullptr);
    virtual ~SessionGUI();
    
    // Override init to allocate framebuffer after OpenGL context is created
    bool init(const char* window_title, int width, int height);
    
    // Override virtual hooks from EmulatorHost
    void handle_events() override;
    void update_frame() override;
    void render_frame() override;
    void render_menu_bar() override;
    void render_screen() override;
    
    // Override optional windows
    void render_memory_viewer() override;
    void render_settings() override;
    void render_about() override;
    
    // System control
    void reset_emulation();
    void step_emulation();
    
    // System switching
    void switch_system(const char* system_name, int memory_option = -1, int region_option = -1, const std::map<std::string, bool>* peripherals = nullptr, const char* pending_file = nullptr, const std::map<std::string, std::string>* custom_settings = nullptr);
    
    // File loading
    void load_file_dialog();

    /// Process a file path received by drag-and-drop or any external source.
    /// Identifies the system, switches if needed, and loads or swap-attaches.
    void handle_dropped_file(const std::string& filepath);
    
    // Override frame delay - VSync handles display pacing, accumulator handles emulation
    uint32_t get_frame_delay_ms() const override { return 0; }

    // Returns true when a virtual mouse device is plugged into the system
    bool has_virtual_mouse_attached() const override;
    
private:
    /// Open file dialog with system-appropriate filters.
    /// @param dialog_key  ImGuiFileDialog key (different keys for different contexts)
    /// @param title       Dialog window title
    void open_file_dialog(const char* dialog_key, const char* title);
    
    /// Check attached 1541 drives for pending file dialog requests.
    void poll_drive_file_dialog_requests();

    /// Load a file into the current system (stop emu, configure, reset, load, restart).
    /// @param resolved_path  VFS-aware path to the file (may be archive inner path)
    /// @param display_path   Optional human-readable path for window title (e.g. container path)
    void load_selected_file(const std::string& resolved_path,
                            const char* display_path = nullptr);
    
    // Helper functions
    void update_window_title();
    void allocate_framebuffer();
    void teardown_current_system();

    /// Cached window title — avoids SDL_SetWindowTitle on every frame.
    std::string last_window_title_;

    // Emulation thread (override of base class pure virtual)
    void emu_thread_func() override;

    // Audio helpers
    void open_audio_device();
};

