#pragma once
/*
 * avg.hpp — Atari Analog Vector Generator (AVG)
 *
 * The AVG is the successor to the DVG, used in Atari color- and later
 * monochrome-vector arcade games starting with Tempest (1980).
 *
 * Key differences from DVG:
 *   - 13-bit address space (8 K words) vs DVG's 12-bit (4 K words)
 *   - 8-level subroutine stack (vs 4 in DVG)
 *   - STAT instruction: sets color index (3-bit) and intensity (4-bit)
 *   - SCAL instruction: sets global binary scale (3-bit) + linear scale (8-bit)
 *   - CNTR instruction: reloads beam to center of screen
 *   - 13-bit sign-magnitude X/Y deltas (bit 12 = sign, bits 11:0 = magnitude)
 *   - Separate binary and linear scale factors
 *
 * Display list opcodes (3-bit command in bits [15:13] of first word):
 *
 *   000  VCTR   — Draw vector.  Two words.
 *                  w0: [15:13]=000  [12:0]=ΔY (13-bit sign-magnitude)
 *                  w1: [15:13]=Z    [12:0]=ΔX (13-bit sign-magnitude)
 *                  Z: 3-bit intensity (0=blank, 1=use STAT, 2-7=direct)
 *   001  HALT   — Stop state machine until next VGGO.  One word.
 *   010  SVEC   — Short vector.  One word.
 *                  [12]=Ysign [11:8]=Ymag [7:5]=Z [4]=Xsign [3:0]=Xmag
 *   011  STAT/SCAL — distinguished by bit 12:
 *            bit12=0: STAT — [7:4]=intensity [2:0]=color
 *            bit12=1: SCAL — [10:8]=binary_scale [7:0]=linear_scale
 *   100  CNTR   — Load center position.  One word.
 *   101  JSRL   — Jump to subroutine (13-bit target).  One word.
 *                  [12:0]=target byte address
 *   110  RTSL   — Return from subroutine.  One word.
 *   111  JMPL   — Jump unconditional (13-bit target).  One word.
 *                  [12:0]=target byte address
 *
 * Scaling model (MAME avg_common_strobe3):
 *   The AVG uses a binary rate multiplier (BRM) approach:
 *   - sign-magnitude delta: bit 12 = direction, bits 11:0 = rate
 *   - DAC sees upper 10 bits of magnitude (>> 3), range 0–511
 *   - Binary scale (SCAL [10:8]): controls draw time (cycles = 2^(15-bin_scale))
 *   - Linear scale (SCAL [7:0]): multiplier on deflection rate (inverted: 255-val)
 *   - Pixel delta = (magnitude >> 3) * (255 - lin_scale) >> (5 + bin_scale)
 *
 * Output:
 *   The AVG emits VectorVideoSample signals with color_index set
 *   to the STAT color value.  Monochrome AVG games (Gravitar, Battlezone)
 *   never issue STAT with a non-zero color, so color_index stays 0.
 */

#include "chip/video/video_chip_base.hpp"
#include "core/signal/vector_video_out.hpp"
#include <cstdint>
#include <cstring>
#include <algorithm>

// ============================================================================
// AVG CONSTANTS
// ============================================================================

namespace avg_constants {

    // AVG opcode address field is 13 bits [12:0] (word address).
    // ADDR_MASK extracts this field from opcode words.
    inline constexpr uint16_t ADDR_MASK        = 0x1FFF;

    // Memory address mask: 14-bit byte addressing supports up to 16 KB
    // of vector memory (e.g. Gravitar/BW: 2 KB RAM + 14 KB ROM).
    // JSRL/JMPL produce byte addresses up to 0x3FFE (13-bit word << 1).
    inline constexpr uint16_t MEM_ADDR_MASK    = 0x3FFF;

    // AVG subroutine stack depth (hardware has 8-level stack)
    inline constexpr int STACK_DEPTH           = 8;

    // Maximum vectors per frame (safety limit for runaway display lists)
    inline constexpr int MAX_VECTORS_PER_FRAME = 8192;

    // Rasterization display dimensions
    inline constexpr int DISPLAY_WIDTH         = 1024;
    inline constexpr int DISPLAY_HEIGHT        = 1024;

