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

#include "../src/core/system.hpp"
#include "../src/core/system_registry.hpp"
#include "../src/core/formats/format_handler.hpp"
#include "../src/core/formats/format_registry.hpp"
#include "../src/core/vfs/vfs.hpp"
#include "../src/core/cermu.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#ifdef CERMU_USE_STD_FILESYSTEM
    #include <filesystem>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

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
#ifdef CERMU_USE_STD_FILESYSTEM
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        auto name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        if (entry.is_directory(ec)) {
            collect_files(entry.path().string(), out, depth + 1);
        } else if (entry.is_regular_file(ec)) {
            std::string ext = get_extension(entry.path().string());
            if (is_loadable_extension(ext))
                out.push_back(entry.path().string());
        }
    }
#else
    DIR* dp = opendir(dir.c_str());
    if (!dp) return;
    while (struct dirent* ep = readdir(dp)) {
        std::string name = ep->d_name;
        if (name.empty() || name[0] == '.') continue;
        std::string full = dir + "/" + name;
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
    closedir(dp);
#endif
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
    struct TestFolder {
        std::string path;
        std::string name;
        const FolderSystemMapping* mapping;
    };
    std::vector<TestFolder> test_folders;

    {
#ifdef CERMU_USE_STD_FILESYSTEM
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(data_dir, ec)) {
            if (ec) break;
            auto name = entry.path().filename().string();
            if (name.empty() || name[0] == '.') continue;
            if (!entry.is_directory(ec)) continue;
            auto* mapping = find_mapping(folder_mappings, name);
            if (mapping)
                test_folders.push_back({ entry.path().string(), name, mapping });
        }
        if (ec) {
            printf("ERROR: Cannot open data directory: %s\n", data_dir.c_str());
            return 1;
        }
#else
        DIR* dp = opendir(data_dir.c_str());
        if (!dp) {
            printf("ERROR: Cannot open data directory: %s\n", data_dir.c_str());
            return 1;
        }
        while (struct dirent* ep = readdir(dp)) {
            std::string name = ep->d_name;
            if (name.empty() || name[0] == '.') continue;
            std::string full = data_dir + "/" + name;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            if (!S_ISDIR(st.st_mode)) continue;
            auto* mapping = find_mapping(folder_mappings, name);
            if (mapping)
                test_folders.push_back({ full, name, mapping });
        }
        closedir(dp);
#endif
    }

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

    // ----------------------------------------------------------------
    // probe_one — probe a single blob of data against expected systems.
    // `id_path` is the path hint passed to identify_system (used for
    //  extension/keyword extraction).  `display` is what we print.
    // ----------------------------------------------------------------
    auto probe_one = [&](const std::string& id_path,
                         const std::string& display,
                         const uint8_t* data, size_t size,
                         const FolderSystemMapping* mapping,
                         SystemStats& st) {
        total_files++;

        auto match = registry.identify_system(id_path.c_str(), data, size);

        bool is_acceptable = false;
        for (const auto& sys : mapping->acceptable_systems) {
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
                mapping->acceptable_systems[0],
                match.system_name, match.confidence });
            printf("  MISMATCH: %s -> %s (%.2f) expected %s\n",
                   display.c_str(), match.system_name.c_str(),
                   match.confidence,
                   mapping->acceptable_systems[0].c_str());
        }
    };

    // ----------------------------------------------------------------
    // probe_container_entries — probe individual entries inside a
    // container format (D64, T64, LNX, …) using list/extract callbacks.
    // ----------------------------------------------------------------
    auto probe_container_entries = [&](const format_descriptor_t* fmt,
                                       const uint8_t* data, size_t size,
                                       const std::string& display_prefix,
                                       const FolderSystemMapping* mapping,
                                       SystemStats& st) {
        if (!fmt->list_entries || !fmt->extract_entry) return;

        static constexpr int MAX_ENTRIES = 512;
        format_container_entry_t entries[MAX_ENTRIES];
        int count = fmt->list_entries(data, size, entries, MAX_ENTRIES);
        if (count <= 0) return;

        for (int i = 0; i < count; ++i) {
            // Skip non-loadable extensions (SEQ, REL, USR, etc.)
            std::string entry_ext = get_extension(entries[i].display_name);
            if (!entry_ext.empty() && !is_loadable_extension(entry_ext))
                continue;

            uint8_t* entry_data = nullptr;
            size_t entry_size = 0;
            if (!fmt->extract_entry(data, size, entries[i].index,
                                    &entry_data, &entry_size))
                continue;

            std::string entry_display = display_prefix + "!/" + entries[i].display_name;
            // Use the full display path as id_path so that Phase 3 keyword
            // matching can leverage ancestor folder/archive names.
            probe_one(entry_display, entry_display,
                      entry_data, entry_size, mapping, st);
            free(entry_data);
        }
    };

    // ----------------------------------------------------------------
    // Forward-declare probe_vfs_entry for recursive archive scanning.
    // ----------------------------------------------------------------
    std::function<void(const std::string& vfs_path,
                       const std::string& display_prefix,
                       const FolderSystemMapping* mapping,
                       SystemStats& st, int depth)> probe_vfs_entry;

    // ----------------------------------------------------------------
    // probe_archive — list entries in an archive via VFS, probe each.
    // ----------------------------------------------------------------
    probe_vfs_entry = [&](const std::string& vfs_path,
                          const std::string& display_prefix,
                          const FolderSystemMapping* mapping,
                          SystemStats& st, int depth) {
        if (depth > 3) return;  // nesting limit

        auto entries = vfs_list_entries(vfs_path.c_str());
        for (const auto& ve : entries) {
            if (ve.type == VfsEntryType::Directory) continue;
            if (ve.name == "." || ve.name == "..") continue;

            std::string entry_ext = get_extension(ve.name);
            std::string entry_vfs = vfs_join_path(vfs_path, ve.name);
            std::string entry_display = display_prefix + "!/" + ve.name;

            // Read the entry data
            size_t entry_size = 0;
            uint8_t* data = vfs_read_file(entry_vfs.c_str(), &entry_size);
            if (!data) {
                printf("  SKIP (read failed): %s\n", entry_display.c_str());
                total_skipped++;
                st.skipped++;
                continue;
            }

            // If this entry is itself an archive, recurse
            if (vfs_is_archive_extension(entry_ext.c_str())) {
                probe_vfs_entry(entry_vfs, entry_display, mapping, st, depth + 1);
                free(data);
                continue;
            }

            // Probe the entry itself (container or regular file)
            if (is_loadable_extension(entry_ext)) {
                probe_one(entry_vfs, entry_display, data, entry_size, mapping, st);

                // If it's a container, also probe its internal entries
                const auto* fmt = FormatRegistry::instance().find_by_extension(entry_ext.c_str());
                if (fmt && (fmt->capabilities & FORMAT_CAP_CONTAINER) &&
                    fmt->list_entries && fmt->extract_entry) {
                    probe_container_entries(fmt, data, entry_size,
                                           entry_display, mapping, st);
                }
            }

            free(data);
        }
    };

    // ----------------------------------------------------------------
    // Main scan loop
    // ----------------------------------------------------------------
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
            std::string ext = get_extension(filepath);

            // Strip the data_dir prefix for display
            std::string display = filepath;
            if (filepath.find(data_dir) == 0)
                display = filepath.substr(data_dir.size() + 1);

            // Archives — scan their contents via VFS
            if (vfs_is_archive_extension(ext.c_str())) {
                probe_vfs_entry(filepath, display, folder.mapping, st, 0);
                continue;
            }

            // Read file
            size_t file_size = 0;
            uint8_t* data = vfs_read_file(filepath.c_str(), &file_size);
            if (!data) {
                printf("  SKIP (read failed): %s\n", display.c_str());
                total_skipped++;
                st.skipped++;
                continue;
            }

            // Probe the file
            probe_one(filepath, display, data, file_size, folder.mapping, st);

            // If it's a container, also probe individual entries
            const auto* fmt = FormatRegistry::instance().find_by_extension(ext.c_str());
            if (fmt && (fmt->capabilities & FORMAT_CAP_CONTAINER) &&
                fmt->list_entries && fmt->extract_entry) {
                probe_container_entries(fmt, data, file_size,
                                       display, folder.mapping, st);
            }

            free(data);
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
    printf("  Skipped:     %d (read failures)\n", total_skipped);

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
