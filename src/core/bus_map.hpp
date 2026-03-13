// =============================================================================
// bus_map.hpp — BusMap<Spec>: address-decode logic extracted from Board
// =============================================================================
//
// Owns slot records and page-table wiring.  Does NOT own the flat mem
// (owned by Board) or chip lifetimes (owned by Board).  Receives buffer and
// bus references through method parameters.
//
// See chip_manifest.hpp for the declarative chip-memory layout and board.hpp
// for the Board<Spec> wrapper that combines BusMap with chip ownership.
//
// =============================================================================
#pragma once

#include "core/chip_manifest.hpp"
#include "core/memory_bus.hpp"

// §3  BusMap<Spec> — address-decode: page tables, MMIO handlers, sub-tables
// =============================================================================
//
// Extracted from Board<Spec>.  Holds the slot vector and all logic that
// programs a MemoryBus viewer from manifest slots + bound chips.
//
// Template parameter Spec must satisfy BusSpecConcept.
//
// Thread safety: none.  External synchronisation required if apply() or
// mutators are called concurrently with bus accesses.
//

template<BusSpecConcept Spec>
class BusMap {
public:
    using Bus         = MemoryBus<Spec>;
    using ChipId      = typename Bus::ChipId;
    using WriteChipId = typename Bus::WriteChipId;
    using PT          = PackingTraits<Spec>;
    using Addr        = typename Spec::AddrType;

    static constexpr size_t kPageSize = Bus::kPageSize;
    static constexpr size_t kPageBits = Spec::PageBits;

    // ── Per-slot runtime record ───────────────────────────────────────────

    struct SlotRecord {
        std::string_view name;                // From bound chip (or provided for dynamic)
        ChipId           base_id;             // First chip id (prefix sum)
        size_t           num_pages;           // Buffer pages (0 = MMIO-only)
        uint32_t         base_addr  = 0;      // Default address in bus space
        uint32_t         addr_mask  = 0;      // Sub-page decode mask
        bool             read_only  = false;  // Derived from bound chip
        bool             dynamic    = false;  // Runtime-added (can be removed)
        ChipBase*        chip       = nullptr; // Bound chip instance
        int              mmio_idx   = -1;     // Assigned MMIO handler index (-1 = none)
        int              sub_table_idx = -1;  // Assigned MaskedSubTable index (-1 = none)

        // Factory and label — copied from ChipSlot at construction.
        // Used by Board::create_chips() to auto-create chip instances.
        ChipSlot::FactoryFn factory   = nullptr;
        const char*         label     = nullptr;
        uint16_t            condition = 0;
    };

    // =====================================================================
    // §3.1  Construction from a ChipManifest
    // =====================================================================
    //
    // Builds the slot vector from the manifest.  No buffer allocation — that
    // is Board's responsibility.
    //

