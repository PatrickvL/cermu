#pragma once

/*
 * CERMU - Cross-platform compiler compatibility macros
 * Provides consistent interface for compiler-specific optimizations and attributes
 *
 * ALL compiler/platform conditional logic belongs here.  Source files should
 * never test _MSC_VER, __GNUC__, __clang__, _WIN32, __APPLE__ etc. directly;
 * instead include this header and use the CERMU_* macros defined below.
 */

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

/* ========================================================================== */
/* COMPILER IDENTIFICATION                                                    */
/* ========================================================================== */

/* Useful for conditional compilation based on compiler capabilities */
#if defined(__GNUC__) && !defined(__clang__)
    #define CERMU_COMPILER_GCC 1
#elif defined(__clang__)
    #define CERMU_COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #define CERMU_COMPILER_MSVC 1
#else
    #define CERMU_COMPILER_UNKNOWN 1
#endif

/* ========================================================================== */
/* PLATFORM IDENTIFICATION                                                    */
/* ========================================================================== */

#if defined(_WIN32) || defined(_WIN64)
    #define CERMU_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define CERMU_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define CERMU_PLATFORM_LINUX 1
#else
    #define CERMU_PLATFORM_UNKNOWN 1
#endif

/* ========================================================================== */
/* PREPROCESSOR CAPABILITIES                                                  */
/* ========================================================================== */

/* __VA_OPT__ is available in C++20, and as an extension in C++17 mode on
   GCC 8+, Clang 6+, and MSVC with /Zc:preprocessor.  MSVC's traditional
   preprocessor (the default before VS 2022 17.0) does NOT support it. */
#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL
    /* MSVC traditional preprocessor — no __VA_OPT__ */
#else
    #define CERMU_HAS_VA_OPT 1
#endif

/* Portable preprocessor utilities for variadic-macro overloading.
   Required for MSVC's traditional preprocessor where __VA_OPT__ is absent;
   available unconditionally for convenience.

   CERMU_PP_EXPAND_    — identity that forces a rescan (fixes MSVC __VA_ARGS__)
   CERMU_PP_CAT_       — token paste with forced expansion of both operands
   CERMU_PP_NARGS_     — count variadic arguments (1..5)
   CERMU_PP_OVERLOAD_  — call prefix##N(...) where N = arg count */
#define CERMU_PP_EXPAND_(...)  __VA_ARGS__
#define CERMU_PP_CAT_(a, b)    CERMU_PP_CAT_I_(a, b)
#define CERMU_PP_CAT_I_(a, b)  a##b
#define CERMU_PP_NARGS_(...)   CERMU_PP_EXPAND_(CERMU_PP_NARGS_I_(__VA_ARGS__, 5, 4, 3, 2, 1))
#define CERMU_PP_NARGS_I_(_1, _2, _3, _4, _5, N, ...)  N
#define CERMU_PP_OVERLOAD_(prefix, ...) \
    CERMU_PP_EXPAND_(CERMU_PP_CAT_(prefix, CERMU_PP_NARGS_(__VA_ARGS__))(__VA_ARGS__))

/* ========================================================================== */
/* PATH SEPARATOR                                                             */
/* ========================================================================== */

#ifdef CERMU_PLATFORM_WINDOWS
    #define CERMU_PATH_SEPARATOR     '\\'
    #define CERMU_PATH_SEPARATOR_STR "\\"
#else
    #define CERMU_PATH_SEPARATOR     '/'
    #define CERMU_PATH_SEPARATOR_STR "/"
#endif

/* ========================================================================== */
/* CASE-INSENSITIVE STRING COMPARISON                                         */
/* ========================================================================== */

/* Portability wrappers for strcasecmp / strncasecmp (POSIX) vs
   _stricmp / _strnicmp (MSVC).  Use cermu_strcasecmp / cermu_strncasecmp
   instead of the platform-specific names in all source files. */
#ifdef CERMU_PLATFORM_WINDOWS
    #include <cstring>
    #define cermu_strcasecmp   _stricmp
    #define cermu_strncasecmp  _strnicmp
#else
    #include <strings.h>
    #define cermu_strcasecmp   strcasecmp
    #define cermu_strncasecmp  strncasecmp
