/*
 * global_chip_style.cpp - Global chip visualization style management implementation
 */

#include "global_chip_style.h"
#include <map>
#include <imgui.h>
#include <algorithm>
#include <cstdio>

// Only compile when ImGui is available
#ifdef IMGUI_VERSION

// ============================================================================
// GLOBAL CHIP STYLE MANAGER IMPLEMENTATION
// ============================================================================

GlobalChipStyleManager& GlobalChipStyleManager::getInstance() {
    static GlobalChipStyleManager instance;
    return instance;
}

void GlobalChipStyleManager::setGlobalConfig(const ChipVisualConfig& config) {
    global_config_ = config;
    notifyAllVisualizations();
}

void GlobalChipStyleManager::applyPresetStyle(VisualStyle style) {
    global_config_ = ChipVisualConfig::get_style(style);
    notifyAllVisualizations();
}

void GlobalChipStyleManager::applyDatasheetMode(DatasheetMode mode) {
    global_config_.datasheet_mode = mode;
    
    // Adjust related settings automatically
    switch (mode) {
        case DatasheetMode::EXTERNAL_LABELING:
            global_config_.labels_inside_package = false;
            global_config_.numbers_inside_package = false;
            global_config_.show_pin_groups = true;
            global_config_.show_datasheet_grid = true;
            break;
        case DatasheetMode::TOP_VIEW_80S:
            global_config_.labels_inside_package = true;
            global_config_.numbers_inside_package = true;
            global_config_.show_pin_groups = false;
            global_config_.show_datasheet_grid = false;
            break;
        case DatasheetMode::BOTTOM_VIEW:
            global_config_.labels_inside_package = true;
            global_config_.numbers_inside_package = true;
            break;
        case DatasheetMode::FUNCTIONAL_BLOCK:
            global_config_.show_pin_labels = false;
            global_config_.show_pin_numbers = false;
            break;
        case DatasheetMode::CONNECTION_DIAGRAM:
            global_config_.show_pin_labels = true;
            global_config_.show_pin_numbers = true;
            break;
        case DatasheetMode::PACKAGE_OUTLINE:
            global_config_.show_dimension_lines = true;
            global_config_.show_pin_pitch_indicators = true;
            break;
        default:
            break;
    }
    
    notifyAllVisualizations();
}

void GlobalChipStyleManager::registerVisualization(const std::string& id, UpdateCallback callback) {
    registered_visualizations_[id] = callback;
    // Immediately update with current config
    callback(global_config_);
}

void GlobalChipStyleManager::unregisterVisualization(const std::string& id) {
    registered_visualizations_.erase(id);
}

void GlobalChipStyleManager::notifyAllVisualizations() {
    for (auto& pair : registered_visualizations_) {
        pair.second(global_config_);
    }
}

// ============================================================================
// CONFIGURATION DIALOG
// ============================================================================

