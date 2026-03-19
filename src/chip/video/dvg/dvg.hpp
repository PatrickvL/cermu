#pragma once
/*
 * dvg.hpp — Atari Digital Vector Generator (DVG)
 *
 * The DVG is a custom state machine used in early Atari vector arcade games
 * (Lunar Lander 1979, Asteroids 1979).  It reads a vector display list from
 * shared memory and produces X/Y beam deflection + intensity signals for
 * an XY (vector) monitor.
 *
 * Display list opcodes (4-bit command in bits [15:12] of second word):
 *
 *   0x0–0x9  VCTR   — Draw vector: 10-bit ΔY, 3-bit intensity (0=off),
 *                      10-bit ΔX, 4-bit scale factor.  Two words.
 *   0xA      LABS   — Load absolute beam position (10-bit X + 10-bit Y)
 *                      + global scale.  Two words.
 *   0xB      HALT   — Stop the state machine until next VGGO trigger.
 *   0xC      JSRL   — Jump to subroutine (12-bit target address).
 *   0xD      RTSL   — Return from subroutine (pop stack).
 *   0xE      JMPL   — Jump unconditional (12-bit target address).
 *   0xF      SVEC   — Short vector: 2-bit ΔY, 2-bit ΔX, 3-bit intensity,
 *                      implicit scale.  One word.
 *
 * Memory:
 *   The DVG accesses vector RAM/ROM through a 12-bit address (4 KB window).
 *   In Asteroids: $4000-$4FFF (vector RAM: $4000-$47FF, vector ROM: $5000-$57FF
 *   mapped as $4800-$4FFF in DVG address space).
 *
 * Cycle timing:
 *   The DVG runs at 1.512 MHz (same as the CPU) and takes a variable
 *   number of clocks per opcode depending on the vector length.
 *   HALT stops the state machine; VGGO restarts from address 0.
 *
 * Signal output:
 *   The DVG emits VectorVideoSample signals via a VectorVideoStream.
 *   Each sample carries {x, y, intensity, flags} describing the beam
 *   state.  Consecutive samples with BeamOn form visible line segments;
 *   the GPU shader (vector_shader.hpp) expands these into beam quads
 *   with gaussian falloff and phosphor tinting.
 *
 * The DVG exposes two bus-visible registers:
 *   Write to VGGO address → start vector state machine
 *   Write to VGRST address → reset vector state machine
 */

#include "chip/video/video_chip_base.hpp"
#include "core/signal/vector_video_stream.hpp"
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdlib>

// ============================================================================
// DVG CONSTANTS
// ============================================================================

namespace dvg_constants {

    // DVG address space is 12 bits (4 KB)
    inline constexpr uint16_t ADDR_SPACE      = 0x1000;  // 4096 bytes
    inline constexpr uint16_t ADDR_MASK        = 0x0FFF;

    // DVG subroutine stack depth (hardware has 4-level stack)
    inline constexpr int STACK_DEPTH           = 4;

    // Maximum vectors per frame (safety limit for runaway display lists)
    inline constexpr int MAX_VECTORS_PER_FRAME = 4096;

    // DVG clocks per microsecond (1.512 MHz)
    inline constexpr uint32_t CLOCK_HZ         = 1512000;

    // Rasterization display dimensions (matches XY monitor resolution)
    inline constexpr int DISPLAY_WIDTH         = 1024;
    inline constexpr int DISPLAY_HEIGHT        = 1024;

    // Coordinate range: DVG uses 10-bit X/Y (0-1023)
    inline constexpr int COORD_RANGE           = 1024;

    // DVG opcode types (from bits [15:12] of second word)
    inline constexpr uint8_t OP_VCTR_MIN       = 0x0;
    inline constexpr uint8_t OP_VCTR_MAX       = 0x9;
    inline constexpr uint8_t OP_LABS            = 0xA;
    inline constexpr uint8_t OP_HALT            = 0xB;
    inline constexpr uint8_t OP_JSRL            = 0xC;
    inline constexpr uint8_t OP_RTSL            = 0xD;
    inline constexpr uint8_t OP_JMPL            = 0xE;
    inline constexpr uint8_t OP_SVEC            = 0xF;

}  // namespace dvg_constants

// ============================================================================
// DVG STATE MACHINE
// ============================================================================

struct dvg_t : public VideoChipBase {

    dvg_t() : VideoChipBase(ChipInfo{"DVG", "Atari"}) {}

    // ========================================================================
    // Initialization
    // ========================================================================

    void init() {
        reset();
    }

    void reset() {
        pc_        = 0;
        sp_        = 0;
        beam_x_    = 0;
        beam_y_    = 0;
        running_   = false;
        halt_      = true;
        clocks_remaining_ = 0;
        std::memset(stack_, 0, sizeof(stack_));
    }

