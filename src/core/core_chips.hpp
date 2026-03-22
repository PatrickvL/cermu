#pragma once
// =============================================================================
// core_chips.hpp — Value-typed chip composition for Board<Spec, Chips>
// =============================================================================
//
// CoreChips<CPU, VIDEO, SOUND, IO> provides the four chip roles that
// nearly every emulated system has: a CPU, a video chip, a sound chip,
// and a primary I/O controller.  Systems add per-board extras by
// inheriting and adding fields.
//
// NoChip is a zero-cost sentinel for absent roles (e.g. LC80 has no video).
// NoChips is the default Board template parameter — opts out of typed chips.
//
// CoreChips contract (duck-typed — inheritance not required):
//   - Must provide type aliases: cpu_type, video_type, sound_type, io_type.
//   - Must provide corresponding member fields: cpu, video, sound, io.
//   - May define bind_extras(Board& board) for extra chip members.
//   - May define register_extras(BoardBase& board) for component indexing.
//
// Example:
//
//   struct MSX1Chips : CoreChips<ZilogZ80A, TMS9918A, AY_3_8910, i8255_t> {
//       // no extras needed — all four standard roles filled
//   };
//
// =============================================================================

#include <type_traits>

class BoardBase;  // forward declaration for HasRegisterExtras

// ── Sentinel for absent chip roles ──────────────────────────────────────────

struct NoChip {};

// ── Sentinel for boards that don't use typed chips ──────────────────────────

struct NoChips {};

// ── Concept: does a chips struct provide bind_extras(Board&)? ───────────────

template<typename CS, typename Board>
concept HasBindExtras = requires(CS& cs, Board& b) {
    cs.bind_extras(b);
};

// ── Concept: does a chips struct provide register_extras(BoardBase&)? ───────

template<typename CS, typename Board>
concept HasRegisterExtras = requires(CS& cs, Board& b) {
    cs.register_extras(b);
};

// ── CoreChips — generic base for the common CPU + VIDEO + SOUND + IO roles ──

template<typename CPU_T, typename VIDEO_T = NoChip, typename SOUND_T = NoChip, typename IO_T = NoChip>
struct CoreChips {
    using cpu_type   = CPU_T;
    using video_type = VIDEO_T;
    using sound_type = SOUND_T;
    using io_type    = IO_T;

    CPU_T                          cpu;
    [[no_unique_address]] VIDEO_T video;
    [[no_unique_address]] SOUND_T sound;
    [[no_unique_address]] IO_T    io;
};

// ── Type traits for chip role detection ─────────────────────────────────────
//
// Safe with any type — yields false for types without the expected aliases
// (e.g. NoChips) instead of a hard error.

template<typename CS>
inline constexpr bool has_video_v = requires {
    requires !std::is_same_v<typename CS::video_type, NoChip>;
};

template<typename CS>
inline constexpr bool has_sound_v = requires {
    requires !std::is_same_v<typename CS::sound_type, NoChip>;
};

template<typename CS>
inline constexpr bool has_io_v = requires {
    requires !std::is_same_v<typename CS::io_type, NoChip>;
};
