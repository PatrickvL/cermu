#define SDL_MAIN_HANDLED
#include "../core/emulated_system.h"
#include "../gui/system_gui.h"
#include "../testing/vicii_test_harness.h"
#include "../testing/vicii_pixel_tests.h"
#include <cstdio>
#include <memory>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifndef ATTACH_TO_PARENT_PROCESS
#define ATTACH_TO_PARENT_PROCESS ((DWORD)-1)
#endif

// When built as a WIN32 (GUI) app, stdout/stderr are not connected to any
// console. If we were launched from a terminal, reattach to the parent
// console so printf / fprintf output appears there as expected.
static bool s_attached_parent_console = false;

// On exit, send a synthetic Enter keypress to the parent console so the
// shell re-displays its prompt (it won't wait for a GUI-subsystem process).
static void win32_detach_console() {
    if (!s_attached_parent_console) return;

    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        INPUT_RECORD ir = {};
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.wRepeatCount = 1;
        ir.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        ir.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(
            MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC));
        ir.Event.KeyEvent.uChar.AsciiChar = '\r';
        ir.Event.KeyEvent.dwControlKeyState = 0;
        DWORD written = 0;
        WriteConsoleInputA(hInput, &ir, 1, &written);
    }
    FreeConsole();
    s_attached_parent_console = false;
}

static void win32_attach_parent_console() {
    if (AttachConsole(ATTACH_TO_PARENT_PROCESS)) {
        s_attached_parent_console = true;
        atexit(win32_detach_console);

        // Redirect stdout
        FILE* fp = nullptr;
        if (_fileno(stdout) < 0 || _get_osfhandle(_fileno(stdout)) == -1) {
            freopen_s(&fp, "CONOUT$", "w", stdout);
            if (fp) setvbuf(fp, nullptr, _IONBF, 0);
        }
        // Redirect stderr
        if (_fileno(stderr) < 0 || _get_osfhandle(_fileno(stderr)) == -1) {
            freopen_s(&fp, "CONOUT$", "w", stderr);
            if (fp) setvbuf(fp, nullptr, _IONBF, 0);
        }
        // Redirect stdin
        if (_fileno(stdin) < 0 || _get_osfhandle(_fileno(stdin)) == -1) {
            freopen_s(&fp, "CONIN$", "r", stdin);
        }
    }
}
#endif

// Force linker to include system registrations
// Systems self-register during static initialization via REGISTER_SYSTEM macro
// We just need to ensure the system object files are linked
#include "../systems/chip8/chip8_system.h"
#include "../systems/commodore/c64/c64_system.h"

