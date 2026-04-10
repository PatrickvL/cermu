#pragma once

#include <cstdint>

#include <SDL_keycode.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

#include "chip/input/commodore_keyboard.hpp"

// ============================================================================
// Layered Keyboard Mapping System
// ============================================================================
//
// Implements a three-layer mapping approach between host (SDL) keyboard
// input and guest (emulated system) keyboard matrix state:
//
// ┌──────────────────────────────────────────────────────────────────────┐
// │ Layer 3: Emulator Modifier (Right Alt + key → guest-specific keys)  │
// │   RESTORE, RUN/STOP, C= key, CTRL, £, ←, ↑, π — keys that have    │
// │   no natural home on the host keyboard.                             │
// ├──────────────────────────────────────────────────────────────────────┤
// │ Layer 2: Direct Key Mapping (non-printable keys)                    │
// │   Return, Delete/Backspace, cursor keys, F-keys, Tab, Escape,      │
// │   Shift, Ctrl — mapped by keycode, not by character.                │
// ├──────────────────────────────────────────────────────────────────────┤
// │ Layer 1: Character-Based Mapping (printable characters)             │
// │   Uses SDL_TEXTINPUT to determine what character the user typed     │
// │   on their host layout, then reverse-maps to the guest key combo   │
// │   that produces the same character. Layout-independent.             │
// └──────────────────────────────────────────────────────────────────────┘
//
// For printable characters, the mapping is:
//   1. User presses keys on host keyboard
//   2. SDL_TEXTINPUT tells us the resulting character (e.g., '"')
//   3. We look up which guest matrix position + shift state produces '"'
//   4. We inject that exact matrix state into the guest keyboard
//
// This means a Dutch user pressing Shift+' to get " and a US user
// pressing Shift+2 both result in the same guest matrix state — the
// C64's Shift+2 which produces ".
//
// For non-printable keys, direct keycode mapping is used (cursor keys,
// Return, F-keys, etc.) since there's no "character" involved.
//
// The emulator modifier key (default: Right Alt) is reserved as a
// namespace for guest-specific keys that have no host equivalent.

// ============================================================================
// Guest key action — what to inject into the guest keyboard matrix
// ============================================================================

struct GuestKeyAction {
    uint8_t row;               // Matrix row (array index, not hardware bit)
    uint8_t col;               // Matrix column (array index, not hardware bit)
    uint8_t modifiers;         // Required modifier bitmask (KEYMOD_SHIFT, KEYMOD_CBM, etc.)
                               // 0 = no modifiers needed
    bool valid;                // Whether this is a valid mapping

    GuestKeyAction()
        : row(0), col(0), modifiers(KEYMOD_NONE), valid(false) {}

    GuestKeyAction(uint8_t r, uint8_t c, uint8_t mods)
        : row(r), col(c), modifiers(mods), valid(true) {}
};

// ============================================================================
// Active injection — tracks what we injected so we can release properly
// ============================================================================

struct ActiveInjection {
    GuestKeyAction action;         // What we injected
    uint8_t forced_modifiers;      // Modifiers we pressed that weren't physically held
    uint8_t suppressed_modifiers;  // Modifiers we released that were physically held
    SDL_Scancode host_scancode;    // The physical host key that triggered this
    bool from_text_input;          // Was this triggered by SDL_TEXTINPUT?
    bool is_restore;               // RESTORE (NMI) — no matrix contact, just a flag

    ActiveInjection()
        : forced_modifiers(0), suppressed_modifiers(0),
          host_scancode(SDL_SCANCODE_UNKNOWN), from_text_input(false),
          is_restore(false) {}
};

// ============================================================================
// Synthetic key mapping — emulator modifier + key → guest action
// ============================================================================

struct SyntheticKeyMapping {
    SDL_Keycode trigger_key;       // Host key to press with emulator modifier
    GuestKeyAction action;         // Guest matrix action
    const char* description;       // Human-readable name (e.g., "RESTORE")
};

