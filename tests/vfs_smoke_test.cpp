/**
 * VFS Smoke Test — verifies archive reading works end-to-end
 *
 * Usage: vfs_smoke_test [archive.zip]
 * Without arguments, tests with a known ROM zip if present.
 */

#include "vfs/vfs.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int tests_run = 0;
static int tests_passed = 0;

static void test(const char* name, bool condition) {
    tests_run++;
    if (condition) {
        tests_passed++;
        printf("  PASS: %s\n", name);
    } else {
        printf("  FAIL: %s\n", name);
    }
}

int main(int argc, char* argv[]) {
    printf("=== VFS Smoke Test ===\n\n");

    // ========================================================================
    // 1. Path parsing tests
    // ========================================================================
    printf("[Path Parsing]\n");

    {
        auto parts = vfs_parse_path("/data/roms/game.nes");
        test("plain path: real_path correct",
             parts.real_path == "/data/roms/game.nes");
        test("plain path: archive_path empty",
             parts.archive_path.empty());
    }
    {
        auto parts = vfs_parse_path("/data/roms.zip!/game.nes");
        test("archive path: real_path correct",
             parts.real_path == "/data/roms.zip");
        test("archive path: archive_path correct",
             parts.archive_path == "game.nes");
    }
    {
        auto parts = vfs_parse_path("/outer.zip!/inner.zip!/game.nes");
        test("nested path: real_path correct",
             parts.real_path == "/outer.zip");
        test("nested path: archive_path correct",
             parts.archive_path == "inner.zip!/game.nes");
    }

    test("vfs_is_archive_path positive",
         vfs_is_archive_path("/data/roms.zip!/game.nes"));
    test("vfs_is_archive_path negative",
         !vfs_is_archive_path("/data/roms/game.nes"));

    // ========================================================================
    // 2. Extension / filename tests
    // ========================================================================
    printf("\n[Filename/Extension]\n");

    test("vfs_filename plain",
         vfs_filename("/data/roms/game.nes") == "game.nes");
    test("vfs_filename archive",
         vfs_filename("/data/roms.zip!/subdir/game.nes") == "game.nes");
    test("vfs_extension plain",
         vfs_extension("/data/roms/game.nes") == ".nes");
    test("vfs_extension archive",
         vfs_extension("/data/roms.zip!/game.nes") == ".nes");
    test("vfs_extension archive outer (should get inner)",
         vfs_extension("/data/roms.zip!/game.prg") == ".prg");

    test("vfs_is_archive_extension .zip", vfs_is_archive_extension(".zip"));
    test("vfs_is_archive_extension .ZIP", vfs_is_archive_extension(".ZIP"));
    test("vfs_is_archive_extension .7z",  vfs_is_archive_extension(".7z"));
    test("vfs_is_archive_extension .rar", vfs_is_archive_extension(".rar"));
    test("vfs_is_archive_extension .tar", vfs_is_archive_extension(".tar"));
    test("vfs_is_archive_extension .nes", !vfs_is_archive_extension(".nes"));

    // ========================================================================
    // 3. Path joining
    // ========================================================================
    printf("\n[Path Joining]\n");

    test("vfs_join regular",
         vfs_join_path("/data/roms", "game.nes") == "/data/roms/game.nes");
    test("vfs_join archive",
         vfs_join_path("/data/roms.zip", "game.nes") == "/data/roms.zip!/game.nes");
    test("vfs_join archive subpath",
         vfs_join_path("/data/roms.zip!/subdir", "game.nes") ==
         "/data/roms.zip!/subdir/game.nes");

    // ========================================================================
    // 4. Real file read (if a test zip is available)
    // ========================================================================
    printf("\n[Archive Reading]\n");

    const char* test_zip = nullptr;
    if (argc > 1) {
        test_zip = argv[1];
    } else {
        // Try to find a known test zip
        static const char* candidates[] = {
            "../data/nes/roms/Castlevania (Europe).zip",
            "data/nes/roms/Castlevania (Europe).zip",
            nullptr
        };
        for (const char** c = candidates; *c; ++c) {
            if (vfs_exists(*c)) { test_zip = *c; break; }
        }
    }

    if (test_zip) {
        printf("  Using test archive: %s\n", test_zip);

        // List entries
        auto entries = vfs_list_entries(test_zip);
        test("archive has entries", !entries.empty());
        printf("  Archive contains %zu entries:\n", entries.size());
        for (const auto& e : entries) {
            const char* type_str = (e.type == VfsEntryType::File) ? "FILE" :
                                   (e.type == VfsEntryType::Archive) ? "ARCHIVE" : "DIR";
            printf("    [%s] %s (%zu bytes) -> %s\n",
                   type_str, e.name.c_str(), e.size, e.full_path.c_str());
        }

        // Find a .nes file and try to read it
        std::string nes_vfs_path;
        for (const auto& e : entries) {
            std::string ext = vfs_extension(e.name.c_str());
            if (ext == ".nes" || ext == ".NES") {
                nes_vfs_path = e.full_path;
                break;
            }
        }

        if (!nes_vfs_path.empty()) {
            printf("  Reading NES ROM via VFS: %s\n", nes_vfs_path.c_str());

            size_t rom_size = 0;
            uint8_t* rom_data = vfs_read_file(nes_vfs_path.c_str(), &rom_size);
            test("read ROM from archive", rom_data != nullptr);
            test("ROM size > 0", rom_size > 0);

            if (rom_data && rom_size >= 4) {
                bool has_ines_header = (rom_data[0] == 'N' && rom_data[1] == 'E' &&
                                        rom_data[2] == 'S' && rom_data[3] == 0x1A);
                test("ROM has valid iNES header", has_ines_header);
                printf("  ROM size: %zu bytes, header: %c%c%c%02X\n",
                       rom_size, rom_data[0], rom_data[1], rom_data[2], rom_data[3]);
            }
            free(rom_data);
        } else {
            printf("  No .nes file found in archive (skipping read test)\n");
        }

        // Test archive scanning (requires FormatRegistry + SystemRegistry,
        // which are tested via the main executables rather than this
        // lightweight VFS-only smoke test)
    } else {
        printf("  No test archive found — skipping archive read tests\n");
        printf("  Run with: vfs_smoke_test <path-to-zip>\n");
    }

    // ========================================================================
    // Summary
    // ========================================================================
    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
