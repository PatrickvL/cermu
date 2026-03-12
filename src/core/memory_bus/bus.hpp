// =============================================================================
// bus.hpp — MemoryBus main class and BusView accessor
// =============================================================================
//
// The MemoryBus is the central dispatch engine.  It maps addresses to chips
// through per-viewer page tables, with optional recursive sub-page dispatch.
//
// Hot-path read/write:
//   page_of(addr) → chip_table[page] → chip_id
//     if chip_id < kReadSentinelMin  →  direct buffer access (~5 instructions)
//     else                           →  slow path (open bus, sub-table, MMIO)
//
// Sub-table dispatch (slow path only, zero cost when unused):
//   chip_id is an indexed or masked sub-table sentinel → resolve iteratively
//   until a terminal chip id (direct, kNoChipSelected, or MMIO) is reached.
//   Supports arbitrary recursion depth (bounded by total sub-table pool).
//
// BusView is a thin compile-time–bound wrapper: BusView<Spec, ViewerId> binds
// a viewer id at compile time so call sites pass only bus_state_t.
//
// =============================================================================
#pragma once

#include "core/memory_bus/masks.hpp"
#include "core/memory_bus/mmio.hpp"
#include "core/memory_bus/sub_tables.hpp"
#include "core/memory_bus/viewer.hpp"

#include "core/system_lines.hpp"   // bus_state_t, BUS_GET_DATA, BUS_SET_DATA, BUS_GET_ADDR, …
#include "core/cermu.hpp"          // FORCE_INLINE, likely(), unlikely()

#include <array>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <variant>


// =============================================================================
// §1  MemoryBus
// =============================================================================

template<BusSpecConcept Spec = DefaultBusSpec>
class MemoryBus {
public:
    using Addr        = typename Spec::AddrType;
    using DataType    = spec_data_type_t<Spec>;
    using PT          = PackingTraits<Spec>;
    using Viewer      = ViewerState<Spec>;
    using Snapshot    = ModeSnapshot<Spec>;
    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;
    using BlockId     = typename PT::BlockId;
    using PageSlot    = typename PT::PageSlot;

    static constexpr bool   kPartialBus     = spec_partial_bus_v<Spec>;
    static constexpr bool   kCsLines        = PT::kCsLines;
    static constexpr size_t kCsLineBits     = PT::kCsLineBits;
    static constexpr size_t kNumPages       = Viewer::kNumPages;
    static constexpr size_t kPageSize       = Viewer::kPageSize;
    static constexpr size_t kPageMask       = Viewer::kPageMask;
    static constexpr size_t kNumViewers     = Spec::NumViewers;
    static constexpr size_t kMaxHandlers    = Spec::EnableMmio ? Spec::MaxMmioHandlers : 0;
    static constexpr bool   kHasIndexedSub  = PT::kHasIndexedSub;
    static constexpr bool   kHasMaskedSub   = PT::kHasMaskedSub;
    static constexpr bool   kHasSubTables   = PT::kHasSubTables;
    static constexpr size_t kMaxIndexedSubs = PT::kMaxIndexedSubs;
    static constexpr size_t kMaxMaskedSubs  = PT::kMaxMaskedSubs;

    MemoryBus() noexcept { reset_all_viewers(); }

    // =========================================================================
    // §1.1  CS resolve — address decode only (CS-enabled systems)
    // =========================================================================
    //
    // Performs the same page-table lookup and sub-table resolution as
    // read()/write(), but does NOT transfer data.  Instead it embeds the
    // resolved chip id into the bus_state_t CS field.  Each chip's tick then
    // checks  get_cs_from_bus(bus) == MY_CHIP_ID  to decide if it is selected.
    //
    // CS resolve is the emulated equivalent of the combinational address
    // decoder (PLA, 74138, etc.) asserting a single /CS line per bus cycle.
    //
    // MMIO-only chips handle register access in-place during their tick.
    // Buffer-backed chips (RAM, ROM) call service_read/service_write to
    // perform the unified-buffer transfer.
    //
    // When CsLineBits = 0 this function is not available — use read()/write()
    // or tick() with callback-based MMIO handlers instead.
    //

    [[nodiscard]] FORCE_INLINE
    bus_state_t resolve(size_t viewer_id, bus_state_t bus) const noexcept
        requires(kCsLines)
    {
        const Addr addr = Addr(BUS_GET_ADDR(bus));

        if (BUS_GET_BIT(bus, BUS_RW_BIT)) {
            ChipId chip_id = viewers_[viewer_id].read_chip(Viewer::page_of(addr));
            if constexpr (kHasSubTables) {
                if (unlikely(chip_id >= PT::kReadSentinelMin))
                    chip_id = resolve_read_terminal(viewer_id, chip_id, addr);
            }
            set_cs(bus, size_t(chip_id));
        } else {
            WriteChipId chip_id = viewers_[viewer_id].write_chip(Viewer::page_of(addr));
            if constexpr (kHasSubTables) {
                if (unlikely(chip_id >= PT::kWriteSentinelMin))
                    chip_id = resolve_write_terminal(viewer_id, chip_id, addr);
            }
            set_cs(bus, size_t(chip_id));
        }

        return bus;
    }

