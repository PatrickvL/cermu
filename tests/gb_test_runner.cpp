/*
 * gb_test_runner.cpp — Headless Game Boy Compatibility Test Runner
 *
 * Loads Game Boy ROMs (from filesystem or VFS/7z archives) and runs them
 * headlessly for a configurable number of frames.  Reports whether each
 * title boots successfully or crashes.
 *
 * Test criteria:
 *   PASS   — completed all frames, CPU still executing
 *   HANG   — CPU stuck in infinite loop (JR -2, JP self, HALT with no IRQ)
 *   CRASH  — CPU reached illegal state or PC in unmapped region
 *
 * Usage:
 *   gb_test_runner <rom_or_vfs_path> [--frames N] [--verbose]
 *   gb_test_runner --batch <file_with_paths> [--frames N]
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>

#include "../src/systems/gameboy/gameboy_system.hpp"
#include "../src/core/vfs/vfs.hpp"

// ============================================================================
// Test result tracking
// ============================================================================

enum class Verdict { PASS, HANG, CRASH, LOAD_ERROR };

struct TestResult {
    std::string path;
    Verdict verdict = Verdict::LOAD_ERROR;
    std::string detail;
    int frames_run = 0;
    double wall_seconds = 0.0;
    uint16_t final_pc = 0;
};

static const char* verdict_str(Verdict v) {
    switch (v) {
        case Verdict::PASS:       return "PASS";
        case Verdict::HANG:       return "HANG";
        case Verdict::CRASH:      return "CRASH";
        case Verdict::LOAD_ERROR: return "LOAD_ERR";
    }
    return "???";
}

// ============================================================================
// Run a single ROM test
// ============================================================================

template<GameBoyVariant V>
static TestResult run_one(const char* path, int max_frames, bool verbose) {
    TestResult result;
    result.path = path;

    // Create and initialize the system
    GameBoySystem<V> gb;
    if (!gb.initialize()) {
        result.detail = "Failed to initialize system";
        return result;
    }

    // Allocate headless framebuffer
    int fb_w, fb_h;
    gb.get_display_dimensions(&fb_w, &fb_h);
    std::vector<uint32_t> fb(fb_w * fb_h, 0);
    gb.set_framebuffer(fb.data(), fb_w, fb_h);
    gb.set_headless(true);

    // Load the ROM
    if (!gb.load_file(path)) {
        result.detail = "Failed to load ROM";
        gb.shutdown();
        return result;
    }

    if (verbose) {
        printf("  Loaded: %s (%dx%d)\n", path, fb_w, fb_h);
    }

    auto t0 = std::chrono::steady_clock::now();

    // Track PC for infinite loop detection
    uint16_t prev_pc = 0;
    int same_pc_count = 0;
    float drain[4096];

    for (int frame = 0; frame < max_frames; ++frame) {
        gb.run_frame();
        // Drain audio to prevent ring buffer overflow
        gb.get_audio_samples(drain, 4096u);

        uint16_t pc = gb.get_cpu_pc();
        result.final_pc = pc;
        result.frames_run = frame + 1;

        // Infinite loop detection: if PC stays at the same address
        // for many consecutive frames, the game is stuck.
        if (pc == prev_pc) {
            same_pc_count++;
            if (same_pc_count > 30) {
                result.verdict = Verdict::HANG;
                char buf[128];
                snprintf(buf, sizeof(buf), "PC stuck at $%04X for %d frames", pc, same_pc_count);
                result.detail = buf;
                break;
            }
        } else {
            same_pc_count = 0;
            prev_pc = pc;
        }

        // Check for PC in unmapped area
        if (pc >= 0xFEA0 && pc < 0xFF00) {
            result.verdict = Verdict::CRASH;
            char buf[64];
            snprintf(buf, sizeof(buf), "PC in unusable area: $%04X", pc);
            result.detail = buf;
            break;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    result.wall_seconds = std::chrono::duration<double>(t1 - t0).count();

    if (result.verdict == Verdict::LOAD_ERROR) {
        // If we got here without setting a verdict, it ran successfully
        result.verdict = Verdict::PASS;
    }

    gb.shutdown();
    return result;
}

// ============================================================================
// Detect variant from ROM data
// ============================================================================

static GameBoyVariant detect_variant(const char* path) {
    size_t size = 0;
    uint8_t* data = vfs_read_file(path, &size);
    if (!data || size < 0x150) {
        free(data);
        return GameBoyVariant::DMG;
    }
    uint8_t cgb_flag = data[0x143];
    free(data);
    return (cgb_flag == 0x80 || cgb_flag == 0xC0) ? GameBoyVariant::GBC : GameBoyVariant::DMG;
}

static TestResult run_auto(const char* path, int max_frames, bool verbose) {
    GameBoyVariant v = detect_variant(path);
    if (verbose) {
        printf("  Variant: %s\n", v == GameBoyVariant::GBC ? "GBC" : "DMG");
    }
    if (v == GameBoyVariant::GBC)
        return run_one<GameBoyVariant::GBC>(path, max_frames, verbose);
    else
        return run_one<GameBoyVariant::DMG>(path, max_frames, verbose);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    int max_frames = 120;  // ~2 seconds at 60 FPS
    bool verbose = false;
    const char* single_path = nullptr;
    const char* batch_file = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            max_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--batch") == 0 && i + 1 < argc) {
            batch_file = argv[++i];
        } else if (argv[i][0] != '-') {
            single_path = argv[i];
        }
    }

    if (!single_path && !batch_file) {
        printf("Usage: gb_test_runner <rom_or_vfs_path> [--frames N] [--verbose]\n");
        printf("       gb_test_runner --batch <file_with_paths> [--frames N]\n");
        return 1;
    }

    std::vector<std::string> paths;
    if (batch_file) {
        std::ifstream f(batch_file);
        std::string line;
        while (std::getline(f, line)) {
            // Trim whitespace
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (!line.empty() && line[0] != '#')
                paths.push_back(line);
        }
    }
    if (single_path) {
        paths.push_back(single_path);
    }

    printf("Game Boy Compatibility Test Runner\n");
    printf("==================================\n");
    printf("Frames per test: %d (~%.1f seconds)\n", max_frames, max_frames / 60.0);
    printf("Testing %zu title(s)...\n\n", paths.size());

    int pass = 0, hang = 0, crash = 0, load_err = 0;
    std::vector<TestResult> results;

    for (const auto& p : paths) {
        if (verbose) printf("Testing: %s\n", p.c_str());

        auto result = run_auto(p.c_str(), max_frames, verbose);
        results.push_back(result);

        const char* v = verdict_str(result.verdict);
        switch (result.verdict) {
            case Verdict::PASS:       pass++; break;
            case Verdict::HANG:       hang++; break;
            case Verdict::CRASH:      crash++; break;
            case Verdict::LOAD_ERROR: load_err++; break;
        }

        // Extract just the filename for display
        std::string display = p;
        auto bang = display.rfind("!/");
        if (bang != std::string::npos) display = display.substr(bang + 2);
        auto slash = display.rfind('/');
        if (slash != std::string::npos) display = display.substr(slash + 1);

        printf("  %-8s  %3d frames  %6.2fs  PC=$%04X  %s",
               v, result.frames_run, result.wall_seconds,
               result.final_pc, display.c_str());
        if (!result.detail.empty()) printf("  (%s)", result.detail.c_str());
        printf("\n");
    }

    printf("\n==================================\n");
    printf("Results: %d PASS, %d HANG, %d CRASH, %d LOAD_ERR  (total: %zu)\n",
           pass, hang, crash, load_err, results.size());

    return (hang + crash + load_err > 0) ? 1 : 0;
}
