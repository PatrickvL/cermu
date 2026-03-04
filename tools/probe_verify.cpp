/**
 * probe_verify — Batch system identification verifier
 *
 * Scans all loadable files under data/<system>/ directories and verifies
 * that each file's identify_system() result matches the expected system.
 *
 * Usage:  probe_verify [data_dir]
 *   data_dir defaults to ../data (relative to executable dir)
 *
 * Exit code: 0 = all pass, 1 = mismatches found
 */

#include "../src/core/emulated_system.h"
#include "../src/core/system_registry.h"
#include "../src/core/formats/format_registry.h"
#include "../src/core/vfs/vfs.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

// ============================================================================
// Helpers
// ============================================================================

static std::string to_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(tolower(c));
    return r;
}

static std::string get_extension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    return to_lower(path.substr(dot));
}

/** Check if a file extension is loadable via the format registry or is an archive. */
static bool is_loadable_extension(const std::string& ext) {
    if (ext.empty()) return false;
    if (FormatRegistry::instance().find_by_extension(ext.c_str()) != nullptr)
        return true;
    if (vfs_is_archive_extension(ext.c_str()))
        return true;
    return false;
}

/** Recursively collect files from a directory. */
static void collect_files(const std::string& dir, std::vector<std::string>& out,
                          int depth = 0) {
    if (depth > 10) return;
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        std::string full = dir + "/" + ent->d_name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            collect_files(full, out, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            std::string ext = get_extension(full);
            if (is_loadable_extension(ext))
                out.push_back(full);
        }
    }
    closedir(d);
}

// ============================================================================
// Build folder → acceptable systems mapping from the SystemRegistry.
// Each system's data_folder declares which directory it lives under;
// multiple systems can share the same folder (e.g. C16, C116, Plus/4
// all declare data_folder = "c16").
// ============================================================================

struct FolderSystemMapping {
    std::string folder;
    std::vector<std::string> acceptable_systems;
};

static std::vector<FolderSystemMapping> build_folder_map() {
    std::map<std::string, std::vector<std::string>> map;
    for (const auto& [desc, factory] : SystemRegistry::instance().get_systems()) {
        if (!desc.data_folder) continue;
        map[desc.data_folder].push_back(desc.short_name);
    }
    std::vector<FolderSystemMapping> result;
    for (auto& [folder, systems] : map)
        result.push_back({ folder, std::move(systems) });
    return result;
}