    // =========================================================================
    // §1.1b  Post-resolve buffer access (CS-enabled systems)
    // =========================================================================
    //
    // Called by buffer-backed chips (RAM, ROM) during their tick, after
    // resolve() has embedded the chip id in the CS field.  Extracts CS from
    // bus, performs the unified-buffer read or write.
    //
    // For MMIO chips this is unnecessary — they handle registers in-place.
    //

    [[nodiscard]] FORCE_INLINE
    bus_state_t service_read(bus_state_t bus) const noexcept
        requires(kCsLines)
    {
        const ChipId chip_id = get_cs(bus);
        if (likely(chip_id < PT::kReadSentinelMin))
            return read_buffer_no_cs(chip_id, bus);
        return bus;  // not a buffer chip — caller handles
    }

    FORCE_INLINE
    bus_state_t service_write(bus_state_t bus) noexcept
        requires(kCsLines)
    {
        const ChipId chip_id = get_cs(bus);
        if (likely(WriteChipId(chip_id) < PT::kWriteSentinelMin))
            return write_buffer_no_cs(WriteChipId(chip_id), bus);
        return bus;  // not a buffer chip — caller handles
    }

    [[nodiscard]] FORCE_INLINE
    bus_state_t service(bus_state_t bus) noexcept
        requires(kCsLines)
    {
        return BUS_GET_BIT(bus, BUS_RW_BIT)
            ? service_read(bus) : service_write(bus);
    }

    // =========================================================================
    // §1.2  Hot-path read (non-CS / callback-driven systems)
    // =========================================================================
    //
    // Full dispatch: page lookup → buffer access or MMIO handler callback.
    // Fast path (chip_id < kReadSentinelMin, full data bus):
    //   page_of → read_chip → CMP → buffer index → BUS_SET_DATA
    //   ≈ 5–6 instructions; zero branches taken on happy path.
    //

    [[nodiscard]] FORCE_INLINE
    bus_state_t read(size_t viewer_id, bus_state_t bus) noexcept {
        const Addr   addr    = Addr(BUS_GET_ADDR(bus));
        const ChipId chip_id = viewers_[viewer_id].read_chip(Viewer::page_of(addr));

        if (likely(chip_id < PT::kReadSentinelMin))
            return read_buffer(chip_id, bus);

        return read_slow(viewer_id, chip_id, bus);
    }

    // =========================================================================
    // §1.3  Hot-path write (non-CS / callback-driven systems)
    // =========================================================================

    FORCE_INLINE
    bus_state_t write(size_t viewer_id, bus_state_t bus) noexcept {
        const Addr        addr    = Addr(BUS_GET_ADDR(bus));
        const WriteChipId chip_id = viewers_[viewer_id].write_chip(Viewer::page_of(addr));

        if (likely(chip_id < PT::kWriteSentinelMin))
            return write_buffer(chip_id, bus);

        return write_slow(viewer_id, chip_id, bus);
    }

    // =========================================================================
    // §1.4  Combined tick (checks R/W bit in bus_state_t)
    // =========================================================================

    [[nodiscard]] FORCE_INLINE
    bus_state_t tick(size_t viewer_id, bus_state_t bus) noexcept {
        return BUS_GET_BIT(bus, BUS_RW_BIT)
            ? read (viewer_id, bus)
            : write(viewer_id, bus);
    }

    // =========================================================================
    // §1.4  Chip mapping API
    // =========================================================================
    //
    // Programs the per-viewer chip tables — the emulator's equivalent of wiring
    // address decoder outputs to chip /CS pins.
    //

    void set_page(size_t viewer_id, size_t page,
                  ChipId rd, WriteChipId wr) noexcept {
        assert(viewer_id < kNumViewers && page < kNumPages);
        viewers_[viewer_id].set_chip(page, rd, wr);
    }

    void set_read_page(size_t viewer_id, size_t page, ChipId id) noexcept {
        assert(viewer_id < kNumViewers && page < kNumPages);
        viewers_[viewer_id].set_read_chip(page, id);
    }

    void set_write_page(size_t viewer_id, size_t page, WriteChipId id) noexcept {
        assert(viewer_id < kNumViewers && page < kNumPages);
        viewers_[viewer_id].set_write_chip(page, id);
    }

    void fill_read_pages(size_t viewer_id, size_t first_page,
                         size_t count, ChipId first_chip) noexcept {
        auto& v = viewers_[viewer_id];
        for (size_t i = 0; i < count; ++i)
            v.set_read_chip(first_page + i, ChipId(first_chip + i));
    }

    void fill_write_pages(size_t viewer_id, size_t first_page,
                          size_t count, WriteChipId first_chip) noexcept {
        auto& v = viewers_[viewer_id];
        for (size_t i = 0; i < count; ++i)
            v.set_write_chip(first_page + i, WriteChipId(first_chip + i));
    }

