#pragma once

// ============================================================================
// GPU RGBI Stream Shader — note on reuse
// ============================================================================
//
// RGBI (C128 VDC, EGA) uses a 4-bit index {R, G, B, I} stored as:
//   struct RGBIVideoSample { uint8_t rgbi; VideoFlags flags; };
//
// This has the same memory layout as CompositeVideoSample:
//   [index_byte, flags_byte] — 2 bytes per sample, R8 stream texture.
//
// The existing composite stream_shader (stream_shader.hpp) handles RGBI
// directly.  The system simply uploads a 16-entry palette instead of 256.
// No separate shader program is needed.
//
// The extraction function below handles the RGBI-specific masking
// (bits [3:0] only, upper nibble zeroed) in case future RGBIVideoSample
// uses upper bits for something.
// ============================================================================

#include <cstdint>

namespace rgbi_stream_shader {

// Extract rgbi index bytes from RGBIVideoSample stream.
// RGBIVideoSample layout: [rgbi, flags] — 2 bytes, aligned to 2.
// Output: masked to 4-bit index (0–15).
inline void extract_rgbi_samples(const void* stream_samples, uint32_t stream_len,
                                 uint8_t* index_buf) {
    const uint8_t* src = static_cast<const uint8_t*>(stream_samples);
    for (uint32_t i = 0; i < stream_len; i++) {
        index_buf[i] = src[i * 2] & 0x0F;   // rgbi is first byte, mask to 4 bits
    }
}

} // namespace rgbi_stream_shader
