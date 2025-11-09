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

// Apply global style to a visualization
inline void ApplyGlobalStyle(ChipVisualization& viz) {
    viz.set_visual_config(GetGlobalChipConfig());
}

// Auto-updating chip visualization wrapper
class AutoUpdatingChipVisualization {
public:
    AutoUpdatingChipVisualization(const std::string& id, const ChipLayout& layout);
    ~AutoUpdatingChipVisualization();
    
    // Forward ChipVisualization interface
    void render(ImVec2 chip_center, const std::vector<PinSignalState>& pin_states, const char* chip_name = nullptr);
    void render_settings_gui();
    ImVec2 get_recommended_size() const;
    
    // Access underlying visualization
    ChipVisualization& getVisualization() { return *viz_; }
    const ChipVisualization& getVisualization() const { return *viz_; }
    
private:
    void onConfigChanged(const ChipVisualConfig& new_config);
    
    std::string id_;
    std::unique_ptr<ChipVisualization> viz_;
    ChipLayout layout_;
};

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

// Helper for existing chip GUIs to integrate with global style system
template<typename ChipType>
class GlobalStyleIntegratedGUI {
public:
    GlobalStyleIntegratedGUI(const std::string& chip_name, ChipType* chip_instance);
    
    void render_chip_visualization(ImVec2 center, const std::vector<PinSignalState>& states);
    void render_style_controls();
    
private:
    std::string chip_name_;
    ChipType* chip_;
    AutoUpdatingChipVisualization viz_;
};

// Menu integration
void RenderGlobalChipStyleMenu();

// Debug window integration  
void RenderChipStyleDebugWindow(bool* show_window = nullptr);