    // ========================================================================
    // Memory access — system provides vector RAM/ROM pointer
    // ========================================================================

    /// Set the pointer to the 4 KB vector memory window.
    /// The system is responsible for mapping vector RAM and ROM into this
    /// contiguous block.  The DVG reads 16-bit words from this buffer.
    void set_vector_memory(const uint8_t* mem, uint16_t size) {
        vec_mem_  = mem;
        vec_size_ = size;
    }

    /// Set the output video stream for vector signals.
    /// The stream is owned by the system's VectorVideoPort.
    void set_stream(VectorVideoStream* stream) {
        stream_ = stream;
    }

    // ========================================================================
    // Bus interface — VGGO / VGRST triggers from CPU
    // ========================================================================

    /// Trigger VGGO: start processing display list from address 0.
    void trigger_go() {
        pc_        = 0;
        sp_        = 0;
        running_   = true;
        halt_      = false;
        clocks_remaining_ = 0;
    }

    /// Trigger VGRST: reset the state machine.
    void trigger_reset() {
        reset();
    }

    /// Returns true if the DVG has halted (finished display list).
    bool is_halted() const { return halt_; }

    // ========================================================================
    // Test pattern — emit random vectors for visual verification
    // ========================================================================

    /// Generate a frame of random vectors for testing the signal pipeline.
    /// Emits ~40 random line segments within the 1024×1024 coordinate space,
    /// followed by a FrameEnd.  Call once per frame instead of tick() when
    /// no ROM is loaded.
    void generate_test_pattern() {
        if (!stream_) return;

        // Deterministic seed from frame counter for varied but repeatable frames
        uint32_t seed = test_frame_counter_++;

        auto rng = [&seed]() -> uint32_t {
            seed = seed * 1103515245u + 12345u;
            return (seed >> 16) & 0x7FFF;
        };

        int num_vectors = 30 + static_cast<int>(rng() % 20);

        // Move beam to a random starting position
        int16_t cx = static_cast<int16_t>(rng() % 1024);
        int16_t cy = static_cast<int16_t>(rng() % 1024);
        stream_->drive(VectorVideoSample{
            cx, cy, 0, 0, VideoFlags::None, 0
        });

        for (int i = 0; i < num_vectors; ++i) {
            int16_t x0 = static_cast<int16_t>(100 + rng() % 824);
            int16_t y0 = static_cast<int16_t>(100 + rng() % 824);
            int16_t x1 = static_cast<int16_t>(100 + rng() % 824);
            int16_t y1 = static_cast<int16_t>(100 + rng() % 824);
            uint8_t bright = static_cast<uint8_t>(72 + rng() % 184);

            // Move to start (break line chain)
            stream_->drive(VectorVideoSample{
                x0, y0, 0, 0, VideoFlags::None, 0
            });
            // Draw: start + end with BeamOn
            stream_->drive(VectorVideoSample{
                x0, y0, bright, 0, VideoFlags::BeamOn, 0
            });
            stream_->drive(VectorVideoSample{
                x1, y1, bright, 0, VideoFlags::BeamOn, 0
            });
        }

        // Terminate frame
        stream_->drive(VectorVideoSample{
            0, 0, 0, 0, VideoFlags::FrameEnd, 0
        });
    }

    uint32_t test_frame_counter_ = 0;

    // ========================================================================
    // Execution — call once per CPU cycle (1.512 MHz)
    // ========================================================================

    /// Tick the DVG one clock cycle.
    /// If the DVG is currently drawing a vector, it counts down clocks.
    /// When ready for the next opcode, it fetches and decodes from the
    /// display list.  Multiple opcodes may execute in a single tick if
    /// they are zero-cost (JSRL, RTSL, JMPL).
    void tick() {
        if (!running_) return;

        // If counting down a vector draw, decrement and return
        if (clocks_remaining_ > 0) {
            --clocks_remaining_;
            return;
        }

        // Process opcodes until we either draw a vector (costs clocks)
        // or halt.  Zero-cost opcodes (JSRL, RTSL, JMPL) execute
        // immediately and fall through to the next opcode.
        int safety = dvg_constants::MAX_VECTORS_PER_FRAME;
        while (running_ && clocks_remaining_ == 0 && --safety > 0) {
            execute_opcode();
        }
    }

    // ========================================================================
    // State (public for debug inspection)
    // ========================================================================

    uint16_t pc_        = 0;        // Program counter (12-bit word address)
    uint8_t  sp_        = 0;        // Stack pointer (0-3)
    int32_t  beam_x_    = 0;        // Current beam X position (signed, 10-bit range)
    int32_t  beam_y_    = 0;        // Current beam Y position (signed, 10-bit range)
    bool     running_   = false;    // True while processing display list
    bool     halt_      = true;     // True when halted (waiting for VGGO)
    int32_t  clocks_remaining_ = 0; // Clocks left for current vector draw

