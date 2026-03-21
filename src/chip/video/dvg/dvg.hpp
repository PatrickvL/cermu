#pragma once
/*
 * dvg.hpp — Atari Digital Vector Generator (DVG)
 *
 * The DVG is a custom state machine used in early Atari vector arcade games
 * (Lunar Lander 1979, Asteroids 1979).  It reads a vector display list from
 * shared memory and produces X/Y beam deflection + intensity signals for
 * an XY (vector) monitor.
 *
 * Display list opcodes (4-bit command in bits [15:12] of first word):
 *
 *   0x0–0x9  VCTR   — Draw vector.  Two words.
 *                      w0: SSSS -mYY YYYY YYYY  (scale, Y sign, 10-bit ΔY)
 *                      w1: BBBB -mXX XXXX XXXX  (4-bit brightness, X sign, 10-bit ΔX)
 *   0xA      LABS   — Load absolute beam position.  Two words.
 *                      w0: 1010 00yy yyyy yyyy  (Y position)
 *                      w1: SSSS 00xx xxxx xxxx  (global scale, X position)
 *   0xB      HALT   — Stop the state machine until next VGGO trigger.  One word.
 *   0xC      JSRL   — Jump to subroutine (12-bit target address).  One word.
 *   0xD      RTSL   — Return from subroutine (pop stack).  One word.
 *   0xE      JMPL   — Jump unconditional (12-bit target address).  One word.
 *   0xF      SVEC   — Short vector.  One word.
 *                      1111 smYY BBBB SmXX  (Ss=scale, 4-bit brightness, 2-bit ΔY/ΔX)
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

    // DVG opcode types (from bits [15:12] of first word)
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

    /// Configure the DVG's vector memory pointers.
    ///
    /// The DVG accesses a 12-bit (4096-word) address space.  The board
    /// determines how RAM and ROM chips are mapped into that space:
    ///
    ///   Asteroids/LL:  RAM at word 0x000-0x3FF, gap 0x400-0x7FF,
    ///                  ROM at word 0x800-0xBFF  (rom_word_offset=0x800)
    ///
    ///   Asteroids DX:  RAM at word 0x000-0x3FF,
    ///                  ROM at word 0x400-0xBFF  (rom_word_offset=0x400)
    ///
    /// @param ram            Pointer to vector RAM (CPU-writable)
    /// @param ram_byte_size  Size of vector RAM in bytes (e.g. 2048)
    /// @param rom            Pointer to vector ROM (read-only)
    /// @param rom_byte_size  Size of vector ROM in bytes (e.g. 2048 or 4096)
    /// @param rom_word_offset  DVG word address where ROM begins (0x800 for AST/LL, 0x400 for AD)
    void set_vector_memory(const uint8_t* ram, uint16_t ram_byte_size,
                           const uint8_t* rom, uint16_t rom_byte_size,
                           uint16_t rom_word_offset = 0x800) {
        vec_ram_       = ram;
        vec_ram_size_  = ram_byte_size;
        vec_rom_       = rom;
        vec_rom_size_  = rom_byte_size;
        rom_word_offset_ = rom_word_offset;
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
        // TEMP TRACE
        if (go_count_ < 5) {
            fprintf(stderr, "[DVG VGGO] frame %d start (was %s)\n",
                    go_count_, halt_ ? "halted" : "RUNNING");
        }
        go_count_++;
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
    int      trace_count_ = 0;      // TEMP — remove after debugging
    int      go_count_ = 0;         // TEMP — VGGO trigger counter

    uint16_t stack_[dvg_constants::STACK_DEPTH] = {};  // Subroutine return stack

private:
    const uint8_t*   vec_ram_  = nullptr;    // Pointer to vector RAM
    uint16_t         vec_ram_size_ = 0;      // Size of vector RAM in bytes
    const uint8_t*   vec_rom_  = nullptr;    // Pointer to vector ROM
    uint16_t         vec_rom_size_ = 0;      // Size of vector ROM in bytes
    uint16_t         rom_word_offset_ = 0x800; // DVG word address where ROM begins
    VectorVideoStream* stream_ = nullptr;  // Output video stream

    // ========================================================================
    // Internal — opcode fetch and decode
    // ========================================================================

    /// Read a 16-bit word from vector memory at the given word address.
    ///
    /// DVG address space is 12-bit (4096 words).  The board determines
    /// where RAM and ROM sit in that space — the DVG chip itself just
    /// indexes linearly.  We use separate RAM/ROM pointers with a
    /// configurable ROM word offset to handle different board layouts:
    ///
    ///   Asteroids/LL:  RAM 0x000-0x3FF, ROM 0x800-0xBFF (gap at 0x400-0x7FF)
    ///   Asteroids DX:  RAM 0x000-0x3FF, ROM 0x400-0xBFF (no gap)
    uint16_t read_word(uint16_t word_addr) const {
        uint16_t w = word_addr & 0xFFF;

        // Vector RAM region
        uint16_t ram_words = vec_ram_size_ / 2;
        if (w < ram_words) {
            uint16_t byte_addr = w * 2;
            if (!vec_ram_ || byte_addr + 1 >= vec_ram_size_) return 0;
            return vec_ram_[byte_addr] | (vec_ram_[byte_addr + 1] << 8);
        }

        // Vector ROM region (starts at rom_word_offset_)
        if (w >= rom_word_offset_) {
            uint16_t rom_word = w - rom_word_offset_;
            uint16_t byte_addr = rom_word * 2;
            if (!vec_rom_ || byte_addr + 1 >= vec_rom_size_) return 0;
            return vec_rom_[byte_addr] | (vec_rom_[byte_addr + 1] << 8);
        }

        return 0;  // unmapped gap
    }

    /// Execute one DVG opcode at the current PC.
    /// All opcodes are decoded from w0[15:12] (top nibble of first word).
    /// Two-word opcodes: VEC (0-9), LABS (0xA).
    /// Single-word opcodes: HALT (0xB), JSR (0xC), RTS (0xD), JMP (0xE), SVEC (0xF).
    void execute_opcode() {
        uint16_t w0 = read_word(pc_);
        uint8_t opcode = (w0 >> 12) & 0xF;

        if (opcode <= dvg_constants::OP_VCTR_MAX) {
            // VCTR — Draw vector (two words)
            // w0: SSSS -mYY YYYY YYYY  (scale in [15:12], Y sign in [10], ΔY in [9:0])
            // w1: BBBB -mXX XXXX XXXX  (brightness in [15:12], X sign in [10], ΔX in [9:0])
            uint16_t w1 = read_word(pc_ + 1);

            int local_scale = opcode;  // 0-9
            int total_scale = ((int)global_scale_ + local_scale) & 0xf;

            int dy_mag = w0 & 0x03FF;
            int dy_sgn = (w0 >> 10) & 1;
            int dx_mag = w1 & 0x03FF;
            int dx_sgn = (w1 >> 10) & 1;
            int intensity = (w1 >> 12) & 0x0F;  // 4-bit brightness

            // BRM scaling: pixel delta = magnitude >> (9 - total_scale)
            // Scales 10-15: hardware counter overflows → zero-length vector
            int32_t dx = 0, dy = 0;
            if (total_scale <= 9) {
                int shift = 9 - total_scale;
                dx = dx_mag >> shift;
                dy = dy_mag >> shift;
            }
            if (dx_sgn) dx = -dx;
            if (dy_sgn) dy = -dy;

            // TEMP TRACE — first 2 frames only
            if (go_count_ <= 2 && intensity > 0) {
                fprintf(stderr, "[DVG VCTR] pc=$%03X gs=%d ls=%d ts=%d dx=%+d dy=%+d beam->(%d,%d) b=%d\n",
                        pc_, global_scale_, local_scale, total_scale,
                        dx, dy, beam_x_+dx, beam_y_+dy, intensity);
            }

            emit_vector(dx, dy, intensity);

            int vec_len = std::max(std::abs(dx), std::abs(dy));
            clocks_remaining_ = std::max(4, vec_len >> 1);
            pc_ += 2;

        } else if (opcode == dvg_constants::OP_LABS) {
            // LABS — Load absolute beam position (two words)
            // w0: 1010 00yy yyyy yyyy  (Y position in [9:0])
            // w1: SSSS 00xx xxxx xxxx  (global scale in [15:12], X position in [9:0])
            uint16_t w1 = read_word(pc_ + 1);

            beam_y_ = w0 & 0x03FF;
            beam_x_ = w1 & 0x03FF;
            global_scale_ = (w1 >> 12) & 0x0F;

            // TEMP TRACE — remove after debugging
            if (go_count_ <= 2) {
                fprintf(stderr, "[DVG LABS] pc=$%03X x=%d y=%d scale=%d (w0=$%04X w1=$%04X)\n",
                        pc_, beam_x_, beam_y_, global_scale_, w0, w1);
                trace_count_++;
            }

            emit_position();
            clocks_remaining_ = 4;
            pc_ += 2;

        } else if (opcode == dvg_constants::OP_HALT) {
            // HALT — Stop DVG (single word)
            // TEMP TRACE
            if (go_count_ <= 5) {
                fprintf(stderr, "[DVG HALT] frame %d done, beam=(%d,%d)\n", go_count_-1, beam_x_, beam_y_);
            }
            emit_frame_end();
            running_ = false;
            halt_    = true;
            pc_ += 1;

        } else if (opcode == dvg_constants::OP_JSRL) {
            // JSRL — Jump to subroutine (single word)
            // w0: 1100 aaaa aaaa aaaa  (12-bit target word address)
            if (sp_ < dvg_constants::STACK_DEPTH) {
                stack_[sp_++] = pc_ + 1;
            }
            pc_ = w0 & 0x0FFF;
            // Zero cost — falls through immediately

        } else if (opcode == dvg_constants::OP_RTSL) {
            // RTSL — Return from subroutine (single word)
            if (sp_ > 0) {
                pc_ = stack_[--sp_];
            } else {
                // Stack underflow — halt
                running_ = false;
                halt_    = true;
            }
            // Zero cost — falls through immediately

        } else if (opcode == dvg_constants::OP_JMPL) {
            // JMPL — Unconditional jump (single word)
            // w0: 1110 aaaa aaaa aaaa  (12-bit target word address)
            pc_ = w0 & 0x0FFF;
            // Zero cost — falls through immediately

        } else {
            // SVEC — Short vector (single word)
            // w0: 1111 smYY BBBB SmXX
            //   s=bit11 (scale high), m=bit10 (Y sign), YY=bits[9:8] (Y mag)
            //   BBBB=bits[7:4] (brightness, 4 bits)
            //   S=bit3 (scale low), m=bit2 (X sign), XX=bits[1:0] (X mag)
            int s_bit = (w0 >> 11) & 1;
            int S_bit = (w0 >> 3) & 1;
            int Ss = (S_bit << 1) | s_bit;  // 0-3 (S is MSB, s is LSB)
            int svec_local_scale = Ss + 2;  // SVEC base scale offset
            int total_scale = ((int)global_scale_ + svec_local_scale) & 0xf;

            int dy_mag = (w0 >> 8) & 0x03;
            int dy_sgn = (w0 >> 10) & 1;
            int dx_mag =  w0        & 0x03;
            int dx_sgn = (w0 >> 2)  & 1;
            int intensity = (w0 >> 4) & 0x0F;  // 4-bit brightness

            // Place 2-bit magnitude at top of 10-bit range, then apply VEC-style scaling
            // Scales 10-15: hardware counter overflows → zero-length vector
            int32_t dx = 0, dy = 0;
            if (total_scale <= 9) {
                int shift = 9 - total_scale;
                dx = (dx_mag << 8) >> shift;
                dy = (dy_mag << 8) >> shift;
            }
            if (dx_sgn) dx = -dx;
            if (dy_sgn) dy = -dy;

            emit_vector(dx, dy, intensity);

            clocks_remaining_ = 2 + ((std::abs(dx) + std::abs(dy)) >> 3);
            pc_ += 1;
        }
    }

    // ========================================================================
    // Vector signal emission — emit beam position samples to the stream
    // ========================================================================

    /// Global scale factor (set by LABS instruction, w1[15:12]).
    uint8_t global_scale_ = 0;

    /// Convert beam Y to screen Y — vector monitors have Y increasing upward,
    /// but screen coordinates have Y increasing downward.
    static int16_t screen_y(int32_t y) {
        return static_cast<int16_t>(dvg_constants::COORD_RANGE - 1 - y);
    }

    /// Emit a vector signal: beam moves from current position by (dx, dy).
    /// Intensity 0 = beam off (move only), 1-15 = draw with brightness.
    /// For draws, emits start and end samples with BeamOn flag so the GPU
    /// shader can extract line segments from consecutive BeamOn samples.
    ///
    /// Y is flipped for display: vector monitors have Y increasing upward,
    /// but screen coordinates have Y increasing downward.
    void emit_vector(int32_t dx, int32_t dy, int intensity) {
        int32_t x0 = beam_x_;
        int32_t y0 = beam_y_;

        // Update beam position (unwrapped for line endpoint rendering)
        beam_x_ += dx;
        beam_y_ += dy;

        if (!stream_) {
            // Still wrap for next instruction even without output
            beam_x_ &= 0x3FF;
            beam_y_ &= 0x3FF;
            return;
        }

        if (intensity > 0) {
            // Draw: emit start + end with BeamOn.
            // Map intensity (1-15) to brightness byte (17-255).
            uint8_t bright = static_cast<uint8_t>(std::min(intensity * 17, 255));
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(x0), screen_y(y0),
                bright, 0, VideoFlags::BeamOn, 0
            });
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(beam_x_), screen_y(beam_y_),
                bright, 0, VideoFlags::BeamOn, 0
            });
        } else {
            // Move: emit position without BeamOn to break the line chain
            stream_->drive(VectorVideoSample{
                static_cast<int16_t>(beam_x_), screen_y(beam_y_),
                0, 0, VideoFlags::None, 0
            });
        }

        // DVG hardware uses 10-bit counters — wrap to 0-1023 range.
        // The emit above uses the unwrapped endpoint so lines extending
        // past the edge are correctly clipped by the renderer.
        beam_x_ &= 0x3FF;
        beam_y_ &= 0x3FF;
    }

    /// Emit a beam position sample (no draw) — used by LABS.
    void emit_position() {
        if (!stream_) return;
        stream_->drive(VectorVideoSample{
            static_cast<int16_t>(beam_x_), screen_y(beam_y_),
            0, 0, VideoFlags::None, 0
        });
    }

    /// Emit frame-end signal — used by HALT.
    void emit_frame_end() {
        if (!stream_) return;
        stream_->drive(VectorVideoSample{
            static_cast<int16_t>(beam_x_), screen_y(beam_y_),
            0, 0, VideoFlags::FrameEnd, 0
        });
    }
};
