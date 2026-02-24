#ifndef SYSTEM_GUI_H
#define SYSTEM_GUI_H

#include "generic_gui.h"
#include "system_selection_dialog.h"
#include "../core/emulated_system.h"
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

class Drive1541Device;  // Forward declaration for drive file dialog

// ============================================================================
// Lock-free Single-Producer Single-Consumer ring buffer for audio samples.
// The emulation thread writes; the SDL audio callback reads.
// ============================================================================
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacity)
        : buf_(capacity, 0.0f), cap_(capacity) {}

    /// Number of samples available for reading.
    size_t available() const {
        size_t w = write_.load(std::memory_order_acquire);
        size_t r = read_.load(std::memory_order_acquire);
        return (w >= r) ? (w - r) : (cap_ - r + w);
    }

    /// Write up to @p count samples.  Returns number actually written.
    size_t write(const float* data, size_t count) {
        size_t w = write_.load(std::memory_order_relaxed);
        size_t r = read_.load(std::memory_order_acquire);
        size_t free = cap_ - 1 - ((w >= r) ? (w - r) : (cap_ - r + w));
        if (count > free) count = free;
        for (size_t i = 0; i < count; i++)
            buf_[(w + i) % cap_] = data[i];
        write_.store((w + count) % cap_, std::memory_order_release);
        return count;
    }

    /// Read up to @p count samples.  Returns number actually read.
    size_t read(float* data, size_t count) {
        size_t r = read_.load(std::memory_order_relaxed);
        size_t w = write_.load(std::memory_order_acquire);
        size_t avail = (w >= r) ? (w - r) : (cap_ - r + w);
        if (count > avail) count = avail;
        for (size_t i = 0; i < count; i++)
            data[i] = buf_[(r + i) % cap_];
        read_.store((r + count) % cap_, std::memory_order_release);
        return count;
    }

    void reset() {
        read_.store(0, std::memory_order_relaxed);
        write_.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<float> buf_;
    size_t cap_;
    std::atomic<size_t> read_{0};
    std::atomic<size_t> write_{0};
};

/**
 * SystemGUI - Generic GUI for any EmulatedSystem
 *
 * This class provides a GUI that works with any system implementing
 * the EmulatedSystem interface. It handles display scaling, keyboard input,
 * and basic menus. Systems can extend menus via their callback methods.
 *
 * Threading model:
 *   - GUI thread   : SDL events, ImGui rendering, texture upload
 *   - Emu thread   : run_frame(), audio sample generation
 *   - SDL audio    : reads from lock-free AudioRingBuffer
 *
 * Synchronisation:
 *   emu_mutex_   — held by the emu thread during run_frame(); GUI thread
 *                  uses try_lock for debug/menu reads (skips if busy).
 *   fb_mutex_    — protects the framebuffer snapshot (very brief lock).
 *   input_mutex_ — protects the queued SDL input events.
 */
class SystemGUI : public GenericEmulatorGUI {
private:
    std::unique_ptr<EmulatedSystem> system_;
    uint32_t* framebuffer_;
    int fb_width_;
    int fb_height_;
    std::atomic<bool> emulation_running_;
    std::atomic<bool> emulation_paused_;
    float speed_multiplier_;

    // Double-buffered GL textures — upload to one while the GPU
    // may still be rendering the previous frame from the other.
    GLuint screen_textures_[2];
    int    texture_write_idx_;
    
    // Statistics (total_frames_ written by emu thread, read by GUI thread)
    std::atomic<uint64_t> total_frames_;
    uint32_t actual_fps_;
    uint32_t last_fps_time_;
    uint64_t last_fps_frame_count_;  // total_frames_ snapshot for FPS delta
    
    // Frame pacing lives on the emu thread (private to emu_thread_func)
    // These are no longer accessed from the GUI thread.
    uint64_t frame_pace_counter_;
    double frame_time_accumulator_;
    
    // System selection dialog
    SystemSelectionDialog system_selection_dialog_;
    
    // Last selected file path for file dialog
    std::string last_file_path_;
    
    // Pending drive insert — set when a 1541 drive requests a file dialog
    Drive1541Device* pending_drive_insert_ = nullptr;
    
    // Pending file to load after system selection (from command line)
    std::string pending_file_path_;

    // SDL audio output
    SDL_AudioDeviceID audio_device_;
    int audio_sample_rate_;           // Actual sample rate obtained from SDL

    // ========================================================================
    // Emulation threading
    // ========================================================================
    std::thread emu_thread_;
    std::atomic<bool> emu_thread_running_{false};

    /// Protects system_ during run_frame() and other mutating operations.
    std::mutex emu_mutex_;

    /// Framebuffer snapshot (written by emu thread, read by GUI for texture
    /// upload).  Protected by fb_mutex_.
    uint32_t* fb_snapshot_;
    std::mutex fb_mutex_;
    std::atomic<bool> fb_new_frame_{false};

    /// Input event queue (pushed by GUI thread, consumed by emu thread).
    std::mutex input_mutex_;
    std::vector<SDL_Event> input_queue_;

    /// Lock-free audio ring buffer (emu thread produces, SDL callback consumes).
    std::unique_ptr<AudioRingBuffer> audio_ring_;

    /// Temporary buffer used by the emu thread to call get_audio_samples().
    std::vector<float> emu_audio_tmp_;
    
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
    void start_emulation();
    void pause_emulation();
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
    void update_fps();
    void reset_frame_pacing();
    void allocate_framebuffer();
    void free_framebuffer();
    void teardown_current_system();

    // Emulation thread
    void emu_thread_func();
    void start_emu_thread();
    void stop_emu_thread();

    // Audio helpers
    void open_audio_device();
    void close_audio_device();
    static void sdl_audio_callback(void* userdata, uint8_t* stream, int len);
};

#endif // SYSTEM_GUI_H