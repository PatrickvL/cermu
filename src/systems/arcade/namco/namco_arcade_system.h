#pragma once
/*
 * namco_arcade_system.h — Namco Pac-Man / Pengo arcade system declaration
 *
 * Templated on NamcoGame enum — Pac-Man and Pengo share the same board
 * with minor ROM layout and memory map differences.
 */


#include "namco_arcade_constants.h"
#include "../../../core/emulated_system.h"
#include "../../../core/system_lines.h"
#include "../../../chip/cpu/z80/zilog_z80a.h"
#include "../../../chip/sound/namco_wsg.h"
#include <cstdint>
#include <vector>

#define NAMCO_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// ── Game variants ────────────────────────────────────────────────────────
enum class NamcoGame { PacMan, Pengo };

template<NamcoGame G> struct NamcoGameTraits;

template<> struct NamcoGameTraits<NamcoGame::PacMan> {
    static constexpr const char* name            = "Pac-Man";
    static constexpr const char* short_name      = "PacMan";
    static constexpr const char* description     = "Namco Pac-Man — Z80A @ 3.072MHz, WSG3 sound, 224×288 (1980)";
    static constexpr uint32_t    rom_size        = namco_arcade_constants::PACMAN_ROM_SIZE;
    static constexpr uint32_t    char_rom_size   = namco_arcade_constants::PACMAN_CHAR_ROM_SIZE;
    static constexpr bool        encrypted_roms  = false;
    static constexpr int         wsg_voices      = 3;
};

template<> struct NamcoGameTraits<NamcoGame::Pengo> {
    static constexpr const char* name            = "Pengo";
    static constexpr const char* short_name      = "Pengo";
    static constexpr const char* description     = "Sega/Coreland Pengo — Z80A @ 3.072MHz, WSG3 sound, 224×288 (1982)";
    static constexpr uint32_t    rom_size        = namco_arcade_constants::PENGO_ROM_SIZE;
    static constexpr uint32_t    char_rom_size   = namco_arcade_constants::PENGO_CHAR_ROM_SIZE;
    static constexpr bool        encrypted_roms  = true;   // Sega encryption on some ROMs
    static constexpr int         wsg_voices      = 3;
};

// ── System ───────────────────────────────────────────────────────────────
template<NamcoGame G>
class NamcoArcadeSystem : public EmulatedSystem {
    using Traits = NamcoGameTraits<G>;
public:
    NamcoArcadeSystem();
    ~NamcoArcadeSystem() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;

    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

private:
    // ── Chips ────────────────────────────────────────────────────────────
    ZilogZ80A*       cpu_ = nullptr;     // Z80A @ 3.072 MHz
    namco_wsg_t      wsg_;               // Namco WSG3 wavetable sound

    // ── Memory ───────────────────────────────────────────────────────────
    std::vector<uint8_t> rom_;           // Program ROM (16 KB or 32 KB)
    std::vector<uint8_t> ram_;           // 1 KB work RAM
    std::vector<uint8_t> video_ram_;     // 1 KB tilemap
    std::vector<uint8_t> color_ram_;     // 1 KB color attributes

    // ── Graphics ROM ─────────────────────────────────────────────────────
    std::vector<uint8_t> char_rom_;      // Character/tile ROM (4 or 8 KB)
    std::vector<uint8_t> sprite_rom_;    // Sprite ROM (4 KB)
    std::vector<uint8_t> palette_prom_;  // 32 bytes palette
    std::vector<uint8_t> colortable_prom_; // 256 bytes color table
    std::vector<uint8_t> waveform_rom_;  // 256 bytes waveform data for WSG

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[namco_arcade_constants::FB_WIDTH *
                          namco_arcade_constants::FB_HEIGHT] = {};

    // ── I/O state ────────────────────────────────────────────────────────
    uint8_t in0_        = 0xFF;          // Input port 0 (P1 + coins)
    uint8_t in1_        = 0xFF;          // Input port 1 (P2 + start)
    uint8_t dsw1_       = 0xFF;          // DIP switches
    bool    int_enable_ = false;         // VBLANK interrupt enable
    bool    sound_enable_ = false;       // Sound output enable
    bool    flip_screen_ = false;        // Cocktail flip

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = NAMCO_BUS_DEFAULT_STATE;
    uint32_t scanline_      = 0;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = namco_arcade_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t mem_tick(bus_state_t pins);
    bool load_roms();
};