static const FolderSystemMapping* find_mapping(
    const std::vector<FolderSystemMapping>& mappings,
    const std::string& folder) {
    std::string low = to_lower(folder);
    for (const auto& m : mappings) {
        if (low == m.folder) return &m;
    }
    return nullptr;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    std::string data_dir = (argc > 1) ? argv[1] : "../data";

    auto& registry = SystemRegistry::instance();
    printf("Registered systems: %zu\n", registry.get_systems().size());

    // Build folder → system mapping from the registry
    auto folder_mappings = build_folder_map();
    printf("Data folder mappings:\n");
    for (const auto& m : folder_mappings) {
        printf("  %s -> ", m.folder.c_str());
        for (size_t i = 0; i < m.acceptable_systems.size(); i++) {
            if (i > 0) printf(", ");
            printf("%s", m.acceptable_systems[i].c_str());
        }
        printf("\n");
    }

    // Discover system folders
    DIR* top = opendir(data_dir.c_str());
    if (!top) {
        printf("ERROR: Cannot open data directory: %s\n", data_dir.c_str());
        return 1;
    }

    struct TestFolder {
        std::string path;
        std::string name;
        const FolderSystemMapping* mapping;
    };
    std::vector<TestFolder> test_folders;

    struct dirent* ent;
    while ((ent = readdir(top)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        std::string full = data_dir + "/" + ent->d_name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        auto* mapping = find_mapping(folder_mappings, ent->d_name);
        if (mapping)
            test_folders.push_back({ full, ent->d_name, mapping });
    }
    closedir(top);

    std::sort(test_folders.begin(), test_folders.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });

    int total_files = 0;
    int total_pass = 0;
    int total_fail = 0;
    int total_unknown = 0;
    int total_skipped = 0;

    struct Mismatch {
        std::string file;
        std::string expected;
        std::string got;
        float confidence;
    };
    std::vector<Mismatch> mismatches;

    // Per-system counters
    struct SystemStats {
        int pass = 0, fail = 0, unknown = 0, skipped = 0;
    };
    std::map<std::string, SystemStats> stats;

    for (const auto& folder : test_folders) {
        printf("\n=== Scanning: %s (expect: ", folder.name.c_str());
        for (size_t i = 0; i < folder.mapping->acceptable_systems.size(); i++) {
            if (i > 0) printf("/");
            printf("%s", folder.mapping->acceptable_systems[i].c_str());
        }
        printf(") ===\n");

        std::vector<std::string> files;
        collect_files(folder.path, files);
        printf("  Found %zu loadable files\n", files.size());

        auto& st = stats[folder.name];

        for (const auto& filepath : files) {
            total_files++;

            std::string ext = get_extension(filepath);

            // Archives (.zip, .7z) can't be identified without extracting —
            // the VFS layer would handle this in the actual UI flow, but
            // for a batch probe test we skip them since identify_system
            // expects the raw file data, not an archive wrapper.
            if (ext == ".zip" || ext == ".7z") {
                total_skipped++;
                st.skipped++;
                continue;
            }

            // Read file
            size_t file_size = 0;
            uint8_t* data = vfs_read_file(filepath.c_str(), &file_size);
            if (!data) {
                printf("  SKIP (read failed): %s\n", filepath.c_str());
                total_skipped++;
                st.skipped++;
                continue;
            }

            auto match = registry.identify_system(filepath.c_str(), data, file_size);
            free(data);

            // Strip the data_dir prefix from display path
            std::string display = filepath;
            if (filepath.find(data_dir) == 0)
                display = filepath.substr(data_dir.size() + 1);

            bool is_acceptable = false;
            for (const auto& sys : folder.mapping->acceptable_systems) {
                if (match.system_name == sys) { is_acceptable = true; break; }
            }

            if (match.system_name.empty() || match.confidence < 0.5f) {
                total_unknown++;
                st.unknown++;
                printf("  UNKNOWN (%.2f %s): %s\n",
                       match.confidence,
                       match.system_name.empty() ? "none" : match.system_name.c_str(),
                       display.c_str());
            } else if (is_acceptable) {
                total_pass++;
                st.pass++;
            } else {
                total_fail++;
                st.fail++;
                mismatches.push_back({ display,
                    folder.mapping->acceptable_systems[0],
                    match.system_name, match.confidence });
                printf("  MISMATCH: %s -> %s (%.2f) expected %s\n",
                       display.c_str(), match.system_name.c_str(),
                       match.confidence,
                       folder.mapping->acceptable_systems[0].c_str());
            }
        }

        printf("  Results: %d pass, %d fail, %d unknown, %d skipped\n",
               st.pass, st.fail, st.unknown, st.skipped);
    }

    // Summary
    printf("\n=========================================================\n");
    printf("PROBE VERIFICATION SUMMARY\n");
    printf("=========================================================\n");
    printf("Total files:   %d\n", total_files);
    printf("  Pass:        %d\n", total_pass);
    printf("  Mismatch:    %d\n", total_fail);
    printf("  Unknown:     %d (confidence < 0.5)\n", total_unknown);
    printf("  Skipped:     %d (archives)\n", total_skipped);

    if (!mismatches.empty()) {
        printf("\n--- MISMATCHES ---\n");
        for (const auto& m : mismatches) {
            printf("  %s\n    expected=%s  got=%s (%.2f)\n",
                   m.file.c_str(), m.expected.c_str(),
                   m.got.c_str(), m.confidence);
        }
    }

    printf("\nPer-folder breakdown:\n");
    for (const auto& [name, s] : stats) {
        int total = s.pass + s.fail + s.unknown + s.skipped;
        printf("  %-8s: %4d pass, %4d fail, %4d unknown, %4d skipped (total %d)\n",
               name.c_str(), s.pass, s.fail, s.unknown, s.skipped, total);
    }

    printf("\n%s\n", total_fail == 0 ? "ALL PASS" : "FAILURES DETECTED");
    return total_fail > 0 ? 1 : 0;
}
