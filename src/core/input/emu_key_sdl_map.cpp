#include "core/input/emu_key_sdl_map.hpp"
#include <SDL_scancode.h>
#include <SDL_keycode.h>
#include <cstring>
#include <cstdio>

// ============================================================================
// EmuKeySDLMap implementation
// ============================================================================

EmuKeySDLMap& EmuKeySDLMap::instance() {
    static EmuKeySDLMap singleton;
    return singleton;
}

EmuKeySDLMap::EmuKeySDLMap() {
    // Clear ASCII-to-scancode table
    memset(ascii_to_scancode_, 0xFF, sizeof(ascii_to_scancode_));

    // ========================================================================
    // Step 1: Identity-map ALL valid SDL scancodes (0–511).
    // Every SDL_SCANCODE_* value that represents a real key gets its bit set.
    // The EmuKey constants are defined to equal these values, so no explicit
    // registration is needed — the bitset IS the mapping.
    // ========================================================================

    // Set bits for all known SDL scancodes.
    // Rather than listing every single one, set the ranges that contain real keys:

    // Letters, digits, common keys (4–99)
    for (int i = 4; i <= 99; i++) identity_.set(i);

    // F13–F24, other keys (104–115)
    for (int i = 104; i <= 115; i++) identity_.set(i);

    // International / language keys (some scattered values)
    for (int i = 133; i <= 135; i++) identity_.set(i);  // INTERNATIONAL1-3

    // Additional keys (various scattered scancodes)
    identity_.set(118);  // MENU
    identity_.set(119);  // POWER
    identity_.set(120);  // KP_EQUALS

    // Volume / media (127–130)
    for (int i = 127; i <= 130; i++) identity_.set(i);

    // Modifiers (224–231)
    for (int i = 224; i <= 231; i++) identity_.set(i);

    // ========================================================================
    // Step 2: Build ASCII keycode → scancode table for sdl_keycode_to_emu_key.
    // SDL keycodes for printable characters are just their ASCII values.
    // We need to map them to the corresponding physical scancode.
    // ========================================================================

    // Letters: SDLK_a (='a'=97) → SDL_SCANCODE_A (=4), etc.
    for (int i = 0; i < 26; i++) {
        ascii_to_scancode_['a' + i] = SDL_SCANCODE_A + i;
    }

    // Digits: SDLK_0 (='0'=48) → SDL_SCANCODE_0 (=39)
    //         SDLK_1 (='1'=49) → SDL_SCANCODE_1 (=30), etc.
    ascii_to_scancode_['0'] = SDL_SCANCODE_0;
    for (int i = 1; i <= 9; i++) {
        ascii_to_scancode_['0' + i] = SDL_SCANCODE_1 + (i - 1);
    }

    // Punctuation / symbols
    ascii_to_scancode_[' ']  = SDL_SCANCODE_SPACE;
    ascii_to_scancode_['-']  = SDL_SCANCODE_MINUS;
    ascii_to_scancode_['=']  = SDL_SCANCODE_EQUALS;
    ascii_to_scancode_['[']  = SDL_SCANCODE_LEFTBRACKET;
    ascii_to_scancode_[']']  = SDL_SCANCODE_RIGHTBRACKET;
    ascii_to_scancode_['\\'] = SDL_SCANCODE_BACKSLASH;
    ascii_to_scancode_[';']  = SDL_SCANCODE_SEMICOLON;
    ascii_to_scancode_['\''] = SDL_SCANCODE_APOSTROPHE;
    ascii_to_scancode_['`']  = SDL_SCANCODE_GRAVE;
    ascii_to_scancode_[',']  = SDL_SCANCODE_COMMA;
    ascii_to_scancode_['.']  = SDL_SCANCODE_PERIOD;
    ascii_to_scancode_['/']  = SDL_SCANCODE_SLASH;

    // Non-printable keys whose SDL keycodes are bare ASCII values
    // (no SDLK_SCANCODE_MASK).  Without these, sdl_keycode_to_emu_key
    // returns EMUKEY_NONE and the keys are silently consumed.
    ascii_to_scancode_['\r'] = SDL_SCANCODE_RETURN;     // SDLK_RETURN = 13
    ascii_to_scancode_['\b'] = SDL_SCANCODE_BACKSPACE;  // SDLK_BACKSPACE = 8
    ascii_to_scancode_['\t'] = SDL_SCANCODE_TAB;        // SDLK_TAB = 9
    ascii_to_scancode_[0x1B] = SDL_SCANCODE_ESCAPE;     // SDLK_ESCAPE = 27
    ascii_to_scancode_[0x7F] = SDL_SCANCODE_DELETE;     // SDLK_DELETE = 127

    // ========================================================================
    // Step 3: Register default non-identity mappings.
    // Emulator-specific keys that should respond to a default host key.
    // ========================================================================
    register_mapping(EMUKEY_CBM_RESTORE, SDL_SCANCODE_GRAVE);  // backtick → RESTORE

    printf("EmuKeySDLMap: Initialised (%zu identity-mapped, %zu deviations)\n",
           identity_.count(), emu_to_sdl_.size());
}

// ============================================================================
// Conversion
// ============================================================================

emu_key_t EmuKeySDLMap::scancode_to_emu_key(SDL_Scancode_t scancode) const {
    if (scancode < EMUKEY_EMU_BASE && identity_[scancode]) {
        return (emu_key_t)scancode;
    }
    auto it = sdl_to_emu_.find(scancode);
    if (it != sdl_to_emu_.end()) {
        return it->second;
    }
    return EMUKEY_NONE;
}

SDL_Scancode_t EmuKeySDLMap::emu_key_to_scancode(emu_key_t key) const {
    if (key < EMUKEY_EMU_BASE && identity_[key]) {
        return key;
    }
    auto it = emu_to_sdl_.find(key);
    if (it != emu_to_sdl_.end()) {
        return it->second;
    }
    return 0xFFFFFFFF;
}

emu_key_t EmuKeySDLMap::sdl_keycode_to_emu_key(SDL_Keycode sdl_keycode) const {
    // If the keycode has the scancode mask, extract the scancode
    if (sdl_keycode & SDLK_SCANCODE_MASK) {
        SDL_Scancode_t scancode = sdl_keycode & ~SDLK_SCANCODE_MASK;
        return scancode_to_emu_key(scancode);
    }

    // Bare ASCII value (printable character keycodes: 'a'=97, '1'=49, etc.)
    if (sdl_keycode >= 0 && sdl_keycode < 128) {
        SDL_Scancode_t sc = ascii_to_scancode_[sdl_keycode];
        if (sc != 0xFFFFFFFF) {
            return scancode_to_emu_key(sc);
        }
    }

    return EMUKEY_NONE;
}

// ============================================================================
// Configuration
// ============================================================================

void EmuKeySDLMap::register_mapping(emu_key_t key, SDL_Scancode_t scancode) {
    emu_to_sdl_[key] = scancode;
    sdl_to_emu_[scancode] = key;
}

void EmuKeySDLMap::remove_mapping(emu_key_t key) {
    auto it = emu_to_sdl_.find(key);
    if (it != emu_to_sdl_.end()) {
        sdl_to_emu_.erase(it->second);
        emu_to_sdl_.erase(it);
    }
}

// ============================================================================
// Query
// ============================================================================

bool EmuKeySDLMap::is_identity_mapped(SDL_Scancode_t scancode) const {
    return scancode < EMUKEY_EMU_BASE && identity_[scancode];
}