    void fill_pages(size_t viewer_id, size_t first_page, size_t count,
                    ChipId rd_first, WriteChipId wr_first) noexcept {
        auto& v = viewers_[viewer_id];
        for (size_t i = 0; i < count; ++i)
            v.set_chip(first_page + i, ChipId(rd_first + i), WriteChipId(wr_first + i));
    }

    // All pages in range → same chip id (mirrored regions, ROM banks, etc.)
    void fill_read_constant(size_t viewer_id, size_t first_page,
                            size_t count, ChipId chip) noexcept {
        auto& v = viewers_[viewer_id];
        for (size_t i = 0; i < count; ++i)
            v.set_read_chip(first_page + i, chip);
    }

    void fill_write_constant(size_t viewer_id, size_t first_page,
                             size_t count, WriteChipId chip) noexcept {
        auto& v = viewers_[viewer_id];
        for (size_t i = 0; i < count; ++i)
            v.set_write_chip(first_page + i, chip);
    }

    // ── Register-file chip (MMIO) mapping ─────────────────────────────────────

    void map_read_register_file(size_t viewer_id, size_t first_page,
                                size_t count, size_t handler_idx) noexcept
        requires(Spec::EnableMmio)
    {
        assert(handler_idx < mmio_count_);
        fill_read_constant(viewer_id, first_page, count,
                           ChipId(PT::kRegChipBase + handler_idx));
    }

    void map_write_register_file(size_t viewer_id, size_t first_page,
                                 size_t count, size_t handler_idx) noexcept
        requires(Spec::EnableMmio)
    {
        assert(handler_idx < mmio_count_);
        fill_write_constant(viewer_id, first_page, count,
                            WriteChipId(PT::kRegChipBaseWrite + handler_idx));
    }

    void map_register_file(size_t viewer_id, size_t first_page,
                           size_t count, size_t handler_idx) noexcept
        requires(Spec::EnableMmio)
    {
        map_read_register_file (viewer_id, first_page, count, handler_idx);
        map_write_register_file(viewer_id, first_page, count, handler_idx);
    }

    // ── No chip selected (unasserted CS lines) ────────────────────────────────

    void map_no_chip_selected(size_t viewer_id, size_t first_page,
                              size_t count) noexcept {
        auto& v = viewers_[viewer_id];
        for (size_t i = 0; i < count; ++i)
            v.set_chip(first_page + i, PT::kNoChipSelected, PT::kNoChipSelectedWrite);
    }

    void map_read_no_chip_selected(size_t viewer_id, size_t first_page,
                                   size_t count) noexcept {
        fill_read_constant(viewer_id, first_page, count, PT::kNoChipSelected);
    }

    void map_write_no_chip_selected(size_t viewer_id, size_t first_page,
                                    size_t count) noexcept {
        fill_write_constant(viewer_id, first_page, count, PT::kNoChipSelectedWrite);
    }

    // =========================================================================
    // §1.5  Partial data-bus mask API  (only when EnablePartialBus = true)
    // =========================================================================

    void set_chip_data_mask(size_t chip_id, DataType mask) noexcept
        requires(kPartialBus) { bus_masks_.set(chip_id, mask); }

    void set_chip_read_mask(size_t chip_id, DataType mask) noexcept
        requires(kPartialBus) { bus_masks_.set_read(chip_id, mask); }

    void set_chip_write_mask(size_t chip_id, DataType mask) noexcept
        requires(kPartialBus) { bus_masks_.set_write(chip_id, mask); }

    void set_chip_data_mask_range(size_t first_chip_id, size_t count,
                                  DataType mask) noexcept
        requires(kPartialBus)
    {
        for (size_t i = 0; i < count; ++i)
            bus_masks_.set(first_chip_id + i, mask);
    }

    [[nodiscard]] DataType chip_read_mask (size_t chip_id) const noexcept
        requires(kPartialBus) { return bus_masks_.read (chip_id); }

    [[nodiscard]] DataType chip_write_mask(size_t chip_id) const noexcept
        requires(kPartialBus) { return bus_masks_.write(chip_id); }

    // =========================================================================
    // §1.6  Chip-id query from bus word (only when CsLineBits > 0)
    // =========================================================================

    [[nodiscard]] static ChipId get_chip_from_bus(bus_state_t bus) noexcept
        requires(kCsLines)
    {
        return get_cs(bus);
    }

    [[nodiscard]] static ChipId get_cs_from_bus(bus_state_t bus) noexcept
        requires(kCsLines) { return get_chip_from_bus(bus); }

    // =========================================================================
    // §1.7  Register-file (MMIO) handler registry
    // =========================================================================

    [[nodiscard]]
    int register_handler(MmioHandler handler) noexcept
        requires(Spec::EnableMmio)
    {
        if (mmio_count_ >= kMaxHandlers) return -1;
        const int idx = int(mmio_count_++);
        handlers_[idx] = handler;
        return idx;
    }

    [[nodiscard]] int register_mmio(MmioHandler h) noexcept
        requires(Spec::EnableMmio) { return register_handler(h); }

    void update_handler(size_t idx, MmioHandler handler) noexcept
        requires(Spec::EnableMmio)
    {
        assert(idx < mmio_count_);
        handlers_[idx] = handler;
    }

