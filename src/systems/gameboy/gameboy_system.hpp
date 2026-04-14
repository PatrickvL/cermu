#pragma once
/*
 * gameboy_system.hpp — Nintendo Game Boy / Game Boy Color Emulated System
 *
 * The Game Boy (1989) uses a Sharp SM83 (LR35902) CPU — a Z80-like with
 * no IX/IY registers, no second register set, and a few unique instructions.
 * The system integrates CPU, PPU, APU, timer, and I/O into a single SoC.
 *
 * Variants:
 *   DMG  (1989): 4 shades of green, 8KB VRAM, 8KB WRAM
 *   GBC  (1998): 32768 colors, 16KB VRAM, 32KB WRAM (banked), double speed
 *
 * Memory map ($0000–$FFFF):
 *   $0000–$3FFF : ROM bank 0 (16KB, always mapped)
 *   $4000–$7FFF : ROM bank 1–N (switchable, MBC-dependent)
 *   $8000–$9FFF : 8KB VRAM (video RAM)
 *   $A000–$BFFF : External RAM (cartridge, MBC-dependent)
 *   $C000–$DFFF : 8KB WRAM (work RAM, GBC: banked $D000–$DFFF)
 *   $E000–$FDFF : Echo RAM (mirror of $C000–$DDFF)
 *   $FE00–$FE9F : OAM (sprite attributes, 160 bytes)
 *   $FEA0–$FEFF : Unusable
 *   $FF00–$FF7F : I/O registers
 *   $FF80–$FFFE : HRAM (127 bytes, fast internal RAM)
 *   $FFFF       : IE (Interrupt Enable register)
 */

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/sharp_sm83.hpp"
#include "chip/video/gb_ppu/gb_ppu.hpp"
#include "chip/sound/gb_apu/gb_apu.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/mmu/gb_mbc/gb_mbc.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>
#include <vector>

enum class GameBoyVariant { DMG, GBC };

namespace gb_constants {
    inline constexpr uint32_t CPU_FREQ_HZ          = 4194304;   // 4.194304 MHz (T-states)
    inline constexpr uint32_t CPU_FREQ_DOUBLE_HZ   = 8388608;   // GBC double speed
    inline constexpr int DISPLAY_WIDTH             = 160;
    inline constexpr int DISPLAY_HEIGHT            = 144;
    inline constexpr uint32_t DOTS_PER_FRAME       = 70224;     // 154 lines × 456 dots
    inline constexpr int TARGET_FPS                = 60;        // ~59.73 Hz
    inline constexpr uint32_t CYCLES_PER_FRAME     = 70224;
    inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;
    inline constexpr uint32_t ROM_BANK_SIZE        = 16384;     // 16KB
    inline constexpr uint32_t VRAM_SIZE            = 8192;
    inline constexpr uint32_t WRAM_SIZE            = 8192;
    inline constexpr uint32_t HRAM_SIZE            = 127;
    inline constexpr uint32_t OAM_SIZE             = 160;

    // I/O register addresses
    inline constexpr uint8_t IO_JOYP               = 0x00;   // $FF00
    inline constexpr uint8_t IO_SB                 = 0x01;   // $FF01 Serial data
    inline constexpr uint8_t IO_SC                 = 0x02;   // $FF02 Serial control
    inline constexpr uint8_t IO_DIV                = 0x04;   // $FF04 Divider
    inline constexpr uint8_t IO_TIMA               = 0x05;   // $FF05 Timer counter
    inline constexpr uint8_t IO_TMA                = 0x06;   // $FF06 Timer modulo
    inline constexpr uint8_t IO_TAC                = 0x07;   // $FF07 Timer control
    inline constexpr uint8_t IO_IF                 = 0x0F;   // $FF0F Interrupt flag
    inline constexpr uint8_t IO_LCDC               = 0x40;   // $FF40 LCD control (PPU)
    inline constexpr uint8_t IO_IE                 = 0xFF;   // $FFFF Interrupt enable
}

#define GB_BUS_DEFAULT_STATE (SharpSM83::default_bus_state())

// ============================================================================
// Game Boy Manifest
// ============================================================================

