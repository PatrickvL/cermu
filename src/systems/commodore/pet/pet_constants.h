#pragma once
/*
 * pet_constants.h — Commodore PET System Constants
 *
 * Timing, memory map boundaries, and default configuration for all PET
 * models (2001, 3032, 4032, 8032, etc.).  Initial target: PET 4032
 * (BASIC 4.0, 40-column normal keyboard, 32KB RAM).
 */

#include <cstdint>

namespace pet_constants {

// ============================================================================
// CPU CLOCK — 1 MHz for all PET models
// ============================================================================
inline constexpr uint32_t CPU_FREQ_HZ           = 1000000;

// ============================================================================
// VIDEO TIMING (NTSC 60 Hz — PET was NTSC-only in early models)
// ============================================================================
inline constexpr double   TARGET_FPS_NTSC       = 60.0;
inline constexpr uint32_t CYCLES_PER_FRAME_NTSC = 16667;   // ~1 MHz / 60
inline constexpr double   TARGET_FPS_PAL        = 50.0;
inline constexpr uint32_t CYCLES_PER_FRAME_PAL  = 20000;   // ~1 MHz / 50

// ============================================================================
// DISPLAY
// ============================================================================
inline constexpr int SCREEN_COLS        = 40;       // 40-column models (default)
inline constexpr int SCREEN_ROWS        = 25;
inline constexpr int PET_CHAR_WIDTH    = 8;
inline constexpr int PET_CHAR_HEIGHT   = 8;
inline constexpr int DISPLAY_WIDTH      = SCREEN_COLS * PET_CHAR_WIDTH;   // 320
inline constexpr int DISPLAY_HEIGHT     = SCREEN_ROWS * PET_CHAR_HEIGHT;  // 200

// ============================================================================
// AUDIO
// ============================================================================
inline constexpr int AUDIO_SAMPLE_RATE  = 44100;

// ============================================================================
// MEMORY MAP BOUNDARIES
// ============================================================================
inline constexpr uint16_t RAM_START         = 0x0000;
inline constexpr uint16_t RAM_END_8K        = 0x2000;
inline constexpr uint16_t RAM_END_16K       = 0x4000;
inline constexpr uint16_t RAM_END_32K       = 0x8000;

inline constexpr uint16_t SCREEN_RAM_START  = 0x8000;   // Screen RAM (1000 bytes)
inline constexpr uint16_t SCREEN_RAM_END    = 0x8400;   // $8000-$83E7 used for 40×25

inline constexpr uint16_t EXPANSION_ROM_A   = 0xA000;   // $A000-$AFFF expansion ROM
// Note: $B000-$BFFF is BASIC ROM on PET 4032 (BASIC 4.0), expansion ROM on earlier models

inline constexpr uint16_t BASIC_ROM_START   = 0xB000;   // $B000-$DFFF BASIC 4.0 ROM (12KB)
inline constexpr uint16_t BASIC_ROM_END     = 0xE000;

inline constexpr uint16_t EDITOR_ROM_START  = 0xE000;   // $E000-$E7FF Editor ROM (2KB)
inline constexpr uint16_t EDITOR_ROM_END    = 0xE800;

// I/O region $E800-$E8FF (mirrors within this page)
inline constexpr uint16_t IO_START          = 0xE800;
inline constexpr uint16_t IO_END            = 0xE900;

// I/O chip base addresses (within the I/O page)
inline constexpr uint16_t CRTC_BASE         = 0xE880;   // MC6845 CRTC: $E880-$E881
inline constexpr uint16_t PIA1_BASE         = 0xE810;   // PIA 1 (keyboard): $E810-$E813
inline constexpr uint16_t PIA2_BASE         = 0xE820;   // PIA 2 (IEEE-488): $E820-$E823
inline constexpr uint16_t VIA_BASE          = 0xE840;   // VIA (user port/timers): $E840-$E84F

inline constexpr uint16_t KERNAL_ROM_START  = 0xF000;   // $F000-$FFFF Kernal ROM (4KB)
// KERNAL_ROM_END: use 0x0000 since uint16_t wraps — check with >= KERNAL_ROM_START instead

// ============================================================================
// KERNAL ADDRESSES (for deferred loading detection)
// ============================================================================
inline constexpr uint16_t BASIC_START_ADDR  = 0x0401;    // Default BASIC start (unexpanded)
inline constexpr uint16_t KBD_BUFFER        = 0x026F;    // KERNAL keyboard buffer
inline constexpr uint16_t KBD_BUFFER_SIZE   = 10;        // Max 10 characters
inline constexpr uint16_t KBD_BUFFER_COUNT  = 0x009E;    // Number of chars in keyboard buffer
inline constexpr uint16_t BASIC_WARMSTART   = 0x0302;    // BASIC warm start vector ($0302-$0303)

// ============================================================================
// KEYBOARD
// ============================================================================
inline constexpr int KEYBOARD_ROWS          = 10;        // PET keyboard: 10 rows
inline constexpr int KEYBOARD_COLS          = 8;         // 8 columns

// ============================================================================
// PET MODEL VARIANTS
// ============================================================================
enum class PetModel : uint8_t {
    PET_2001_8N = 0,    // PET 2001-8N (8KB, BASIC 1.0, normal keyboard)
    PET_3032,           // PET 3032 (32KB, BASIC 2.0, normal keyboard)
    PET_4032,           // PET 4032 (32KB, BASIC 4.0, normal keyboard) [DEFAULT]
    PET_8032,           // PET 8032 (32KB, BASIC 4.0, 80-column)
};

} // namespace pet_constants
