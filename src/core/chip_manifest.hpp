// =============================================================================
// chip_manifest.hpp — Chip manifest, unified buffer ownership, hot-swap
// =============================================================================
//
// This header provides the layer between the physical chip description of a
// system and the MemoryBus page tables.  It answers the question: "which chip
// ids does each chip get?" and owns the flat unified buffer whose layout those
// ids index into.
//
// Three types are provided:
//
//   ChipDef          — describes one chip (name, size, ROM/RAM, page count)
//
//   ChipManifest<N>  — compile-time array of N ChipDef entries.
//                      Assigns chip ids as prefix sums of num_pages, so every
//                      chip's base chip id is a constexpr and can be captured
//                      in a named symbol.  Optionally reserves a dynamic pool
//                      at the end for hot-swap chips.
//
//   BusMemory<Cfg>   — runtime owner of the unified buffer.  Initialised from
//                      a ChipManifest.  Provides:
//                        • chip_buffer(base_id) → uint8_t*  (pointer into buf)
//                        • load(base_id, data)              (fill from span)
//                        • add_chip(...)  / remove_chip(...) (dynamic hot-swap)
//                        • connect(MemoryBus<Cfg>&)          (wire buffer ptr)
//
// Usage model:
//
//   1.  Define a constexpr ChipManifest for your system.
//   2.  Capture the base chip ids as constexpr symbols via ChipManifest::base_id().
//   3.  Construct a BusMemory from the manifest; it allocates the buffer.
//   4.  Load ROM/RAM content via mem.load() or mem.chip_buffer().
//   5.  Call mem.connect(bus) to wire the buffer pointer into MemoryBus.
//   6.  Program the MemoryBus page tables using the captured chip ids.
//   7.  For hot-swap: mem.add_chip() / mem.remove_chip() at runtime.
//
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

#include "memory_bus.hpp"


// =============================================================================
// §1  ChipDef — compile-time description of one chip
// =============================================================================

struct ChipDef {
    std::string_view name;
    size_t           num_pages  = 0;    // size in bus pages (2^PageBits bytes each)
    bool             read_only  = false; // true = ROM; write side maps to kNoChipSelected
};


// =============================================================================
// §2  ChipManifest<N> — compile-time chip-id assignment
// =============================================================================
//
// Chip ids are assigned as the prefix sum of num_pages across the array.
// A chip of M pages occupies ids [base_id, base_id + M - 1].
//
// The hot-path formula for a chip_id → buffer offset is:
//   offset = (chip_id << PageBits) | page_local_offset
// which requires that the first page of chip K sits at the page-granular
// position sum(chips[0..K-1].num_pages) in the unified buffer — exactly
// what this assignment guarantees.
//
// An optional dynamic pool of num_dynamic_pages pages is appended at the end
// of the buffer.  BusMemory uses this pool for runtime chip addition (hot-swap).
// The dynamic region is invisible to the MemoryBus itself; only the chip ids
// and the buffer pointer matter there.
//
// Example — C64:
//
//   constexpr auto kC64Chips = make_chip_manifest<6>({
//       {"ROML",    1, true },   // 4 KB ROM
//       {"ROMH",    1, true },   // 4 KB ROM
//       {"KERNAL",  2, true },   // 8 KB ROM
//       {"BASIC",   2, true },   // 8 KB ROM
//       {"CHARROM", 2, true },   // 8 KB ROM
//       {"RAM",    16, false},   // 64 KB RAM
//   });
//
//   // Compile-time chip ids — usable in constexpr, template args, if constexpr
//   constexpr auto kRomlId   = kC64Chips.base_id(0);  // = 0
//   constexpr auto kRomhId   = kC64Chips.base_id(1);  // = 1
//   constexpr auto kKernalId = kC64Chips.base_id(2);  // = 2
//   constexpr auto kBasicId  = kC64Chips.base_id(3);  // = 4
//   constexpr auto kCharId   = kC64Chips.base_id(4);  // = 6
//   constexpr auto kRamId    = kC64Chips.base_id(5);  // = 8
//   // MaxChipId = 8 + 16 - 1 = 23
//   static_assert(kC64Chips.max_chip_id() == 23);
//   static_assert(C64BusConfig::MaxChipId  == 23);  // config must match
//

template<size_t N>
struct ChipManifest {
    std::array<ChipDef, N> chips{};
    size_t num_dynamic_pages = 0;  // reserved for runtime-added chips