void GlobalChipStyleManager::renderConfigDialog(bool* show_dialog) {
    if (show_dialog && !*show_dialog) {
        // Dialog is being closed - restore original config if we were in preview mode
        if (preview_mode_ && using_temp_config_) {
            // Don't call setGlobalConfig here to avoid recursive notifications
            // The original config is already preserved in global_config_
        }
        return;
    }
    
    config_changed_this_frame_ = false;
    
    // Initialize temp config on first use or when preview mode changes
    static bool temp_config_initialized = false;
    static bool was_in_preview_mode = false;
    
    if (!temp_config_initialized || (using_temp_config_ != preview_mode_)) {
        // Save original config when entering preview mode
        if (preview_mode_ && !was_in_preview_mode) {
            original_config_ = global_config_;
        }
        // Restore original config when leaving preview mode
        else if (!preview_mode_ && was_in_preview_mode) {
            global_config_ = original_config_;
        }
        
        using_temp_config_ = preview_mode_;
        if (using_temp_config_) {
            temp_config_ = global_config_;
        }
        was_in_preview_mode = preview_mode_;
        temp_config_initialized = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Global Chip Visual Style Configuration", show_dialog, ImGuiWindowFlags_MenuBar)) {
        
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Presets")) {
                renderPresetManagement();
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                ImGui::Checkbox("Show Advanced Options", &show_advanced_options_);
                ImGui::Checkbox("Real-time Preview", &preview_mode_);
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }
        
        // Main content in columns
        ImGui::Columns(2, "StyleColumns", true);
        
        // Left column - Controls
        ImGui::Text("Style Configuration");
        ImGui::Separator();
        
        renderStylePresets();
        ImGui::Spacing();
        
        renderDatasheetModeControls();
        ImGui::Spacing();
        
        renderDisplayOptions();
        ImGui::Spacing();
        
        renderColorControls();
        ImGui::Spacing();
        
        renderDimensionControls();
        
        ImGui::NextColumn();
        
        // Right column - Preview
        ImGui::Text("Real-time Preview");
        ImGui::Separator();
        
        renderRealTimePreview();
        
        ImGui::Columns(1);
        
        // Bottom buttons
        ImGui::Separator();
        if (ImGui::Button("Apply Global")) {
            if (using_temp_config_) {
                setGlobalConfig(temp_config_);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Default")) {
            global_config_ = ChipVisualConfig::get_default();
            if (using_temp_config_) {
                temp_config_ = global_config_;
            }
            notifyAllVisualizations();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            if (show_dialog) *show_dialog = false;
        }
    }
    ImGui::End();
}

void GlobalChipStyleManager::renderStylePresets() {
    ImGui::Text("Historical Style Presets");
    
    static const char* preset_names[] = {
        "CLASSIC_DARK", "CLASSIC_LIGHT", "HIGH_CONTRAST", "COLORFUL", 
        "MONOCHROME", "DATASHEET", "DATASHEET_70S", "DATASHEET_80S", "SCHEMATIC"
    };
    
    static const VisualStyle preset_styles[] = {
        VisualStyle::CLASSIC_DARK, VisualStyle::CLASSIC_LIGHT, VisualStyle::HIGH_CONTRAST,
        VisualStyle::COLORFUL, VisualStyle::MONOCHROME, VisualStyle::DATASHEET,
        VisualStyle::DATASHEET_70S, VisualStyle::DATASHEET_80S, VisualStyle::SCHEMATIC
    };
    
    for (int i = 0; i < 9; i++) {
        if (ImGui::Button(preset_names[i], ImVec2(100, 0))) {
            ChipVisualConfig new_config = ChipVisualConfig::get_style(preset_styles[i]);
            if (using_temp_config_) {
                temp_config_ = new_config;
                config_changed_this_frame_ = true;
            } else {
                setGlobalConfig(new_config);
            }
        }
        if ((i + 1) % 3 != 0) ImGui::SameLine();
    }
}

void GlobalChipStyleManager::renderDatasheetModeControls() {
    ImGui::Text("Datasheet Mode");
    
    ChipVisualConfig& config = using_temp_config_ ? temp_config_ : global_config_;
    
    static const char* mode_names[] = {
        "MODERN", "EXTERNAL_LABELING", "TOP_VIEW_80S", "BOTTOM_VIEW", 
        "FUNCTIONAL_BLOCK", "CONNECTION_DIAGRAM", "PACKAGE_OUTLINE"
    };
    
    int current_mode = (int)config.datasheet_mode;
    if (ImGui::Combo("Mode", &current_mode, mode_names, 7)) {
        config.datasheet_mode = (DatasheetMode)current_mode;
        
        // Auto-adjust related settings
        switch (config.datasheet_mode) {
            case DatasheetMode::EXTERNAL_LABELING:
                config.labels_inside_package = false;
                config.numbers_inside_package = false;
                config.show_pin_groups = true;
                config.show_datasheet_grid = true;
                config.notation_style = PinNotationStyle::SLASH_PREFIX;
                break;
            case DatasheetMode::TOP_VIEW_80S:
                config.labels_inside_package = true;
                config.numbers_inside_package = true;
                config.show_pin_groups = false;
                config.show_datasheet_grid = false;
                config.notation_style = PinNotationStyle::HASH_SUFFIX;
                break;
            case DatasheetMode::BOTTOM_VIEW:
                config.labels_inside_package = true;
                config.numbers_inside_package = true;
                break;
            default:
                break;
        }
        
        config_changed_this_frame_ = true;
    }
    
    // Pin notation style
    static const char* notation_names[] = {
        "SLASH_PREFIX (/IRQ)", "OVERLINE", "TILDE_PREFIX (~IRQ)", 
        "HASH_SUFFIX (IRQ#)", "ASTERISK_SUFFIX (IRQ*)", 
        "N_SUFFIX (IRQ_N)", "BAR_SUFFIX (IRQ_BAR)"
    };
    
    int current_notation = (int)config.notation_style;
    if (ImGui::Combo("Active-Low Style", &current_notation, notation_names, 7)) {
        config.notation_style = (PinNotationStyle)current_notation;
        config_changed_this_frame_ = true;
    }
}