    void update_mmio(size_t idx, MmioHandler h) noexcept
        requires(Spec::EnableMmio) { update_handler(idx, h); }

    [[nodiscard]] MmioHandler&       handler(size_t idx)       noexcept
        requires(Spec::EnableMmio) { return handlers_[idx]; }
    [[nodiscard]] const MmioHandler& handler(size_t idx) const noexcept
        requires(Spec::EnableMmio) { return handlers_[idx]; }
    [[nodiscard]] size_t handler_count() const noexcept { return mmio_count_; }

    // =========================================================================
    // §1.8  Mode snapshot support
    // =========================================================================
    //
    // Snapshots capture main-level chip tables only — data-bus masks are fixed
    // physical wiring and sub-table contents are secondary decode configuration.
    // Both are orthogonal to bank switching.
    //

    void save_snapshot(size_t viewer_id, Snapshot& snap) const noexcept {
        assert(viewer_id < kNumViewers);
        const auto& v = viewers_[viewer_id];
        std::memcpy(snap.chip_table.data(), v.chip_table_.data(),
                    kNumPages * sizeof(PageSlot));
        if constexpr (!PT::kPackedRW)
            std::memcpy(snap.write_chip_table.data(), v.write_chip_table_.data(),
                        kNumPages * sizeof(BlockId));
    }

    void load_snapshot(size_t viewer_id, const Snapshot& snap) noexcept {
        assert(viewer_id < kNumViewers);
        auto& v = viewers_[viewer_id];
        std::memcpy(v.chip_table_.data(), snap.chip_table.data(),
                    kNumPages * sizeof(PageSlot));
        if constexpr (!PT::kPackedRW)
            std::memcpy(v.write_chip_table_.data(), snap.write_chip_table.data(),
                        kNumPages * sizeof(BlockId));
    }

    // =========================================================================
    // §1.9  Viewer management
    // =========================================================================

    void copy_viewer(size_t src_id, size_t dst_id) noexcept {
        assert(src_id < kNumViewers && dst_id < kNumViewers);
        viewers_[dst_id] = viewers_[src_id];
    }

    void reset_viewer(size_t viewer_id) noexcept {
        assert(viewer_id < kNumViewers);
        viewers_[viewer_id].reset();
    }

    void reset_all_viewers() noexcept {
        for (size_t i = 0; i < kNumViewers; ++i)
            viewers_[i].reset();
    }

    // =========================================================================
    // §1.10  Unified buffer management
    // =========================================================================

    void set_unified_buffer(uint8_t* buf) noexcept { unified_buf_ = buf; }
    [[nodiscard]] uint8_t*       unified_buffer()       noexcept { return unified_buf_; }
    [[nodiscard]] const uint8_t* unified_buffer() const noexcept { return unified_buf_; }

    // =========================================================================
    // §1.11  Low-level viewer access
    // =========================================================================

    [[nodiscard]] Viewer&       viewer(size_t id)       noexcept { return viewers_[id]; }
    [[nodiscard]] const Viewer& viewer(size_t id) const noexcept { return viewers_[id]; }

    // =========================================================================
    // §1.12  Indexed sub-table API
    // =========================================================================
    //
    // Creates and configures indexed sub-tables for O(1) sub-page dispatch.
    //
    // add_indexed_sub_table() allocates a sub-table with the given sub_bits
    // and bit_shift, returning its index (or -1 if pool full).  Entries
    // default to kNoChipSelected.
    //
    // Use indexed_sub_chip() / indexed_sub_write_chip() to get the sentinel
    // chip ids for routing a parent page or sub-table entry to this sub-table.
    //

    [[nodiscard]]
    int add_indexed_sub_table(size_t viewer_id,
                              uint8_t sub_bits, uint8_t bit_shift) noexcept
        requires(kHasIndexedSub)
    {
        assert(viewer_id < kNumViewers);
        auto& count = indexed_sub_count_[viewer_id];
        if (count >= kMaxIndexedSubs) return -1;
        const int idx = int(count++);
        indexed_subs_[viewer_id][idx].init(sub_bits, bit_shift);
        return idx;
    }

    void set_indexed_entry(size_t viewer_id, size_t table_idx,
                           size_t entry_idx,
                           ChipId rd, WriteChipId wr) noexcept
        requires(kHasIndexedSub)
    {
        assert(viewer_id < kNumViewers);
        assert(table_idx < indexed_sub_count_[viewer_id]);
        indexed_subs_[viewer_id][table_idx].set_entry(entry_idx, rd, wr);
    }

    void set_indexed_read_entry(size_t viewer_id, size_t table_idx,
                                size_t entry_idx, ChipId id) noexcept
        requires(kHasIndexedSub)
    {
        assert(viewer_id < kNumViewers);
        assert(table_idx < indexed_sub_count_[viewer_id]);
        indexed_subs_[viewer_id][table_idx].set_read_entry(entry_idx, id);
    }