// ============================================================================
// KeyboardMapper — the central mapping engine
// ============================================================================

class KeyboardMapper {
public:
    KeyboardMapper();
    ~KeyboardMapper();

    // ========================================================================
    // Configuration — called once per system during initialization
    // ========================================================================

    // Set the guest keyboard this mapper controls
    void set_guest_keyboard(commodore_keyboard_t* keyboard);

    // Build character map from the keyboard matrix configuration.
    // Auto-derives character mappings from SDL_Keycode identity for
    // printable keys, then applies character overrides for keys where
    // the guest output differs from the host.
    void build_character_map_from_matrix(const keyboard_matrix_config_t* config);

    // Set the emulator modifier key (default: Right Alt)
    void set_emulator_modifier(SDL_Keycode key);

    // Add a synthetic key mapping (emulator modifier + trigger → guest action)
    void add_synthetic_mapping(SDL_Keycode trigger, GuestKeyAction action, const char* description);

    // Add a direct key mapping override (host keycode → guest action).
    // These take priority over the character map for specific keys.
    void add_direct_mapping(SDL_Keycode host_key, GuestKeyAction action);

    // Add a manual character mapping entry (ASCII char → guest action).
    // Use this for characters that can't be discovered from the matrix tables
    // (e.g., characters corresponding to keys with value 0 in the matrix).
    void add_char_mapping(char c, GuestKeyAction action);

    // Register default synthetic mappings for the current keyboard model.
    // Call after build_character_map_from_matrix().
    void register_default_synthetic_mappings();

    // Register a key redirect: when host_key arrives from SDL, the mapper
    // treats it as guest_key for matrix lookup.  Used for keys that exist
    // on the guest but map to different host keys (e.g., SDLK_BACKQUOTE
    // → CERMU_KEY_CBM_ARROW_LEFT on C64).
    void register_key_redirect(SDL_Keycode host_key, SDL_Keycode guest_key);

    // ========================================================================
    // Event processing — called from the GUI event loop
    // ========================================================================

    // Process SDL_KEYDOWN event. Returns true if the event was consumed.
    // Parameters:
    //   sym       - SDL keycode (event.key.keysym.sym)
    //   scancode  - SDL scancode (event.key.keysym.scancode)
    //   mod       - SDL modifier state (event.key.keysym.mod)
    //   repeat    - Whether this is a key repeat event
    bool process_key_down(SDL_Keycode sym, SDL_Scancode scancode, uint16_t mod, bool repeat);

    // Process SDL_KEYUP event. Returns true if the event was consumed.
    bool process_key_up(SDL_Keycode sym, SDL_Scancode scancode, uint16_t mod);

    // Process SDL_TEXTINPUT event. Returns true if the event was consumed.
    // This is the primary path for printable character mapping.
    bool process_text_input(const char* text);

    // ========================================================================
    // State management
    // ========================================================================

    // Release all currently-injected keys (e.g., on focus loss)
    void release_all();

    // Reset all state (mappings are preserved)
    void reset_state();

    // ========================================================================
    // Queries
    // ========================================================================

    // Check if the emulator modifier is currently held
    bool is_emulator_modifier_held() const { return emu_modifier_held_; }

    // Get the emulator modifier key
    SDL_Keycode get_emulator_modifier() const { return emu_modifier_key_; }

    // Look up a character in the character map
    GuestKeyAction lookup_character(char c) const;

    // Get all synthetic mappings (for UI display)
    const std::vector<SyntheticKeyMapping>& get_synthetic_mappings() const { return synthetic_mappings_; }

    // Get the keyboard model this mapper is configured for
    keyboard_model_t get_model() const { return model_; }

    // Check if text input mode is active (character-based mapping enabled)
    bool is_text_input_enabled() const { return text_input_enabled_; }

