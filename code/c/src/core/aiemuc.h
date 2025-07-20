#ifndef AIEMUC_H
#define AIEMUC_H

#ifdef _MSC_VER
#pragma message("Compiling with MSVC")
#endif
#ifdef _M_X64
#pragma message("Targeting x64")
#endif
#ifdef _M_IX86
#pragma message("Targeting x86")
#endif
#ifdef __GNUC__
#pragma message("Compiling with GCC")
#endif
#ifdef __clang__
#pragma message("Compiling with Clang")
#endif
#ifdef __x86_64__
#pragma message("Detected x86_64")
#endif
#ifdef __i386__
#pragma message("Detected i386")
#endif

// Compiler optimization hints
#ifdef _MSC_VER
//#include <stdalign.h>
#define alignas(x) __declspec(align(x))
#else
#define alignas(x) __attribute__((aligned(x)))
#endif

#ifdef _MSC_VER
    #define likely(x)   (x)
    #define unlikely(x) (x)
#else
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// Cross-platform calling convention for register arguments/returns
#ifdef _MSC_VER
    #define REGISTER_CALL __vectorcall  // Ensures register passing on MSVC
#elif defined(__GNUC__) || defined(__clang__)
    #define REGISTER_CALL __attribute__((regparm(3)))  // Register calling on GCC/Clang
#else
    #define REGISTER_CALL
#endif

// Cross-platform force inline macro
#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define aiemuc_popcount(x) __builtin_popcount(x)
#else
    static inline int aiemuc_popcount(unsigned int x) {
        int count = 0;
        while (x) {
            count += x & 1;
            x >>= 1;
        }
        return count;
    }
#endif

#endif // AIEMUC_H