    template<size_t N>
    explicit BusMap(const ChipManifest<N>& manifest) {
        slots_.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            const ChipSlot& s = manifest.chips[i];
            slots_.push_back({
                {},                             // name: set during bind_chip
                ChipId(manifest.base_id(i, kPageBits)),
                s.pages(kPageBits),
                s.base_addr,
                s.addr_mask,
                false,                          // read_only: set during bind_chip
                false,                          // dynamic
                nullptr,                        // chip
                -1,                             // mmio_idx
                -1,                             // sub_table_idx
                s.factory,
                s.label,
                s.condition,
            });
        }
    }

    // =====================================================================
    // §3.2  Chip binding
    // =====================================================================
    //
    // Associates a runtime ChipBase* with a manifest slot.  Derives the
    // chip's name and read_only from its virtual interface.  Assigns the
    // bus chip id on the chip.
    //
    // Does NOT return a buffer pointer — that is Board's concern.
    //

    void bind_chip(size_t slot_index, ChipBase* chip) noexcept {
        assert(slot_index < slots_.size());
        auto& s = slots_[slot_index];
        s.chip = chip;
        if (chip) {
            s.name      = chip->chip_info().part_number;
            s.read_only = chip->is_read_only();
            chip->set_bus_chip_id(uint16_t(s.base_id));
        }
    }

    // =====================================================================
    // §3.3  Auto-wiring: apply()
    // =====================================================================
    //
    // Programs a MemoryBus viewer from the manifest slots + bound chips:
    //   - Resets the viewer and its masked sub-tables.
    //   - Maps buffer chips (read pages for all, write pages for non-read-only).
    //   - Registers MMIO handlers for chips with has_mmio().
    //   - Auto-creates MaskedSubTables for sub-page MMIO regions.
    //
    // Safe to call multiple times (e.g. after RAM size reconfiguration).
    // MMIO handlers are re-used on subsequent calls (update, not re-register).
    //
    // flat_mem: pointer to the flat buffer owned by Board.
    //

    void apply(Bus& bus, uint8_t* flat_mem, size_t viewer_id = 0) {
        bus.set_flat_mem(flat_mem);
        bus.reset_viewer(viewer_id);
        if constexpr (Bus::kHasMaskedSub) bus.reset_masked_subs(viewer_id);

        // ── Phase 1: Map buffer chips ─────────────────────────────────────
        //
        // Clip num_pages to the address space so that bank-switching pools
        // (buffer larger than the visible window) don't overflow the page
        // table.  The system's configure_bus_memory_map() remaps banks later.
        //
        static constexpr size_t kNumPages = Bus::kNumPages;
        for (const auto& slot : slots_) {
            if (slot.num_pages == 0 || slot.dynamic) continue;
            const size_t first_page = slot.base_addr >> kPageBits;
            const size_t mappable   = std::min(slot.num_pages, kNumPages - first_page);
            bus.fill_read_pages(viewer_id, first_page, mappable, slot.base_id);
            if (!slot.read_only) {
                bus.fill_write_pages(viewer_id, first_page, mappable,
                                     WriteChipId(slot.base_id));
            }
        }

        // ── Phase 2: MMIO handlers and sub-tables ─────────────────────────
        if constexpr (Spec::EnableMmio) {
            // Track MaskedSubTable assignments for page deduplication.
            struct SubInfo { size_t page; int sub_idx; };
            static constexpr size_t kMaxSubs =
                Bus::kHasMaskedSub ? Bus::kMaxMaskedSubs : 1;
            std::array<SubInfo, kMaxSubs> sub_infos{};
            size_t num_subs = 0;

            for (auto& slot : slots_) {
                if (!slot.chip || !slot.chip->has_mmio() || slot.dynamic)
                    continue;

                // Register (or update) MMIO handler
                MmioHandler handler{
                    slot.chip,
                    mmio_read_trampoline_,
                    mmio_write_trampoline_
                };
                if (slot.mmio_idx >= 0) {
                    bus.update_handler(size_t(slot.mmio_idx), handler);
                } else {
                    slot.mmio_idx = bus.register_handler(handler);
                    assert(slot.mmio_idx >= 0);
                }

                if constexpr (Bus::kHasMaskedSub) {
                    if (slot.addr_mask != 0) {
                        // Sub-page MMIO — route through a MaskedSubTable
                        const size_t page = slot.base_addr >> kPageBits;

                        // Find existing sub-table for this page, or create one
                        int sub_idx = -1;
                        for (size_t k = 0; k < num_subs; ++k) {
                            if (sub_infos[k].page == page) {
                                sub_idx = sub_infos[k].sub_idx;
                                break;
                            }
                        }
                        if (sub_idx < 0) {
                            // Capture the page's current base chip before overwriting
                            auto base_rd = bus.viewer(viewer_id).read_chip(page);
                            auto base_wr = bus.viewer(viewer_id).write_chip(page);
                            sub_idx = bus.add_masked_sub_table(
                                viewer_id, base_rd, base_wr);
                            assert(sub_idx >= 0);
                            bus.map_to_masked_sub(
                                viewer_id, page, size_t(sub_idx));
                            sub_infos[num_subs++] = {page, sub_idx};
                        }

                        slot.sub_table_idx = sub_idx;

                        bus.add_masked_region(
                            viewer_id, size_t(sub_idx),
                            Addr(slot.addr_mask),
                            Addr(slot.base_addr & slot.addr_mask),
                            ChipId(size_t(PT::kRegChipBase) + size_t(slot.mmio_idx)),
                            WriteChipId(size_t(PT::kRegChipBaseWrite)
                                        + size_t(slot.mmio_idx)));
                        continue;  // skip full-page fallback
                    }
                }

                // Full-page MMIO (or no masked sub-table support).
                //
                // Skip page-table mapping for chips with base_addr=0 and no
                // mask — these are non-bus chips whose I/O is dispatched
                // manually by the system (e.g. VIC-20 VIAs handled in
                // io_tick()).  The handler is still registered above so the
                // system can route to it explicitly if desired.
                if (slot.base_addr == 0 && slot.addr_mask == 0
                    && slot.num_pages == 0)
                    continue;

                const size_t first_page = slot.base_addr >> kPageBits;
                const size_t count = slot.num_pages > 0 ? slot.num_pages : 1;
                bus.map_register_file(
                    viewer_id, first_page, count, size_t(slot.mmio_idx));
            }
        }
    }

    // =====================================================================
    // §3.4  Convenience: manual page mapping
    // =====================================================================
    //
    // Helpers for systems that need manual page-table manipulation beyond
    // what apply() provides (e.g. cartridge bank switching, PLA modes).
    //

    void map_chip_read(Bus& bus, size_t viewer_id,
                       size_t first_page, ChipId base_id) const noexcept {
        const SlotRecord* info = find(base_id);
        if (!info) return;
        bus.fill_read_pages(viewer_id, first_page, info->num_pages, base_id);
    }

    void map_chip_write(Bus& bus, size_t viewer_id,
                        size_t first_page, ChipId base_id) const noexcept {
        const SlotRecord* info = find(base_id);
        if (!info) return;
        bus.fill_write_pages(viewer_id, first_page, info->num_pages,
                             WriteChipId(base_id));
    }

    void map_chip(Bus& bus, size_t viewer_id,
                  size_t first_page, ChipId base_id) const noexcept {
        const SlotRecord* info = find(base_id);
        if (!info) return;
        bus.fill_pages(viewer_id, first_page, info->num_pages,
                       base_id, WriteChipId(base_id));
    }

    // =====================================================================
    // §3.5  Slot lookup and access
    // =====================================================================

    [[nodiscard]] const SlotRecord* find(ChipId base_id) const noexcept {
        for (const auto& s : slots_)
            if (s.base_id == base_id) return &s;
        return nullptr;
    }

    [[nodiscard]] const SlotRecord* find(std::string_view name) const noexcept {
        for (const auto& s : slots_)
            if (s.name == name) return &s;
        return nullptr;
    }

    [[nodiscard]] SlotRecord& slot(size_t index) noexcept {
        assert(index < slots_.size());
        return slots_[index];
    }

    [[nodiscard]] const SlotRecord& slot(size_t index) const noexcept {
        assert(index < slots_.size());
        return slots_[index];
    }

    [[nodiscard]] std::span<const SlotRecord> slots() const noexcept {
        return slots_;
    }

    [[nodiscard]] size_t slot_count() const noexcept {
        return slots_.size();
    }

    // =====================================================================
    // §3.6  Dynamic slot management (for Board's hot-swap pool)
    // =====================================================================
    //
    // Board owns the free-list allocator and calls these to insert/remove
    // slot records when chips are dynamically added or removed.
    //

    void add_slot(SlotRecord rec) {
        slots_.push_back(std::move(rec));
    }

    bool remove_slot(ChipId base_id) noexcept {
        auto it = std::find_if(slots_.begin(), slots_.end(),
            [base_id](const SlotRecord& s){ return s.base_id == base_id; });
        if (it == slots_.end() || !it->dynamic) return false;
        slots_.erase(it);
        return true;
    }

private:
    // ── MMIO dispatch trampolines ────────────────────────────────────────
    // Bridge ChipBase virtual on_bus_read/write to MmioHandler function ptrs.

    static bus_state_t mmio_read_trampoline_(void* ctx, bus_state_t bus) noexcept {
        return static_cast<ChipBase*>(ctx)->on_bus_read(bus);
    }

    static bus_state_t mmio_write_trampoline_(void* ctx, bus_state_t bus) noexcept {
        return static_cast<ChipBase*>(ctx)->on_bus_write(bus);
    }

    // ── State ─────────────────────────────────────────────────────────────

    std::vector<SlotRecord> slots_;
};
