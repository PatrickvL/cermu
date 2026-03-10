#pragma once
// =============================================================================
// SID Register Write Log — Binary Capture & Replay Format
// =============================================================================
// Compact binary format for recording every SID register write during C64
// emulation.  Used by:
//   - cermu --sid-log <file>          (headless capture mode)
//   - sid_comparison_runner --log <file>  (replay against reSID)
//
// Format:
//   Header  (32 bytes)  — magic, version, chip model, clock, entry count
//   Entries (6 bytes ea) — absolute cycle + register + value, packed
//
// At PAL clock (985248 Hz), uint32_t cycle wraps after ~72 minutes,
// which is sufficient for demo captures.
// =============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace sid_log {

// ─────────────────────────────────────────────────────────────────────────────
// Binary structures (all little-endian on x86/ARM)
// ─────────────────────────────────────────────────────────────────────────────

static constexpr char MAGIC[8] = {'S','I','D','L','O','G','\0','\0'};
static constexpr uint32_t FORMAT_VERSION = 1;

#pragma pack(push, 1)
struct header_t {
    char     magic[8];          // "SIDLOG\0\0"
    uint32_t version;           // Format version (1)
    uint32_t chip_model;        // 0 = 6581, 1 = 8580
    uint32_t cpu_clock;         // CPU clock in Hz (e.g. 985248)
    uint32_t entry_count;       // Number of write entries
    uint32_t reserved[2];       // Future use (zero)
};

struct entry_t {
    uint32_t cycle;             // Absolute SID cycle count
    uint8_t  reg;               // Register offset (0x00–0x18)
    uint8_t  value;             // Data written
};
#pragma pack(pop)

static_assert(sizeof(header_t) == 32, "header must be 32 bytes");
static_assert(sizeof(entry_t)  ==  6, "entry must be 6 bytes");

// ─────────────────────────────────────────────────────────────────────────────
// In-memory log used during capture
// ─────────────────────────────────────────────────────────────────────────────

struct write_log_t {
    uint32_t chip_model = 0;    // 0 = 6581, 1 = 8580
    uint32_t cpu_clock  = 0;    // CPU clock Hz
    std::vector<entry_t> entries;
};

// ─────────────────────────────────────────────────────────────────────────────
// Callback for mos6581_t::write_capture_fn
// Context pointer must point to a valid write_log_t.
// ─────────────────────────────────────────────────────────────────────────────

inline void capture_callback(void* ctx, uint32_t cycle, uint8_t reg, uint8_t value) {
    auto* log = static_cast<write_log_t*>(ctx);
    log->entries.push_back({cycle, reg, value});
}

// ─────────────────────────────────────────────────────────────────────────────
// File I/O
// ─────────────────────────────────────────────────────────────────────────────

inline bool write_file(const char* path, const write_log_t& log) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    header_t hdr{};
    memcpy(hdr.magic, MAGIC, 8);
    hdr.version     = FORMAT_VERSION;
    hdr.chip_model  = log.chip_model;
    hdr.cpu_clock   = log.cpu_clock;
    hdr.entry_count = static_cast<uint32_t>(log.entries.size());

    bool ok = fwrite(&hdr, sizeof(hdr), 1, f) == 1
           && fwrite(log.entries.data(), sizeof(entry_t), log.entries.size(), f) == log.entries.size();

    fclose(f);
    return ok;
}

inline bool read_file(const char* path, write_log_t& log) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    header_t hdr{};
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return false; }
    if (memcmp(hdr.magic, MAGIC, 8) != 0)     { fclose(f); return false; }
    if (hdr.version != FORMAT_VERSION)         { fclose(f); return false; }

    log.chip_model = hdr.chip_model;
    log.cpu_clock  = hdr.cpu_clock;
    log.entries.resize(hdr.entry_count);

    bool ok = fread(log.entries.data(), sizeof(entry_t), hdr.entry_count, f) == hdr.entry_count;
    fclose(f);
    return ok;
}

} // namespace sid_log
