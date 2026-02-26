#pragma once

/* 
 * CERMU - Cross-platform compiler compatibility macros
 * Provides consistent interface for compiler-specific optimizations and attributes
 *
 * ALL compiler/platform conditional logic belongs here.  Source files should
 * never test _MSC_VER, __GNUC__, __clang__, _WIN32, __APPLE__ etc. directly;
 * instead include this header and use the CERMU_* macros defined below.
 */

/* ========================================================================== */
/* COMPILER IDENTIFICATION */
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
/* PLATFORM IDENTIFICATION */
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
/* PATH SEPARATOR */
/* ========================================================================== */

#ifdef CERMU_PLATFORM_WINDOWS
    #define CERMU_PATH_SEPARATOR     '\\'
    #define CERMU_PATH_SEPARATOR_STR "\\"
#else
    #define CERMU_PATH_SEPARATOR     '/'
    #define CERMU_PATH_SEPARATOR_STR "/"
#endif

/* ========================================================================== */
/* CASE-INSENSITIVE STRING COMPARISON */
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
/* POSIX STAT COMPATIBILITY */
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
/* DIRECTORY ITERATION SUPPORT */
/* ========================================================================== */

/* MSVC does not ship <dirent.h>; use C++17 <filesystem> instead. */
#ifdef CERMU_COMPILER_MSVC
    #define CERMU_USE_STD_FILESYSTEM 1
#else
    #define CERMU_USE_DIRENT 1
#endif

/* ========================================================================== */
/* SEH (STRUCTURED EXCEPTION HANDLING) SUPPORT */
/* ========================================================================== */

/* Only available on Windows (MSVC). */
#ifdef CERMU_PLATFORM_WINDOWS
    #define CERMU_HAS_SEH 1
#endif

/* ========================================================================== */
/* WARNING MANAGEMENT */
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
/* ALIGNMENT MACROS */
/* ========================================================================== */

/* C11 standard alignment support with fallback to compiler-specific versions */
#ifdef __cplusplus
    /* In C++, alignas is a keyword - don't redefine it */
#elif __STDC_VERSION__ >= 201112L
    #include <stdalign.h>
    /* alignas is already defined in C11 */
#elif defined(_MSC_VER)
    #define alignas(x) __declspec(align(x))
#elif defined(__GNUC__) || defined(__clang__)
    #define alignas(x) __attribute__((aligned(x)))
#else
    #define alignas(x) /* alignment not supported */
#endif

/* ========================================================================== */
/* BRANCH PREDICTION HINTS */
/* ========================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#else
    #define likely(x)   (x)
    #define unlikely(x) (x)
#endif

/* ========================================================================== */
/* FALLTHROUGH ATTRIBUTE */
/* ========================================================================== */

/* Fall-through marker for switch statements to suppress compiler warnings */
#if defined(__cplusplus) && __cplusplus >= 201703L
    #define FALLTHROUGH [[fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
    #define FALLTHROUGH __attribute__((fallthrough))
#elif defined(__clang__)
    #define FALLTHROUGH __attribute__((fallthrough))
#else
    #define FALLTHROUGH ((void)0)
#endif

/* ========================================================================== */
/* INLINE FORCING */
/* ========================================================================== */

#if defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

/* ========================================================================== */
/* REGISTER CALLING CONVENTIONS */
/* ========================================================================== */

/* Optimize function calls by using register calling convention where supported */
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    #define REGISTER_CALL __fastcall
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(__i386__) || defined(__x86_64__)
        #define REGISTER_CALL __attribute__((regparm(3)))
    #else
        #define REGISTER_CALL
    #endif
#else
    #define REGISTER_CALL
#endif

/* ========================================================================== */
/* ALIGNED MEMORY ALLOCATION */
/* ========================================================================== */

/* Cross-platform aligned memory allocation and deallocation */
#include <cstdlib>

#if defined(_WIN32)
    #include <malloc.h>
    #define cermu_aligned_alloc(alignment, size) _aligned_malloc((size), (alignment))
    #define cermu_aligned_free(ptr) _aligned_free(ptr)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /* C11 aligned_alloc */
    #define cermu_aligned_alloc(alignment, size) aligned_alloc((alignment), (size))
    #define cermu_aligned_free(ptr) free(ptr)
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    /* POSIX posix_memalign */
    static inline void* cermu_aligned_alloc(size_t alignment, size_t size) {
        void* ptr = NULL;
        return (posix_memalign(&ptr, alignment, size) == 0) ? ptr : NULL;
    }
    #define cermu_aligned_free(ptr) free(ptr)
#else
    /* Fallback to regular malloc - alignment not guaranteed */
    #define cermu_aligned_alloc(alignment, size) malloc(size)
    #define cermu_aligned_free(ptr) free(ptr)
#endif

/* ========================================================================== */
/* BIT MANIPULATION FUNCTIONS */
/* ========================================================================== */

/* Population count (number of set bits) */
#if defined(__GNUC__) || defined(__clang__)
    #define cermu_popcount(x) __builtin_popcount(x)
    #define cermu_popcountl(x) __builtin_popcountl(x)
    #define cermu_popcountll(x) __builtin_popcountll(x)
#elif defined(_MSC_VER) && defined(_WIN64)
    #include <intrin.h>
    #define cermu_popcount(x) __popcnt(x)
    #define cermu_popcountl(x) __popcnt(x)
    #define cermu_popcountll(x) __popcnt64(x)
#else
    /* Fallback implementations */
    static inline int cermu_popcount(unsigned int x) {
        x = x - ((x >> 1) & 0x55555555u);
        x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
        x = (x + (x >> 4)) & 0x0f0f0f0fu;
        x = x + (x >> 8);
        x = x + (x >> 16);
        return x & 0x3fu;
    }
    
    static inline int cermu_popcountl(unsigned long x) {
        return cermu_popcount((unsigned int)x) + 
               (sizeof(long) > sizeof(int) ? cermu_popcount((unsigned int)(x >> 32)) : 0);
    }
    
    static inline int cermu_popcountll(unsigned long long x) {
        return cermu_popcount((unsigned int)x) + cermu_popcount((unsigned int)(x >> 32));
    }
#endif

#include <cstdint>

#if defined(__riscv) && __riscv_xlen >= 32 && defined(__riscv_zbt)
static inline uint8_t bitmix(uint8_t a, uint8_t b, uint8_t mask) {
    uint32_t r;
    __asm__("cmix %0, %1, %2, %3" : "=r"(r) : "r"(mask), "r"(a), "r"(b));
    return (uint8_t)r;
}
#else
static inline uint8_t bitmix(uint8_t a, uint8_t b, uint8_t mask) {
    return b ^ ((a ^ b) & mask);
}
#endif