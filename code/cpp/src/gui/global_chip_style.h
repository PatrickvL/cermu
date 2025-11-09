/*
 * global_chip_style.h - Global chip visualization style management
 * 
 * Provides centralized control over all chip visualization styles throughout
 * the emulator. Includes real-time configuration dialog and automatic updates
 * to all active chip visualizations.
 */

#pragma once

#ifndef IMGUI_VERSION
#error "global_chip_style.h requires IMGUI_VERSION to be defined. This header should only be included in GUI builds."
#endif

#include "chip_visualization.h"
#include <functional>
#include <vector>
#include <memory>
#include <map>
#include <string>

// ============================================================================
// GLOBAL CHIP STYLE MANAGER
// ============================================================================

class GlobalChipStyleManager {
public:
    // Singleton access
    static GlobalChipStyleManager& getInstance();
    
    // Get current global configuration
    const ChipVisualConfig& getGlobalConfig() const { return global_config_; }
    
    // Update global configuration and notify all listeners
    void setGlobalConfig(const ChipVisualConfig& config);
    
    // Apply preset style globally
    void applyPresetStyle(VisualStyle style);
    void applyDatasheetMode(DatasheetMode mode);
    
    // Register/unregister visualization instances for automatic updates
    using UpdateCallback = std::function<void(const ChipVisualConfig&)>;
    void registerVisualization(const std::string& id, UpdateCallback callback);
    void unregisterVisualization(const std::string& id);
    
    // Configuration dialog
    void renderConfigDialog(bool* show_dialog = nullptr);
    
    // Preset management
    void savePreset(const std::string& name);
    void loadPreset(const std::string& name);
    void deletePreset(const std::string& name);
    std::vector<std::string> getPresetNames() const;
    
    // Configuration persistence
    void saveToFile(const std::string& filename);
    void loadFromFile(const std::string& filename);
    
    // Real-time preview
    void setPreviewMode(bool enabled) { preview_mode_ = enabled; }
    bool isPreviewMode() const { return preview_mode_; }
    
private:
    GlobalChipStyleManager() = default;
    ~GlobalChipStyleManager() = default;
    GlobalChipStyleManager(const GlobalChipStyleManager&) = delete;
    GlobalChipStyleManager& operator=(const GlobalChipStyleManager&) = delete;
    
    void notifyAllVisualizations();
    void renderStylePresets();
    void renderColorControls();
    void renderDimensionControls();
    void renderDisplayOptions();
    void renderDatasheetModeControls();
    void renderPresetManagement();
    void renderRealTimePreview();
    
    ChipVisualConfig global_config_ = ChipVisualConfig::get_default();
public:
    std::map<std::string, UpdateCallback> registered_visualizations_;
    std::map<std::string, ChipVisualConfig> saved_presets_;
    
private:
    
    // Dialog state
    bool preview_mode_ = true;
    bool show_advanced_options_ = false;
    bool config_changed_this_frame_ = false;
    char preset_name_buffer_[64] = "";
    
    // Temporary config for real-time preview
    ChipVisualConfig temp_config_;
    ChipVisualConfig original_config_;  // Backup of original config before preview mode
    bool using_temp_config_ = false;
};

// ============================================================================
// CONVENIENCE MACROS AND FUNCTIONS
// ============================================================================

// Easy access to global style manager
#define GLOBAL_CHIP_STYLE GlobalChipStyleManager::getInstance()

// Get current global chip configuration
inline const ChipVisualConfig& GetGlobalChipConfig() {
    return GlobalChipStyleManager::getInstance().getGlobalConfig();
}


// Simple utility functions for chip GUIs - no wrapper needed!
// ChipVisualization now automatically uses global config:
//
// Example usage in chip GUI:
//   ChipVisualization* viz = get_chip_visualization_instance();
//   viz->render(center, pin_states, chip_name);
//   // That's it! Global config is used automatically

// ============================================================================
// HISTORICAL STYLE PRESETS
// ============================================================================

namespace HistoricalPresets {
    void RegisterAllPresets();
    
    // Specific historical configurations
    ChipVisualConfig Intel8080_1974();      // Original Intel 8080 style
    ChipVisualConfig MOS6502_1975();        // Early MOS Technology style  
    ChipVisualConfig Zilog80_1976();        // Original Z80 style
    ChipVisualConfig Intel8086_1978();      // Late 70s Intel transition
    ChipVisualConfig Motorola68000_1980();  // Early 80s Motorola style
    ChipVisualConfig Generic80s_1985();     // Universal 80s standard
    
    // Manufacturing-specific styles
    ChipVisualConfig IntelHouseStyle();     // Intel's conservative approach
    ChipVisualConfig MotorolaHouseStyle();  // Motorola's progressive style
    ChipVisualConfig TexasInstruments();    // TI's technical style
    ChipVisualConfig SigneticsStyle();     // Signetics/Philips style
}

// ============================================================================
// INTEGRATION HELPERS
// ============================================================================

// Example integration pattern for chip GUIs:
//
// static ChipVisualization* get_chip_viz_instance() {
//     static std::unique_ptr<ChipVisualization> viz = nullptr;
//     if (!viz) {
//         ChipLayout layout = create_chip_layout();
//         viz = std::make_unique<ChipVisualization>(layout);
//     }
//     return viz.get();
// }
//
// void render_chip_debug_window() {
//     ChipVisualization* viz = get_chip_viz_instance();
//     viz->set_visual_config(GetGlobalChipConfig());  // Apply global config
//     viz->render(center, pin_states, chip_name);     // Render with current style
// }

// Menu integration
void RenderGlobalChipStyleMenu();

// Debug window integration  
void RenderChipStyleDebugWindow(bool* show_window = nullptr);