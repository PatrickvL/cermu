/*
 * nes_test_runner.cpp — Headless NES Test ROM Validation Runner
 *
 * Runs open-source NES test ROMs (nestest, Blargg's suites) and validates
 * results automatically.  Uses the full NintendoSystem infrastructure
 * (CPU, PPU, Bus, Cartridge, Mappers) in headless mode.
 *
 * Test protocol detection:
 *   1. nestest.nes — CPU instruction tests; reports results in RAM $02/$03
 *   2. Blargg $6000 protocol — writes status to $6000, text to $6004+
 *   3. Generic — runs N frames, checks for infinite loop (JMP self)
 *
 * Usage:
 *   nes_test_runner <test_rom.nes> [--max-frames N] [--verbose]
 *   nes_test_runner --all <directory>
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

#include "../src/systems/nes/nes_system.h"

using NES = nes_system::NESSystem;

// ============================================================================
// Test result tracking
// ============================================================================

enum class TestVerdict { PASS, FAIL, TIMEOUT, ERROR };

struct TestResult {
    std::string rom_path;
    TestVerdict verdict = TestVerdict::ERROR;
    std::string detail;
    int frames_run = 0;
    double wall_seconds = 0.0;
};

// ============================================================================
// Nametable text reader — reads ASCII-mapped tiles from PPU nametable
// ============================================================================
// Blargg's test ROMs use a font where tile indices match ASCII codes.
// The nametable at PPU $2000 is 32 columns × 30 rows.
// We scan the entire nametable and extract printable text lines.
// ============================================================================

static std::string read_nametable_text(NES& nes) {
    std::string result;
    constexpr uint16_t NT_BASE = 0x2000;
    constexpr int COLS = 32;
    constexpr int ROWS = 30;

    for (int row = 0; row < ROWS; ++row) {
        std::string line;
        for (int col = 0; col < COLS; ++col) {
            uint8_t tile = nes.peek_ppu_memory(NT_BASE + row * COLS + col);
            if (tile >= 0x20 && tile < 0x7F) {
                line += static_cast<char>(tile);
            } else {
                line += ' ';
            }
        }
        // Trim trailing spaces
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (!line.empty()) {
            if (!result.empty()) result += '\n';
            result += line;
        }
    }
    return result;
}

// ============================================================================
// Test protocol: nestest.nes
// ============================================================================
// nestest enters automation mode at $C000 (JMP $C5F5).
// After ~26k instructions it reaches $C66E (RTS into undefined).
// Result: RAM[$02] = unofficial error, RAM[$03] = official error.
// Both zero = PASS.
// ============================================================================

static TestResult run_nestest(NES& nes, int max_frames) {
    TestResult result;
    result.rom_path = "nestest.nes";

    // Override PC to $C000 for automation mode (reset vector $C004 is interactive)
    nes.set_cpu_pc(0xC000);

    auto t0 = std::chrono::steady_clock::now();

    // Run frames, watching for completion
    uint16_t prev_pc = 0;
    int stuck_count = 0;

    for (int frame = 0; frame < max_frames; ++frame) {
        nes.run_frame();
        result.frames_run = frame + 1;

        uint16_t pc = nes.get_cpu_pc();

        // Detect stuck (JMP self or RTS to garbage)
        if (pc == prev_pc) {
            stuck_count++;
            if (stuck_count >= 3) {
                break;  // CPU is stuck — test complete
            }
        } else {
            stuck_count = 0;
        }

        // nestest completes very fast (< 1 frame), but the RTS
        // sends PC to garbage where it may wander before getting stuck.
        // Also check if we left valid code range.
        if (pc < 0x8000 && frame > 0) {
            break;  // PC fell out of PRG ROM — test done
        }

        prev_pc = pc;
    }

    auto t1 = std::chrono::steady_clock::now();
    result.wall_seconds = std::chrono::duration<double>(t1 - t0).count();

    uint8_t err_official   = nes.peek_memory(0x02);
    uint8_t err_unofficial = nes.peek_memory(0x03);

    if (err_official == 0 && err_unofficial == 0) {
        result.verdict = TestVerdict::PASS;
        result.detail = "All CPU instruction tests passed";
    } else {
        result.verdict = TestVerdict::FAIL;
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Official error=$%02X, Unofficial error=$%02X",
                 err_official, err_unofficial);
        result.detail = buf;
    }

    return result;
}

// ============================================================================
// Test protocol: Blargg $6000
// ============================================================================
// Status at $6000: $80=running, $81=needs reset, $00=passed, $01-$7F=fail
// Magic at $6001-$6003: $DE $B0 $61
// Result text at $6004+ (null-terminated ASCII)
// ============================================================================

static TestResult run_blargg_6000(NES& nes, const std::string& name,
                                   int max_frames, bool verbose = false) {
    TestResult result;
    result.rom_path = name;

    auto t0 = std::chrono::steady_clock::now();

    uint16_t prev_pc = 0;
    int stuck_count = 0;

    // Run frames until $6000 protocol resolves or infinite loop detected
    for (int frame = 0; frame < max_frames; ++frame) {
        nes.run_frame();
        result.frames_run = frame + 1;

        // Check magic signature first (PRG RAM must be present)
        uint8_t m1 = nes.peek_memory(0x6001);
        uint8_t m2 = nes.peek_memory(0x6002);
        uint8_t m3 = nes.peek_memory(0x6003);

        if (verbose && (frame < 10 || (frame % 500 == 0))) {
            uint8_t s = nes.peek_memory(0x6000);
            uint16_t pc = nes.get_cpu_pc();
            uint8_t b0 = nes.peek_memory(pc);
            uint8_t b1 = nes.peek_memory(pc + 1);
            uint8_t b2 = nes.peek_memory(pc + 2);
            uint8_t sp = nes.peek_memory(0x01FD);
            uint8_t ram0 = nes.peek_memory(0x0000);
            uint8_t ram1 = nes.peek_memory(0x0001);
            printf("  frame %5d: PC=$%04X [%02X %02X %02X]  $6000=$%02X  "
                   "$6001-3=$%02X %02X %02X  stk=$%02X r0/1=%02X/%02X\n",
                   frame, pc, b0, b1, b2, s, m1, m2, m3, sp, ram0, ram1);
        }

        // --- Check for infinite loop (JMP self) ---
        uint16_t pc = nes.get_cpu_pc();
        // Detect JMP-self: check PC and nearby bytes for $4C xx yy where xxyy == addr
        // Due to mid-instruction sampling, PC might be at the JMP or 1-2 bytes ahead
        bool is_jmp_self = false;
        for (int off = 0; off <= 2; ++off) {
            uint16_t check = pc - off;
            if (nes.peek_memory(check) == 0x4C) {  // JMP absolute
                uint16_t target = nes.peek_memory(check + 1) |
                                  (nes.peek_memory(check + 2) << 8);
                if (target == check) {
                    is_jmp_self = true;
                    break;
                }
            }
        }
        if (is_jmp_self) {
            stuck_count++;
            if (stuck_count >= 3) {
                // ROM completed — check $6000 first, then nametable
                if (m1 == 0xDE && m2 == 0xB0 && m3 == 0x61) {
                    uint8_t status = nes.peek_memory(0x6000);
                    std::string text;
                    for (uint16_t addr = 0x6004; addr < 0x7000; ++addr) {
                        uint8_t ch = nes.peek_memory(addr);
                        if (ch == 0) break;
                        if (ch >= 0x20 && ch < 0x7F) text += static_cast<char>(ch);
                        else text += '.';
                    }
                    if (status == 0x00) {
                        result.verdict = TestVerdict::PASS;
                        result.detail = text.empty() ? "Passed" : text;
                    } else {
                        result.verdict = TestVerdict::FAIL;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "Error $%02X: ", status);
                        result.detail = std::string(buf) + text;
                    }
                } else {
                    // No $6000 protocol — read nametable text
                    std::string nt_text = read_nametable_text(nes);
                    if (verbose) {
                        printf("  Nametable text:\n%s\n", nt_text.c_str());
                    }
                    // Detect pass/fail from nametable content
                    if (nt_text.find("Passed") != std::string::npos ||
                        nt_text.find("PASSED") != std::string::npos ||
                        nt_text.find("passed") != std::string::npos) {
                        result.verdict = TestVerdict::PASS;
                        result.detail = nt_text;
                    } else if (nt_text.find("Failed") != std::string::npos ||
                               nt_text.find("FAILED") != std::string::npos ||
                               nt_text.find("failed") != std::string::npos) {
                        result.verdict = TestVerdict::FAIL;
                        result.detail = nt_text;
                    } else {
                        // Can't determine — report nametable content
                        result.verdict = TestVerdict::FAIL;
                        result.detail = "Unknown result (nametable): " + nt_text;
                    }
                }
                break;
            }
        } else {
            stuck_count = 0;
        }
        prev_pc = pc;

        // --- $6000 protocol check ---
        if (m1 != 0xDE || m2 != 0xB0 || m3 != 0x61) {
            continue;
        }

        uint8_t status = nes.peek_memory(0x6000);

        if (status == 0x80) {
            continue;
        }

        if (status == 0x81) {
            result.verdict = TestVerdict::ERROR;
            result.detail = "Test requested reset ($81) — not supported";
            break;
        }

        // Read result text from $6004+
        std::string text;
        for (uint16_t addr = 0x6004; addr < 0x7000; ++addr) {
            uint8_t ch = nes.peek_memory(addr);
            if (ch == 0) break;
            if (ch >= 0x20 && ch < 0x7F)
                text += static_cast<char>(ch);
            else
                text += '.';
        }

        if (status == 0x00) {
            result.verdict = TestVerdict::PASS;
            result.detail = text.empty() ? "Passed" : text;
        } else {
            result.verdict = TestVerdict::FAIL;
            char buf[16];
            snprintf(buf, sizeof(buf), "Error $%02X: ", status);
            result.detail = std::string(buf) + text;
        }
        break;
    }

    auto t1 = std::chrono::steady_clock::now();
    result.wall_seconds = std::chrono::duration<double>(t1 - t0).count();

    if (result.verdict == TestVerdict::ERROR && result.detail.empty()) {
        result.verdict = TestVerdict::TIMEOUT;
        result.detail = "Timed out — $6000 protocol did not resolve";
    }

    return result;
}

// ============================================================================
// Test protocol: Generic (screen-output ROMs)
// ============================================================================
// Runs until JMP-self detected or max frames reached.
// Reports whether the ROM completed (reached infinite loop).
// ============================================================================

static TestResult run_generic(NES& nes, const std::string& name,
                               int max_frames) {
    TestResult result;
    result.rom_path = name;

    auto t0 = std::chrono::steady_clock::now();

    uint16_t prev_pc = 0;
    int stuck_count = 0;
    bool completed = false;

    for (int frame = 0; frame < max_frames; ++frame) {
        nes.run_frame();
        result.frames_run = frame + 1;

        uint16_t pc = nes.get_cpu_pc();
        if (pc == prev_pc) {
            stuck_count++;
            if (stuck_count >= 5) {
                completed = true;
                break;
            }
        } else {
            stuck_count = 0;
        }
        prev_pc = pc;
    }

    auto t1 = std::chrono::steady_clock::now();
    result.wall_seconds = std::chrono::duration<double>(t1 - t0).count();

    if (completed) {
        result.verdict = TestVerdict::PASS;
        result.detail = "ROM completed (reached infinite loop)";
    } else {
        result.verdict = TestVerdict::TIMEOUT;
        result.detail = "Max frames reached without completion";
    }

    return result;
}

// ============================================================================
// Test protocol detection
// ============================================================================

enum class TestProtocol { NESTEST, BLARGG_6000, GENERIC };

static TestProtocol detect_protocol(const std::string& filename) {
    // Check by filename patterns
    if (filename.find("nestest") != std::string::npos) {
        return TestProtocol::NESTEST;
    }

    // All Blargg test ROMs use the $6000 PRG-RAM result protocol
    if (filename.find("blargg") != std::string::npos) {
        return TestProtocol::BLARGG_6000;
    }

    return TestProtocol::GENERIC;
}

// ============================================================================
// Print helpers
// ============================================================================

static const char* verdict_str(TestVerdict v) {
    switch (v) {
        case TestVerdict::PASS:    return "\033[32mPASS\033[0m";
        case TestVerdict::FAIL:    return "\033[31mFAIL\033[0m";
        case TestVerdict::TIMEOUT: return "\033[33mTIMEOUT\033[0m";
        case TestVerdict::ERROR:   return "\033[31mERROR\033[0m";
    }
    return "???";
}

static void print_result(const TestResult& r) {
    printf("  [%s] %s\n", verdict_str(r.verdict), r.rom_path.c_str());
    printf("         %s\n", r.detail.c_str());
    printf("         %d frames, %.2f seconds\n", r.frames_run, r.wall_seconds);
}

// ============================================================================
// Run a single test ROM
// ============================================================================

static TestResult run_test_rom(const std::string& filepath, int max_frames,
                                bool verbose) {
    // Extract filename for display
    std::string filename = filepath;
    auto sep = filepath.rfind('/');
    if (sep != std::string::npos) filename = filepath.substr(sep + 1);

    if (verbose) {
        printf("\n--- Running: %s ---\n", filename.c_str());
    }

    // Create NES system
    NES nes;
    if (!nes.initialize()) {
        TestResult r;
        r.rom_path = filename;
        r.verdict = TestVerdict::ERROR;
        r.detail = "Failed to initialize NES system";
        return r;
    }

    // Allocate a dummy framebuffer using system's visible dimensions
    int fb_w = 0, fb_h = 0;
    nes.get_display_dimensions(&fb_w, &fb_h);
    std::vector<uint32_t> framebuffer(fb_w * fb_h, 0);
    nes.set_framebuffer(framebuffer.data(), fb_w, fb_h);

    // Load ROM
    if (!nes.load_file(filepath.c_str())) {
        TestResult r;
        r.rom_path = filename;
        r.verdict = TestVerdict::ERROR;
        r.detail = "Failed to load ROM file";
        return r;
    }

    // Detect protocol
    TestProtocol protocol = detect_protocol(filepath);

    if (verbose) {
        const char* proto_names[] = { "nestest", "blargg_$6000", "generic" };
        printf("  Protocol: %s\n", proto_names[static_cast<int>(protocol)]);
        printf("  Max frames: %d\n", max_frames);
    }

    // Run appropriate test
    TestResult result;
    switch (protocol) {
        case TestProtocol::NESTEST:
            result = run_nestest(nes, max_frames);
            break;
        case TestProtocol::BLARGG_6000:
            result = run_blargg_6000(nes, filename, max_frames, verbose);
            break;
        case TestProtocol::GENERIC:
            result = run_generic(nes, filename, max_frames);
            break;
    }

    // Pixel diagnostics: dump leftmost pixel data from the framebuffer
    if (verbose) {
        // Show first 16 pixels for a few content-bearing scanlines
        printf("  First 16px of select scanlines:\n");
        int shown = 0;
        for (int y = 0; y < fb_h && shown < 15; y++) {
            // Check if this scanline has content in first 16 pixels
            bool has = false;
            for (int x = 0; x < 16 && x < fb_w; x++) {
                if ((framebuffer[y * fb_w + x] & 0x00FFFFFF) != 0) { has = true; break; }
            }
            if (!has) continue;
            printf("    SL %3d: ", y);
            for (int x = 0; x < 16 && x < fb_w; x++) {
                uint32_t px = framebuffer[y * fb_w + x] & 0x00FFFFFF;
                printf("%06X ", px);
            }
            printf("\n");
            shown++;
        }
    }

    // Clean shutdown
    nes.shutdown();

    return result;
}

// ============================================================================
// Main
// ============================================================================

static void print_usage(const char* argv0) {
    printf("Usage: %s [options] <rom_file.nes> [rom_file2.nes ...]\n", argv0);
    printf("       %s --all <directory>\n", argv0);
    printf("\nOptions:\n");
    printf("  --max-frames N   Maximum frames to run (default: 7200)\n");
    printf("  --verbose        Print detailed progress\n");
    printf("  --all <dir>      Run all .nes files in directory\n");
    printf("  --help           Show this help\n");
}

int main(int argc, char* argv[]) {
    int max_frames = 7200;  // ~2 minutes at 60fps
    bool verbose = false;
    bool run_all = false;
    std::string all_dir;
    std::vector<std::string> rom_files;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--max-frames") == 0 && i + 1 < argc) {
            max_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--all") == 0 && i + 1 < argc) {
            run_all = true;
            all_dir = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            rom_files.push_back(argv[i]);
        }
    }

    // Collect ROM files
    if (run_all) {
        namespace fs = std::filesystem;
        if (!fs::is_directory(all_dir)) {
            fprintf(stderr, "Error: %s is not a directory\n", all_dir.c_str());
            return 1;
        }
        for (const auto& entry : fs::directory_iterator(all_dir)) {
            if (entry.path().extension() == ".nes") {
                rom_files.push_back(entry.path().string());
            }
        }
        // Sort for deterministic order
        std::sort(rom_files.begin(), rom_files.end());
    }

    if (rom_files.empty()) {
        fprintf(stderr, "No ROM files specified. Use --help for usage.\n");
        return 1;
    }

    // Run tests
    printf("============================================================\n");
    printf("  NES Test ROM Validation Runner\n");
    printf("  %zu test ROM(s), max %d frames each\n",
           rom_files.size(), max_frames);
    printf("============================================================\n");

    std::vector<TestResult> results;
    int pass = 0, fail = 0, timeout = 0, error = 0;

    for (const auto& rom : rom_files) {
        TestResult r = run_test_rom(rom, max_frames, verbose);
        print_result(r);

        switch (r.verdict) {
            case TestVerdict::PASS:    ++pass;    break;
            case TestVerdict::FAIL:    ++fail;    break;
            case TestVerdict::TIMEOUT: ++timeout; break;
            case TestVerdict::ERROR:   ++error;   break;
        }
        results.push_back(std::move(r));
    }

    // Summary
    printf("\n============================================================\n");
    printf("  Results: %d passed, %d failed, %d timeout, %d error\n",
           pass, fail, timeout, error);
    printf("============================================================\n");

    return (fail == 0 && error == 0) ? 0 : 1;
}
