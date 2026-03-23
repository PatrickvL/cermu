#pragma once

// ============================================================================
// RegisterFile<Size, NativeT> — unified register storage
// ============================================================================
//
// OVERVIEW
//
// A single, aligned byte array that serves as both the working register
// file and the debug backing store.  No sync, no mirror.
//
// Three access patterns, all compiling to a single host instruction:
//
//   1. Typed constant (RegIdx):   regs[PC] = 0x1000;   val = regs[A];
//   2. Runtime integer index:     regs[n]  — native-width register #n
//   3. Runtime byte-offset:       regs.at<uint16_t>(off)
//
// Multi-byte values are stored in HOST byte order.  Sub-register byte
// offsets are computed at compile time via lo_b/hi_b/lo_w/hi_w, which
// use CERMU_LITTLE_ENDIAN from cermu.hpp — the single endian-conditional
// in the project.
//
// USAGE — CPU registers:
//
//   CpuRegisterFile<32, uint16_t>  regs_;           // Z80, 6502, 6809
//   CpuRegisterFile<100, uint32_t> regs_;           // M68K
//
//   constexpr r16 AF {0};
//   constexpr r8  A = hi_b(AF);
//   constexpr r8  F = lo_b(AF);
//
//   regs_[A] = regs_[B];       // proxy: both read and write target
//   regs_[AF] = 0x1234;        // 16-bit write
//   regs_[n]  = val;           // runtime index → native width
//
// USAGE — MMIO registers:
//
//   RegisterFile<64>  regs_;                        // VIC-II, 64 × 8-bit
//   RegisterFile<32>  write_regs_, read_regs_;      // POKEY, TIA
//
//   regs_.at<uint8_t>(addr & 0x3F) = data;          // masked write
//   val = regs_.at<uint8_t>(status_reg);             // read
//
// DEBUG — ChipDebugRegistry:
//
//   r.set_registers(regs_.debug_ptr(), regs_.debug_size(), REG_INFO);
//
// ============================================================================

#include "core/cermu.hpp"   // CERMU_LITTLE_ENDIAN
#include <cstdint>
#include <cstring>           // std::memcpy
#include <type_traits>
#include <utility>           // std::pair

// ============================================================================
// RegIdx<T> — typed register index (carries width at compile time)
// ============================================================================

template <typename T>
struct RegIdx {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 4);
    uint16_t offset;        // Byte offset into the register file
    constexpr RegIdx(uint16_t o) : offset(o) {}
};

using r8  = RegIdx<uint8_t>;
using r16 = RegIdx<uint16_t>;
using r32 = RegIdx<uint32_t>;

// ============================================================================
// Sub-register extractors — named by output size, one argument
// ============================================================================
//
// Split a parent register into byte or word sub-registers.
// The returned RegIdx points to the correct byte regardless of host
// endianness.  All constexpr — resolved at compile time.
//
//   lo_b(r16)  → r8   bits [7:0]      hi_b(r16)  → r8   bits [15:8]
//   lo_w(r32)  → r16  bits [15:0]     hi_w(r32)  → r16  bits [31:16]
//   lo_b(r32)  → r8   bits [7:0]      hi_b(r32)  → r8   bits [31:24]

// 16-bit parent → 8-bit halves
constexpr r8 lo_b(r16 p) {
#ifdef CERMU_LITTLE_ENDIAN
    return {p.offset};
#else
    return {static_cast<uint16_t>(p.offset + 1)};
#endif
}
constexpr r8 hi_b(r16 p) {
#ifdef CERMU_LITTLE_ENDIAN
    return {static_cast<uint16_t>(p.offset + 1)};
#else
    return {p.offset};
#endif
}

// 32-bit parent → 16-bit halves
constexpr r16 lo_w(r32 p) {
#ifdef CERMU_LITTLE_ENDIAN
    return {p.offset};
#else
    return {static_cast<uint16_t>(p.offset + 2)};
#endif
}
constexpr r16 hi_w(r32 p) {
#ifdef CERMU_LITTLE_ENDIAN
    return {static_cast<uint16_t>(p.offset + 2)};
#else
    return {p.offset};
#endif
}

// 32-bit parent → 8-bit extremes
constexpr r8 lo_b(r32 p) {
#ifdef CERMU_LITTLE_ENDIAN
    return {p.offset};
#else
    return {static_cast<uint16_t>(p.offset + 3)};
#endif
}
constexpr r8 hi_b(r32 p) {
#ifdef CERMU_LITTLE_ENDIAN
    return {static_cast<uint16_t>(p.offset + 3)};
#else
    return {p.offset};
#endif
}

// ============================================================================
// RegRef<T> — proxy reference for single-instruction read/write
// ============================================================================
//
// Returned by RegisterFile::operator[] and at<T>().  Acts as both an
// lvalue (write target) and rvalue (read source).
//
// Uses std::memcpy for type-punning instead of reinterpret_cast.  This is
// the only standards-compliant way to alias through a uint8_t[] backing
// store — writing as uint32_t then reading overlapping bytes as uint16_t
// via reinterpret_cast is a strict-aliasing violation that GCC miscompiles
// at -O2 (sub-register reads return stale/zero values).  All three major
// compilers (GCC, Clang, MSVC) optimize memcpy of 1/2/4 bytes to a single
// native load/store instruction at -O1 and above.