#endif

/* ========================================================================== */
/* POSIX STAT COMPATIBILITY                                                   */
/* ========================================================================== */

/* MSVC <sys/stat.h> does not define S_ISREG / S_ISDIR.  Provide them here
   so that source files can use the standard POSIX macros unconditionally. */
#ifdef CERMU_COMPILER_MSVC
    #include <sys/stat.h>
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif
#endif

/* ========================================================================== */
/* DIRECTORY ITERATION SUPPORT                                                */
/* ========================================================================== */

/* MSVC does not ship <dirent.h>; use C++17 <filesystem> instead. */
#ifdef CERMU_COMPILER_MSVC
    #define CERMU_USE_STD_FILESYSTEM 1
#else
    #define CERMU_USE_DIRENT 1
#endif

/* ========================================================================== */
/* SEH (STRUCTURED EXCEPTION HANDLING) SUPPORT                                */
/* ========================================================================== */

/* Only available on Windows (MSVC). */
#ifdef CERMU_PLATFORM_WINDOWS
    #define CERMU_HAS_SEH 1
#endif

/* ========================================================================== */
/* WARNING MANAGEMENT                                                         */
/* ========================================================================== */

/* Portable MSVC-warning push/pop/disable.  On non-MSVC compilers these
   expand to nothing so call-sites don't need their own #ifdefs. */
#if defined(CERMU_COMPILER_MSVC)
    #define CERMU_MSVC_WARNING_PUSH          __pragma(warning(push))
    #define CERMU_MSVC_WARNING_DISABLE(num)  __pragma(warning(disable: num))
    #define CERMU_MSVC_WARNING_POP           __pragma(warning(pop))
#else
    #define CERMU_MSVC_WARNING_PUSH
    #define CERMU_MSVC_WARNING_DISABLE(num)
    #define CERMU_MSVC_WARNING_POP
#endif

/* Mark a region as a "system header" to suppress all warnings.
   Useful for included files that are not under our control or for
   intentionally non-conforming code (e.g. lint-prevention headers). */
#if defined(CERMU_COMPILER_CLANG)
    #define CERMU_PRAGMA_SYSTEM_HEADER  _Pragma("clang system_header")
#elif defined(CERMU_COMPILER_GCC)
    #define CERMU_PRAGMA_SYSTEM_HEADER  _Pragma("GCC system_header")
#elif defined(CERMU_COMPILER_MSVC)
    #define CERMU_PRAGMA_SYSTEM_HEADER  __pragma(warning(push, 0))
#else
    #define CERMU_PRAGMA_SYSTEM_HEADER
#endif

/* ========================================================================== */
/* BRANCH PREDICTION HINTS                                                    */
/* ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#else
    #define likely(x)   (x)
    #define unlikely(x) (x)
#endif

/* ========================================================================== */
/* FALLTHROUGH ATTRIBUTE                                                      */
/* ========================================================================== */

/* [[fallthrough]] is a C++17 standard attribute. */
#define FALLTHROUGH [[fallthrough]]

/* ========================================================================== */
/* INLINE FORCING                                                             */
/* ========================================================================== */

#if defined(CERMU_COMPILER_MSVC)
    #define FORCE_INLINE __forceinline
#elif defined(CERMU_COMPILER_GCC) || defined(CERMU_COMPILER_CLANG)
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

#if defined(CERMU_COMPILER_MSVC)
    #define FORCE_NOINLINE __declspec(noinline)
#elif defined(CERMU_COMPILER_GCC) || defined(CERMU_COMPILER_CLANG)
    #define FORCE_NOINLINE __attribute__((noinline))
#else
    #define FORCE_NOINLINE
#endif

/* ========================================================================== */
/* COUNT TRAILING ZEROS                                                       */
/* ========================================================================== */

/* Cross-platform count-trailing-zeros for unsigned 32-bit values.
 * Undefined when x == 0 (matches hardware CTZ behavior). */
#if defined(CERMU_COMPILER_MSVC)
    #include <intrin.h>
    [[nodiscard]] FORCE_INLINE int cermu_ctz(unsigned int x) noexcept {
        unsigned long idx;
        _BitScanForward(&idx, x);
        return static_cast<int>(idx);
    }