    // Coordinate center (AVG uses signed coordinates centered on screen)
    inline constexpr int CENTER_X              = 512;
    inline constexpr int CENTER_Y              = 512;

    // AVG opcode types (from bits [15:13] of first word)
    // STAT and SCAL share opcode 3, distinguished by bit 12.
    inline constexpr uint8_t OP_VCTR           = 0;   // 000 — two-word vector
    inline constexpr uint8_t OP_HALT           = 1;   // 001 — halt
    inline constexpr uint8_t OP_SVEC           = 2;   // 010 — short vector
    inline constexpr uint8_t OP_STAT_SCAL      = 3;   // 011 — STAT (bit12=0) or SCAL (bit12=1)
    inline constexpr uint8_t OP_CNTR           = 4;   // 100 — center beam
    inline constexpr uint8_t OP_JSRL           = 5;   // 101 — jump subroutine
    inline constexpr uint8_t OP_RTSL           = 6;   // 110 — return from subroutine
    inline constexpr uint8_t OP_JMPL           = 7;   // 111 — jump unconditional

}  // namespace avg_constants

// ============================================================================
// AVG STATE MACHINE
// ============================================================================

struct avg_t : public VideoChipBase {

    avg_t() : VideoChipBase(ChipInfo{"AVG", "Atari", "Atari AVG"}) {}

    // ── 16.16 fixed-point helpers ────────────────────────────────
    static constexpr int FP_SHIFT = 16;
    static constexpr int32_t to_fp(int32_t i) { return i << FP_SHIFT; }
    static constexpr int32_t to_fp(int32_t i, uint16_t frac) { return (i << FP_SHIFT) | frac; }
    static constexpr int32_t fp_to_int(int32_t fp) { return fp >> FP_SHIFT; }

    // ========================================================================
    // Game-specific configuration
    // ========================================================================

    /// Enable Tempest-specific STAT encoding:
    ///   bit 11 = 1 → color_index = bits [3:0]  (4-bit color)
    ///   bit 11 = 0 → intensity   = bits [7:4]  (4-bit intensity)
    /// Generic AVG: always sets both color (bits [2:0]) and intensity (bits [7:4]).
    void set_tempest_stat(bool enable) { tempest_stat_ = enable; }

    /// Enable X/Y axis swap for rotated monitor (Tempest).
    void set_swap_xy(bool enable) { swap_xy_ = enable; }

    /// Set the visible display area in logical pixels (matching MAME visarea).
    /// The beam center is derived as (width/2, height/2).  Coordinates are
    /// stored internally in 16.16 fixed-point for sub-pixel precision.
    void set_display_area(uint16_t width, uint16_t height) {
        display_width_  = width;
        display_height_ = height;
        center_x_fp_    = to_fp(width  / 2);
        center_y_fp_    = to_fp(height / 2);
    }

    // ========================================================================
    // Initialization
    // ========================================================================

    void init() {
        reset();
    }

    void reset() {
        pc_            = 0;
        sp_            = 0;
        beam_x_fp_     = center_x_fp_;
        beam_y_fp_     = center_y_fp_;
        running_       = false;
        halt_          = true;
        clocks_remaining_ = 0;
        color_index_   = 0;
        intensity_     = 0;
        bin_scale_     = 0;
        lin_scale_     = 0;
        std::memset(stack_, 0, sizeof(stack_));
    }

    // ========================================================================
    // Memory access — system provides vector RAM/ROM pointers
    // ========================================================================

    void set_vector_memory(const uint8_t* ram, uint16_t ram_byte_size,
                           const uint8_t* rom, uint16_t rom_byte_size,
                           uint16_t /*rom_word_offset (unused)*/ = 0) {
        vec_ram_         = ram;
        vec_ram_size_    = ram_byte_size;
        vec_rom_         = rom;
        vec_rom_size_    = rom_byte_size;
    }

    void set_video_out(VectorVideoOut* out) {
        video_out_ = out;
    }

    // ========================================================================
    // Bus interface — VGGO / VGRST triggers from CPU
    // ========================================================================

    void trigger_go() {
        pc_            = 0;
        sp_            = 0;
        running_       = true;
        halt_          = false;
        clocks_remaining_ = 0;
    }

    void trigger_reset() {
        reset();
    }

    bool is_halted() const { return halt_; }