    void set_indexed_write_entry(size_t viewer_id, size_t table_idx,
                                 size_t entry_idx, WriteChipId id) noexcept
        requires(kHasIndexedSub)
    {
        assert(viewer_id < kNumViewers);
        assert(table_idx < indexed_sub_count_[viewer_id]);
        indexed_subs_[viewer_id][table_idx].set_write_entry(entry_idx, id);
    }

    // Sentinel chip ids for routing to an indexed sub-table.
    [[nodiscard]] static constexpr ChipId indexed_sub_chip(size_t idx) noexcept
        requires(kHasIndexedSub)
    {
        return ChipId(size_t(PT::kIndexedSubBase) + idx);
    }

    [[nodiscard]] static constexpr WriteChipId indexed_sub_write_chip(size_t idx) noexcept
        requires(kHasIndexedSub)
    {
        return WriteChipId(size_t(PT::kIndexedSubBaseWrite) + idx);
    }

    // Convenience: set a page to route through an indexed sub-table.
    void map_to_indexed_sub(size_t viewer_id, size_t page,
                            size_t table_idx) noexcept
        requires(kHasIndexedSub)
    {
        set_page(viewer_id, page,
                 indexed_sub_chip(table_idx),
                 indexed_sub_write_chip(table_idx));
    }

    // Direct access to an indexed sub-table (for advanced manipulation).
    [[nodiscard]] IndexedSubTable<Spec>& indexed_sub(size_t viewer_id,
                                                     size_t table_idx) noexcept
        requires(kHasIndexedSub)
    {
        return indexed_subs_[viewer_id][table_idx];
    }

    [[nodiscard]] const IndexedSubTable<Spec>& indexed_sub(
        size_t viewer_id, size_t table_idx) const noexcept
        requires(kHasIndexedSub)
    {
        return indexed_subs_[viewer_id][table_idx];
    }

    // =========================================================================
    // §1.13  Masked sub-table API
    // =========================================================================
    //
    // Creates and configures masked sub-tables for sparse address-match dispatch.
    //

    [[nodiscard]]
    int add_masked_sub_table(size_t viewer_id,
                             ChipId base_read,
                             WriteChipId base_write) noexcept
        requires(kHasMaskedSub)
    {
        assert(viewer_id < kNumViewers);
        auto& count = masked_sub_count_[viewer_id];
        if (count >= kMaxMaskedSubs) return -1;
        const int idx = int(count++);
        auto& tbl      = masked_subs_[viewer_id][idx];
        tbl.base_read  = base_read;
        tbl.base_write = base_write;
        tbl.num_regions = 0;
        return idx;
    }

    bool add_masked_region(size_t viewer_id, size_t table_idx,
                           Addr mask, Addr match,
                           ChipId rd, WriteChipId wr) noexcept
        requires(kHasMaskedSub)
    {
        assert(viewer_id < kNumViewers);
        assert(table_idx < masked_sub_count_[viewer_id]);
        return masked_subs_[viewer_id][table_idx].add_region(mask, match, rd, wr);
    }

    void set_masked_base(size_t viewer_id, size_t table_idx,
                         ChipId rd, WriteChipId wr) noexcept
        requires(kHasMaskedSub)
    {
        assert(viewer_id < kNumViewers);
        assert(table_idx < masked_sub_count_[viewer_id]);
        masked_subs_[viewer_id][table_idx].set_base(rd, wr);
    }

    [[nodiscard]] static constexpr ChipId masked_sub_chip(size_t idx) noexcept
        requires(kHasMaskedSub)
    {
        return ChipId(size_t(PT::kMaskedSubBase) + idx);
    }

    [[nodiscard]] static constexpr WriteChipId masked_sub_write_chip(size_t idx) noexcept
        requires(kHasMaskedSub)
    {
        return WriteChipId(size_t(PT::kMaskedSubBaseWrite) + idx);
    }

    void map_to_masked_sub(size_t viewer_id, size_t page,
                           size_t table_idx) noexcept
        requires(kHasMaskedSub)
    {
        set_page(viewer_id, page,
                 masked_sub_chip(table_idx),
                 masked_sub_write_chip(table_idx));
    }

    [[nodiscard]] MaskedSubTable<Spec>& masked_sub(size_t viewer_id,
                                                    size_t table_idx) noexcept
        requires(kHasMaskedSub)
    {
        return masked_subs_[viewer_id][table_idx];
    }

    [[nodiscard]] const MaskedSubTable<Spec>& masked_sub(
        size_t viewer_id, size_t table_idx) const noexcept
        requires(kHasMaskedSub)
    {
        return masked_subs_[viewer_id][table_idx];
    }

    // =========================================================================
    // §1.14  Sub-table reset
    // =========================================================================

    void reset_indexed_subs(size_t viewer_id) noexcept
        requires(kHasIndexedSub)
    {
        assert(viewer_id < kNumViewers);
        for (size_t i = 0; i < indexed_sub_count_[viewer_id]; ++i)
            indexed_subs_[viewer_id][i].reset();
        indexed_sub_count_[viewer_id] = 0;
    }