// ============================================================================
// MAIN FUNCTION - Multi-System Emulator with Automatic Detection
// ============================================================================
int main(int argc, char** argv) {
#ifdef _WIN32
    win32_attach_parent_console();
#endif
    const char* file_path = nullptr;
    const char* system_name = nullptr;
    bool vicii_test_mode = false;
    bool vicii_dump_mode = false;
    bool skip_memtest = false;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--system") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                system_name = argv[++i];
                printf("System specified: %s\n", system_name);
            } else {
                printf("ERROR: --system requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--vicii-test") == 0) {
            vicii_test_mode = true;
            system_name = "C64";  // Force C64 system
            printf("VIC-II test mode enabled\n");
        } else if (strcmp(argv[i], "--vicii-dump") == 0) {
            vicii_dump_mode = true;
            system_name = "C64";
            printf("VIC-II dump mode enabled\n");
        } else if (strcmp(argv[i], "--skip-memtest") == 0) {
            skip_memtest = true;
            printf("KERNAL memory test skip enabled\n");
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options] [file]\n", argv[0]);
            printf("\nOptions:\n");
            printf("  --system, -s <name>   Select system by short name (e.g., C64, CHIP8)\n");
            printf("  --vicii-test          Run VIC-II register test suite (headless)\n");
            printf("  --skip-memtest        Patch C64 KERNAL to skip RAMTAS memory test\n");
            printf("  --help, -h            Show this help message\n");
            printf("\nAvailable systems:\n");
            for (const auto& desc : SystemRegistry::instance().get_all_descriptors()) {
                printf("  %-10s  %s\n", desc.short_name, desc.name);
            }
            return 0;
        } else if (file_path == nullptr) {
            file_path = argv[i];
            printf("File specified: %s\n", file_path);
        }
    }
    
    std::unique_ptr<EmulatedSystem> system;
    
    // If system name specified, create it directly
    if (system_name != nullptr) {
        printf("Creating system: %s\n", system_name);
        system = SystemRegistry::instance().create_system(system_name);
        
        if (!system) {
            printf("ERROR: System not found: %s\n", system_name);
            printf("Available systems:\n");
            for (const auto& desc : SystemRegistry::instance().get_all_descriptors()) {
                printf("  %s (%s)\n", desc.short_name, desc.name);
            }
            return 1;
        }
        
        printf("Created system: %s (%s)\n", 
               system->get_descriptor().name,
               system->get_descriptor().short_name);
        
        // If a file was specified, auto-detect optimal configuration
        // (e.g. memory expansion) before initializing
        if (file_path != nullptr) {
            system->apply_file_configuration(file_path);
        }

        // Initialize the system
        if (!system->initialize()) {
            printf("ERROR: Failed to initialize %s system\n", 
                   system->get_descriptor().name);
            return 1;
        }
        
        // Load file if specified
        if (file_path != nullptr) {
            if (!system->load_file(file_path)) {
                printf("ERROR: Failed to load file: %s\n", file_path);
                system->shutdown();
                return 1;
            }
            printf("Successfully loaded file into %s\n", system->get_descriptor().name);
        }
    }
    // If file was specified (but no system), try to auto-detect system
    else if (file_path != nullptr) {
        printf("Detecting system for file: %s\n", file_path);
        system = SystemRegistry::instance().create_system_for_file(file_path);
        
        if (!system) {
            printf("WARNING: Could not detect system for file: %s\n", file_path);
            printf("No emulator supports this file format.\n");
            printf("System selection dialog will be shown...\n\n");
            // Keep file_path so the GUI can load it after user picks a system
        } else {
            printf("Detected system: %s (%s)\n", 
                   system->get_descriptor().name,
                   system->get_descriptor().short_name);
            printf("Description: %s\n", system->get_descriptor().description);
            
            // Initialize the system
            if (!system->initialize()) {
                printf("ERROR: Failed to initialize %s system\n", 
                       system->get_descriptor().name);
                return 1;
            }
            
            // Load the file
            if (!system->load_file(file_path)) {
                printf("ERROR: Failed to load file: %s\n", file_path);
                system->shutdown();
                return 1;
            }
            printf("Successfully loaded file into %s\n", system->get_descriptor().name);
        }
    }
    
    // =========================================================================
    // Apply --skip-memtest patch for C64 systems
    // TODO: This should eventually be part of a configurable patch registry
    // where users can enable/disable patches per system.  For now, only
    // applies when explicitly requested via CLI.  Some whitelisted software
    // (SID files) already applies this automatically via ensure_compatible_for_sid.
    // =========================================================================
    if (skip_memtest && system) {
        auto* c64_sys = dynamic_cast<C64System*>(system.get());
        if (c64_sys) {
            c64_sys->patch_skip_memtest();
        } else {
            printf("WARNING: --skip-memtest is only supported for C64 systems\n");
        }
    }

    // =========================================================================
    // VIC-II DUMP MODE — normal boot + framebuffer pixel dump
    // =========================================================================
    if (vicii_dump_mode && system) {
        C64System* c64 = dynamic_cast<C64System*>(system.get());
        if (!c64) { printf("ERROR: --vicii-dump requires C64\n"); return 1; }

        // Allocate headless framebuffer (no GUI)
        int fb_width, fb_height;
        system->get_display_dimensions(&fb_width, &fb_height);
        std::unique_ptr<uint32_t[]> fb(new uint32_t[fb_width * fb_height]());
        system->set_framebuffer(fb.get(), fb_width, fb_height);

        printf("VICII-DUMP: Normal boot, %dx%d framebuffer\n", fb_width, fb_height);

        // Palette lookup
        static const uint32_t PAL[16] = {
            0xFF000000, 0xFFFFFFFF, 0xFF2B3768, 0xFFB2A470,
            0xFF863D6F, 0xFF438D58, 0xFF792835, 0xFF6FC7B8,
            0xFF254F6F, 0xFF003943, 0xFF59679A, 0xFF444444,
            0xFF6C6C6C, 0xFF84D29A, 0xFFB55E6C, 0xFF959595
        };
        [[maybe_unused]] auto color_name = [&](uint32_t rgba) -> const char* {
            for (int i = 0; i < 16; i++) {
                if (PAL[i] == rgba) {
                    static const char* n[16] = {"BLK","WHT","RED","CYN","PUR","GRN","BLU","YEL","ORN","BRN","LRD","DG1","DG2","LGN","LBL","LG3"};
                    return n[i];
                }
            }
            return "???";
        };
        auto fb_px = [&](int x, int y) -> uint32_t {
            if (x < 0 || y < 0 || x >= fb_width || y >= fb_height) return 0xDEADBEEF;
            return fb.get()[y * fb_width + x];
        };

        // Run frames and dump at key points
        float drain[4096];
        int frame_targets[] = { 5, 500 };
        int frame_count = 0;
        for (int t = 0; t < 2; t++) {
            while (frame_count < frame_targets[t]) {
                system->run_frame();
                system->get_audio_samples(drain, 4096);
                frame_count++;
            }
            printf("\n=== Frame %d ===\n", frame_count);
            uint8_t d011 = c64->vicii->registers.data[0x11];
            uint8_t d016 = c64->vicii->registers.data[0x16];
            uint8_t d018 = c64->vicii->registers.data[0x18];
            uint8_t d020 = c64->vicii->registers.data[0x20];
            uint8_t d021 = c64->vicii->registers.data[0x21];
            uint8_t yscroll = d011 & 0x07;
            uint8_t xscroll = d016 & 0x07;
            uint16_t bank_base = c64->vicii->memory.bank_base;
            uint16_t vm_base = c64->vicii->memory.vm_base;
            uint16_t cb_base = c64->vicii->memory.cb_base;
            bool bmm = (d011 & 0x20) != 0;
            printf("$D011=$%02X $D016=$%02X $D018=$%02X $D020=$%02X $D021=$%02X YSCROLL=%d BMM=%d\n",
                   d011, d016, d018, d020, d021, yscroll, bmm?1:0);
            printf("VIC bank=%d base=$%04X  VM=$%04X (abs=$%04X)  CB=$%04X (abs=$%04X)\n",
                   bank_base / 0x4000, bank_base, vm_base, bank_base | vm_base, cb_base, bank_base | cb_base);
            
            // ===== SPRITE STATE =====
            uint8_t d015 = c64->vicii->registers.data[0x15]; // enable
            uint8_t d010 = c64->vicii->registers.data[0x10]; // X bit 8
            uint8_t d017 = c64->vicii->registers.data[0x17]; // Y expand
            uint8_t d01b = c64->vicii->registers.data[0x1B]; // priority
            uint8_t d01c = c64->vicii->registers.data[0x1C]; // multicolor
            uint8_t d01d = c64->vicii->registers.data[0x1D]; // X expand
            printf("\nSPRITE STATE:\n");
            printf("$D015=$%02X(enable) $D01C=$%02X(mc) $D01D=$%02X(xexp) $D017=$%02X(yexp) $D01B=$%02X(pri) $D010=$%02X(x8)\n",
                   d015, d01c, d01d, d017, d01b, d010);
            
            for (int s = 0; s < 8; s++) {
                uint16_t sx = c64->vicii->registers.data[0x00 + s*2] | ((d010 & (1<<s)) ? 256 : 0);
                uint8_t sy = c64->vicii->registers.data[0x01 + s*2];
                uint8_t sc = c64->vicii->registers.data[0x27 + s]; // color
                bool en = (d015 & (1<<s)) != 0;
                bool xexp = (d01d & (1<<s)) != 0;
                bool yexp = (d017 & (1<<s)) != 0;
                
                // Read sprite pointer from the CORRECT screen area
                uint16_t sp_ptr_addr = (bank_base | vm_base) + 0x3F8 + s;
                uint8_t sp_ptr = c64->ram->data()[sp_ptr_addr];
                uint16_t sp_data_addr = bank_base + (uint16_t)sp_ptr * 64;
                
                printf("  Spr%d: %s X=%3d Y=%3d col=%d ptr=$%02X (data@$%04X) %s%s",
                       s, en ? "ON " : "off", sx, sy, sc, sp_ptr, sp_data_addr,
                       xexp ? "Xexp " : "", yexp ? "Yexp " : "");
                
                if (en) {
                    // Show first 3 bytes of sprite data (first row)
                    printf(" data[0..2]=%02X %02X %02X",
                           c64->ram->data()[sp_data_addr],
                           c64->ram->data()[sp_data_addr+1],
                           c64->ram->data()[sp_data_addr+2]);
                }
                printf("\n");
            }
            
            // ===== EMULATOR SPRITE INTERNAL STATE =====
            printf("\nSprite internal state (emulator):\n");
            for (int s = 0; s < 8; s++) {
                auto& spr = c64->vicii->sprites.sprites[s];
                printf("  Spr%d: enabled=%d dma=%d display=%d dp=$%02X mc=%d shift=$%06X\n",
                       s, spr.enabled, spr.dma_enabled, spr.display_state,
                       spr.data_pointer, spr.mc, spr.shift_reg);
            }
            
            // ===== FRAMEBUFFER SCAN =====
            // Scan entire framebuffer to find non-black rows
            int fb_w = c64->vicii->pixel.framebuffer_width;
            int fb_h = c64->vicii->pixel.framebuffer_height;
            printf("\nFramebuffer size: %dx%d\n", fb_w, fb_h);
            printf("Scanning for non-black rows (showing first non-bg pixel per row):\n");
            int shown_rows = 0;
            for (int y = 0; y < fb_h && shown_rows < 80; y++) {
                // Count non-black pixels in this row
                int non_black = 0;
                int first_non_black_x = -1;
                uint32_t first_color = 0;
                for (int x = 0; x < fb_w; x++) {
                    uint32_t c = fb_px(x, y);
                    if (c != PAL[0]) { // not black
                        non_black++;
                        if (first_non_black_x < 0) {
                            first_non_black_x = x;
                            first_color = c;
                        }
                    }
                }
                if (non_black > 0) {
                    int ci = -1;
                    for (int i = 0; i < 16; i++) {
                        if (PAL[i] == first_color) { ci = i; break; }
                    }
                    printf("  Y=%3d: %4d non-bg pixels, first at X=%d (color=%d)\n", 
                           y, non_black, first_non_black_x, ci);
                    shown_rows++;
                }
            }
            
            // Show a few representative rows in detail
            int check_rows[] = { 42, 50, 58, 66, 74, 82, 90, 100, 120, 140 };
            for (int r = 0; r < 10; r++) {
                int y = check_rows[r];
                if (y >= fb_h) continue;
                printf("Row Y=%d (X=0..%d): ", y, fb_w < 160 ? fb_w-1 : 159);
                for (int x = 0; x < fb_w && x < 160; x++) {
                    uint32_t c = fb_px(x, y);
                    int ci = -1;
                    for (int i = 0; i < 16; i++) {
                        if (PAL[i] == c) { ci = i; break; }
                    }
                    if (x > 0 && x % 8 == 0) printf("|");
                    printf("%X", ci >= 0 ? ci : 0);
                }
                printf("\n");
            }
            
            // ECM mode check
            bool ecm = (d011 & 0x40) != 0;
            printf("\nECM=%d BMM=%d → mode: %s\n", ecm?1:0, bmm?1:0,
                   ecm && !bmm ? "Extended Color Mode" : 
                   !ecm && bmm ? "Bitmap Mode" : 
                   !ecm && !bmm ? "Standard Text Mode" : "Invalid");

            // Check CIA2 DD00 for bank config
            printf("\nCIA2 $DD00 port A value: $%02X\n", c64->ram->data()[0xDD00]);
            // Actually read from CIA2 register directly
            printf("CIA2 PRA register: $%02X\n", c64->cia2->reg[0] & 0x03);
            
            // ===== BITMAP MODE DATA =====
            if (bmm) {
                printf("\nBITMAP MODE DATA (bank=$%04X):\n", bank_base);
                uint16_t bitmap_base = bank_base + (cb_base & 0x2000); // CB13 selects $0000 or $2000
                printf("Bitmap base: $%04X (CB13=%d)\n", bitmap_base, (cb_base & 0x2000) ? 1 : 0);
                printf("Bitmap[0..7] at $%04X: ", bitmap_base);
                for (int i = 0; i < 8; i++) printf("$%02X ", c64->ram->data()[bitmap_base + i]);
                printf("\n");
                // Show bitmap data for cell(5,0) = offset 5*8 = 40
                printf("Bitmap cell(5,0) at $%04X: ", bitmap_base + 40);
                for (int i = 0; i < 8; i++) printf("$%02X ", c64->ram->data()[bitmap_base + 40 + i]);
                printf("\n");
            }
        }
        
        // Save final framebuffer as PNG for visual inspection
        printf("\nSaving framebuffer to /tmp/vicii_dump_f200.png...\n");
        c64->set_framebuffer(fb.get(), fb_width, fb_height);
        // Use base class save_screenshot method
        c64->save_screenshot("/tmp/vicii_dump_f200.png");
        printf("Done. Check /tmp/vicii_dump_f200.png\n");
        
        // Save full 64K RAM dump for offline analysis of decompressed demos
        {
            FILE* f = fopen("/tmp/c64_memdump.bin", "wb");
            if (f) {
                fwrite(c64->ram->data(), 1, 65536, f);
                fclose(f);
                printf("Saved 64K RAM dump to /tmp/c64_memdump.bin\n");
            }
            // Also dump IRQ vector and key zero-page/hardware state
            uint16_t irq_lo = c64->ram->data()[0xFFFE] | (c64->ram->data()[0xFFFF] << 8);
            uint16_t nmi_lo = c64->ram->data()[0xFFFA] | (c64->ram->data()[0xFFFB] << 8);
            // Hardware IRQ vector (from KERNAL RAM copy at $0314/$0315)
            uint16_t hw_irq = c64->ram->data()[0x0314] | (c64->ram->data()[0x0315] << 8);
            printf("IRQ vector: $%04X, NMI vector: $%04X, HW IRQ ($0314): $%04X\n", 
                   irq_lo, nmi_lo, hw_irq);
            printf("CIA1 ICR mask: $%02X, VIC $D01A: $%02X\n",
                   c64->vicii->registers.data[0x1A],
                   c64->vicii->registers.data[0x1A]);
        }
        
        system->shutdown();
        return 0;
    }

    // =========================================================================
    // VIC-II TEST MODE — headless test suite
    // =========================================================================
    if (vicii_test_mode && system) {
        C64System* c64 = dynamic_cast<C64System*>(system.get());
        if (!c64) {
            printf("ERROR: --vicii-test requires C64 system\n");
            return 1;
        }

        // 1. Patch KERNAL to skip memory test + redirect BASIC→test program
        vicii_test::patch_kernal_for_test(c64);

        // 2. Inject the 6510 test program into RAM
        vicii_test::inject_test_program(c64);

        // 3. Allocate headless framebuffer
        int fb_width, fb_height;
        system->get_display_dimensions(&fb_width, &fb_height);
        std::unique_ptr<uint32_t[]> fb(new uint32_t[fb_width * fb_height]());
        system->set_framebuffer(fb.get(), fb_width, fb_height);

        // 4. Initialize harness
        vicii_test::vicii_test_state_t test_state;
        vicii_test::harness_init(&test_state);

        printf("VICII-TEST: Running headless test suite...\n");
        printf("VICII-TEST: Display: %dx%d, FPS target: %u\n",
               fb_width, fb_height, system->get_target_fps());

        // 5. Run frames until tests complete or timeout
        float drain_buf[4096];
        while (vicii_test::harness_poll(&test_state, c64)) {
            system->run_frame();
            // Drain audio to prevent overflow
            system->get_audio_samples(drain_buf, 4096);
        }

        // One final poll to capture last result
        vicii_test::harness_poll(&test_state, c64);

        // 6. Read results buffer and print details
        vicii_test::harness_read_results(&test_state, c64);

        // 7. Print summary
        vicii_test::harness_summary(&test_state);

        // =====================================================================
        // Phase 2: Pixel verification tests (C++ driven)
        // =====================================================================
        printf("\nVICII-TEST: Starting Phase 2 — Pixel Verification...\n");
        auto pixel_results = vicii_test::run_pixel_verification_tests(
            c64, system.get(), fb.get(), fb_width, fb_height);

        int total_fail = test_state.total_fail + pixel_results.total_fail;
        int total_pass = test_state.total_pass + pixel_results.total_pass;
        printf("\n═══ COMBINED RESULTS ═══\n");
        printf("Phase 1 (register):  %d pass / %d fail\n",
               test_state.total_pass, test_state.total_fail);
        printf("Phase 2 (pixel):     %d pass / %d fail\n",
               pixel_results.total_pass, pixel_results.total_fail);
        printf("TOTAL:               %d pass / %d fail\n", total_pass, total_fail);

        // Cleanup
        system->shutdown();
        return (total_fail == 0 && test_state.all_done) ? 0 : 1;
    }

    // Create GUI (with or without a system)
    // If no system, nullptr will cause GUI to show system selection dialog
    // Pass any pending file path so it can be loaded after system selection
    SystemGUI gui(std::move(system), file_path);
    
    // Initialize GUI — window title is managed dynamically by update_window_title()
    if (!gui.init("cermu", 1200, 800)) {
        printf("ERROR: Failed to initialize GUI\n");
        return 1;
    }
    
    printf("Starting GUI main loop\n");
    
    // Run the GUI main loop
    gui.run();
    
    printf("Shutting down\n");
    
    // Cleanup
    gui.cleanup();
    
    return 0;
}