    // ========================================================================
    // Execution — call once per CPU cycle
    // ========================================================================

    void tick() {
        if (!running_) return;

        if (clocks_remaining_ > 0) {
            --clocks_remaining_;
            return;
        }

        int safety = avg_constants::MAX_VECTORS_PER_FRAME;
        while (running_ && clocks_remaining_ == 0 && --safety > 0) {
            execute_opcode();
        }
    }

    // ========================================================================
    // State (public for debug inspection)
    // ========================================================================

    uint16_t pc_            = 0;
    uint8_t  sp_            = 0;
    int32_t  beam_x_fp_     = to_fp(avg_constants::CENTER_X);  // 16.16 fixed-point
    int32_t  beam_y_fp_     = to_fp(avg_constants::CENTER_Y);  // 16.16 fixed-point
    bool     running_       = false;
    bool     halt_          = true;
    int32_t  clocks_remaining_ = 0;

    uint8_t  color_index_   = 0;    // STAT color [2:0]
    uint8_t  intensity_     = 0;    // STAT intensity [3:0]
    uint8_t  bin_scale_     = 0;    // SCAL binary scale [2:0]
    uint8_t  lin_scale_     = 0;    // SCAL linear scale [7:0]

    uint16_t stack_[avg_constants::STACK_DEPTH] = {};

    // Display area configuration (set via set_display_area)
    uint16_t display_width_  = avg_constants::DISPLAY_WIDTH;
    uint16_t display_height_ = avg_constants::DISPLAY_HEIGHT;
    int32_t  center_x_fp_    = to_fp(avg_constants::CENTER_X);
    int32_t  center_y_fp_    = to_fp(avg_constants::CENTER_Y);

private:
    const uint8_t*   vec_ram_  = nullptr;
    uint16_t         vec_ram_size_ = 0;
    const uint8_t*   vec_rom_  = nullptr;
    uint16_t         vec_rom_size_ = 0;
    VectorVideoOut* video_out_ = nullptr;
    bool tempest_stat_ = false;   // Tempest-specific STAT encoding
    bool swap_xy_      = false;   // X/Y axis swap (rotated monitor)

    // ========================================================================
    // Internal — opcode fetch and decode
    // ========================================================================

    /// Read a 16-bit word at a BYTE address in the AVG address space.
    /// The AVG PC is byte-addressed.  JSRL/JMPL can produce 14-bit byte
    /// addresses (13-bit word field << 1), supporting up to 16 KB total
    /// vector memory (e.g. Gravitar/BW: 2 KB RAM + 14 KB ROM).
    /// RAM occupies bytes [0, vec_ram_size_), ROM occupies [vec_ram_size_, ...).
    uint16_t read_word(uint16_t byte_addr) const {
        uint16_t a = byte_addr & avg_constants::MEM_ADDR_MASK;

        if (a + 1 < vec_ram_size_) {
            if (!vec_ram_) return 0;
            return vec_ram_[a] | (vec_ram_[a + 1] << 8);
        }

        if (a >= vec_ram_size_) {
            uint16_t rom_off = a - vec_ram_size_;
            if (!vec_rom_ || rom_off + 1 >= vec_rom_size_) return 0;
            return vec_rom_[rom_off] | (vec_rom_[rom_off + 1] << 8);
        }

        return 0;
    }

    /// Apply AVG scaling to a raw 13-bit vector component.
    ///
    /// The AVG's Binary Rate Multiplier integrates ALL bits of the delta
    /// value over the timer period.  MAME approximates this by truncating
    /// to the DAC's upper 10 bits (>> 3), but with 16.16 fixed-point
    /// coordinates those 3 extra bits contribute real sub-pixel precision.
    ///
    /// Full-precision formula:
    ///   1. Convert 13-bit offset-binary to signed [-4096, +4095]:
    ///        signed_val = (raw ^ 0x1000) - 0x1000
    ///   2. Displacement (16.16 FP) = signed_val * (255 - lin) << (8 - bin - extra_shift)
    ///      (shift is 3 less than MAME's 11 to compensate for the 8× larger input)
    ///
    /// Max product: 4095 × 255 = 1,044,225.  After << 8: 267,321,600.  Fits int32_t.
    ///
    /// Returns displacement in 16.16 fixed-point.
    int32_t apply_scale(int32_t raw_13bit, int extra_shift = 0) const {
        int32_t dac = (raw_13bit ^ 0x1000) - 0x1000;
        int32_t product = dac * (255 - lin_scale_);
        int shift = 8 - bin_scale_ - extra_shift;
        return (shift >= 0) ? (product << shift) : (product >> (-shift));
    }