template <typename T>
struct RegRef {
    uint8_t* p;

    // Read
    inline operator T() const {
        T v; std::memcpy(&v, p, sizeof(T)); return v;
    }

    // Write
    inline RegRef& operator=(T v) {
        std::memcpy(p, &v, sizeof(T)); return *this;
    }

    // Proxy-to-proxy
    inline RegRef& operator=(const RegRef& o) {
        T v; std::memcpy(&v, o.p, sizeof(T));
        std::memcpy(p, &v, sizeof(T)); return *this;
    }

    // Compound assignment
    inline RegRef& operator|=(T v)  { *this = T(*this) | v; return *this; }
    inline RegRef& operator&=(T v)  { *this = T(*this) & v; return *this; }
    inline RegRef& operator^=(T v)  { *this = T(*this) ^ v; return *this; }
    inline RegRef& operator+=(T v)  { *this = T(*this) + v; return *this; }
    inline RegRef& operator-=(T v)  { *this = T(*this) - v; return *this; }

    // Increment / decrement
    inline T operator++()     { T v = T(*this) + 1; *this = v; return v; }
    inline T operator++(int)  { T v = *this; *this = T(v + 1); return v; }
    inline T operator--()     { T v = T(*this) - 1; *this = v; return v; }
    inline T operator--(int)  { T v = *this; *this = T(v - 1); return v; }
};

// ============================================================================
// RegisterFile<Size, NativeT>
// ============================================================================
//
// Template parameters:
//   Size    — total byte count (must be multiple of 4 for alignment)
//   NativeT — type returned by integer-indexed operator[] (default uint8_t)
//
// CPU usage:   RegisterFile<32, uint16_t>   — Z80, 6502, 6809
//              RegisterFile<100, uint32_t>  — M68K
// MMIO usage:  RegisterFile<64>             — VIC-II (64 × uint8_t)
//              RegisterFile<32>             — POKEY write set or read set

template <uint16_t Size, typename NativeT = uint8_t>
struct RegisterFile {
    static_assert(Size > 0 && Size % 4 == 0,
                  "Size must be a positive multiple of 4");
    static_assert(std::is_integral_v<NativeT> && sizeof(NativeT) <= 4);

    static constexpr uint16_t N = sizeof(NativeT);

    alignas(4) uint8_t data[Size]{};

    // ── Typed constant index ─────────────────────────────────────
    //    regs[PC] = 0x1000;       val = regs[A];

    template <typename T>
    inline RegRef<T> operator[](RegIdx<T> idx) {
        return {&data[idx.offset]};
    }
    template <typename T>
    inline T operator[](RegIdx<T> idx) const {
        T v; std::memcpy(&v, &data[idx.offset], sizeof(T)); return v;
    }

    // ── Runtime register-number index → native width ─────────────
    //    regs[n] for M68K Dn (32-bit), Z80 reg pair (16-bit), etc.
    //    Index is a register NUMBER, not byte offset (scaled by N).

    inline RegRef<NativeT> operator[](uint16_t idx) {
        return {&data[idx * N]};
    }
    inline NativeT operator[](uint16_t idx) const {
        NativeT v; std::memcpy(&v, &data[idx * N], sizeof(NativeT)); return v;
    }

    // ── Runtime byte-offset, explicit width ──────────────────────
    //    regs.at<uint8_t>(addr & 0x3F) — MMIO address-masked access

    template <typename T>
    inline RegRef<T> at(uint16_t byte_off) {
        return {&data[byte_off]};
    }
    template <typename T>
    inline T at(uint16_t byte_off) const {
        T v; std::memcpy(&v, &data[byte_off], sizeof(T)); return v;
    }

    // ── Edge detection helper ────────────────────────────────────
    //    auto [old, ref] = regs.edge(idx);
    //    ref = new_val;
    //    if ((old ^ T(ref)) & BIT) { ... }

    template <typename T>
    inline std::pair<T, RegRef<T>> edge(RegIdx<T> idx) {
        T old; std::memcpy(&old, &data[idx.offset], sizeof(T));
        return {old, {&data[idx.offset]}};
    }

    template <typename T = uint8_t>
    inline std::pair<T, RegRef<T>> edge_at(uint16_t byte_off) {
        T old; std::memcpy(&old, &data[byte_off], sizeof(T));
        return {old, {&data[byte_off]}};
    }

    // ── Swap two registers ─────────────────────────────────────
    //    regs.swap(AF, AF_);  — EX AF,AF' etc.

    template <typename T>
    inline void swap(RegIdx<T> a, RegIdx<T> b) {
        T tmp;
        std::memcpy(&tmp, &data[a.offset], sizeof(T));
        std::memcpy(&data[a.offset], &data[b.offset], sizeof(T));
        std::memcpy(&data[b.offset], &tmp, sizeof(T));
    }

    // ── Debug — zero copy ────────────────────────────────────────

    const uint8_t*             debug_ptr()  const { return data; }
    static constexpr uint16_t  debug_size()       { return Size; }
};

// ── Type alias for CPU register files ────────────────────────────

template <uint16_t Size, typename NativeT = uint8_t>
using CpuRegisterFile = RegisterFile<Size, NativeT>;