    // Base chip id of chip at index chip_index (0-based).
    [[nodiscard]] constexpr size_t base_id(size_t chip_index) const noexcept {
        size_t id = 0;
        for (size_t i = 0; i < chip_index; ++i)
            id += chips[i].num_pages;
        return id;
    }

    // Total pages occupied by all static chips.
    [[nodiscard]] constexpr size_t static_pages() const noexcept {
        return base_id(N);
    }

    // First chip id in the dynamic pool (= one past the last static chip id).
    [[nodiscard]] constexpr size_t dynamic_base_id() const noexcept {
        return static_pages();
    }

    // Total pages in the unified buffer (static + dynamic pool).
    [[nodiscard]] constexpr size_t total_pages() const noexcept {
        return static_pages() + num_dynamic_pages;
    }

    // Highest chip id that will ever appear in a page table.
    // Set MaxChipId in your BusConfig to this value.
    [[nodiscard]] constexpr size_t max_chip_id() const noexcept {
        return total_pages() > 0 ? total_pages() - 1 : 0;
    }

    // Buffer size in bytes for a given page size (= 2^PageBits).
    [[nodiscard]] constexpr size_t buffer_bytes(size_t page_size) const noexcept {
        return total_pages() * page_size;
    }

    // Convenience: number of static chips.
    [[nodiscard]] static constexpr size_t num_chips() noexcept { return N; }
};


// =============================================================================
// §2.1  make_chip_manifest — factory for clean brace-init syntax
// =============================================================================
//
// Usage:
//   constexpr auto kMyChips = make_chip_manifest<5>({
//       {"ROM",  4, true },
//       {"RAM",  8, false},
//       {"IO",   1, false},
//       {"EXP",  2, true },
//       {"VRAM", 2, false},
//   }, /*num_dynamic_pages=*/4);
//

template<size_t N>
[[nodiscard]] constexpr ChipManifest<N>
make_chip_manifest(const ChipDef (&defs)[N],
                   size_t num_dynamic_pages = 0) noexcept
{
    ChipManifest<N> manifest{};
    for (size_t i = 0; i < N; ++i)
        manifest.chips[i] = defs[i];
    manifest.num_dynamic_pages = num_dynamic_pages;
    return manifest;
}


// =============================================================================
// §3  BusMemory<Cfg> — runtime chip owner
// =============================================================================
//
// Owns the flat unified buffer.  Provides pointer access into the buffer for
// each chip, handles dynamic chip add/remove for hot-swap, and wires the
// buffer pointer into MemoryBus.
//
// Template parameter Cfg must satisfy BusConfigConcept.  BusMemory uses
// Bus::kPageSize to compute buffer offsets and buffer capacity.
//
// Thread safety: none.  External synchronisation required if add/remove_chip
// is called concurrently with bus accesses.
//

template<BusConfigConcept Cfg>
class BusMemory {
public:
    using Bus         = MemoryBus<Cfg>;
    using ChipId      = typename Bus::ChipId;
    using WriteChipId = typename Bus::WriteChipId;

    static constexpr size_t kPageSize = Bus::kPageSize;

    // ── Per-chip runtime record ───────────────────────────────────────────────

    struct ChipInfo {
        std::string_view name;
        ChipId           base_id;    // first chip id (page-granular)
        size_t           num_pages;  // number of page-granular ids occupied
        bool             read_only;  // true = ROM
        bool             dynamic;    // true = added at runtime (can be removed)
    };

    // =========================================================================
    // §3.1  Construction from a ChipManifest
    // =========================================================================
    //
    // Allocates the unified buffer to exactly fit all static chips plus the
    // dynamic pool declared in the manifest.
    //

    template<size_t N>
    explicit BusMemory(const ChipManifest<N>& manifest) {
        const size_t total = manifest.total_pages();
        buffer_.assign(total * kPageSize, uint8_t(0xFF));  // default: 0xFF = pulled-high

        // Record static chips
        chips_.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            const ChipDef& d = manifest.chips[i];
            assert(d.num_pages > 0);
            chips_.push_back({
                d.name,
                ChipId(manifest.base_id(i)),
                d.num_pages,
                d.read_only,
                /*dynamic=*/false,
            });
        }