    void reset_masked_subs(size_t viewer_id) noexcept
        requires(kHasMaskedSub)
    {
        assert(viewer_id < kNumViewers);
        for (size_t i = 0; i < masked_sub_count_[viewer_id]; ++i)
            masked_subs_[viewer_id][i].reset();
        masked_sub_count_[viewer_id] = 0;
    }

private:
    // =========================================================================
    // §1.99  CS line helpers (config-driven, no macros needed)
    // =========================================================================

    static FORCE_INLINE
    ChipId get_cs(bus_state_t bus) noexcept {
        return ChipId((bus >> PT::kCsBitShift) & ((bus_state_t(1) << PT::kCsLineBits) - 1));
    }

    static FORCE_INLINE
    void set_cs(bus_state_t& bus, size_t cs_id) noexcept {
        bus = (bus & ~PT::kCsMask) | (bus_state_t(cs_id) << PT::kCsBitShift);
    }

    // =========================================================================
    // §1.99b  Resolve helpers — chase sub-tables to a terminal chip id
    // =========================================================================
    //
    // Used by resolve() when the initial page lookup yields a sentinel.
    // Returns the terminal chip id (direct, kNoChipSelected, or MMIO) —
    // unlike resolve_read_chip which may still return a sub-table sentinel
    // when max depth is hit, these flatten all the way through.
    //

    [[nodiscard]] FORCE_INLINE
    ChipId resolve_read_terminal(size_t viewer_id, ChipId chip_id,
                                 Addr addr) const noexcept {
        if (chip_id == PT::kNoChipSelected) return chip_id;
        if constexpr (kHasSubTables)
            chip_id = resolve_read_chip(viewer_id, chip_id, addr);
        return chip_id;
    }

    [[nodiscard]] FORCE_INLINE
    WriteChipId resolve_write_terminal(size_t viewer_id, WriteChipId chip_id,
                                       Addr addr) const noexcept {
        if (chip_id == PT::kNoChipSelectedWrite) return chip_id;
        if constexpr (kHasSubTables)
            chip_id = resolve_write_chip(viewer_id, chip_id, addr);
        return chip_id;
    }

    // =========================================================================
    // §2  Buffer read/write helpers (shared by hot path and slow path)
    // =========================================================================

    // read_buffer / write_buffer: used by tick()/read()/write() (non-CS path).
    // They optionally set CS as a side-effect when kCsLines is enabled.

    [[nodiscard]] FORCE_INLINE
    bus_state_t read_buffer(ChipId chip_id, bus_state_t bus) const noexcept {
        const Addr     addr    = Addr(BUS_GET_ADDR(bus));
        const size_t   offset  = (size_t(chip_id) << Spec::PageBits)
                                 | Viewer::offset_of(addr);
        const DataType mem_val = static_cast<DataType>(unified_buf_[offset]);

        if constexpr (kCsLines) { set_cs(bus, size_t(chip_id)); }

        if constexpr (kPartialBus) {
            const DataType mask = bus_masks_.read(size_t(chip_id));
            if (unlikely(mask != DataBusMasks<Spec>::kFullMask)) {
                BUS_BITMIX_DATA(bus, mem_val, mask);
                return bus;
            }
        }

        BUS_SET_DATA(bus, mem_val);
        return bus;
    }

    FORCE_INLINE
    bus_state_t write_buffer(WriteChipId chip_id, bus_state_t bus) noexcept {
        const Addr     addr    = Addr(BUS_GET_ADDR(bus));
        const size_t   offset  = (size_t(chip_id) << Spec::PageBits)
                                 | Viewer::offset_of(addr);
        const DataType bus_val = static_cast<DataType>(BUS_GET_DATA(bus));

        if constexpr (kCsLines) { set_cs(bus, size_t(chip_id)); }

        if constexpr (kPartialBus) {
            const DataType mask = bus_masks_.write(size_t(chip_id));
            if (unlikely(mask != DataBusMasks<Spec>::kFullMask)) {
                const DataType old_val = static_cast<DataType>(unified_buf_[offset]);
                unified_buf_[offset] =
                    static_cast<uint8_t>(bitmix(bus_val, old_val, mask));
                return bus;
            }
        }

        unified_buf_[offset] = static_cast<uint8_t>(bus_val);
        return bus;
    }

    // read_buffer_no_cs / write_buffer_no_cs: used by service_read/service_write
    // after resolve() has already set the CS field.  Skips set_cs().

    [[nodiscard]] FORCE_INLINE
    bus_state_t read_buffer_no_cs(ChipId chip_id, bus_state_t bus) const noexcept {
        const Addr     addr    = Addr(BUS_GET_ADDR(bus));
        const size_t   offset  = (size_t(chip_id) << Spec::PageBits)
                                 | Viewer::offset_of(addr);
        const DataType mem_val = static_cast<DataType>(unified_buf_[offset]);

        if constexpr (kPartialBus) {
            const DataType mask = bus_masks_.read(size_t(chip_id));
            if (unlikely(mask != DataBusMasks<Spec>::kFullMask)) {
                BUS_BITMIX_DATA(bus, mem_val, mask);
                return bus;
            }
        }

        BUS_SET_DATA(bus, mem_val);
        return bus;
    }