    /// Decode Z (3-bit intensity from VCTR/SVEC) into draw intensity value.
    /// Z=0: blank (beam off), Z=1: use STAT intensity, Z>=2: direct (Z*2).
    /// Returns 0 for blank, or a value suitable for emit_vector's intensity param.
    int decode_z_intensity(int z) const {
        if (z == 0) return 0;
        if (z == 1) return intensity_;
        return z * 2;  // Z=2→4, Z=3→6, ... Z=7→14; brightness via intensity*17
    }

    void execute_opcode() {
        uint16_t w0 = read_word(pc_);
        uint8_t opcode = (w0 >> 13) & 0x07;

        switch (opcode) {
            case avg_constants::OP_VCTR: {
                // VCTR — Draw vector (two words)
                //   w0[12:0] = ΔY (13-bit raw: sign at bit 12, processed by DAC XOR)
                //   w1[15:13] = Z (3-bit intensity: 0=blank, 1=STAT, 2-7=direct)
                //   w1[12:0] = ΔX (13-bit raw: sign at bit 12, processed by DAC XOR)
                uint16_t w1 = read_word(pc_ + 2);

                int32_t dy = apply_scale(w0 & 0x1FFF);
                int32_t dx = apply_scale(w1 & 0x1FFF);
                int z      = (w1 >> 13) & 0x07;

                int draw_intensity = decode_z_intensity(z);
                emit_vector(dx, dy, draw_intensity);

                int vec_len = std::max(std::abs(fp_to_int(dx)), std::abs(fp_to_int(dy)));
                clocks_remaining_ = std::max(4, vec_len >> 1);
                pc_ += 4;  // two words = 4 bytes
                break;
            }

            case avg_constants::OP_HALT:
                // HALT — Stop AVG
                emit_frame_end();
                running_ = false;
                halt_    = true;
                pc_ += 2;  // one word = 2 bytes
                break;

            case avg_constants::OP_SVEC: {
                // SVEC — Short vector (one word)
                //   [12]   = Y sign
                //   [11:8] = Y magnitude (4 bits)
                //   [7:5]  = Z intensity (3 bits: 0=blank, 1=STAT, 2-7=direct)
                //   [4]    = X sign
                //   [3:0]  = X magnitude (4 bits)
                //
                // Assemble the same 13-bit format as VCTR: sign at bit 12,
                // 4-bit magnitude placed at bits [11:8] (bits [7:0] = 0).
                // The DAC XOR formula in apply_scale handles the sign correctly.
                int z = (w0 >> 5) & 0x07;

                int32_t raw_dy = ((w0 >> 12) & 1) << 12 | (((w0 >> 8) & 0xF) << 8);
                int32_t raw_dx = ((w0 >> 4)  & 1) << 12 | ((w0 & 0xF) << 8);

                // SVEC uses an 8-bit timer (max 256 cycles) vs VCTR's
                // 15-bit timer (max 32768 cycles).  Ratio = 128 = 2^7.
                int32_t dy = apply_scale(raw_dy, 7);
                int32_t dx = apply_scale(raw_dx, 7);

                int draw_intensity = decode_z_intensity(z);
                emit_vector(dx, dy, draw_intensity);

                clocks_remaining_ = 2 + ((std::abs(fp_to_int(dx)) + std::abs(fp_to_int(dy))) >> 3);
                pc_ += 2;  // one word = 2 bytes
                break;
            }

            case avg_constants::OP_STAT_SCAL:
                // Opcode 011 — STAT or SCAL, distinguished by bit 12.
                if (w0 & 0x1000) {
                    // SCAL (bit 12 = 1)
                    //   [10:8] = binary scale (3-bit right-shift, 0-7)
                    //   [7:0]  = linear scale (8-bit, inverted at draw time)
                    bin_scale_ = (w0 >> 8) & 0x07;
                    lin_scale_ = w0 & 0xFF;
                } else if (tempest_stat_) {
                    // STAT — Tempest variant (MAME tempest_device::handler_6)
                    // Bit 11 selects what is being updated:
                    //   bit 11 = 1 → color_index = bits [3:0] (4-bit)
                    //   bit 11 = 0 → intensity   = bits [7:4] (4-bit)
                    if (w0 & 0x0800)
                        color_index_ = w0 & 0x0F;
                    else
                        intensity_   = (w0 >> 4) & 0x0F;
                } else {
                    // STAT — Generic AVG (bit 12 = 0)
                    //   [7:4] = intensity (4-bit)
                    //   [2:0] = color index (3-bit)
                    intensity_   = (w0 >> 4) & 0x0F;
                    color_index_ = w0 & 0x07;
                }
                clocks_remaining_ = 1;
                pc_ += 2;  // one word = 2 bytes
                break;

            case avg_constants::OP_CNTR:
                // CNTR — Load center position
                beam_x_fp_ = center_x_fp_;
                beam_y_fp_ = center_y_fp_;
                emit_position();
                clocks_remaining_ = 4;
                pc_ += 2;  // one word = 2 bytes
                break;

            case avg_constants::OP_JSRL:
                // JSRL — Jump to subroutine
                // The 13-bit field [12:0] is a word address; shift left by 1
                // to get a byte address.  (MAME: m_pc = m_dvy << 1)
                if (sp_ < avg_constants::STACK_DEPTH) {
                    stack_[sp_++] = pc_ + 2;  // return past this word
                }
                pc_ = (w0 & avg_constants::ADDR_MASK) << 1;
                // Zero cost
                break;

            case avg_constants::OP_RTSL:
                // RTSL — Return from subroutine
                if (sp_ > 0) {
                    pc_ = stack_[--sp_];
                } else {
                    running_ = false;
                    halt_    = true;
                }
                // Zero cost
                break;

            case avg_constants::OP_JMPL:
                // JMPL — Unconditional jump
                // The 13-bit field [12:0] is a word address; shift left by 1.
                pc_ = (w0 & avg_constants::ADDR_MASK) << 1;
                // Zero cost
                break;
        }
    }