        // Initialise dynamic allocator
        if (manifest.num_dynamic_pages > 0) {
            dynamic_base_  = manifest.dynamic_base_id();
            free_list_.push_back({dynamic_base_, manifest.num_dynamic_pages});
        }
    }

    // =========================================================================
    // §3.2  Buffer and chip access
    // =========================================================================

    // Pointer to the start of the chip's region in the unified buffer.
    [[nodiscard]] uint8_t* chip_buffer(ChipId base_id) noexcept {
        return buffer_.data() + size_t(base_id) * kPageSize;
    }

    [[nodiscard]] const uint8_t* chip_buffer(ChipId base_id) const noexcept {
        return buffer_.data() + size_t(base_id) * kPageSize;
    }

    // Total size of a chip's buffer region in bytes.
    [[nodiscard]] static constexpr size_t chip_bytes(size_t num_pages) noexcept {
        return num_pages * kPageSize;
    }

    // =========================================================================
    // §3.3  Load ROM/RAM data
    // =========================================================================
    //
    // Copies data into the chip's region.  Returns false if data is larger than
    // the chip's allocated region.  Partial loads (data.size() < chip_bytes) fill
    // only the beginning of the region; the rest retains its current value.
    //

    bool load(ChipId base_id, std::span<const uint8_t> data) noexcept {
        const ChipInfo* info = find(base_id);
        if (!info) return false;
        const size_t capacity = info->num_pages * kPageSize;
        if (data.size() > capacity) return false;
        std::memcpy(chip_buffer(base_id), data.data(), data.size());
        return true;
    }

    // Fill a chip's region with a constant byte (e.g. 0xFF for unpopulated ROM).
    void fill(ChipId base_id, uint8_t value = 0xFF) noexcept {
        const ChipInfo* info = find(base_id);
        if (!info) return;
        std::memset(chip_buffer(base_id), value, info->num_pages * kPageSize);
    }

    // =========================================================================
    // §3.4  Dynamic chip management (hot-swap)
    // =========================================================================
    //
    // add_chip allocates num_pages contiguous pages from the dynamic pool and
    // returns the base chip id.  Returns kInvalidChipId on failure (pool full
    // or fragmented).
    //
    // remove_chip returns the pages to the free list and merges adjacent runs.
    // The caller must remap or unmap the chip in all MemoryBus viewers before
    // or after calling remove_chip.
    //
    // The dynamic pool is declared in the ChipManifest as num_dynamic_pages.
    // All dynamic chip ids fall in [dynamic_base_, dynamic_base_ + pool_size).
    //

    static constexpr ChipId kInvalidChipId = ChipId(~ChipId(0));

    [[nodiscard]] ChipId add_chip(std::string_view name,
                                  size_t           num_pages,
                                  bool             read_only = false) noexcept
    {
        if (num_pages == 0) return kInvalidChipId;

        // First-fit search in the free list
        for (auto it = free_list_.begin(); it != free_list_.end(); ++it) {
            if (it->pages >= num_pages) {
                const ChipId base = ChipId(it->base);
                chips_.push_back({name, base, num_pages, read_only, /*dynamic=*/true});

                // Consume from the free run
                it->base  += num_pages;
                it->pages -= num_pages;
                if (it->pages == 0)
                    free_list_.erase(it);

                return base;
            }
        }
        return kInvalidChipId;  // pool exhausted or too fragmented
    }

    void remove_chip(ChipId base_id) noexcept {
        auto it = std::find_if(chips_.begin(), chips_.end(),
            [base_id](const ChipInfo& c){ return c.base_id == base_id; });
        if (it == chips_.end() || !it->dynamic) return;

        const size_t base  = size_t(it->base_id);
        const size_t pages = it->pages;  // wait, field is num_pages
        const size_t npages = it->num_pages;
        chips_.erase(it);

        // Return pages to free list and merge adjacent runs
        FreeRun run{base, npages};
        free_list_.push_back(run);
        std::sort(free_list_.begin(), free_list_.end(),
            [](const FreeRun& a, const FreeRun& b){ return a.base < b.base; });
        merge_free_list();
    }

    // =========================================================================
    // §3.5  Connect to a MemoryBus
    // =========================================================================
    //
    // Passes the unified buffer pointer to the bus.  Must be called before any
    // bus accesses; re-call if the buffer is reallocated (e.g. after a resize).
    //

    void connect(Bus& bus) noexcept {
        bus.set_unified_buffer(buffer_.data());
    }

    // =========================================================================
    // §3.6  Chip lookup
    // =========================================================================

    [[nodiscard]] const ChipInfo* find(ChipId base_id) const noexcept {
        for (const auto& c : chips_)
            if (c.base_id == base_id) return &c;
        return nullptr;
    }

    [[nodiscard]] const ChipInfo* find(std::string_view name) const noexcept {
        for (const auto& c : chips_)
            if (c.name == name) return &c;
        return nullptr;
    }

    [[nodiscard]] std::span<const ChipInfo> chips() const noexcept {
        return chips_;
    }

    // =========================================================================
    // §3.7  Buffer introspection
    // =========================================================================

    [[nodiscard]] uint8_t*       buffer()       noexcept { return buffer_.data(); }
    [[nodiscard]] const uint8_t* buffer() const noexcept { return buffer_.data(); }
    [[nodiscard]] size_t buffer_size() const noexcept { return buffer_.size(); }

    // =========================================================================
    // §3.8  Convenience: program a MemoryBus viewer from ChipManifest
    // =========================================================================
    //
    // Helper that maps a chip's pages into a viewer's address space.
    //
    //   map_chip_read(bus, viewer, chip_base_addr_page, chip_base_id)
    //     → fills num_pages pages starting at chip_base_addr_page in the
    //       viewer's read table with sequential chip ids starting at chip_base_id.
    //
    //   map_chip_write: same for write table.
    //   map_chip: both.
    //
    // For mirrored chips (all pages → same chip id, e.g. 2 KB RAM mirrored
    // across 8 KB): use bus.fill_read_constant() directly.
    //

    void map_chip_read(Bus& bus, size_t viewer_id,
                       size_t first_page, ChipId base_id) const noexcept {
        const ChipInfo* info = find(base_id);
        if (!info) return;
        bus.fill_read_pages(viewer_id, first_page, info->num_pages, base_id);
    }

    void map_chip_write(Bus& bus, size_t viewer_id,
                        size_t first_page, ChipId base_id) const noexcept {
        const ChipInfo* info = find(base_id);
        if (!info) return;
        bus.fill_write_pages(viewer_id, first_page, info->num_pages,
                             WriteChipId(base_id));
    }

    void map_chip(Bus& bus, size_t viewer_id,
                  size_t first_page, ChipId base_id) const noexcept {
        const ChipInfo* info = find(base_id);
        if (!info) return;
        bus.fill_pages(viewer_id, first_page, info->num_pages,
                       base_id, WriteChipId(base_id));
    }

