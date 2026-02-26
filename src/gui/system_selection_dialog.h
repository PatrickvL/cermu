#pragma once

#include <map>
#include <string>

/**
 * SystemSelectionDialog - Modal dialog for selecting emulated systems
 *
 * This dialog displays all registered systems from the SystemRegistry and
 * allows the user to select one. It's designed to be used within an existing
 * ImGui context (e.g., in SystemGUI).
 */
class SystemSelectionDialog {
private:
    bool is_open_;
    int selected_system_index_;
    int selected_memory_option_;
    int selected_region_option_;
    const char* selected_system_name_;
    bool selection_confirmed_;
    
    // Selected peripherals (map of peripheral ID -> enabled state)
    std::map<std::string, bool> selected_peripherals_;
    
    // Selected custom options (map of option ID -> selected choice string)
    std::map<std::string, std::string> selected_custom_settings_;
    
    // Filtering options
    char search_filter_[256];
    bool search_descriptions_;  // Whether to also search in descriptions
    int region_filter_;  // 0 = all, 1 = NTSC, 2 = PAL, etc.
    
public:
    SystemSelectionDialog();
    
    /**
     * Open the dialog (should be called when you want to show it)
     */
    void open();
    
    /**
     * Close the dialog
     */
    void close();
    
    /**
     * Check if dialog is currently open
     */
    bool is_open() const { return is_open_; }
    
    /**
     * Render the dialog (call this every frame in your render loop)
     * @param allow_cancel If false, user must select a system (no cancel button)
     */
    void render(bool allow_cancel = true);
    
    /**
     * Check if a selection was confirmed (OK button pressed)
     */
    bool selection_confirmed() const { return selection_confirmed_; }
    
    /**
     * Get the selected system's short name (e.g., "C64", "CHIP8")
     * Returns nullptr if no selection was made
     */
    const char* get_selected_system() const { return selected_system_name_; }
    
    /**
     * Get the selected memory option index (-1 if none or not applicable)
     */
    int get_selected_memory_option() const { return selected_memory_option_; }
    
    /**
     * Get the selected region option index (-1 if none or not applicable)
     */
    int get_selected_region_option() const { return selected_region_option_; }
    
    /**
     * Get the selected peripherals map (peripheral ID -> enabled state)
     */
    const std::map<std::string, bool>& get_selected_peripherals() const { return selected_peripherals_; }
    
    /**
     * Get the selected custom settings (option ID -> selected choice)
     */
    const std::map<std::string, std::string>& get_selected_custom_settings() const { return selected_custom_settings_; }
    
    /**
     * Reset the selection state
     */
    void reset();
};

