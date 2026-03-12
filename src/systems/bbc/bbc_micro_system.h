#pragma once

#include "../../core/system.h"
#include "../../core/system_lines.h"
#include "../../core/board.hpp"
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "../../chip/io/mos6522.h"
#include "../../chip/video/mc6845/mc6845.h"
#include "../../chip/sound/sn76489/sn76489.h"
#include "../../chip/memory/memory_chip.h"
#include "bbc_micro_constants.h"
#include <cstdint>
#include <memory>

// BBC Micro bus is derived from 6502 — shares address, data, and control signals.
#define BBC_BUS_DEFAULT_STATE (MOS6502::default_bus_state())

// =============================================================================
// BBC Micro chip manifest — declarative memory layout
// =============================================================================
//
// Slot 0: RAM — 32 KB at $0000-$7FFF
// Slot 1: Paged ROM pool — 256 KB at $8000 (16 × 16 KB sideways ROM slots)
//         Only one 16 KB bank visible at $8000-$BFFF; selected by rom_select_.
//         apply() clips to available pages; configure_bus_memory_map() remaps.
// Slot 2: OS ROM — 16 KB at $C000-$FFFF
//         $FC00-$FEFF (FRED/JIM/SHEILA) handled by sheila_tick(), not the bus.
//
inline constexpr auto kBBCMicroChips = make_chip_manifest(
    Slot<RAMChip>{0x0000,  32768, 0, "RAM"},
    Slot<ROMChip>{0x8000, 262144, 0, "Paged ROM"},      // 16 × 16 KB banks
    Slot<ROMChip>{0xC000,  16384, 0, "MOS ROM"}
);

namespace bbc_chips {
    inline constexpr size_t kRamSlot       = 0;
    inline constexpr size_t kPagedRomSlot  = 1;
    inline constexpr size_t kOsRomSlot     = 2;
}

using BBCMicroBusSpec = ManifestBusSpec<kBBCMicroChips, 16, 8>;

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
class BBCMicroSystem : public System {
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

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using Bus = MemoryBus<BBCMicroBusSpec>;
    using PT  = PackingTraits<BBCMicroBusSpec>;
    using Mem = Board<BBCMicroBusSpec>;
    Bus bus_;
    Mem bus_mem_{kBBCMicroChips};

    // Memory chip pointers (into registered_chips_; bus_mem_ owns buffer)
    RAMChip* ram_chip_       = nullptr;
    ROMChip* paged_rom_chip_ = nullptr;   // 256 KB pool (16 × 16 KB sideways slots)
    ROMChip* os_rom_chip_    = nullptr;

    // Convenience pointers into the unified buffer
    uint8_t*    memory_ = nullptr;       // → ram_chip_->data() (for rendering)

    // Paged ROM state
    uint8_t     rom_select_ = 0;         // Currently selected paged ROM bank (0-15)

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
    bus_state_t sheila_tick(bus_state_t s);   // FRED/JIM/SHEILA I/O ($FC00-$FEFF)
    void configure_bus_memory_map();          // Post-apply() page table fixups
    void update_paged_rom();                  // Remap $8000-$BFFF after rom_select_ change

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