    // Enable/disable text input mode
    void set_text_input_enabled(bool enabled) { text_input_enabled_ = enabled; }

private:
    // ========================================================================
    // Internal helpers
    // ========================================================================

    // Inject a guest key action into the matrix (press)
    void inject_press(const GuestKeyAction& action, SDL_Scancode host_scancode, bool from_text);

    // Release a previously injected guest key action
    void release_injection(const ActiveInjection& injection);

    // Find the matrix position of the shift key for this keyboard model
    GuestKeyAction find_shift_key_position() const;

    // Close/open a specific matrix contact
    void close_contact(uint8_t row, uint8_t col);
    void open_contact(uint8_t row, uint8_t col);

    // Check if a scancode represents a printable key that should use text input
    bool is_printable_key(SDL_Keycode sym) const;

    // Check if a scancode is a modifier key
    bool is_modifier_key(SDL_Keycode sym) const;

    // Check if a key has a direct mapping
    bool has_direct_mapping(SDL_Keycode sym) const;

    // Resolve a key through the redirect map.  Returns the guest key if
    // a redirect is registered, otherwise returns the key unchanged.
    SDL_Keycode resolve_redirect(SDL_Keycode key) const {
        auto it = key_redirects_.find(key);
        return (it != key_redirects_.end()) ? it->second : key;
    }

    // ========================================================================
    // State
    // ========================================================================

    commodore_keyboard_t* keyboard_;           // Guest keyboard we control
    keyboard_model_t model_;                   // Guest keyboard model

    // Layer 1: Character → guest key combo (reverse lookup from matrix)
    // Maps ASCII characters (0–127) to guest key actions
    GuestKeyAction char_map_[128];

    // Layer 2: Direct key mappings (host keycode → guest action)
    std::unordered_map<SDL_Keycode, GuestKeyAction> direct_map_;

    // Key redirects: host SDLK_ → guest key code (SDL_Keycode or CERMU_KEY_*).
    // Applied before matrix lookup for keys with no direct host equivalent.
    std::unordered_map<SDL_Keycode, SDL_Keycode> key_redirects_;

    // Layer 3: Emulator modifier
    SDL_Keycode emu_modifier_key_;             // The emulator modifier key (default: SDLK_RALT)
    bool emu_modifier_held_;                   // Is the emulator modifier currently held?
    std::unordered_map<SDL_Keycode, SyntheticKeyMapping> synthetic_map_;
    std::vector<SyntheticKeyMapping> synthetic_mappings_;  // For UI enumeration

    // Active injection tracking: host scancode → what we injected
    std::unordered_map<int, ActiveInjection> active_injections_;

    // Physical host modifier tracking
    bool host_lshift_held_;                    // Is host LEFT shift physically held?
    bool host_rshift_held_;                    // Is host RIGHT shift physically held?
    bool host_ctrl_held_;                      // Is host ctrl physically held?
    bool host_cbm_held_;                       // Is host Commodore (LGUI) key physically held?

    // Convenience: true if either host shift is held
    bool host_shift_held() const { return host_lshift_held_ || host_rshift_held_; }

    // Modifier key guest matrix positions (cached at init)
    GuestKeyAction shift_left_pos_;
    GuestKeyAction shift_right_pos_;
    GuestKeyAction cbm_key_pos_;               // Commodore (C=) key matrix position
    GuestKeyAction ctrl_key_pos_;              // CTRL key matrix position

    // Text input enabled (character-based mapping active)
    bool text_input_enabled_;

    // Track keys waiting for text input (printable keys that haven't
    // received a TEXTINPUT event yet)
    struct PendingKey {
        SDL_Keycode sym;
        SDL_Scancode scancode;
        uint16_t mod;
    };
    PendingKey pending_key_;
    bool has_pending_key_;

    // Matrix dimensions (cached from keyboard)
    uint8_t matrix_rows_;
    uint8_t matrix_cols_;
};