void GlobalChipStyleManager::renderDisplayOptions() {
    ImGui::Text("Display Options");
    
    ChipVisualConfig& config = using_temp_config_ ? temp_config_ : global_config_;
    
    if (ImGui::Checkbox("Show Pin Numbers", &config.show_pin_numbers)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show Pin Labels", &config.show_pin_labels)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show LED Indicators", &config.show_led_indicators)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show Package Name", &config.show_package_name)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show Chip Markings", &config.show_chip_markings)) config_changed_this_frame_ = true;
    
    ImGui::Separator();
    
    if (ImGui::Checkbox("Labels Inside Package", &config.labels_inside_package)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Numbers Inside Package", &config.numbers_inside_package)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show Pin Groups", &config.show_pin_groups)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show Datasheet Grid", &config.show_datasheet_grid)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show Dimension Lines", &config.show_dimension_lines)) config_changed_this_frame_ = true;
    if (ImGui::Checkbox("Show Pin Pitch Indicators", &config.show_pin_pitch_indicators)) config_changed_this_frame_ = true;
    
    if (show_advanced_options_) {
        ImGui::Separator();
        if (ImGui::Checkbox("Show Thermal Pad", &config.show_thermal_pad)) config_changed_this_frame_ = true;
        if (ImGui::Checkbox("Show Alternate Functions", &config.show_alternate_functions)) config_changed_this_frame_ = true;
        if (ImGui::Checkbox("Show Voltage Levels", &config.show_voltage_levels)) config_changed_this_frame_ = true;
        if (ImGui::Checkbox("Show PWM Indicators", &config.show_pwm_indicators)) config_changed_this_frame_ = true;
        if (ImGui::Checkbox("Use Compact Layout", &config.use_compact_layout)) config_changed_this_frame_ = true;
    }
}

void GlobalChipStyleManager::renderColorControls() {
    ImGui::Text("Colors");
    
    ChipVisualConfig& config = using_temp_config_ ? temp_config_ : global_config_;
    
    // Convert ABGR to RGBA for ImGui color picker
    auto abgr_to_rgba = [](uint32_t abgr) -> ImVec4 {
        return ImVec4(
            ((abgr >> 0) & 0xFF) / 255.0f,   // R
            ((abgr >> 8) & 0xFF) / 255.0f,   // G
            ((abgr >> 16) & 0xFF) / 255.0f,  // B
            ((abgr >> 24) & 0xFF) / 255.0f   // A
        );
    };
    
    auto rgba_to_abgr = [](const ImVec4& rgba) -> uint32_t {
        return ((uint32_t)(rgba.w * 255) << 24) |
               ((uint32_t)(rgba.z * 255) << 16) |
               ((uint32_t)(rgba.y * 255) << 8) |
               ((uint32_t)(rgba.x * 255) << 0);
    };
    
    ImVec4 color;
    
    color = abgr_to_rgba(config.chip_body_color);
    if (ImGui::ColorEdit3("Chip Body", &color.x)) {
        config.chip_body_color = rgba_to_abgr(color);
        config_changed_this_frame_ = true;
    }
    
    color = abgr_to_rgba(config.chip_border_color);
    if (ImGui::ColorEdit3("Chip Border", &color.x)) {
        config.chip_border_color = rgba_to_abgr(color);
        config_changed_this_frame_ = true;
    }
    
    color = abgr_to_rgba(config.text_color);
    if (ImGui::ColorEdit3("Text", &color.x)) {
        config.text_color = rgba_to_abgr(color);
        config_changed_this_frame_ = true;
    }
    
    color = abgr_to_rgba(config.pin_number_color);
    if (ImGui::ColorEdit3("Pin Numbers", &color.x)) {
        config.pin_number_color = rgba_to_abgr(color);
        config_changed_this_frame_ = true;
    }
    
    if (show_advanced_options_) {
        color = abgr_to_rgba(config.led_active_color);
        if (ImGui::ColorEdit3("LED Active", &color.x)) {
            config.led_active_color = rgba_to_abgr(color);
            config_changed_this_frame_ = true;
        }
        
        color = abgr_to_rgba(config.datasheet_grid_color);
        if (ImGui::ColorEdit3("Grid Lines", &color.x)) {
            config.datasheet_grid_color = rgba_to_abgr(color);
            config_changed_this_frame_ = true;
        }
    }
}