template<GameBoyVariant V>
inline constexpr auto kGameBoyManifest = make_manifest(
    Slot<SharpSM83>{.base_addr = 0x0000, .label = "SM83 (LR35902)"},

    // Boot ROM ($0000–$00FF on DMG, $0000–$08FF on GBC) — optional system ROM
    Slot<ROMChip>{.base_addr = 0x0000,
                  .size_bytes = (V == GameBoyVariant::GBC) ? size_t(0x0900) : size_t(0x0100),
                  .label = "Boot ROM",
                  .rom = {.filenames = (V == GameBoyVariant::DMG)
                      ? "dmg_boot.bin|mgb_boot.bin|sgb_boot.bin|sgb2_boot.bin"
                      : "cgb_boot.bin|cgb0_boot.bin",
                      .optional = true}},

    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x8000,
                  .label = "Cartridge ROM"},
    Slot<RAMChip>{.base_addr = 0x8000, .size_bytes = gb_constants::VRAM_SIZE,
                  .label = "VRAM"},
    Slot<RAMChip>{.base_addr = 0xA000, .size_bytes = 0x2000,
                  .label = "External RAM"},
    Slot<RAMChip>{.base_addr = 0xC000, .size_bytes = gb_constants::WRAM_SIZE,
                  .label = "WRAM"},

    // APU before PPU so PPU overrides APU's sub-page for $FF40–$FF4F
    Slot<gb_apu_t>{.base_addr = 0xFF10, .addr_mask = 0x003F,
                   .label = "APU"},
    Slot<gb_ppu_t>{.base_addr = 0xFF40, .addr_mask = 0x000F,
                   .label = "PPU"},

    // Ports
    Slot<PortExpansion>{.name = "Cartridge Slot"},
    Slot<PortCompositeVideo>{.name = "LCD", .default_device = "lcd_panel"},
    Slot<PortAudioStereo>{.name = "Headphone Jack"}
);

template<GameBoyVariant V>
using GameBoyBusSpec = ManifestBusSpec<kGameBoyManifest<V>, 16, 8, 1, true>;

template<GameBoyVariant V>
struct GameBoyBoard : Board<GameBoyBusSpec<V>> {
    using ComponentTuple = typename decltype(kGameBoyManifest<V>)::component_tuple;
    ComponentTuple components_;

    SharpSM83& cpu     = std::get<0>(components_);
    ROMChip&   bootrom = std::get<1>(components_);
    ROMChip&   rom     = std::get<2>(components_);
    RAMChip&   vram    = std::get<3>(components_);
    RAMChip&   extram  = std::get<4>(components_);
    RAMChip&   wram    = std::get<5>(components_);
    gb_apu_t&  apu     = std::get<6>(components_);
    gb_ppu_t&  ppu     = std::get<7>(components_);

    PortExpansion&      cart_port  = std::get<8>(components_);
    PortCompositeVideo& video_port = std::get<9>(components_);
    PortAudioStereo&    audio_port = std::get<10>(components_);

    GameBoyBoard() : Board<GameBoyBusSpec<V>>(kGameBoyManifest<V>) {}
};

// ============================================================================
// Game Boy System
// ============================================================================

template<GameBoyVariant V>
class GameBoySystem : public System {
public:
    GameBoySystem();
    ~GameBoySystem() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;
    void set_headless(bool headless) override;

    bool load_file(const char* filepath) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    using Bus       = MemoryBus<GameBoyBusSpec<V>>;
    using MainBoard = GameBoyBoard<V>;
    Bus       bus_;
    MainBoard board_;

    std::unique_ptr<CompositeVideoPort> video_port_;
    uint32_t audio_sample_rate_ = gb_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // MBC (Memory Bank Controller) — handles ROM/RAM banking
    std::unique_ptr<GbMbcBase> mbc_;
    std::vector<uint8_t> cart_rom_;     // Full cartridge ROM (up to 8MB)
    std::vector<uint8_t> cart_ram_;     // External cartridge RAM (up to 128KB)

    // Joypad state (active-low)
    uint8_t joypad_buttons_   = 0x0F;  // A, B, Select, Start
    uint8_t joypad_dpad_      = 0x0F;  // Right, Left, Up, Down
    uint8_t joypad_select_    = 0;     // Which half to read ($FF00 bits 4-5)

    // Timer — internal 16-bit divider, TIMA counter, TMA modulo, TAC control
    uint16_t div_counter_     = 0;   // Internal 16-bit divider (incremented every T-state)
    uint8_t  io_regs_[128]    = {};  // $FF00–$FF7F shadow
    uint8_t  hram_[127]       = {};  // $FF80–$FFFE
    uint8_t  ie_              = 0;   // $FFFF interrupt enable

    // Boot ROM overlay — $0000–$00FF (DMG) or $0000–$08FF (GBC)
    // mapped over cartridge ROM until the boot sequence writes to $FF50.
    bool boot_rom_active_     = false;

    // Serial output capture — accumulates bytes sent via $FF01/$FF02
    // for test ROM serial link output (Blargg's tests, etc.).
    std::string serial_output_;
    uint16_t serial_cycles_    = 0;  // Transfer clock counter
    bool     serial_active_    = false;
    std::string drain_debug_text() override;

    bus_state_t pins_ = GB_BUS_DEFAULT_STATE;
    bool mreq_prev_ = false;   // MREQ edge detection for single-dispatch
    uint32_t frame_counter_ = 0;

    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
