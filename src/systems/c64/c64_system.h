#pragma once

#include "../commodore/commodore_system.h"
#include "../../core/connector.h"
#include "../../core/device_registry.h"
#include "../../core/formats/format_handler.h"
#include "../../core/formats/sid_format.h"
#include "c64.h"
#include "c64_config.h"
#include "c64_kernal_patches.h"
#include <string>
#include <vector>

class LightpenDevice;

/**
 * C64System — Commodore 64 system emulation.
 *
 * CONSOLIDATION STATUS (see docs/C64_CONSOLIDATION_PLAN.md):
 * ==========================================================
 * Phases 1a-1c, 2, 3, 4c, 5 are complete:
 *   - c64_t embedded directly (no heap allocation)
 *   - tick/reset/init/shutdown/framebuffer absorbed as methods
 *   - Bus back-pointer typed to C64SystemData*, container_of removed
 *   - gui_state_t eliminated; chip debug uses generic ChipInfo
 *   - Legacy chip registry (system_8bit_t) eliminated from wrapper path
 *   - Class renamed C64SystemWrapper → C64System
 *   - SimpleSystemGUI → SystemGUI
 *
 * Remaining:
 *   - Phase 4a: Absorb c64_pla_maps_generate, c64_memory_init, c64_cpu_banking_callback
 *   - Phase 4b: Merge c64_config_t into SystemConfiguration
 *   - Phase 6: Remove dead legacy GUI code
 *
 * A few C functions in c64.cpp are still called (PLA generation, memory init,
 * CPU banking callback, screenshot). These are shared with the test framework
 * and will be absorbed or redirected in Phase 4a.
 */
class C64System : public CommodoreSystem {
public:
    C64System();
    ~C64System() override;
    
    // EmulatedSystem interface - system-specific overrides
    const SystemDescriptor& get_descriptor() const override;
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    void tick() override;
    void run_frame() override;
    bool load_file(const char* filepath) override;
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) override;
    void handle_controller_event(int controller, int button, bool pressed) override;
    
    // GUI forwarding methods - delegate to old C64 GUI code (imgui_interface.cpp)
    void render_system_menu_items() override;      // Forwards to gui_render_c64_system_menu_items()
    void render_debug_windows(void* gui_state) override;  // Forwards to chip debug system

    // Chip enumeration and debug windows (generic interface)
    std::vector<ChipInfo> get_chip_info() const override;
    void render_chip_debug_window(int chip_index, bool* show) override;
    void render_chip_settings_window(int chip_index, bool* show) override;
    
    // Configuration interface
    bool apply_configuration() override;
    void render_configuration_ui() override;

    // Auto-detect region (PAL/NTSC) from file contents
    SystemConfiguration detect_optimal_configuration(
        const char* filepath, const uint8_t* data, size_t size) override;

    // Audio output — drains SID ring buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

    // Update SID sample rate to match actual audio device rate
    void set_audio_sample_rate(int sample_rate_hz) override;
    
    // Note: The following methods are now provided by CommodoreSystem base class:
    // - set_configuration() - stores config_
    // - get_target_fps() - reads from region_options
    // - set_speed_multiplier() - stores speed_multiplier_
    // - handle_text_input() - delegates to keyboard_mapper_
    // - release_all_keys() - delegates to keyboard_mapper_
    //
    // Note: The following methods are provided by EmulatedSystem base class:
    // - get_configuration() - returns config_
    // - get_hardware_traits() - returns hardware_traits_
    // - get_current_timing() - returns hardware_traits_.timing
    // - get_display_traits() - returns hardware_traits_.display
    // - get_audio_traits() - returns hardware_traits_.audio
    // - get_total_cycles() - returns total_cycles_
    // - get_speed_multiplier() - returns speed_multiplier_
    
    // Get the underlying C64 system (for compatibility with existing GUI code)
    c64_t* get_c64_system() { return c64_; }
    
    // --- Connector Port Access -----------------------------------------
    //
    // Connector ports, owned devices, attach/detach, and the generic
    // peripheral connector UI are all provided by the EmulatedSystem base
    // class.  The C64 only defines its port layout constants and the
    // system-specific setup_connector_ports() initializer below.
    //
    