#elif defined(CERMU_COMPILER_GCC) || defined(CERMU_COMPILER_CLANG)
    #define cermu_ctz(x) __builtin_ctz(x)
#else
    [[nodiscard]] FORCE_INLINE int cermu_ctz(unsigned int x) noexcept {
        int n = 0;
        if (!(x & 0x0000FFFFu)) { n += 16; x >>= 16; }
        if (!(x & 0x000000FFu)) { n +=  8; x >>=  8; }
        if (!(x & 0x0000000Fu)) { n +=  4; x >>=  4; }
        if (!(x & 0x00000003u)) { n +=  2; x >>=  2; }
        if (!(x & 0x00000001u)) { n +=  1; }
        return n;
    }
#endif

/* ========================================================================== */
/* REGISTER CALLING CONVENTIONS                                               */
/* ========================================================================== */

/* Optimize function calls by using register calling convention where supported */
#if defined(CERMU_COMPILER_MSVC) && (defined(_M_IX86) || defined(_M_X64))
    #define REGISTER_CALL __fastcall
#elif defined(CERMU_COMPILER_GCC) || defined(CERMU_COMPILER_CLANG)
    #if defined(__i386__) || defined(__x86_64__)
        #define REGISTER_CALL __attribute__((regparm(3)))
    #else
        #define REGISTER_CALL
    #endif
#else
    #define REGISTER_CALL
#endif

/* ========================================================================== */
/* ALIGNED MEMORY ALLOCATION                                                  */
/* ========================================================================== */

#if defined(CERMU_PLATFORM_WINDOWS)
    #include <malloc.h>
    #define cermu_aligned_alloc(alignment, size) _aligned_malloc((size), (alignment))
    #define cermu_aligned_free(ptr)              _aligned_free(ptr)
#else
    /* C++17 std::aligned_alloc is available on all remaining targets. */
    #define cermu_aligned_alloc(alignment, size) std::aligned_alloc((alignment), (size))
    #define cermu_aligned_free(ptr)              std::free(ptr)
#endif

/* ========================================================================== */
/* BIT MANIPULATION FUNCTIONS                                                 */
/* ========================================================================== */

/* Population count (number of set bits) */
#if defined(CERMU_COMPILER_GCC) || defined(CERMU_COMPILER_CLANG)
    #define cermu_popcount(x)   __builtin_popcount(x)
    #define cermu_popcountl(x)  __builtin_popcountl(x)
    #define cermu_popcountll(x) __builtin_popcountll(x)
#elif defined(CERMU_COMPILER_MSVC)
    #include <intrin.h>
    #define cermu_popcount(x)   __popcnt(x)
    #define cermu_popcountl(x)  __popcnt(x)
    #define cermu_popcountll(x) __popcnt64(x)
#else
    [[nodiscard]] FORCE_INLINE int cermu_popcount(unsigned int x) noexcept {
        x = x - ((x >> 1) & 0x55555555u);
        x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
        x = (x + (x >> 4)) & 0x0F0F0F0Fu;
        x = x + (x >> 8);
        x = x + (x >> 16);
        return static_cast<int>(x & 0x3Fu);
    }
    [[nodiscard]] FORCE_INLINE int cermu_popcountl(unsigned long x) noexcept {
        return cermu_popcount(static_cast<unsigned int>(x)) +
               (sizeof(long) > sizeof(int) ? cermu_popcount(static_cast<unsigned int>(x >> 32)) : 0);
    }
    [[nodiscard]] FORCE_INLINE int cermu_popcountll(unsigned long long x) noexcept {
        return cermu_popcount(static_cast<unsigned int>(x)) +
               cermu_popcount(static_cast<unsigned int>(x >> 32));
    }
#endif

/* Parity (1 when an odd number of bits are set, 0 otherwise).
   Equivalent to __builtin_parity on GCC/Clang. */
#if defined(CERMU_COMPILER_GCC) || defined(CERMU_COMPILER_CLANG)
    #define cermu_parity(x) __builtin_parity(x)
#else
    #define cermu_parity(x) (cermu_popcount(x) & 1)
#endif

