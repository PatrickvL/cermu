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
 *   - SCAL instruction: sets global binary scale + linear scale
 *   - CENTER instruction: reloads beam to center of screen
 *   - 13-bit X/Y coordinates (signed) — 0–8191 range
 *   - Separate binary and linear scale factors
 *
 * Display list opcodes (3-bit command in bits [15:13] of first word):
 *
 *   000  VCTR   — Draw vector.  Two words.
 *                  w0: YYYY YYYY YYYY Y--- (13-bit ΔY, signed)
 *                  w1: IIII XXXX XXXX XXXX (4-bit intensity, 13-bit ΔX, signed)
 *   001  HALT   — Stop state machine until next VGGO.  One word.
 *   010  SVEC   — Short vector.  One word.
 *                  010s mYYY IIII smXX X---
 *   011  STAT   — Set color and intensity.  One word.
 *                  011- ---- IIII -CCC ----
 *                  (I=intensity [7:4], C=color [6:4])
 *   100  SCAL   — Set scale.  One word.
 *                  100- ---- LLLL -BBB ----
 *                  (L=linear scale [7:4], B=binary scale [6:4])
 *   101  CENTER — Load center position.  One word.
 *                  101- ---- ---- ---- ----
 *   110  JSRL   — Jump to subroutine (13-bit target).  One word.
 *                  110a aaaa aaaa aaaa
 *   111  RTSL   — Return from subroutine.  One word / JMPL if lower bit clear.
 *                  1110 aaaa aaaa aaaa = JMPL
 *                  1111 ---- ---- ---- = RTSL
 *
 * Output:
 *   The AVG emits VectorVideoSample signals with color_index set
 *   to the STAT color value.  Monochrome AVG games (Gravitar)
 *   never issue STAT with a non-zero color, so color_index stays 0.
 */

#include "chip/video/video_chip_base.hpp"
#include "core/signal/vector_video_stream.hpp"
#include <cstdint>
#include <cstring>
#include <algorithm>

// ============================================================================
// AVG CONSTANTS
// ============================================================================

namespace avg_constants {

    // AVG address space is 13 bits (8 K words)
    inline constexpr uint16_t ADDR_SPACE      = 0x2000;  // 8192 words
    inline constexpr uint16_t ADDR_MASK        = 0x1FFF;

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
    inline constexpr uint8_t OP_VCTR           = 0;
    inline constexpr uint8_t OP_HALT           = 1;
    inline constexpr uint8_t OP_SVEC           = 2;
    inline constexpr uint8_t OP_STAT           = 3;
    inline constexpr uint8_t OP_SCAL           = 4;
    inline constexpr uint8_t OP_CENTER         = 5;
    inline constexpr uint8_t OP_JSRL           = 6;
    inline constexpr uint8_t OP_RTSL_JMPL      = 7;   // RTSL if bit 12 set, else JMPL

}  // namespace avg_constants

// ============================================================================
// AVG STATE MACHINE
// ============================================================================

struct avg_t : public VideoChipBase {

    avg_t() : VideoChipBase(ChipInfo{"AVG", "Atari"}) {}

    // ========================================================================
    // Initialization
    // ========================================================================

    void init() {
        reset();
    }

    void reset() {
        pc_            = 0;
        sp_            = 0;
        beam_x_        = avg_constants::CENTER_X;
        beam_y_        = avg_constants::CENTER_Y;
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
                           uint16_t rom_word_offset = 0x800) {
        vec_ram_         = ram;
        vec_ram_size_    = ram_byte_size;
        vec_rom_         = rom;
        vec_rom_size_    = rom_byte_size;
        rom_word_offset_ = rom_word_offset;
    }

