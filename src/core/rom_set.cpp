/**
 * ROM Set — Implementation
 *
 * Contains rom_set_probe() which iterates all registered systems,
 * collects their ROM set descriptors, and matches files at a VFS path.
 */

#include "core/rom_set.hpp"
#include "core/system_registry.hpp"
#include "core/system.hpp"

#include <cstdio>

RomSetProbeResult rom_set_probe(const char* vfs_base_path) {
    RomSetProbeResult result;
    result.confidence = 0.0f;

    if (!vfs_base_path) return result;

    const auto& registry = SystemRegistry::instance();

    // Collect all ROM set descriptors from all registered systems
    for (const auto& [descriptor, factory] : registry.get_systems()) {
        // Create a temporary system to query its ROM set descriptors.
        // This is inexpensive — constructors only set up metadata.
        auto sys = factory();
        if (!sys) continue;

        auto rom_sets = sys->get_rom_set_descriptors();
        if (rom_sets.empty()) continue;

        // Build pointer array for rom_set_scan_and_match
        std::vector<const RomSetDescriptor*> ptrs;
        ptrs.reserve(rom_sets.size());
        for (const auto* rs : rom_sets)
            ptrs.push_back(rs);

        auto match = rom_set_scan_and_match(
            vfs_base_path, ptrs.data(), static_cast<int>(ptrs.size()));

        if (match.matched && match.confidence > result.confidence) {
            result.system_name = descriptor.short_name;
            result.confidence  = match.confidence;
            result.rom_match   = std::move(match);
        }
    }


    return result;
}
