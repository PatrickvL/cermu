#pragma once
// gcr.hpp — GCR (Group Code Recording) encoding/decoding for Commodore disk drives
//
// The 1541's MOS 325572-01 R/W logic uses 4-to-5 bit GCR encoding to ensure
// at most one consecutive zero in the bitstream, which is required for reliable
// flux transition detection on the magnetic surface.
//
// Each raw byte (8 bits) is encoded as two GCR nybbles (10 bits):
//   hi_nybble → 5 GCR bits, then lo_nybble → 5 GCR bits.
//
// Track layout: each sector is preceded by a SYNC mark (≥10 consecutive 1-bits)
// and consists of a header block and a data block, separated by header/data gaps.
//
// This module provides:
//   1. GCR nybble encode/decode tables
//   2. Byte-level GCR encode/decode
//   3. Full track encoder: D64 sector data → GCR bitstream per track

#include <cstdint>
#include <cstring>
#include <vector>

namespace gcr {

// ============================================================================
// GCR nybble tables (from Commodore's 325572-01 R/W logic)
// ============================================================================

/// 4-bit → 5-bit GCR encoding table.  Index is raw nybble (0x0–0xF).
inline constexpr uint8_t encode_nybble[16] = {
    0x0A, 0x0B, 0x12, 0x13,   // 0→01010  1→01011  2→10010  3→10011
    0x0E, 0x0F, 0x16, 0x17,   // 4→01110  5→01111  6→10110  7→10111
    0x09, 0x19, 0x1A, 0x1B,   // 8→01001  9→11001  A→11010  B→11011
    0x0D, 0x1D, 0x1E, 0x15,   // C→01101  D→11101  E→11110  F→10101
};

/// 5-bit → 4-bit GCR decoding table.  Index is GCR code (0x00–0x1F).
/// Invalid codes decode to 0xFF.
inline constexpr uint8_t decode_nybble[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x00-0x07: invalid
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  // 0x08-0x0F
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  // 0x10-0x17
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF,  // 0x18-0x1F
};

// ============================================================================
// Byte-level GCR encode: 4 raw bytes → 5 GCR bytes (32 bits → 40 bits)
// ============================================================================
//
// The encoding processes 4 bytes at a time (8 nybbles → 8 GCR codes = 40 bits).
// This is the natural grouping since 40 bits pack evenly into 5 bytes.

/// Encode 4 raw bytes into 5 GCR bytes.
inline void encode_4_to_5(const uint8_t* raw, uint8_t* gcr_out) {
    // Convert 4 bytes to 8 GCR nybbles (5 bits each = 40 bits total)
    uint8_t g[8];
    for (int i = 0; i < 4; ++i) {
        g[i * 2]     = encode_nybble[raw[i] >> 4];
        g[i * 2 + 1] = encode_nybble[raw[i] & 0x0F];
    }
    // Pack 8 × 5-bit codes into 5 bytes (MSB first)
    gcr_out[0] = (g[0] << 3) | (g[1] >> 2);
    gcr_out[1] = (g[1] << 6) | (g[2] << 1) | (g[3] >> 4);
    gcr_out[2] = (g[3] << 4) | (g[4] >> 1);
    gcr_out[3] = (g[4] << 7) | (g[5] << 2) | (g[6] >> 3);
    gcr_out[4] = (g[6] << 5) | g[7];
}

// ============================================================================
// Track layout constants
// ============================================================================

/// Number of SYNC bytes before header and data blocks.
inline constexpr int SYNC_BYTES = 5;

/// Header gap (between header and data SYNC).
inline constexpr int HEADER_GAP_BYTES = 9;

/// Header block: 8 raw bytes → 10 GCR bytes (2 groups of 4→5).
inline constexpr int HEADER_RAW_BYTES = 8;
inline constexpr int HEADER_GCR_BYTES = 10;

/// Data block: 260 raw bytes → 325 GCR bytes (65 groups of 4→5).
inline constexpr int DATA_RAW_BYTES = 260;
inline constexpr int DATA_GCR_BYTES = 325;

/// Header block ID byte (0x08 for sector header).
inline constexpr uint8_t HEADER_ID = 0x08;

/// Data block ID byte (0x07 for sector data).
inline constexpr uint8_t DATA_ID = 0x07;

/// Sectors per track lookup (1-indexed, index 0 is unused).
inline constexpr uint8_t sectors_per_track(uint8_t track) {
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;  // Tracks 31-42
}

/// Speed zone for a given track (0-3).
inline constexpr uint8_t speed_zone_for_track(uint8_t track) {
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

/// Raw GCR bytes per full track rotation, indexed by speed zone.
/// These are the actual number of bytes the drive clocks per rotation at 300 RPM.
inline constexpr uint32_t bytes_per_track[4] = {
    6250,   // Zone 0: tracks 31-42  (250,000 bps / 300 RPM × 60 / 8)
    6666,   // Zone 1: tracks 25-30  (266,667 bps)
    7142,   // Zone 2: tracks 18-24  (285,714 bps)
    7692,   // Zone 3: tracks 1-17   (307,692 bps)
};

/// Inter-sector gap size in bytes, indexed by speed zone.
/// These fill the remaining space after all sectors are encoded.
inline constexpr int inter_sector_gap[4] = {
    9,    // Zone 0
    12,   // Zone 1
    17,   // Zone 2
    8,    // Zone 3
};

// ============================================================================
// GCR track encoder — D64 sector data → GCR bitstream
// ============================================================================

/// A GCR-encoded track as a byte array.  The bitstream wraps around at
/// `num_bits` when the head reaches the end (circular track).
struct GCRTrack {
    std::vector<uint8_t> data;   // GCR byte stream (packed bits, MSB first)
    uint32_t num_bits  = 0;      // Total bits on this track
    uint32_t num_bytes = 0;      // = (num_bits + 7) / 8

    /// Read one bit at a given bit position (auto-wraps).
    uint8_t read_bit(uint32_t bit_pos) const {
        bit_pos %= num_bits;
        return (data[bit_pos >> 3] >> (7 - (bit_pos & 7))) & 1;
    }

    /// Write one bit at a given bit position.
    void write_bit(uint32_t bit_pos, uint8_t value) {
        bit_pos %= num_bits;
        uint32_t byte_idx = bit_pos >> 3;
        uint8_t  bit_mask = 0x80 >> (bit_pos & 7);
        if (value)
            data[byte_idx] |= bit_mask;
        else
            data[byte_idx] &= ~bit_mask;
    }
};

/// Encode one D64 track into a GCR bitstream.
///
/// Parameters:
///   track        — 1-based track number
///   sector_data  — pointer to array of sector buffers (each 256 bytes)
///   num_sectors  — number of sectors on this track
///   disk_id1/id2 — disk ID bytes (from BAM, track 18 sector 0, offsets 0xA2-0xA3)
///
/// Returns a GCRTrack with the complete circular bitstream.
inline GCRTrack encode_track(uint8_t track, const uint8_t* const* sector_data,
                              uint8_t num_sectors, uint8_t disk_id1, uint8_t disk_id2)
{
    uint8_t zone = speed_zone_for_track(track);
    uint32_t track_bytes = bytes_per_track[zone];
    int gap_size = inter_sector_gap[zone];

    GCRTrack result;
    result.num_bits  = track_bytes * 8;
    result.num_bytes = track_bytes;
    result.data.resize(track_bytes, 0x55);  // Fill with 0x55 (alternating pattern)

    // Build the track byte-by-byte
    std::vector<uint8_t> track_buf;
    track_buf.reserve(track_bytes);

    for (uint8_t s = 0; s < num_sectors; ++s) {
        // ── SYNC mark (header) ──────────────────────────────────────
        for (int i = 0; i < SYNC_BYTES; ++i)
            track_buf.push_back(0xFF);

        // ── Header block (8 raw bytes → 10 GCR bytes) ──────────────
        uint8_t hdr_checksum = track ^ s ^ disk_id2 ^ disk_id1;
        uint8_t hdr_raw[8] = {
            HEADER_ID, hdr_checksum, s, track,
            disk_id2, disk_id1, 0x0F, 0x0F
        };
        uint8_t hdr_gcr[10];
        encode_4_to_5(hdr_raw,     hdr_gcr);
        encode_4_to_5(hdr_raw + 4, hdr_gcr + 5);
        for (int i = 0; i < HEADER_GCR_BYTES; ++i)
            track_buf.push_back(hdr_gcr[i]);

        // ── Header gap ──────────────────────────────────────────────
        for (int i = 0; i < HEADER_GAP_BYTES; ++i)
            track_buf.push_back(0x55);

        // ── SYNC mark (data) ────────────────────────────────────────
        for (int i = 0; i < SYNC_BYTES; ++i)
            track_buf.push_back(0xFF);

        // ── Data block (260 raw bytes → 325 GCR bytes) ─────────────
        // Raw data: [0x07] [256 data bytes] [checksum] [0x00] [0x00]
        uint8_t data_raw[260];
        data_raw[0] = DATA_ID;
        std::memcpy(data_raw + 1, sector_data[s], 256);
        uint8_t checksum = 0;
        for (int i = 0; i < 257; ++i)
            checksum ^= data_raw[i];
        data_raw[257] = checksum;
        data_raw[258] = 0x00;
        data_raw[259] = 0x00;

        // Encode 260 bytes in groups of 4 → 5 (65 groups = 325 GCR bytes)
        uint8_t data_gcr[325];
        for (int i = 0; i < 65; ++i)
            encode_4_to_5(data_raw + i * 4, data_gcr + i * 5);
        for (int i = 0; i < DATA_GCR_BYTES; ++i)
            track_buf.push_back(data_gcr[i]);

        // ── Inter-sector gap ────────────────────────────────────────
        for (int i = 0; i < gap_size; ++i)
            track_buf.push_back(0x55);
    }

    // Copy into the result buffer.  If sectors + gaps don't fill the
    // full track, the remaining bytes stay as 0x55 (gap fill).
    // If they overflow (shouldn't happen with correct gap sizes), truncate.
    size_t copy_len = (track_buf.size() <= track_bytes) ? track_buf.size() : track_bytes;
    std::memcpy(result.data.data(), track_buf.data(), copy_len);

    return result;
}

} // namespace gcr