private:
    // ── Dynamic allocator internal state ─────────────────────────────────────

    struct FreeRun {
        size_t base;
        size_t pages;
    };

    void merge_free_list() noexcept {
        // Assumes free_list_ is sorted by base; merges adjacent runs.
        for (size_t i = 0; i + 1 < free_list_.size(); ) {
            FreeRun& cur  = free_list_[i];
            FreeRun& next = free_list_[i + 1];
            if (cur.base + cur.pages == next.base) {
                cur.pages += next.pages;
                free_list_.erase(free_list_.begin() + static_cast<ptrdiff_t>(i + 1));
            } else {
                ++i;
            }
        }
    }

    // ── State ─────────────────────────────────────────────────────────────────

    std::vector<uint8_t>  buffer_;
    std::vector<ChipInfo> chips_;

    size_t               dynamic_base_ = 0;
    std::vector<FreeRun> free_list_;
};


// =============================================================================
// §4  Usage examples
// =============================================================================
//
// ── C64 setup ─────────────────────────────────────────────────────────────────
//
//  // §4.1  Declare the chip manifest — one entry per physical chip
//  //       num_pages is relative to PageBits=12 (4 KB pages)
//
//  inline constexpr auto kC64Chips = make_chip_manifest<6>({
//      {"ROML",    1, true },   // $8000–$8FFF  4 KB
//      {"ROMH",    1, true },   // $A000–$AFFF  4 KB
//      {"KERNAL",  2, true },   // $E000–$FFFF  8 KB
//      {"BASIC",   2, true },   // $A000–$BFFF  8 KB
//      {"CHARROM", 2, true },   // mapped by VIC-II
//      {"RAM",    16, false},   // 64 KB
//  });
//
//  // §4.2  Capture compile-time chip ids — zero runtime cost
//  constexpr auto kRomlId   = kC64Chips.base_id(0);  // 0
//  constexpr auto kRomhId   = kC64Chips.base_id(1);  // 1
//  constexpr auto kKernalId = kC64Chips.base_id(2);  // 2
//  constexpr auto kBasicId  = kC64Chips.base_id(3);  // 4
//  constexpr auto kCharId   = kC64Chips.base_id(4);  // 6
//  constexpr auto kRamId    = kC64Chips.base_id(5);  // 8
//  static_assert(kC64Chips.max_chip_id() == 23);
//  // → C64BusConfig::MaxChipId must be 23
//
//  // §4.3  Create BusMemory; it allocates the unified buffer
//  BusMemory<C64BusConfig> mem{kC64Chips};
//
//  // §4.4  Load ROM images
//  mem.load(kRomlId,   roml_bytes);
//  mem.load(kKernalId, kernal_bytes);
//  // RAM region is pre-filled with 0xFF; overwrite if needed
//
//  // §4.5  Wire buffer into the bus
//  C64Bus bus;
//  mem.connect(bus);
//
//  // §4.6  Program one PLA mode — chip ids are the symbols above
//  //       (In practice use pre-computed ModeSnapshot arrays)
//  bus.set_read_page (C64BusConfig::Cpu, 0x8, ChipId(kRomlId));
//  bus.set_read_page (C64BusConfig::Cpu, 0xA, ChipId(kBasicId));
//  bus.set_read_page (C64BusConfig::Cpu, 0xB, ChipId(kBasicId + 1));
//  bus.fill_read_pages(C64BusConfig::Cpu, 0x0, 16, ChipId(kRamId));
//  // etc. for all 16 pages × all 32 PLA modes → saved as ModeSnapshot
//
// ── NES cartridge hot-swap (dynamic chip) ─────────────────────────────────────
//
//  // Reserve a 256 KB dynamic pool for PRG-ROM cartridges
//  inline constexpr auto kNesChips = make_chip_manifest<3>({
//      {"WRAM",  2, false},   // 2 KB internal RAM   (2 × 1 KB pages at PageBits=10)
//      {"CIRAM", 2, false},   // 2 KB nametable RAM
//      {"SRAM",  8, false},   // 8 KB battery-backed RAM
//  }, /*num_dynamic_pages=*/256);  // 256 × 1 KB dynamic pool for cartridge ROM
//
//  constexpr auto kWramId  = kNesChips.base_id(0);  // 0
//  constexpr auto kCiramId = kNesChips.base_id(1);  // 2
//  constexpr auto kSramId  = kNesChips.base_id(2);  // 4
//  // dynamic pool starts at kNesChips.dynamic_base_id() = 12
//
//  BusMemory<NesCpuBusConfig> mem{kNesChips};
//
//  // Insert cartridge at runtime (e.g. on file load)
//  void insert_cartridge(BusMemory<NesCpuBusConfig>& mem,
//                        NesCpuBus& bus,
//                        std::span<const uint8_t> prg_data) {
//      const size_t num_pages = (prg_data.size() + 1023) / 1024;
//      const auto cart_id = mem.add_chip("PRG-ROM", num_pages, /*read_only=*/true);
//      assert(cart_id != BusMemory<NesCpuBusConfig>::kInvalidChipId);
//      mem.load(cart_id, prg_data);
//      // Map last 16 KB of ROM fixed at $C000–$FFFF
//      const size_t last_bank = num_pages - 16;
//      bus.fill_read_pages(0, 48, 16, cart_id + last_bank);
//  }
//
//  // Remove cartridge
//  void remove_cartridge(BusMemory<NesCpuBusConfig>& mem,
//                        NesCpuBus& bus,
//                        ChipId cart_id) {
//      bus.map_no_chip_selected(0, 48, 16);  // unmap before removing
//      mem.remove_chip(cart_id);
//  }
//
// ── Partial-bus mask setup (Apple I) ─────────────────────────────────────────
//
//  inline constexpr auto kApple1Chips = make_chip_manifest<4>({
//      {"RAM_LO", 1, false},  // first 2114:  D0–D3
//      {"RAM_HI", 1, false},  // second 2114: D4–D7
//      {"ROM",    2, true },  // 2 KB ROM
//      {"PIA",    1, false},  // 6820 PIA
//  });
//
//  constexpr auto kRamLoId = kApple1Chips.base_id(0);
//  constexpr auto kRamHiId = kApple1Chips.base_id(1);
//  constexpr auto kRomId   = kApple1Chips.base_id(2);
//  constexpr auto kPiaId   = kApple1Chips.base_id(3);
//
//  MemoryBus<Generic8BitConfig> bus;
//  BusMemory<Generic8BitConfig> mem{kApple1Chips};
//  mem.connect(bus);
//
//  // Set 4-bit data masks for the two 2114 chips
//  bus.set_chip_data_mask(kRamLoId, 0x0Fu);  // D0–D3
//  bus.set_chip_data_mask(kRamHiId, 0xF0u);  // D4–D7
//
//  // Normal mode: treat each pair as a single 8-bit chip (default mask=0xFF)
//  // Fault-injection: route both chip ids to the same page; each bitmix pass
//  // contributes its nibble, leaving the other nibble from the bus word.
//
// =============================================================================