    uint16_t stack_[dvg_constants::STACK_DEPTH] = {};  // Subroutine return stack

private:
    const uint8_t*   vec_mem_  = nullptr;  // Pointer to 4 KB vector memory
    uint16_t         vec_size_ = 0;        // Size of vector memory in bytes
    VectorVideoStream* stream_ = nullptr;  // Output video stream

    // ========================================================================
    // Internal — opcode fetch and decode
    // ========================================================================

    /// Read a 16-bit word from vector memory at the given word address.
    uint16_t read_word(uint16_t word_addr) const {
        uint16_t byte_addr = (word_addr * 2) & (vec_size_ - 1);
        if (!vec_mem_ || byte_addr + 1 >= vec_size_) return 0;
        return vec_mem_[byte_addr] | (vec_mem_[byte_addr + 1] << 8);
    }

    /// Execute one DVG opcode at the current PC.
    void execute_opcode() {
        uint16_t w0 = read_word(pc_);

        // For single-word opcodes (SVEC, HALT, RTSL), only w0 is used.
        // For two-word opcodes, w1 contains the opcode type in [15:12].
        // But for the DVG, the first word is fetched at PC, second at PC+1.

        // DVG opcode encoding:
        // Single-word instructions test the top 4 bits of w0 directly for
        // the 0xF (SVEC) case.  All other opcodes use the second word.
        //
        // However, the DVG's actual encoding is:
        //   Word 0 (at PC):   operand data
        //   Word 1 (at PC+1): [15:12]=opcode, [11:0]=operand data
        //
        // Exception: SVEC uses only Word 0 with [15:12]=0xF

        uint8_t op_check = (w0 >> 12) & 0xF;

        if (op_check == dvg_constants::OP_SVEC) {
            // SVEC — Short vector (single word)
            // Bits: [15:12]=0xF, [11:9]=intensity, [8]=dy_sign, [7:4]=scale',
            //       [3]=dx_sign, [2:1]=dy_mag, [0]= reserved (often 0)
            //
            // Actually the DVG SVEC encoding is:
            //   [15:12] = 0xF
            //   [11]    = Y sign (1=negative)
            //   [10:8]  = intensity (0=move)
            //   [7:4]   = ΔY magnitude (2-bit, shifted)
            //   [3]     = X sign (1=negative)
            //   [2:0]   = ΔX magnitude (2-bit, shifted)
            //
            // Scale is always 2 (multiplied by 2^(2+scale_from_context)?).
            // The real DVG applies the implicit scale of the current VCTR
            // scale factor / binary rate multiplier.  For simplicity, use
            // the standard SVEC formula: delta = magnitude << (scale + 2)

            int intensity = (w0 >> 4) & 0x07;

            int dy_mag = (w0 >> 8) & 0x03;
            int dy_sgn = (w0 >> 10) & 0x01;
            int dx_mag =  w0        & 0x03;
            int dx_sgn = (w0 >> 2)  & 0x01;

            // SVEC uses a fixed scale shift of 2 (equivalent to scale=2 in VCTR)
            int shift = 2 + 2;  // base 2 + SVEC implicit 2

            int32_t dx = dx_mag << shift;
            int32_t dy = dy_mag << shift;
            if (dx_sgn) dx = -dx;
            if (dy_sgn) dy = -dy;

            emit_vector(dx, dy, intensity);

            // SVEC cost: small fixed cost
            clocks_remaining_ = 2 + ((std::abs(dx) + std::abs(dy)) >> 3);
            pc_ += 1;
            return;
        }

        // Two-word opcodes: fetch second word
        uint16_t w1 = read_word(pc_ + 1);
        uint8_t opcode = (w1 >> 12) & 0xF;

        switch (opcode) {
            case dvg_constants::OP_LABS: {
                // LABS — Load absolute beam position
                // w0: [9:0]=X position, [11:10]=global scale
                // w1: [15:12]=0xA, [9:0]=Y position
                beam_x_ = w0 & 0x03FF;
                beam_y_ = w1 & 0x03FF;
                global_scale_ = (w0 >> 10) & 0x03;
                emit_position();
                clocks_remaining_ = 4;
                pc_ += 2;
                break;
            }

            case dvg_constants::OP_HALT: {
                // HALT — Stop the DVG; emit FrameEnd signal
                emit_frame_end();
                running_ = false;
                halt_    = true;
                pc_ += 2;
                break;
            }

            case dvg_constants::OP_JSRL: {
                // JSRL — Jump to subroutine
                // w1: [11:0]=target word address
                if (sp_ < dvg_constants::STACK_DEPTH) {
                    stack_[sp_++] = pc_ + 2;
                }
                pc_ = w1 & 0x0FFF;
                // Zero cost — falls through immediately
                break;
            }

            case dvg_constants::OP_RTSL: {
                // RTSL — Return from subroutine
                if (sp_ > 0) {
                    pc_ = stack_[--sp_];
                } else {
                    // Stack underflow — halt
                    running_ = false;
                    halt_    = true;
                }
                // Zero cost — falls through immediately
                break;
            }

            case dvg_constants::OP_JMPL: {
                // JMPL — Unconditional jump
                // w1: [11:0]=target word address
                pc_ = w1 & 0x0FFF;
                // Zero cost — falls through immediately
                break;
            }

            default: {
                // VCTR — Draw vector (opcodes 0x0 through 0x9 = scale factor)
                // The opcode field IS the scale factor (0-9).
                //
                // w0: [12]=Y sign, [9:0]=ΔY magnitude
                // w1: [15:12]=scale, [12]=X sign, [9:0]=ΔX magnitude
                //
                // Actual delta = magnitude << scale (binary rate multiplier)

                int scale = opcode;  // 0-9

                int dy_mag = w0 & 0x03FF;
                int dy_sgn = (w0 >> 10) & 0x01;
                int dx_mag = w1 & 0x03FF;
                int dx_sgn = (w1 >> 10) & 0x01;

                int intensity = (w0 >> 12) & 0x07;

                // Apply scale: shift magnitude by (9 - scale).
                // Scale 9 = no shift (fastest), scale 0 = shift by 9 (longest vector).
                // Wait — the DVG uses the scale as the number of shifts:
                //   Effective delta = magnitude * 2^(scale_factor - 9)
                //   But if scale < 9, that's a right-shift (shorter vector).
                //
                // Actually: the BRM (binary rate multiplier) timing means
                // the vector length in clocks depends on magnitude >> (9 - scale).
                // For rendering, the pixel delta = magnitude >> (9 - scale).

                int shift = 9 - scale;
                int32_t dx, dy;
                if (shift >= 0) {
                    dx = dx_mag >> shift;
                    dy = dy_mag >> shift;
                } else {
                    dx = dx_mag << (-shift);
                    dy = dy_mag << (-shift);
                }
                if (dx_sgn) dx = -dx;
                if (dy_sgn) dy = -dy;

                emit_vector(dx, dy, intensity);

                // Vector draw time proportional to magnitude at current scale.
                // The BRM clocks = 2^(13 - scale) for maximum-length vectors,
                // but short vectors take less time.  Approximate:
                int vec_len = std::max(std::abs(dx), std::abs(dy));
                clocks_remaining_ = std::max(4, vec_len >> 1);
                pc_ += 2;
                break;
            }
        }
    }