void GlobalChipStyleManager::renderDimensionControls() {
    ImGui::Text("Dimensions");
    
    ChipVisualConfig& config = using_temp_config_ ? temp_config_ : global_config_;
    
    if (ImGui::SliderFloat("Pin Width", &config.pin_width, 2.0f, 20.0f, "%.1f")) config_changed_this_frame_ = true;
    if (ImGui::SliderFloat("Pin Height", &config.pin_height, 4.0f, 24.0f, "%.1f")) config_changed_this_frame_ = true;
    if (ImGui::SliderFloat("Pin Spacing", &config.pin_spacing_factor, 0.8f, 2.0f, "%.1f")) config_changed_this_frame_ = true;
    if (ImGui::SliderFloat("Font Size", &config.font_size, 8.0f, 20.0f, "%.1f")) config_changed_this_frame_ = true;
    if (ImGui::SliderFloat("Label Offset", &config.label_offset, 5.0f, 30.0f, "%.1f")) config_changed_this_frame_ = true;
    
    if (show_advanced_options_) {
        if (ImGui::SliderFloat("Border Width", &config.chip_border_width, 0.5f, 5.0f, "%.1f")) config_changed_this_frame_ = true;
        if (ImGui::SliderFloat("Marker Size", &config.marker_size, 2.0f, 16.0f, "%.1f")) config_changed_this_frame_ = true;
        if (ImGui::SliderFloat("LED Radius", &config.led_radius, 1.0f, 8.0f, "%.1f")) config_changed_this_frame_ = true;
    }
}

