#pragma once

#include "../../core/emulated_system.h"
#include "../../core/system_lines.h"
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/io/mos6522.h"
#include "../../chip/video/mc6845/mc6845.h"
#include "../../chip/sound/sn76489/sn76489.h"
#include "../../chip/memory/memory_chip.h"
#include <cstdint>
#include <memory>

// BBC Micro bus is derived from 6502 — shares address, data, and control signals.
#define BBC_BUS_DEFAULT_STATE (MOS6502::default_bus_state())

/**
 * BBC Micro Model B System Implementation
 *
 * Acorn BBC Micro Model B (1981):
 *   - MOS 6502A CPU @ 2 MHz
 *   - 32 KB RAM
 *   - 16 KB MOS (OS) ROM + up to 16 sideways ROM slots (16 KB each)
 *   - MC6845 CRTC for display timing
 *   - SN76489 sound chip (3 tone + 1 noise)
 *   - 2x MOS 6522 VIA (System VIA + User VIA)
 *   - Video ULA for pixel generation
 *   - SHEILA I/O page at $FE00-$FEFF
 *
 * Display modes: text (Mode 7 teletext — 40×25) and bitmap (Modes 0-6).
 * Initial implementation: Mode 7 text + bitmap Modes 0-6 for booting the MOS.
 */
class BBCMicroSystem : public EmulatedSystem {
public:
    BBCMicroSystem();
    ~BBCMicroSystem() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;

    // Execution
    void tick() override;
    void run_frame() override;

    // File loading
    bool load_file(const char* filepath) override;

    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Speed control
    void set_speed_multiplier(float multiplier) override;

private:
    // Chip instances
    MOS6502*    cpu_ = nullptr;
    mc6845_t*   crtc_ = nullptr;
    sn76489_t*  psg_ = nullptr;
    mos6522_t   system_via_;        // System VIA ($FE40-$FE5F)
    mos6522_t   user_via_;          // User VIA ($FE60-$FE7F)

    // Memory
    uint8_t*    memory_ = nullptr;       // 64 KB flat address space (for fast dispatch)
    uint8_t*    os_rom_ = nullptr;       // 16 KB MOS ROM data
    uint8_t*    paged_rom_[16]{};        // Up to 16 sideways ROM slots (16 KB each)
    uint8_t     rom_select_ = 0;         // Currently selected paged ROM bank

    // Memory chips (for Hardware debug menu — own the ROM data)
    MemoryChip* ram_chip_ = nullptr;
    MemoryChip* os_rom_chip_ = nullptr;
    MemoryChip* basic_rom_chip_ = nullptr;

    // Video ULA state
    uint8_t     video_ula_control_ = 0;  // $FE20 control register
    uint8_t     video_ula_palette_[16]{}; // Logical→physical color mapping

    // Display state
    uint32_t    screen_pixel_x_ = 0;
    uint32_t    screen_pixel_y_ = 0;

    // Keyboard matrix (10 columns × 8 rows)
    // Each element: true = key pressed
    bool        key_matrix_[10][8]{};
    bool        any_key_pressed_ = false;

    // Addressable latch (accent accent accent accent accent accent accent)
    // Written via System VIA PB0-PB3.  8 bits:
    //   D0: sound chip /WE
    //   D1: speech processor /RS and /WS
    //   D2: speech processor enable
    //   D3: keyboard auto-scan enable
    //   D4: shift lock LED
    //   D5: caps lock LED
    //   D6: not used
    //   D7: not used
    uint8_t     addressable_latch_ = 0;

    // System state
    bus_state_t pins_;
    uint32_t    cycles_per_frame_;
    uint32_t    crtc_divider_ = 0;       // CPU runs at 2 MHz, CRTC at 1 MHz

    // Helper methods
    void tick_cpu();
    bus_state_t mem_tick(bus_state_t s);

    // CRTC display callbacks
    void crtc_display_char(uint16_t ma, uint8_t ra, bool cursor);
    void crtc_vsync();
    void crtc_hsync();

    // Render a single character in Mode 7 (teletext)
    void render_mode7_char(uint16_t screen_offset, uint8_t ra, bool cursor);

    // Render bitmap mode pixels (Modes 0-6)
    void render_bitmap_pixels(uint16_t ma, uint8_t ra, bool cursor);

    // Video ULA helpers
    int  get_display_mode() const;       // Current display mode (0-7)
    int  get_pixels_per_byte() const;    // Pixels packed per byte (mode-dependent)
    int  get_colors_per_mode() const;    // Number of colors available in current mode

    // VIA callbacks (System VIA)
    static uint8_t sys_via_port_a_read(void* ctx, uint8_t output);
    static uint8_t sys_via_port_b_read(void* ctx, uint8_t output);

    // ROM loading
    bool load_roms();

    // Keyboard mapping
    void update_key_matrix(SDL_Keycode key, bool pressed);
    uint8_t scan_keyboard(uint8_t column) const;

    // Chip registration (for debug hardware menu)
    void register_chips();
};