    // ========================================================================
    // Vector signal emission — emit beam position samples to the stream
    // ========================================================================

    /// Global scale factor (set by LABS instruction, bits [11:10] of w0).
    uint8_t global_scale_ = 0;

    /// Emit a vector signal: beam moves from current position by (dx, dy).
    /// Intensity 0 = beam off (move only), 1-7 = draw with brightness.
    /// For draws, emits start and end samples with BeamOn flag so the GPU
    /// shader can extract line segments from consecutive BeamOn samples.
    void emit_vector(int32_t dx, int32_t dy, int intensity) {
        int32_t x0 = beam_x_;
        int32_t y0 = beam_y_;

        // Update beam position
        beam_x_ += dx;
        beam_y_ += dy;

        if (!stream_) return;

        if (intensity > 0) {
            // Draw: emit start + end with BeamOn.
            // Map intensity (1-7) to brightness byte (36-252).
            uint8_t bright = static_cast<uint8_t>(std::min(intensity * 36, 255));
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(x0), static_cast<int16_t>(y0),
                bright, 0, VideoFlags::BeamOn, 0
            });
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(beam_x_), static_cast<int16_t>(beam_y_),
                bright, 0, VideoFlags::BeamOn, 0
            });
        } else {
            // Move: emit position without BeamOn to break the line chain
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(beam_x_), static_cast<int16_t>(beam_y_),
                0, 0, VideoFlags::None, 0
            });
        }
    }

    /// Emit a beam position sample (no draw) — used by LABS.
    void emit_position() {
        if (!stream_) return;
        stream_->drive(VectorVideoSample{
            static_cast<int16_t>(beam_x_), static_cast<int16_t>(beam_y_),
            0, 0, VideoFlags::None, 0
        });
    }

    /// Emit frame-end signal — used by HALT.
    void emit_frame_end() {
        if (!stream_) return;
        stream_->drive(VectorVideoSample{
            static_cast<int16_t>(beam_x_), static_cast<int16_t>(beam_y_),
            0, 0, VideoFlags::FrameEnd, 0
        });
    }
};
