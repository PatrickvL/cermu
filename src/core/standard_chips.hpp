#pragma once
// =============================================================================
// standard_chips.hpp — Value-typed chip composition for Board<Spec, ChipSet>
// =============================================================================
//
// CommonBoardChips<CPU, VIDEO, SOUND, IO> provides the four chip roles that
// nearly every emulated system has: a CPU, a video chip, a sound chip,
// and a primary I/O controller.  Systems add per-board extras by
// inheriting and adding fields.
//
// NoChip is a zero-cost sentinel for absent roles (e.g. LC80 has no video).
// NoChipSet is the default Board template parameter — opts out of typed chips.
//
// ChipSet contract:
//   - Must inherit CommonBoardChips<CPU, VIDEO, SOUND, IO>  (or at least
//     provide the type aliases: cpu_type, video_type, sound_type, io_type).
//   - May define bind_extras(Board& board) for extra chip members.
//
// Example:
//
//   struct MSX1Chips : CommonBoardChips<ZilogZ80A, TMS9918A, AY_3_8910, i8255_t> {
//       // no extras needed — all four standard roles filled
//   };
//
// =============================================================================

#include <type_traits>

// ── Sentinel for absent chip roles ──────────────────────────────────────────

struct NoChip {};

// ── Sentinel for boards that don't use typed chips ──────────────────────────

struct NoChipSet {};

// ── Concept: does a ChipSet provide bind_extras(Board&)? ────────────────────

template<typename CS, typename Board>
concept HasBindExtras = requires(CS& cs, Board& b) {
    cs.bind_extras(b);
};

// ── Concept: does a ChipSet provide register_extras(BoardBase&)? ────────────

template<typename CS>
concept HasRegisterExtras = requires(CS& cs, BoardBase& b) {
    cs.register_extras(b);
};

// ── CommonBoardChips — generic base for the common CPU + VIDEO + SOUND + IO roles

template<typename CPU_T, typename VIDEO_T = NoChip, typename SOUND_T = NoChip, typename IO_T = NoChip>
struct CommonBoardChips {
    using cpu_type   = CPU_T;
    using video_type = VIDEO_T;
    using sound_type = SOUND_T;
    using io_type    = IO_T;

    CPU_T   cpu;
    VIDEO_T video;
    SOUND_T sound;
    IO_T    io;
};

// ── Type traits for chip role detection ─────────────────────────────────────

template<typename CS>
inline constexpr bool has_video_v = !std::is_same_v<typename CS::video_type, NoChip>;

template<typename CS>
inline constexpr bool has_sound_v = !std::is_same_v<typename CS::sound_type, NoChip>;

template<typename CS>
inline constexpr bool has_io_v = !std::is_same_v<typename CS::io_type, NoChip>;