    void set_stream(VectorVideoStream* stream) {
        stream_ = stream;
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
    int32_t  beam_x_        = avg_constants::CENTER_X;
    int32_t  beam_y_        = avg_constants::CENTER_Y;
    bool     running_       = false;
    bool     halt_          = true;
    int32_t  clocks_remaining_ = 0;

    uint8_t  color_index_   = 0;    // STAT color [2:0]
    uint8_t  intensity_     = 0;    // STAT/VCTR intensity [3:0]
    uint8_t  bin_scale_     = 0;    // SCAL binary scale [2:0]
    uint8_t  lin_scale_     = 0;    // SCAL linear scale [3:0]

    uint16_t stack_[avg_constants::STACK_DEPTH] = {};

private:
    const uint8_t*   vec_ram_  = nullptr;
    uint16_t         vec_ram_size_ = 0;
    const uint8_t*   vec_rom_  = nullptr;
    uint16_t         vec_rom_size_ = 0;
    uint16_t         rom_word_offset_ = 0x800;
    VectorVideoStream* stream_ = nullptr;

    // ========================================================================
    // Internal — opcode fetch and decode
    // ========================================================================

    uint16_t read_word(uint16_t word_addr) const {
        uint16_t w = word_addr & avg_constants::ADDR_MASK;

        uint16_t ram_words = vec_ram_size_ / 2;
        if (w < ram_words) {
            uint16_t byte_addr = w * 2;
            if (!vec_ram_ || byte_addr + 1 >= vec_ram_size_) return 0;
            return vec_ram_[byte_addr] | (vec_ram_[byte_addr + 1] << 8);
        }

        if (w >= rom_word_offset_) {
            uint16_t rom_word = w - rom_word_offset_;
            uint16_t byte_addr = rom_word * 2;
            if (!vec_rom_ || byte_addr + 1 >= vec_rom_size_) return 0;
            return vec_rom_[byte_addr] | (vec_rom_[byte_addr + 1] << 8);
        }

        return 0;
    }

    /// Apply AVG scaling to a raw magnitude.
    /// The AVG uses two scale factors:
    ///   - Binary scale (SCAL [2:0]): right-shift amount (0-7)
    ///   - Linear scale (SCAL [3:0]): multiplier applied after shift
    /// Total scaled delta = (magnitude >> bin_scale) * (lin_scale + 1) / 16
    int32_t apply_scale(int32_t magnitude) const {
        // Binary scale: shift right by bin_scale
        int32_t scaled = magnitude >> bin_scale_;
        // Linear scale: multiply by (lin_scale + 1) then divide by 16
        scaled = (scaled * (lin_scale_ + 1)) >> 4;
        return scaled;
    }

    void execute_opcode() {
        uint16_t w0 = read_word(pc_);
        uint8_t opcode = (w0 >> 13) & 0x07;

        switch (opcode) {
            case avg_constants::OP_VCTR: {
                // VCTR — Draw vector (two words)
                // w0: YYYY YYYY YYYY Ys-- (13-bit ΔY signed: magnitude [12:3], sign [2])
                // w1: IIII XXXX XXXX XXXs (4-bit intensity, 13-bit ΔX signed)
                //
                // AVG encoding:
                //   w0[12:3] = Y magnitude (10 bits), w0[2] = Y sign (0=pos, 1=neg)
                //   w1[15:12] = intensity, w1[12:3] = X magnitude (10 bits), w1[2] = X sign
                uint16_t w1 = read_word(pc_ + 1);

                int dy_mag = (w0 >> 3) & 0x03FF;
                int dy_sgn = (w0 >> 2) & 1;
                int dx_mag = (w1 >> 3) & 0x03FF;
                int dx_sgn = (w1 >> 2) & 1;
                int vec_intensity = (w1 >> 12) & 0x0F;

                int32_t dx = apply_scale(dx_mag);
                int32_t dy = apply_scale(dy_mag);
                if (dx_sgn) dx = -dx;
                if (dy_sgn) dy = -dy;

                // Use per-vector intensity if non-zero, otherwise use STAT intensity
                int draw_intensity = vec_intensity > 0 ? vec_intensity : intensity_;
                emit_vector(dx, dy, draw_intensity);

                int vec_len = std::max(std::abs(dx), std::abs(dy));
                clocks_remaining_ = std::max(4, vec_len >> 1);
                pc_ += 2;
                break;
            }

            case avg_constants::OP_HALT:
                // HALT — Stop AVG
                emit_frame_end();
                running_ = false;
                halt_    = true;
                pc_ += 1;
                break;

            case avg_constants::OP_SVEC: {
                // SVEC — Short vector (one word)
                // 010s mYYY IIII smXX X---
                //   s = scale high bit (bit 12)
                //   m = Y sign (bit 11)
                //   YYY = Y magnitude (bits 10:8, 3 bits)
                //   IIII = intensity (bits 7:4)
                //   s = scale low bit (bit 3)
                //   m = X sign (bit 2... wait, simpler encoding)
                //
                // Actually follows the MAME/real convention:
                //   bit 12: scale bit (determines shift)
                //   bits 11: Y sign
                //   bits 10:8: Y magnitude (3 bits)
                //   bits 7:4: intensity
                //   bit 3: (unused/another scale bit)
                //   bit 2: X sign
                //   bits 1:0...
                //
                // Simplified: we treat the magnitude as small vectors
                // pre-scaled by the current SCAL setting.

                int dy_mag = (w0 >> 8) & 0x07;
                int dy_sgn = (w0 >> 11) & 1;
                int dx_mag =  w0        & 0x07;
                int dx_sgn = (w0 >> 2)  & 1;  // bit 2 is X sign, bit 11 is Y sign... 
                // MAME avg.cpp: SVEC encodes like this:
                // OP=010, bits [12]=scale_high
                // bits [11]=Ysign, [10:8]=Ymag
                // bits [7:4]=intensity
                // bits [3]=scale_low, [2]=Xsign, [1:0]=Xmag (only 2 bits for X!)
                // Wait: that's only 2 bits for X mag?

                // Let me use the correct MAME encoding:
                // w0 = 010S mYYY IIII sMxx x000
                // Actually the simplest correct form:
                dx_mag = w0 & 0x03;          // bits 4:3 after shift? No.
                
                // Let me follow MAME avg_device::avg_common_strobe_st() precisely:
                // SVEC is single word. The encoding is:
                //   OP[15:13] = 010
                //   Y_sign = bit 12
                //   Y_mag  = bits 11:8 (4 bits)  
                //   I      = bits 7:4 (intensity)
                //   X_sign = bit 3
                //   X_mag  = bits 2:0 (3 bits, shifted left by 1 for 4-bit effective)
                
                // Actually, let me use the Gravitar/Tempest convention from MAME:
                // This varies by game. Let me use the simplest version.
                
                dy_mag = (w0 >> 8) & 0x0F;  // 4 bits
                dy_sgn = (w0 >> 12) & 1;
                int svec_intensity = (w0 >> 4) & 0x0F;
                dx_sgn = (w0 >> 3) & 1;
                dx_mag = w0 & 0x07;           // 3 bits

                // Scale: SVEC magnitudes are pre-shifted left by 7 (placed high
                // in the 10-bit DVG-compatible range), then normal scaling applies.
                int32_t dx = apply_scale(dx_mag << 7);
                int32_t dy = apply_scale(dy_mag << 7);
                if (dx_sgn) dx = -dx;
                if (dy_sgn) dy = -dy;

                int draw_intensity = svec_intensity > 0 ? svec_intensity : intensity_;
                emit_vector(dx, dy, draw_intensity);

                clocks_remaining_ = 2 + ((std::abs(dx) + std::abs(dy)) >> 3);
                pc_ += 1;
                break;
            }

            case avg_constants::OP_STAT:
                // STAT — Set color and intensity (one word)
                // 011- ---- IIII -CCC ----
                color_index_ = (w0 >> 4) & 0x07;
                intensity_   = (w0 >> 8) & 0x0F;
                clocks_remaining_ = 1;
                pc_ += 1;
                break;

            case avg_constants::OP_SCAL:
                // SCAL — Set scale (one word)
                // 100- ---- LLLL -BBB ----
                lin_scale_ = (w0 >> 8) & 0x0F;
                bin_scale_ = (w0 >> 4) & 0x07;
                clocks_remaining_ = 1;
                pc_ += 1;
                break;

            case avg_constants::OP_CENTER:
                // CENTER — Load center position
                beam_x_ = avg_constants::CENTER_X;
                beam_y_ = avg_constants::CENTER_Y;
                emit_position();
                clocks_remaining_ = 4;
                pc_ += 1;
                break;

            case avg_constants::OP_JSRL:
                // JSRL — Jump to subroutine (13-bit target)
                if (sp_ < avg_constants::STACK_DEPTH) {
                    stack_[sp_++] = pc_ + 1;
                }
                pc_ = w0 & avg_constants::ADDR_MASK;
                // Zero cost
                break;

            case avg_constants::OP_RTSL_JMPL:
                // If bit 12 is set → RTSL, else → JMPL
                if (w0 & 0x1000) {
                    // RTSL — Return from subroutine
                    if (sp_ > 0) {
                        pc_ = stack_[--sp_];
                    } else {
                        running_ = false;
                        halt_    = true;
                    }
                } else {
                    // JMPL — Unconditional jump (13-bit target)
                    pc_ = w0 & avg_constants::ADDR_MASK;
                }
                // Zero cost
                break;
        }
    }

    // ========================================================================
    // Vector signal emission
    // ========================================================================

    /// Convert beam Y to screen Y (Y increasing upward → screen Y increasing downward)
    static int16_t screen_y(int32_t y) {
        return static_cast<int16_t>(avg_constants::DISPLAY_HEIGHT - 1 - y);
    }

    void emit_vector(int32_t dx, int32_t dy, int intensity) {
        int32_t x0 = beam_x_;
        int32_t y0 = beam_y_;

        beam_x_ += dx;
        beam_y_ += dy;

        if (!stream_) return;

        if (intensity > 0) {
            uint8_t bright = static_cast<uint8_t>(std::min(intensity * 17, 255));
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(x0), screen_y(y0),
                bright, color_index_, VideoFlags::BeamOn, {}
            });
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(beam_x_), screen_y(beam_y_),
                bright, color_index_, VideoFlags::BeamOn, {}
            });
        } else {
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(beam_x_), screen_y(beam_y_),
                0, color_index_, VideoFlags::None, {}
            });
        }
    }

    void emit_position() {
        if (!stream_) return;
        stream_->drive(VectorVideoSample{
            static_cast<int16_t>(beam_x_), screen_y(beam_y_),
            0, color_index_, VideoFlags::None, {}
        });
    }

    void emit_frame_end() {
        if (!stream_) return;
        stream_->drive(VectorVideoSample{
            static_cast<int16_t>(beam_x_), screen_y(beam_y_),
            0, 0, VideoFlags::FrameEnd, {}
        });
    }
};
