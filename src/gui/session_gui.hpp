#pragma once

#include "gui/emulator_host.hpp"
#include "gui/system_selection_dialog.hpp"
#include "gui/launcher_panel.hpp"
#include "gui/scan_root_manager.hpp"
#include "core/session.hpp"
#include <memory>
#include <string>

class Drive1541Device;  // Forward declaration for drive file dialog
class DisplayPipelineBase;  // Forward declaration for display pipeline

/**
 * SessionGUI - Owns a Session (which owns systems) and implements EmulatorHost hooks
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
    Session  session_;             // Owns all systems and inter-system connections
    System*  system_ = nullptr;    // Non-owning convenience pointer to focused system

    // System selection dialog (legacy — retained as fallback)
    SystemSelectionDialog system_selection_dialog_;

    // Launcher panel (new unified launcher UI)
    LauncherPanel launcher_panel_;
    
    // Last selected file path for file dialog
    std::string last_file_path_;
    
    // Pending drive insert — set when a 1541 drive requests a file dialog
    Drive1541Device* pending_drive_insert_ = nullptr;
    
    // Pending file to load after system selection (from command line)
    std::string pending_file_path_;

    // Filepath received via SDL drag-and-drop, processed next frame
    std::string pending_drop_path_;

    // ========================================================================
    // Auto-hide menu bar state (§11.1)
    // ========================================================================
    bool   menu_bar_visible_     = true;   ///< Current visibility (drives animation)
    bool   menu_bar_pinned_      = false;  ///< Pinned via F12 (stays until F12 again)
    float  menu_bar_anim_        = 1.0f;   ///< Animation progress 0.0 (hidden) → 1.0 (visible)
    Uint32 menu_bar_show_time_   = 0;      ///< SDL_GetTicks() when bar became visible
    bool   menu_bar_hover_active_= false;  ///< Hover dwell is counting

    static constexpr float  kMenuBarAnimSpeed  = 8.0f;   ///< ~125ms at 60 FPS ease
    static constexpr Uint32 kMenuBarHoverDelay = 450;     ///< ms dwell at top before showing
    static constexpr Uint32 kMenuBarIdleHide   = 3000;    ///< ms idle before auto-hiding (hover trigger only)
    static constexpr float  kMenuBarHoverZone  = 8.0f;    ///< Pixels at top edge for hover trigger

    // ========================================================================
    // Emulation HUD state (§11.4)
    // ========================================================================
    bool show_hud_          = true;   ///< Master HUD toggle
    int  hud_corner_        = 1;      ///< 0=TL, 1=TR, 2=BL, 3=BR

    // ========================================================================
    // Scan root manager (§12)
    // ========================================================================
    scan_roots::ScanRootManager scan_root_manager_;
    bool show_scan_roots_dialog_ = false;  ///< Library → Scan roots… dialog
    
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
    void render_display_settings();
    void render_emulation_hud();

    /// Re-scan ports and owned devices for the active DisplayDevice.
    /// Locks the SDL audio device to prevent the audio callback from
    /// seeing a stale display_device_ pointer mid-swap.
    void refresh_display_device();

    /// Cached display device ID for change detection (avoids ABA
    /// pointer-reuse when the allocator gives the same address).
    std::string cached_display_id_;

    /// Display pipeline — owns the double-buffered frame sample buffers.
    /// Created during allocate_framebuffer() and connected to the system's
    /// VideoPort.  Automatically swaps buffers at frame boundaries via
    /// the on_frame_end callback.
    std::unique_ptr<DisplayPipelineBase> display_pipeline_;

    /// Cached window title — avoids SDL_SetWindowTitle on every frame.
    std::string last_window_title_;

    // Emulation thread (override of base class pure virtual)
    void emu_thread_func() override;

    // Audio helpers
    void open_audio_device();
};

