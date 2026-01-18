#ifndef SYSTEM_SELECTION_DIALOG_H
#define SYSTEM_SELECTION_DIALOG_H

/**
 * SystemSelectionDialog - Modal dialog for selecting emulated systems
 * 
 * This dialog displays all registered systems from the SystemRegistry and
 * allows the user to select one. It's designed to be used within an existing
 * ImGui context (e.g., in SimpleSystemGUI).
 */
class SystemSelectionDialog {
private:
    bool is_open_;
    int selected_system_index_;
    const char* selected_system_name_;
    bool selection_confirmed_;
    
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
     * Reset the selection state
     */
    void reset();
};

#endif // SYSTEM_SELECTION_DIALOG_H
