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
inline constexpr uint32_t CYCLES_PER_FRAME_NTSC = 16667;   // ~1 MHz / 60
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
inline constexpr uint16_t RAM_END_32K       = 0x8000;

inline constexpr uint16_t SCREEN_RAM_START  = 0x8000;   // Screen RAM (1000 bytes)
inline constexpr uint16_t SCREEN_RAM_END    = 0x8400;   // $8000-$83E7 used for 40×25

// I/O region $E800-$E8FF (mirrors within this page)
inline constexpr uint16_t IO_START          = 0xE800;
inline constexpr uint16_t IO_END            = 0xE900;

// I/O chip base addresses (within the I/O page)
inline constexpr uint16_t CRTC_BASE         = 0xE880;   // MC6845 CRTC: $E880-$E881
inline constexpr uint16_t PIA1_BASE         = 0xE810;   // PIA 1 (keyboard): $E810-$E813
inline constexpr uint16_t PIA2_BASE         = 0xE820;   // PIA 2 (IEEE-488): $E820-$E823
inline constexpr uint16_t VIA_BASE          = 0xE840;   // VIA (user port/timers): $E840-$E84F

// ============================================================================
// KERNAL ADDRESSES (for deferred loading detection)
// ============================================================================
inline constexpr uint16_t KBD_BUFFER        = 0x026F;    // KERNAL keyboard buffer
inline constexpr uint16_t KBD_BUFFER_SIZE   = 10;        // Max 10 characters
inline constexpr uint16_t KBD_BUFFER_COUNT  = 0x009E;    // Number of chars in keyboard buffer
// ============================================================================
// PALETTE (indexed rendering: black + green phosphor)
// ============================================================================
inline constexpr uint32_t PALETTE[2] = {
    0xFF000000,  // 0: Black
    0xFF33FF33,  // 1: Green phosphor
};

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
