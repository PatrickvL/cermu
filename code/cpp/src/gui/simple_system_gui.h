#ifndef SIMPLE_SYSTEM_GUI_H
#define SIMPLE_SYSTEM_GUI_H

#include "generic_gui.h"
#include "system_selection_dialog.h"
#include "../core/emulated_system.h"
#include <memory>

/**
 * SimpleSystemGUI - Generic GUI for any EmulatedSystem
 *
 * This class provides a simple GUI that works with any system implementing
 * the EmulatedSystem interface. It handles display scaling, keyboard input,
 * and basic menus. Systems can extend menus via their callback methods.
 */
class SimpleSystemGUI : public GenericEmulatorGUI {
private:
    std::unique_ptr<EmulatedSystem> system_;
    uint32_t* framebuffer_;
    int fb_width_;
    int fb_height_;
    bool emulation_running_;
    bool emulation_paused_;
    float speed_multiplier_;
    
    // Statistics
    uint64_t total_frames_;
    uint32_t actual_fps_;
    uint32_t last_fps_time_;
    uint64_t last_fps_frame_count_;  // total_frames_ snapshot for FPS delta
    
    // Frame pacing (time accumulator for correct emulation speed)
    uint64_t frame_pace_counter_;       // SDL_GetPerformanceCounter value
    double frame_time_accumulator_;     // Accumulated real time in seconds
    
    // System selection dialog
    SystemSelectionDialog system_selection_dialog_;
    
    // Last selected file path for file dialog
    std::string last_file_path_;
    
    // Pending file to load after system selection (from command line)
    std::string pending_file_path_;

    // SDL audio output
    SDL_AudioDeviceID audio_device_;
    int audio_sample_rate_;           // Actual sample rate obtained from SDL
    
public:
    /**
     * Constructor - takes ownership of the system (can be nullptr to show selection dialog)
     */
    explicit SimpleSystemGUI(std::unique_ptr<EmulatedSystem> system, const char* pending_file = nullptr);
    virtual ~SimpleSystemGUI();
    
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
    void start_emulation();
    void pause_emulation();
    void reset_emulation();
    void step_emulation();
    
    // System switching
    void switch_system(const char* system_name, int memory_option = -1, int region_option = -1, const std::map<std::string, bool>* peripherals = nullptr, const char* pending_file = nullptr);
    
    // File loading
    void load_file_dialog();
    
    // Override frame delay - VSync handles display pacing, accumulator handles emulation
    uint32_t get_frame_delay_ms() const override { return 0; }
    
private:
    // Helper functions
    void update_fps();
    void reset_frame_pacing();
    void allocate_framebuffer();
    void free_framebuffer();
    void teardown_current_system();

    // Audio helpers
    void open_audio_device();
    void close_audio_device();
    static void sdl_audio_callback(void* userdata, uint8_t* stream, int len);
};

#endif // SIMPLE_SYSTEM_GUI_H