    FORCE_INLINE
    bus_state_t write_buffer_no_cs(WriteChipId chip_id, bus_state_t bus) noexcept {
        const Addr     addr    = Addr(BUS_GET_ADDR(bus));
        const size_t   offset  = (size_t(chip_id) << Spec::PageBits)
                                 | Viewer::offset_of(addr);
        const DataType bus_val = static_cast<DataType>(BUS_GET_DATA(bus));

        if constexpr (kPartialBus) {
            const DataType mask = bus_masks_.write(size_t(chip_id));
            if (unlikely(mask != DataBusMasks<Spec>::kFullMask)) {
                const DataType old_val = static_cast<DataType>(unified_buf_[offset]);
                unified_buf_[offset] =
                    static_cast<uint8_t>(bitmix(bus_val, old_val, mask));
                return bus;
            }
        }

        unified_buf_[offset] = static_cast<uint8_t>(bus_val);
        return bus;
    }

    // =========================================================================
    // §3  MMIO dispatch helpers
    // =========================================================================

    [[nodiscard]] FORCE_INLINE
    bus_state_t read_mmio(ChipId chip_id, bus_state_t bus) noexcept {
        if constexpr (Spec::EnableMmio) {
            const size_t idx = size_t(chip_id) - size_t(PT::kRegChipBase);
            if (idx < mmio_count_) {
                if constexpr (kCsLines) { set_cs(bus, size_t(chip_id)); }
                const MmioHandler& h = handlers_[idx];
                if (h.on_read) return h.on_read(h.ctx, bus);
            }
        }
        return bus;
    }

    FORCE_INLINE
    bus_state_t write_mmio(WriteChipId chip_id, bus_state_t bus) noexcept {
        if constexpr (Spec::EnableMmio) {
            const size_t idx = size_t(chip_id) - size_t(PT::kRegChipBaseWrite);
            if (idx < mmio_count_) {
                if constexpr (kCsLines) { set_cs(bus, size_t(chip_id)); }
                const MmioHandler& h = handlers_[idx];
                if (h.on_write) return h.on_write(h.ctx, bus);
            }
        }
        return bus;
    }

    // =========================================================================
    // §4  Sub-table resolution (iterative, bounded by pool size)
    // =========================================================================
    //
    // resolve_read_chip / resolve_write_chip:
    //   If chip_id is a sub-table sentinel, look it up and iterate.
    //   Terminates when chip_id is a terminal value (direct, open-bus, MMIO).
    //   Max iterations = kMaxIndexedSubs + kMaxMaskedSubs (no cycles possible
    //   as long as sub-tables don't reference themselves).
    //

    [[nodiscard]] FORCE_INLINE
    ChipId resolve_read_chip(size_t viewer_id, ChipId chip_id,
                             Addr addr) const noexcept {
        constexpr size_t kMaxDepth = kMaxIndexedSubs + kMaxMaskedSubs;
        for (size_t depth = 0; depth < kMaxDepth; ++depth) {
            if constexpr (kHasIndexedSub) {
                const auto base = size_t(PT::kIndexedSubBase);
                const auto id   = size_t(chip_id);
                if (id >= base && id < base + kMaxIndexedSubs) {
                    chip_id = indexed_subs_[viewer_id][id - base].read_chip(addr);
                    continue;
                }
            }
            if constexpr (kHasMaskedSub) {
                const auto base = size_t(PT::kMaskedSubBase);
                const auto id   = size_t(chip_id);
                if (id >= base && id < base + kMaxMaskedSubs) {
                    chip_id = masked_subs_[viewer_id][id - base].resolve_read(addr);
                    continue;
                }
            }
            break;
        }
        return chip_id;
    }

    [[nodiscard]] FORCE_INLINE
    WriteChipId resolve_write_chip(size_t viewer_id, WriteChipId chip_id,
                                   Addr addr) const noexcept {
        constexpr size_t kMaxDepth = kMaxIndexedSubs + kMaxMaskedSubs;
        for (size_t depth = 0; depth < kMaxDepth; ++depth) {
            if constexpr (kHasIndexedSub) {
                const auto base = size_t(PT::kIndexedSubBaseWrite);
                const auto id   = size_t(chip_id);
                if (id >= base && id < base + kMaxIndexedSubs) {
                    chip_id = indexed_subs_[viewer_id][id - base].write_chip(addr);
                    continue;
                }
            }
            if constexpr (kHasMaskedSub) {
                const auto base = size_t(PT::kMaskedSubBaseWrite);
                const auto id   = size_t(chip_id);
                if (id >= base && id < base + kMaxMaskedSubs) {
                    chip_id = masked_subs_[viewer_id][id - base].resolve_write(addr);
                    continue;
                }
            }
            break;
        }
        return chip_id;
    }

    // =========================================================================
    // §5  Slow paths — sub-table, MMIO, and open-bus dispatch
    // =========================================================================

