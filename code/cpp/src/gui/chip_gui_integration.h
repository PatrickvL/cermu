/*
 * chip_gui_integration.h - Integration helpers for existing chip GUIs
 * 
 * Provides easy integration of the global chip style system with existing
 * chip-specific GUI implementations (MOS6502, MOS6526, MOS6567, etc.)
 */

#pragma once

#ifndef IMGUI_VERSION
#error "chip_gui_integration.h requires IMGUI_VERSION to be defined. This header should only be included in GUI builds."
#endif

#include "global_chip_style.h"
#include "../core/emulation_context.h"

// ============================================================================
// CHIP GUI INTEGRATION MACROS
// ============================================================================

// Add to existing chip GUI classes to integrate with global style system
#define INTEGRATE_GLOBAL_CHIP_STYLE(chip_class_name) \
private: \
    std::unique_ptr<AutoUpdatingChipVisualization> integrated_viz_; \
    std::string chip_instance_id_; \
public: \
    void init_global_style_integration(const std::string& instance_id, const ChipLayout& layout) { \
        chip_instance_id_ = instance_id; \
        integrated_viz_ = std::make_unique<AutoUpdatingChipVisualization>(instance_id, layout); \
    } \
    void render_with_global_style(ImVec2 center, const std::vector<PinSignalState>& states, const char* name = nullptr) { \
        if (integrated_viz_) integrated_viz_->render(center, states, name); \
    } \
    AutoUpdatingChipVisualization* get_visualization() { return integrated_viz_.get(); }

// ============================================================================
// EXISTING CHIP GUI WRAPPERS
// ============================================================================

// Forward declarations for existing chip types
struct mos6502_t;
struct mos6526_t;
struct mos6567_t;
struct mos6569_t;
struct mos6581_t;

// Wrapper for MOS 6502 CPU GUI integration
class GlobalStyle_MOS6502_GUI {
public:
    GlobalStyle_MOS6502_GUI(mos6502_t* cpu_instance, const std::string& instance_id = "cpu_6502");
    ~GlobalStyle_MOS6502_GUI() = default;
    
    void render_debug_window(bool* show_window, emulation_context_t* context);
    void render_chip_visualization(ImVec2 center, emulation_context_t* context);
    
    // Access to underlying visualization for advanced customization
    AutoUpdatingChipVisualization* get_visualization() { return viz_.get(); }
    
private:
    mos6502_t* cpu_;
    std::string instance_id_;
    std::unique_ptr<AutoUpdatingChipVisualization> viz_;
    ChipLayout layout_;
    
    void update_pin_states(std::vector<PinSignalState>& states, emulation_context_t* context);
    ChipLayout create_6502_layout();
};

// Wrapper for MOS 6526 CIA GUI integration
class GlobalStyle_MOS6526_GUI {
public:
    GlobalStyle_MOS6526_GUI(mos6526_t* cia_instance, const std::string& instance_id);
    ~GlobalStyle_MOS6526_GUI() = default;
    
    void render_debug_window(bool* show_window, emulation_context_t* context);
    void render_chip_visualization(ImVec2 center, emulation_context_t* context);
    
    AutoUpdatingChipVisualization* get_visualization() { return viz_.get(); }
    
private:
    mos6526_t* cia_;
    std::string instance_id_;
    std::unique_ptr<AutoUpdatingChipVisualization> viz_;
    ChipLayout layout_;
    
    void update_pin_states(std::vector<PinSignalState>& states, emulation_context_t* context);
    ChipLayout create_6526_layout();
};

// Wrapper for MOS 6567/6569 VIC-II GUI integration
class GlobalStyle_VICII_GUI {
public:
    GlobalStyle_VICII_GUI(void* vic_instance, bool is_6569, const std::string& instance_id);
    ~GlobalStyle_VICII_GUI() = default;
    
    void render_debug_window(bool* show_window, emulation_context_t* context);
    void render_chip_visualization(ImVec2 center, emulation_context_t* context);
    
    AutoUpdatingChipVisualization* get_visualization() { return viz_.get(); }
    
private:
    void* vic_;
    bool is_6569_;
    std::string instance_id_;
    std::unique_ptr<AutoUpdatingChipVisualization> viz_;
    ChipLayout layout_;
    
    void update_pin_states(std::vector<PinSignalState>& states, emulation_context_t* context);
    ChipLayout create_vicii_layout();
};

// Wrapper for MOS 6581 SID GUI integration
class GlobalStyle_MOS6581_GUI {
public:
    GlobalStyle_MOS6581_GUI(mos6581_t* sid_instance, const std::string& instance_id = "sid_6581");
    ~GlobalStyle_MOS6581_GUI() = default;
    
    void render_debug_window(bool* show_window, emulation_context_t* context);
    void render_chip_visualization(ImVec2 center, emulation_context_t* context);
    
    AutoUpdatingChipVisualization* get_visualization() { return viz_.get(); }
    
private:
    mos6581_t* sid_;
    std::string instance_id_;
    std::unique_ptr<AutoUpdatingChipVisualization> viz_;
    ChipLayout layout_;
    
    void update_pin_states(std::vector<PinSignalState>& states, emulation_context_t* context);
    ChipLayout create_6581_layout();
};

// ============================================================================
// SYSTEM-WIDE INTEGRATION
// ============================================================================

// Manager for all chip visualizations in a system (e.g., C64)
class SystemChipVisualizationManager {
public:
    SystemChipVisualizationManager(const std::string& system_name);
    ~SystemChipVisualizationManager() = default;
    