/* ========================================================================== */
/* BITMIX — bitwise multiplexer: select bits from A or B per mask             */
/* ========================================================================== */
/*
 * bitmix(a, b, mask) = (a & mask) | (b & ~mask)
 *
 * Equivalent to the RISC-V Zbt "cmix" instruction.
 * The XOR-AND-XOR form  b ^ ((a ^ b) & mask)  avoids NOT and compiles to
 * the optimal sequence on every target: ANDN+OR (x86 BMI1), BIC+ORR (AArch64),
 * single cmix (RISC-V Zbt).  When mask is a compile-time constant the compiler
 * eliminates even the XOR pair.
 */
template<std::unsigned_integral T>
[[nodiscard]] FORCE_INLINE T bitmix(T a, T b, T mask) noexcept {
#if defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen >= 32 \
    && defined(__riscv_zbt)
    if constexpr (sizeof(T) <= sizeof(uint32_t)) {
        uint32_t r;
        __asm__ volatile("cmix %0, %1, %2, %3"
                         : "=r"(r)
                         : "r"(uint32_t(mask)), "r"(uint32_t(a)), "r"(uint32_t(b)));
        return T(r);
    } else {
        uint64_t r;
        __asm__ volatile("cmix %0, %1, %2, %3"
                         : "=r"(r)
                         : "r"(uint64_t(mask)), "r"(uint64_t(a)), "r"(uint64_t(b)));
        return T(r);
    }
#else
    return T(b ^ ((a ^ b) & mask));
#endif
}

/* Smallest unsigned type that can hold any value in [0, 2^Bits). */
template<std::size_t Bits>
using uint_least_bits_t =
    std::conditional_t<(Bits <=  8), uint8_t,
    std::conditional_t<(Bits <= 16), uint16_t,
    std::conditional_t<(Bits <= 32), uint32_t,
    uint64_t>>>;

/* ========================================================================== */
/* LFSR — Galois linear-feedback shift registers                              */
/* ========================================================================== */
/*
 * Maximal-length Galois LFSRs for pseudorandom bit generation.
 * Primary use: bus-decay simulation where undriven data lines drift toward
 * an idle level.  Feed the low 8 bits of the LFSR state as a mask into
 * BUS_FLOAT_DATA_DECAY_HIGH/LOW to randomly select ~50% of data bits to
 * float each tick.  After a handful of steps all bits have been floated.
 *
 * Galois form: new_state = (state >> 1) ^ ((0 - (state & 1)) & taps)
 * Zero is an absorbing state — callers must seed with a non-zero value.
 */
template<std::unsigned_integral T, T Taps>
[[nodiscard]] FORCE_INLINE T lfsr_step(T state) noexcept {
    return T((state >> 1) ^ ((T{0} - (state & T{1})) & Taps));
}

/* 16-bit Galois LFSR — maximal period 65535. Taps: 0xB400 (poly x^16+x^14+x^13+x^11+1). */
[[nodiscard]] FORCE_INLINE uint16_t lfsr16_step(uint16_t state) noexcept {
    return lfsr_step<uint16_t, 0xB400>(state);
}

/* 8-bit Galois LFSR — maximal period 255. Taps: 0xB4 (poly x^8+x^6+x^5+x^4+1). */
[[nodiscard]] FORCE_INLINE uint8_t lfsr8_step(uint8_t state) noexcept {
    return lfsr_step<uint8_t, 0xB4>(state);
}

/* ========================================================================== */
/* RUNTIME VERBOSITY                                                          */
/* ========================================================================== */

/* When true, registries and subsystems print informational messages during
   startup ("Registered ...", "Attached ...", etc.).  Off by default;
   enable with --verbose / -v on the command line. */
inline bool g_verbose = false;

/* ========================================================================== */
/* FEATURE FLAGS — AUTOMATIC IMPLICATIONS                                     */
/* ========================================================================== */

/* GUI builds always include chip-debug instrumentation (registry, fields).
   Headless-debug builds may define CERMU_HAS_CHIP_DEBUG independently. */
#if defined(CERMU_HAS_GUI) && !defined(CERMU_HAS_CHIP_DEBUG)
    #define CERMU_HAS_CHIP_DEBUG
#endif