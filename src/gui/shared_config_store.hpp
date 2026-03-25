#pragma once

// =============================================================================
// shared_config_store.hpp — In-session cross-system configuration memory
// =============================================================================
//
// Remembers user choices (region, peripherals, custom settings) across system
// switches within a single application session.  NOT persisted to disk.
//
// =============================================================================

#include <map>
#include <optional>
#include <string>

#include "core/hardware_traits.hpp"  // VideoStandard, SystemConfiguration, HardwareTraits

class SharedConfigStore {
public:
    // --- Region ---
    void set_region(VideoStandard standard) { region_ = standard; }
    std::optional<VideoStandard> get_region() const { return region_; }

    // --- Peripherals ---
    void set_peripheral(const std::string& periph_id, bool enabled) {
        peripherals_[periph_id] = enabled;
    }
    std::optional<bool> get_peripheral(const std::string& periph_id) const {
        auto it = peripherals_.find(periph_id);
        if (it != peripherals_.end()) return it->second;
        return std::nullopt;
    }

    // --- Custom settings ---
    void set_custom(const std::string& option_id, const std::string& value) {
        custom_[option_id] = value;
    }
    std::optional<std::string> get_custom(const std::string& option_id) const {
        auto it = custom_.find(option_id);
        if (it != custom_.end()) return it->second;
        return std::nullopt;
    }

    // --- Memory (system-specific, keyed by short_name) ---
    void set_memory(const std::string& system_short_name, int index) {
        memory_per_system_[system_short_name] = index;
    }
    std::optional<int> get_memory(const std::string& system_short_name) const {
        auto it = memory_per_system_.find(system_short_name);
        if (it != memory_per_system_.end()) return it->second;
        return std::nullopt;
    }

    /// Populate a SystemConfiguration from stored preferences.
    /// Only applies fields that have been explicitly set; leaves the rest
    /// at the defaults provided by traits.
    SystemConfiguration build_config(
        const std::string& system_short_name,
        const HardwareTraits& traits) const
    {
        SystemConfiguration cfg;

        // Region
        if (region_) {
            for (int i = 0; i < static_cast<int>(traits.video_standard_configs.size()); ++i) {
                if (traits.video_standard_configs[i].standard == *region_) {
                    cfg.region_option_index = i;
                    break;
                }
            }
        }

        // Memory
        auto mem = get_memory(system_short_name);
        if (mem && *mem >= 0 && *mem < static_cast<int>(traits.memory_options.size())) {
            cfg.memory_option_index = *mem;
        }

        // Peripherals
        for (const auto& po : traits.peripheral_options) {
            auto stored = get_peripheral(po.id);
            cfg.enabled_peripherals[po.id] = stored.value_or(po.enabled_by_default);
        }

        // Custom settings
        for (const auto& co : traits.custom_options) {
            auto stored = get_custom(co.id);
            if (stored) {
                cfg.custom_settings[co.id] = *stored;
            } else if (co.default_index >= 0 && co.default_index < static_cast<int>(co.choices.size())) {
                cfg.custom_settings[co.id] = co.choices[co.default_index];
            }
        }

        return cfg;
    }

    /// Record all choices from a config back into the store.
    void record_config(
        const std::string& system_short_name,
        const HardwareTraits& traits,
        const SystemConfiguration& config)
    {
        // Region
        if (config.region_option_index >= 0 &&
            config.region_option_index < static_cast<int>(traits.video_standard_configs.size())) {
            region_ = traits.video_standard_configs[config.region_option_index].standard;
        }

        // Memory
        memory_per_system_[system_short_name] = config.memory_option_index;

        // Peripherals
        for (const auto& [id, enabled] : config.enabled_peripherals) {
            peripherals_[id] = enabled;
        }

        // Custom
        for (const auto& [id, value] : config.custom_settings) {
            custom_[id] = value;
        }
    }

private:
    std::optional<VideoStandard>         region_;
    std::map<std::string, bool>          peripherals_;
    std::map<std::string, std::string>   custom_;
    std::map<std::string, int>           memory_per_system_;
};
