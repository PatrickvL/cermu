#pragma once
// warp_controller.hpp — Automatic warp mode for IEC disk/tape I/O
//
// Monitors drive activity (motor spin, IEC bus traffic) and signals the
// host system to enter/exit warp mode.  In warp mode the host system:
//   - Skips video rendering (or freezes on the last complete frame)
//   - Skips frame-sync sleep (runs as fast as the host CPU allows)
//   - Continues ticking both CPUs at the correct ratio
//
// This gives full fastloader compatibility at 10-50× real-time speed.
//
// Usage:
//   WarpController warp;
//   warp.set_auto_warp(true);
//
//   // In the host system's tick loop, after advancing the drive:
//   warp.update(drive.is_active(), total_cycles);
//
//   // In the host system's run_frame(), gate the sleep:
//   if (!warp.is_warping()) frame_sync_sleep();
//
//   // Gate video rendering:
//   if (!warp.is_warping()) emit_video();

#include <cstdint>

class WarpController {
public:
    // ── Configuration ───────────────────────────────────────────────────

    /// Enable/disable automatic warp (triggered by drive motor).
    void set_auto_warp(bool enabled)  { auto_warp_enabled_ = enabled; }
    bool is_auto_warp_enabled() const { return auto_warp_enabled_; }

    /// Enable/disable manual (user-toggled) warp.
    void set_manual_warp(bool enabled) { manual_warp_ = enabled; }
    bool is_manual_warp() const        { return manual_warp_; }

    /// How many cycles after drive activity stops before exiting warp.
    /// Default: 200,000 cycles (~200 ms at 1 MHz).  Prevents warp
    /// flicker during multi-file loads with brief motor gaps.
    void set_cooldown_cycles(uint64_t cycles) { cooldown_cycles_ = cycles; }

    // ── State ───────────────────────────────────────────────────────────

    /// True when the system should be warping (skipping video + sleep).
    bool is_warping() const { return manual_warp_ || auto_warp_active_; }

    // ── Update (call every host cycle or every N cycles) ────────────────

    /// Update warp state based on drive activity.
    /// @param drive_active  True if any attached drive has its motor spinning.
    /// @param host_cycle    Current host system cycle count (for cooldown timing).
    void update(bool drive_active, uint64_t host_cycle) {
        if (!auto_warp_enabled_) {
            auto_warp_active_ = false;
            return;
        }

        if (drive_active) {
            auto_warp_active_ = true;
            last_active_cycle_ = host_cycle;
        } else if (auto_warp_active_) {
            // Cooldown: stay in warp until enough idle cycles pass
            if (host_cycle - last_active_cycle_ >= cooldown_cycles_)
                auto_warp_active_ = false;
        }
    }

    /// Reset warp state (call on system reset).
    void reset() {
        auto_warp_active_ = false;
        last_active_cycle_ = 0;
    }

private:
    bool     auto_warp_enabled_ = true;   // Auto-warp on drive activity
    bool     manual_warp_       = false;   // User-toggled warp
    bool     auto_warp_active_  = false;   // Currently auto-warping
    uint64_t last_active_cycle_ = 0;       // Last cycle with drive activity
    uint64_t cooldown_cycles_   = 200000;  // ~200 ms at 1 MHz
};
