#pragma once
// =============================================================================
// standard_chips.hpp — Value-typed chip composition for Board<Spec, ChipSet>
// =============================================================================
//
// StandardChips<CPU, VDP, PSG> provides the three chip roles that nearly
// every emulated system has: a CPU, a video chip, and a sound chip.
// Systems add per-board extras by inheriting and adding fields.
//
// NoChip is a zero-cost sentinel for absent roles (e.g. LC80 has no VDP).
// NoChipSet is the default Board template parameter — opts out of typed chips.
//
// ChipSet contract:
//   - Must inherit StandardChips<CPU, VDP, PSG>  (or at least provide the
//     three type aliases: cpu_type, vdp_type, psg_type).
//   - May define bind_extras(Board& board) for extra chip members.
//
// Example:
//
//   struct MSX1Chips : StandardChips<ZilogZ80A, TMS9918A, AY_3_8910> {
//       i8255_t ppi;
//       template<typename B> void bind_extras(B& board) {
//           board.bind_chip(board.template find_index<i8255_t>(), &ppi);
//       }
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

// ── StandardChips — generic base for the common CPU + VDP + PSG roles ───────

template<typename CPU_T, typename VDP_T = NoChip, typename PSG_T = NoChip>
struct StandardChips {
    using cpu_type = CPU_T;
    using vdp_type = VDP_T;
    using psg_type = PSG_T;

    CPU_T cpu;
    VDP_T vdp;
    PSG_T psg;
};

// ── Type traits for chip role detection ─────────────────────────────────────

template<typename CS>
inline constexpr bool has_vdp_v = !std::is_same_v<typename CS::vdp_type, NoChip>;

template<typename CS>
inline constexpr bool has_psg_v = !std::is_same_v<typename CS::psg_type, NoChip>;
