#pragma once

#include "generic_gui.h"
#include "system_selection_dialog.h"
#include "../core/emulated_system.h"
#include <memory>
#include <string>

class Drive1541Device;  // Forward declaration for drive file dialog

/**
 * SystemGUI - GUI for any EmulatedSystem
 *
 * This class provides a GUI that works with any system implementing
 * the EmulatedSystem interface. It handles display scaling, keyboard input,
 * and basic menus. Systems can extend menus via their callback methods.
 *
 * Generic emulation infrastructure (threading, audio, framebuffer,
 * frame pacing, FPS tracking) lives in the GenericEmulatorGUI base class.
 */
class SystemGUI : public GenericEmulatorGUI {
private:
    std::unique_ptr<EmulatedSystem> system_;

    // System selection dialog
    SystemSelectionDialog system_selection_dialog_;
    
    // Last selected file path for file dialog
    std::string last_file_path_;
    
    // Pending drive insert — set when a 1541 drive requests a file dialog
    Drive1541Device* pending_drive_insert_ = nullptr;
    
    // Pending file to load after system selection (from command line)
    std::string pending_file_path_;
    
public:
    /**
     * Constructor - takes ownership of the system (can be nullptr to show selection dialog)
     */
    explicit SystemGUI(std::unique_ptr<EmulatedSystem> system, const char* pending_file = nullptr);
    virtual ~SystemGUI();
    
    // Override init to allocate framebuffer after OpenGL context is created
    bool init(const char* window_title, int width, int height);
    
    // Override virtual hooks from GenericEmulatorGUI
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
    
    // Override frame delay - VSync handles display pacing, accumulator handles emulation
    uint32_t get_frame_delay_ms() const override { return 0; }
    
private:
    /// Open file dialog with system-appropriate filters.
    /// @param dialog_key  ImGuiFileDialog key (different keys for different contexts)
    /// @param title       Dialog window title
    void open_file_dialog(const char* dialog_key, const char* title);
    
    /// Check attached 1541 drives for pending file dialog requests.
    void poll_drive_file_dialog_requests();
    
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