    // ========================================================================
    // Vector signal emission
    // ========================================================================

    /// Convert beam Y to screen Y (Y increasing upward → screen Y increasing downward)
    int16_t screen_y(int32_t y) const {
        return static_cast<int16_t>(display_height_ - 1 - y);
    }

    void emit_vector(int32_t dx_fp, int32_t dy_fp, int intensity) {
        // Tempest uses a 90°-rotated monitor: swap X/Y axes
        if (swap_xy_) {
            int32_t tmp = dx_fp;
            dx_fp = dy_fp;
            dy_fp = tmp;
        }

        beam_x_fp_ += dx_fp;
        beam_y_fp_ += dy_fp;

        if (!video_out_) return;

        // One sample per instruction: the beam destination.
        // BeamOn = drawing (intensity > 0); None = dark repositioning.
        // The shader draws a line from the previous sample's position
        // to this sample's position whenever BeamOn is set.
        if (intensity > 0) {
            uint8_t bright = static_cast<uint8_t>(std::min(intensity * 17, 255));
            video_out_->drive(VectorVideoSample{
                static_cast<int16_t>(fp_to_int(beam_x_fp_)),
                screen_y(fp_to_int(beam_y_fp_)),
                bright, color_index_, SyncFlag::BeamOn, {}
            });
        } else {
            video_out_->drive(VectorVideoSample{
                static_cast<int16_t>(fp_to_int(beam_x_fp_)),
                screen_y(fp_to_int(beam_y_fp_)),
                0, color_index_, SyncFlag::None, {}
            });
        }
    }

    void emit_position() {
        if (!video_out_) return;
        video_out_->drive(VectorVideoSample{
            static_cast<int16_t>(fp_to_int(beam_x_fp_)),
            screen_y(fp_to_int(beam_y_fp_)),
            0, color_index_, SyncFlag::None, {}
        });
    }

    void emit_frame_end() {
        if (!video_out_) return;
        video_out_->drive(VectorVideoSample{
            static_cast<int16_t>(fp_to_int(beam_x_fp_)),
            screen_y(fp_to_int(beam_y_fp_)),
            0, 0, SyncFlag::FrameEnd, {}
        });
    }
};