    // Register chips from a system
    void register_cpu(mos6502_t* cpu, const std::string& id = "cpu");
    void register_cia(mos6526_t* cia, const std::string& id);
    void register_vic(void* vic, bool is_6569, const std::string& id = "vic");
    void register_sid(mos6581_t* sid, const std::string& id = "sid");
    
    // Render all chips
    void render_system_overview(emulation_context_t* context);
    void render_individual_debug_windows(emulation_context_t* context);
    
    // Style management
    void apply_historical_style(const std::string& style_name);
    void show_global_style_menu();
    
    // Access to individual chip visualizations
    GlobalStyle_MOS6502_GUI* get_cpu_gui() { return cpu_gui_.get(); }
    GlobalStyle_MOS6526_GUI* get_cia_gui(const std::string& id);
    GlobalStyle_VICII_GUI* get_vic_gui() { return vic_gui_.get(); }
    GlobalStyle_MOS6581_GUI* get_sid_gui() { return sid_gui_.get(); }
    
private:
    std::string system_name_;
    
    std::unique_ptr<GlobalStyle_MOS6502_GUI> cpu_gui_;
    std::map<std::string, std::unique_ptr<GlobalStyle_MOS6526_GUI>> cia_guis_;
    std::unique_ptr<GlobalStyle_VICII_GUI> vic_gui_;
    std::unique_ptr<GlobalStyle_MOS6581_GUI> sid_gui_;
    
    // Window state
    bool show_cpu_debug_ = false;
    bool show_cia1_debug_ = false;
    bool show_cia2_debug_ = false;
    bool show_vic_debug_ = false;
    bool show_sid_debug_ = false;
    bool show_system_overview_ = false;
    bool show_style_config_ = false;
};

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

// Quick integration for existing chip debug windows
void integrate_chip_debug_window(const char* window_title, 
                                bool* show_window,
                                AutoUpdatingChipVisualization* viz,
                                const std::vector<PinSignalState>& pin_states,
                                const char* chip_name,
                                std::function<void()> render_chip_specific_content = nullptr);

// Add global style menu to existing menu bars
void add_global_style_to_menubar();

// Render style selection combo for individual chip windows
bool render_style_selection_combo(const char* label = "Style");

// Quick historical style application
void apply_intel_8080_style();
void apply_mos_6502_style();
void apply_motorola_68000_style();
void apply_modern_style();

// ============================================================================
// MIGRATION HELPERS
// ============================================================================

// Helper to convert existing chip GUI classes to use global style system
template<typename ChipGUIClass>
class GlobalStyleMigrationWrapper {
public:
    GlobalStyleMigrationWrapper(ChipGUIClass* original_gui, 
                               const std::string& chip_id,
                               const ChipLayout& layout) 
        : original_gui_(original_gui), chip_id_(chip_id) {
        
        // Create auto-updating visualization
        global_viz_ = std::make_unique<AutoUpdatingChipVisualization>(chip_id, layout);
    }
    
    // Forward calls to original GUI but use global style for visualization
    void render_debug_window(bool* show_window, emulation_context_t* context) {
        if (show_window && !*show_window) return;
        
        if (ImGui::Begin(("Debug: " + chip_id_).c_str(), show_window)) {
            // Render chip visualization with global style
            ImVec2 viz_size = global_viz_->get_recommended_size();
            ImVec2 center = {viz_size.x / 2, viz_size.y / 2};
            
            // Get pin states from original GUI (implementation specific)
            std::vector<PinSignalState> pin_states = get_pin_states_from_original(context);
            global_viz_->render(center, pin_states, chip_id_.c_str());
            
            ImGui::Separator();
            
            // Render original GUI content
            if (original_gui_) {
                render_original_content(context);
            }
            
            // Add style controls
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Style Controls")) {
                render_style_selection_combo();
                if (ImGui::Button("Open Global Style Config")) {
                    static bool show_config = true;
                    GLOBAL_CHIP_STYLE.renderConfigDialog(&show_config);
                }
            }
        }
        ImGui::End();
    }
    
private:
    ChipGUIClass* original_gui_;
    std::string chip_id_;
    std::unique_ptr<AutoUpdatingChipVisualization> global_viz_;
    
    virtual std::vector<PinSignalState> get_pin_states_from_original(emulation_context_t* context) = 0;
    virtual void render_original_content(emulation_context_t* context) = 0;
};

// ============================================================================
// USAGE EXAMPLES IN COMMENTS
// ============================================================================

/*
// Example usage in existing C64 main GUI:

void c64_main_gui() {
    static SystemChipVisualizationManager c64_viz("C64");
    static bool initialized = false;
    
    if (!initialized) {
        // Register all C64 chips with global style system
        c64_viz.register_cpu(&c64.cpu, "6510");
        c64_viz.register_cia(&c64.cia1, "cia1");  
        c64_viz.register_cia(&c64.cia2, "cia2");
        c64_viz.register_vic(&c64.vic, false, "6567");
        c64_viz.register_sid(&c64.sid, "6581");
        
        // Apply historical style
        c64_viz.apply_historical_style("MOS 6502 (1975)");
        initialized = true;
    }
    
    // Main menu with style options
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("System Overview")) {
                // Show all chips in one window
            }
            ImGui::EndMenu();
        }
        
        // Add global chip style menu
        c64_viz.show_global_style_menu();
        
        ImGui::EndMainMenuBar();
    }
    
    // Render individual debug windows
    c64_viz.render_individual_debug_windows(&context);
}

// Example usage in existing MOS6502 GUI:

void mos6502_gui_render(mos6502_t* cpu, bool* show_window) {
    static GlobalStyle_MOS6502_GUI integrated_gui(cpu, "main_6502");
    integrated_gui.render_debug_window(show_window, &emulation_context);
}
*/