void GlobalChipStyleManager::renderPresetManagement() {
    ImGui::InputText("Preset Name", preset_name_buffer_, sizeof(preset_name_buffer_));
    
    if (ImGui::Button("Save Current")) {
        if (strlen(preset_name_buffer_) > 0) {
            savePreset(preset_name_buffer_);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        if (strlen(preset_name_buffer_) > 0) {
            loadPreset(preset_name_buffer_);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        if (strlen(preset_name_buffer_) > 0) {
            deletePreset(preset_name_buffer_);
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Saved Presets:");
    for (const auto& pair : saved_presets_) {
        if (ImGui::Selectable(pair.first.c_str())) {
            strncpy(preset_name_buffer_, pair.first.c_str(), sizeof(preset_name_buffer_) - 1);
            preset_name_buffer_[sizeof(preset_name_buffer_) - 1] = '\0';
        }
    }
}

void GlobalChipStyleManager::renderRealTimePreview() {
    if (preview_mode_ && config_changed_this_frame_) {
        // In preview mode, temporarily update the global config so all renderers see the changes
        // This is safe because we restore it when preview mode is disabled or dialog is closed
        ChipVisualConfig old_config = global_config_;
        global_config_ = temp_config_;
        
        // Also notify registered visualizations for compatibility
        for (auto& pair : registered_visualizations_) {
            pair.second(temp_config_);
        }
    }
    
    ImGui::Text("Live Preview Information");
    ImGui::Separator();
    ImGui::Text("Active visualizations: %zu", registered_visualizations_.size());
    
    if (preview_mode_) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Real-time preview: ENABLED");
        ImGui::Text("Changes apply immediately to all chip visualizations");
        ImGui::Text("Use 'Apply Global' to make changes permanent");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Real-time preview: DISABLED");
        ImGui::Text("Click 'Apply Global' to update all visualizations");
    }
    
    if (config_changed_this_frame_) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Configuration updated this frame");
    }
}

// ============================================================================
// PRESET MANAGEMENT
// ============================================================================

void GlobalChipStyleManager::savePreset(const std::string& name) {
    saved_presets_[name] = using_temp_config_ ? temp_config_ : global_config_;
}

void GlobalChipStyleManager::loadPreset(const std::string& name) {
    auto it = saved_presets_.find(name);
    if (it != saved_presets_.end()) {
        if (using_temp_config_) {
            temp_config_ = it->second;
            config_changed_this_frame_ = true;
        } else {
            setGlobalConfig(it->second);
        }
    }
}

void GlobalChipStyleManager::deletePreset(const std::string& name) {
    saved_presets_.erase(name);
}

std::vector<std::string> GlobalChipStyleManager::getPresetNames() const {
    std::vector<std::string> names;
    for (const auto& pair : saved_presets_) {
        names.push_back(pair.first);
    }
    return names;
}

// ============================================================================
// CHIP GUI INTEGRATION - SIMPLIFIED APPROACH
// ============================================================================

// No wrapper needed! Chip GUIs simply call GetGlobalChipConfig() directly:
//
// Example usage in any chip GUI rendering function:
//   ChipVisualization* viz = get_chip_visualization_instance();
//   viz->render(chip_center, pin_states, chip_name);
//   // ChipVisualization automatically uses global config - no setup needed!
//
// This is much simpler and more efficient than complex wrapper classes.

// ============================================================================
// HISTORICAL PRESETS
// ============================================================================

namespace HistoricalPresets {
    
void RegisterAllPresets() {
    auto& manager = GlobalChipStyleManager::getInstance();
    
    manager.savePreset("Intel 8080 (1974)");
    manager.savePreset("MOS 6502 (1975)");
    manager.savePreset("Zilog Z80 (1976)");
    manager.savePreset("Intel 8086 (1978)");
    manager.savePreset("Motorola 68000 (1980)");
    manager.savePreset("Generic 80s (1985)");
}

ChipVisualConfig Intel8080_1974() {
    ChipVisualConfig config = ChipVisualConfig::get_style(VisualStyle::DATASHEET_70S);
    config.datasheet_mode = DatasheetMode::EXTERNAL_LABELING;
    config.notation_style = PinNotationStyle::SLASH_PREFIX;  // Intel style
    config.labels_inside_package = false;
    config.numbers_inside_package = false;
    config.show_pin_groups = true;
    config.show_datasheet_grid = true;
    config.chip_border_width = 2.5f;  // Hand-drafted thick lines
    return config;
}

ChipVisualConfig MOS6502_1975() {
    ChipVisualConfig config = ChipVisualConfig::get_style(VisualStyle::DATASHEET_70S);
    config.datasheet_mode = DatasheetMode::EXTERNAL_LABELING;
    config.notation_style = PinNotationStyle::SLASH_PREFIX;
    config.labels_inside_package = false;
    config.numbers_inside_package = false;
    config.show_pin_groups = true;  // MOS used functional grouping
    return config;
}

ChipVisualConfig Motorola68000_1980() {
    ChipVisualConfig config = ChipVisualConfig::get_style(VisualStyle::DATASHEET_80S);
    config.datasheet_mode = DatasheetMode::TOP_VIEW_80S;
    config.notation_style = PinNotationStyle::HASH_SUFFIX;  // Motorola early adopter
    config.labels_inside_package = true;
    config.numbers_inside_package = true;
    config.show_pin_groups = false;  // Clean 80s look
    return config;
}

} // namespace HistoricalPresets

// ============================================================================
// INTEGRATION HELPERS
// ============================================================================

void RenderGlobalChipStyleMenu() {
    if (ImGui::BeginMenu("Chip Style")) {
        static bool show_config_dialog = false;
        
        if (ImGui::MenuItem("Style Configuration...")) {
            show_config_dialog = true;
        }
        
        ImGui::Separator();
        
        if (ImGui::BeginMenu("Quick Presets")) {
            if (ImGui::MenuItem("70s External Labeling")) {
                GLOBAL_CHIP_STYLE.applyPresetStyle(VisualStyle::DATASHEET_70S);
            }
            if (ImGui::MenuItem("80s TOP VIEW")) {
                GLOBAL_CHIP_STYLE.applyPresetStyle(VisualStyle::DATASHEET_80S);
            }
            if (ImGui::MenuItem("Modern Dark")) {
                GLOBAL_CHIP_STYLE.applyPresetStyle(VisualStyle::CLASSIC_DARK);
            }
            if (ImGui::MenuItem("High Contrast")) {
                GLOBAL_CHIP_STYLE.applyPresetStyle(VisualStyle::HIGH_CONTRAST);
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Datasheet Mode")) {
            if (ImGui::MenuItem("External Labeling (70s)")) {
                GLOBAL_CHIP_STYLE.applyDatasheetMode(DatasheetMode::EXTERNAL_LABELING);
            }
            if (ImGui::MenuItem("TOP VIEW (80s)")) {
                GLOBAL_CHIP_STYLE.applyDatasheetMode(DatasheetMode::TOP_VIEW_80S);
            }
            if (ImGui::MenuItem("BOTTOM VIEW (Rare)")) {
                GLOBAL_CHIP_STYLE.applyDatasheetMode(DatasheetMode::BOTTOM_VIEW);
            }
            if (ImGui::MenuItem("Functional Block")) {
                GLOBAL_CHIP_STYLE.applyDatasheetMode(DatasheetMode::FUNCTIONAL_BLOCK);
            }
            if (ImGui::MenuItem("Connection Diagram")) {
                GLOBAL_CHIP_STYLE.applyDatasheetMode(DatasheetMode::CONNECTION_DIAGRAM);
            }
            if (ImGui::MenuItem("Package Outline")) {
                GLOBAL_CHIP_STYLE.applyDatasheetMode(DatasheetMode::PACKAGE_OUTLINE);
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMenu();
        
        // Render dialog if needed
        if (show_config_dialog) {
            GLOBAL_CHIP_STYLE.renderConfigDialog(&show_config_dialog);
        }
    }
}

void RenderChipStyleDebugWindow(bool* show_window) {
    if (show_window && !*show_window) return;
    
    if (ImGui::Begin("Chip Style Debug", show_window)) {
        auto& manager = GlobalChipStyleManager::getInstance();
        
        ImGui::Text("Global Chip Style System");
        ImGui::Separator();
        
        ImGui::Text("Registered visualizations: %zu", manager.registered_visualizations_.size());
        
        if (ImGui::Button("Open Configuration Dialog")) {
            static bool show_config = true;
            manager.renderConfigDialog(&show_config);
        }
        
        ImGui::Separator();
        ImGui::Text("Current Configuration:");
        
        const auto& config = manager.getGlobalConfig();
        ImGui::Text("Style: %d", (int)config.style);
        ImGui::Text("Datasheet Mode: %d", (int)config.datasheet_mode);
        ImGui::Text("Labels Inside: %s", config.labels_inside_package ? "Yes" : "No");
        ImGui::Text("Numbers Inside: %s", config.numbers_inside_package ? "Yes" : "No");
        ImGui::Text("Show Pin Groups: %s", config.show_pin_groups ? "Yes" : "No");
        ImGui::Text("Show Grid: %s", config.show_datasheet_grid ? "Yes" : "No");
    }
    ImGui::End();
}

#endif // IMGUI_VERSION