private:
    c64_t c64_data_{};          // Embedded C64 system struct (no heap allocation)
    c64_t* c64_ = nullptr;     // Points to &c64_data_ when initialized, nullptr otherwise
    c64_config_t c64_config_;  // Renamed to avoid conflict with base class config_
    vicii_standard_t created_vicii_standard_ = VIC_PAL;  // Actual VIC-II standard at creation time
    sid_revision_t pending_sid_revision_ = SID_REVISION_6581_R4AR;  // Applied after SID creation

    // =========================================================================
    // DEFERRED LOADING
    // =========================================================================
    // File loading is deferred until KERNAL/BASIC boot completes. This avoids
    // the problem where BASIC's cold-start NEW routine zeros $0801/$0802,
    // corrupting program data loaded before boot. The wrapper stores the
    // parsed format result and applies it only after BASIC reaches its READY
    // state (warm-start vector set, keyboard buffer empty).
    //
    // MEDIA ATTACHMENT: For D64 and TAP files, the media is inserted into the
    // appropriate storage device (1541 drive or datasette). D64 files also
    // extract the first PRG for fast direct-load (hybrid approach: disk is
    // available for directory listing AND the first program auto-runs). TAP
    // files are loaded into the datasette; full tape loading requires KERNAL
    // cassette I/O integration (CASS_READ signal wiring to CPU I/O port).
    //
    // FUTURE OPTIMIZATION: Some files (e.g. raw ML at $C000, or programs
    // that never touch KERNAL/BASIC-initialized memory) could be loaded
    // earlier — even before BASIC or KERNAL init completes. This would
    // reduce the perceived startup latency. Combined with techniques like
    // patching out KERNAL's memory test/clear loops, this could allow
    // near-instant startup for many programs. Not yet implemented; the
    // current approach prioritizes correctness over speed.
    // =========================================================================

    /** How the deferred load should be applied after BASIC READY. */
    enum class LoadMode {
        DIRECT,         ///< Standard: write program data to RAM, inject RUN
        DISK_FAST,      ///< D64: disk inserted in 1541 + fast PRG extraction to RAM
        TAPE_INSERTED   ///< TAP: tape loaded in datasette, inject LOAD + press play
    };

    struct PendingLoad {
        format_load_result_t result;  // Parsed file data (owns heap allocations)
        std::string filepath;         // Original filepath for SYS-from-filename
        bool active = false;          // Whether a deferred load is pending
        LoadMode mode = LoadMode::DIRECT;  // How to apply the load
    };
    PendingLoad pending_load_;
    bool boot_completed_ = false;  // Set after first deferred load; skips VARTAB check

    /** Check if BASIC has reached its READY state (safe to inject program). */
    bool is_basic_ready() const;

    /** Apply the pending load result to RAM and inject auto-run. */
    void apply_pending_load();

    /** Single system tick — ticks all chips in correct phase order. */
    void system_tick();

    /**
     * Ensure the C64 is configured compatibly for a SID file's requirements.
     *
     * Checks the SID header's video standard (PAL/NTSC) and SID model
     * (6581/8580) against the current system configuration.  If the region
     * doesn't match, the C64 is destroyed and recreated with the correct
     * VIC-II standard.  If only the SID revision differs, it is updated
     * in place.  In all cases a reset is performed and the RAMTAS memory
     * test is patched out for fast boot.
     */
    void ensure_compatible_for_sid(const sid_header_t* sid);

    // =========================================================================
    // SID PLAYER STATE
    // =========================================================================
    // When a SID file is loaded, we keep a copy of its header and payload
    // so the user can switch subtunes interactively (digits 0-9 for direct
    // selection, left/right cursor keys for prev/next with wrapping).
    // =========================================================================
    bool sid_player_active_ = false;            ///< True while a SID file is playing
    sid_header_t active_sid_header_{};           ///< Copy of the loaded SID header
    std::vector<uint8_t> active_sid_data_;       ///< Copy of original payload bytes
    uint16_t active_subtune_ = 0;               ///< Current 0-based subtune index

    /** Handle SID player keyboard shortcuts (subtune selection).
     *  Returns true if the key was consumed (should not be forwarded to C64). */
    bool handle_sid_player_key(SDL_Keycode key);

    // Note: hardware_traits_, config_ (SystemConfiguration), speed_multiplier_,
    // total_cycles_ are now stored in EmulatedSystem base class
    
    // =========================================================================
    // CONNECTOR PORTS — C64-SPECIFIC LAYOUT
    // =========================================================================
public:
    // Indices into connector_ports_ for quick access (public for CIA1 callbacks)
    static constexpr int PORT_CONTROL1   = 0;
    static constexpr int PORT_CONTROL2   = 1;
    static constexpr int PORT_IEC_SERIAL = 2;
    static constexpr int PORT_CASSETTE   = 3;
    static constexpr int PORT_USER       = 4;
    static constexpr int PORT_EXPANSION  = 5;

    /// Cached lightpen pointer (used by LP pin callback for zero-overhead access).
    LightpenDevice* get_cached_lightpen() const { return cached_lightpen_; }

    /// Update cached lightpen pointer when devices change on Control Port 1.
    void on_port_device_changed(int port_index) override;

private:
    /// Create and wire up all C64 connector ports (CIA1 joystick callbacks etc).
    void setup_connector_ports();

    /// Pass the current display rect to any lightpen on Control Port 1 (once per frame).
    void update_lightpen_display_rect();

    /// Cached pointer to lightpen device on Control Port 1 (nullptr if none).
    /// Updated by on_port_device_changed() to avoid per-cycle lookups.
    LightpenDevice* cached_lightpen_ = nullptr;
};