#ifndef AIEMUC_H
#define AIEMUC_H

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

#endif // AIEMUC_H