    [[nodiscard]] FORCE_NOINLINE
    bus_state_t read_slow(size_t viewer_id, ChipId chip_id,
                          bus_state_t bus) noexcept {
        // Fast exit for open bus (common sentinel)
        if (chip_id == PT::kNoChipSelected) return bus;

        // Resolve through sub-tables if chip_id is a sub-table sentinel
        if constexpr (kHasSubTables) {
            chip_id = resolve_read_chip(viewer_id, chip_id,
                                        Addr(BUS_GET_ADDR(bus)));
            if (chip_id < PT::kReadSentinelMin)
                return read_buffer(chip_id, bus);
            if (chip_id == PT::kNoChipSelected)
                return bus;
        }

        // MMIO handler dispatch
        if constexpr (Spec::EnableMmio) {
            if (chip_id >= PT::kRegChipBase)
                return read_mmio(chip_id, bus);
        }

        return bus;   // fallthrough: bus floats
    }

    FORCE_NOINLINE
    bus_state_t write_slow(size_t viewer_id, WriteChipId chip_id,
                           bus_state_t bus) noexcept {
        if (chip_id == PT::kNoChipSelectedWrite) return bus;

        if constexpr (kHasSubTables) {
            chip_id = resolve_write_chip(viewer_id, chip_id,
                                         Addr(BUS_GET_ADDR(bus)));
            if (chip_id < PT::kWriteSentinelMin)
                return write_buffer(chip_id, bus);
            if (chip_id == PT::kNoChipSelectedWrite)
                return bus;
        }

        if constexpr (Spec::EnableMmio) {
            if (chip_id >= PT::kRegChipBaseWrite)
                return write_mmio(chip_id, bus);
        }

        return bus;   // write silently dropped
    }

    // =========================================================================
    // §6  State
    // =========================================================================

    uint8_t* unified_buf_ = nullptr;

    std::array<Viewer, kNumViewers> viewers_{};

    // Data-bus masks — zero storage when EnablePartialBus is absent or false.
    [[no_unique_address]]
    std::conditional_t<kPartialBus,
        DataBusMasks<Spec>,
        std::monostate> bus_masks_{};

    // MMIO handler table — zero storage when EnableMmio is false.
    [[no_unique_address]]
    std::conditional_t<Spec::EnableMmio,
        std::array<MmioHandler, (kMaxHandlers > 0 ? kMaxHandlers : 1)>,
        std::monostate> handlers_{};

    size_t mmio_count_ = 0;

    // ── Indexed sub-tables — per-viewer, zero storage when disabled ─────────
    [[no_unique_address]]
    std::conditional_t<kHasIndexedSub,
        std::array<std::array<IndexedSubTable<Spec>, kMaxIndexedSubs>, kNumViewers>,
        std::monostate> indexed_subs_{};

    [[no_unique_address]]
    std::conditional_t<kHasIndexedSub,
        std::array<size_t, kNumViewers>,
        std::monostate> indexed_sub_count_{};

    // ── Masked sub-tables — per-viewer, zero storage when disabled ──────────
    [[no_unique_address]]
    std::conditional_t<kHasMaskedSub,
        std::array<std::array<MaskedSubTable<Spec>, kMaxMaskedSubs>, kNumViewers>,
        std::monostate> masked_subs_{};

    [[no_unique_address]]
    std::conditional_t<kHasMaskedSub,
        std::array<size_t, kNumViewers>,
        std::monostate> masked_sub_count_{};
};


// =============================================================================
// §7  BusView — compile-time–bound viewer accessor
// =============================================================================

template<BusSpecConcept Spec, size_t ViewerId>
class BusView {
    static_assert(ViewerId < Spec::NumViewers, "ViewerId out of range");
public:
    using Bus = MemoryBus<Spec>;

    explicit BusView(Bus& bus) noexcept : bus_(bus) {}

    // ── CS-enabled workflow: resolve then service ──────────────────────────

    [[nodiscard]] FORCE_INLINE
    bus_state_t resolve(bus_state_t bus) const noexcept
        requires(Bus::kCsLines) { return bus_.resolve(ViewerId, bus); }

    [[nodiscard]] FORCE_INLINE
    bus_state_t service_read(bus_state_t bus) const noexcept
        requires(Bus::kCsLines) { return bus_.service_read(bus); }

    FORCE_INLINE
    bus_state_t service_write(bus_state_t bus) noexcept
        requires(Bus::kCsLines) { return bus_.service_write(bus); }

    [[nodiscard]] FORCE_INLINE
    bus_state_t service(bus_state_t bus) noexcept
        requires(Bus::kCsLines) { return bus_.service(bus); }

    // ── Non-CS workflow: full dispatch ─────────────────────────────────────

    [[nodiscard]] FORCE_INLINE
    bus_state_t read (bus_state_t bus) noexcept { return bus_.read (ViewerId, bus); }

    FORCE_INLINE
    bus_state_t write(bus_state_t bus) noexcept { return bus_.write(ViewerId, bus); }

    [[nodiscard]] FORCE_INLINE
    bus_state_t tick (bus_state_t bus) noexcept { return bus_.tick (ViewerId, bus); }

    [[nodiscard]] Bus& bus() noexcept { return bus_; }

private:
    Bus& bus_;
};
