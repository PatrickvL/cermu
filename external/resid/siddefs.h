// ---------------------------------------------------------------------------
// Standalone siddefs.h for reSID — replaces the autoconf-generated version.
// Used by cermu's SID comparison harness to compile reSID without VICE.
//
// This file is part of reSID, a MOS6581 SID emulator engine.
// Copyright (C) 2010  Dag Lem <resid@nimrod.no>
// Licensed under GPL v2+. See the reSID source for full license text.
// ---------------------------------------------------------------------------

#ifndef RESID_SIDDEFS_H
#define RESID_SIDDEFS_H

// Compilation configuration — sensible defaults for standalone use.
#define RESID_INLINING 1
#define RESID_INLINE inline
#define RESID_BRANCH_HINTS 1

// Use the improved 8580 filter model (filter8580new.cc/h).
#define NEW_8580_FILTER 1

// Compiler feature detection.
#define HAVE_BOOL 1

#if defined(__GNUC__) || defined(__clang__)
#define HAVE_BUILTIN_EXPECT 1
#else
#define HAVE_BUILTIN_EXPECT 0
#endif

// log1p() availability — present in C99/C++11 and later.
#define HAVE_LOG1P 1
#define HAS_LOG1P

// Branch prediction macros (lifted from the Linux kernel).
#if RESID_BRANCH_HINTS && HAVE_BUILTIN_EXPECT
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

// Provide a VERSION string for version.cc.
#ifndef VERSION
#define VERSION "1.0-cermu-standalone"
#endif

namespace reSID {

typedef unsigned int reg4;
typedef unsigned int reg8;
typedef unsigned int reg12;
typedef unsigned int reg16;
typedef unsigned int reg24;

typedef int cycle_count;
typedef short short_point[2];
typedef double double_point[2];

enum chip_model { MOS6581, MOS8580 };

enum sampling_method {
    SAMPLE_FAST,
    SAMPLE_INTERPOLATE,
    SAMPLE_RESAMPLE,
    SAMPLE_RESAMPLE_FASTMEM
};

} // namespace reSID

extern "C"
{
#ifndef RESID_VERSION_CC
extern const char* resid_version_string;
#else
const char* resid_version_string = VERSION;
#endif
}

#endif // not RESID_SIDDEFS_H
