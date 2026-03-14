#pragma once
/*
 * thread_timer.hpp — Cross-platform high-resolution thread timer
 *
 * Provides two affinity policies:
 *   AffinityPinned   → RDTSC on x86 (pin emulation thread to a single core)
 *   AffinityFloating  → OS monotonic clock (let scheduler float freely)
 *
 * Usage:
 *
 *   using Timer = cermu::ThreadTimer<cermu::AffinityPinned>;
 *   Timer::init(core_index);          // once at thread startup
 *
 *   auto t0 = Timer::sample();
 *   cpu.run_until(target);
 *   auto t1 = Timer::sample();
 *   double ns = Timer::delta_ns(t0, t1);
 */

#include "core/cermu.hpp"
#include <cstdint>
#include <ctime>

// ── Platform headers (gated on cermu.hpp macros) ─────────────────────────────

#ifdef CERMU_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <intrin.h>
#elif defined(CERMU_PLATFORM_MACOS)
    #include <mach/mach_time.h>
    #include <pthread.h>
    #ifdef CERMU_ARCH_X86
        #include <x86intrin.h>
    #endif
#elif defined(CERMU_PLATFORM_LINUX)
    #include <pthread.h>
    #ifdef CERMU_ARCH_X86
        #include <x86intrin.h>
    #endif
#endif

namespace cermu {

// ── Affinity policy tags ─────────────────────────────────────────────────────

struct AffinityPinned   {};   // Pin emulation thread to a single core
struct AffinityFloating {};   // Let OS schedule freely

// ── Forward declaration ──────────────────────────────────────────────────────

template<typename AffinityPolicy>
class ThreadTimer;

// ── Internal backends ────────────────────────────────────────────────────────

namespace detail {

// ── RDTSC backend (pinned, x86 only) ────────────────────────────────────────

#ifdef CERMU_ARCH_X86

struct RdtscBackend {
    using sample_t = uint64_t;

    static void pin_to_core(int core_index) {
#if defined(CERMU_PLATFORM_WINDOWS)
        SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << core_index);
#elif defined(CERMU_PLATFORM_LINUX)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_index, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#elif defined(CERMU_PLATFORM_MACOS)
        // macOS does not expose thread affinity for specific cores;
        // use affinity tag to hint the scheduler to keep threads together.
        thread_affinity_policy_data_t policy{ core_index + 1 };
        thread_policy_set(pthread_mach_thread_np(pthread_self()),
                          THREAD_AFFINITY_POLICY,
                          reinterpret_cast<thread_policy_t>(&policy), 1);
#endif
    }

    [[nodiscard]] static sample_t now() noexcept {
        return __rdtsc();
    }

    // Returns nanoseconds from a delta of TSC ticks.
    // Call once at startup to calibrate; cache the result.
    [[nodiscard]] static double ticks_to_ns(sample_t ticks) noexcept {
        return static_cast<double>(ticks) / ticks_per_ns();
    }

private:
    // Calibrate ticks/ns once via a platform sleep.
    static double ticks_per_ns() noexcept {
        static const double value = calibrate();
        return value;
    }

    static double calibrate() noexcept {
        constexpr uint64_t sleep_ns = 50'000'000; // 50 ms

#if defined(CERMU_PLATFORM_WINDOWS)
        LARGE_INTEGER freq, t0, t1;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);
        uint64_t r0 = __rdtsc();
        Sleep(50);
        uint64_t r1 = __rdtsc();
        QueryPerformanceCounter(&t1);
        double wall_ns = static_cast<double>(t1.QuadPart - t0.QuadPart)
                         * 1e9 / static_cast<double>(freq.QuadPart);
#else
        timespec ts0, ts1;
        clock_gettime(CLOCK_MONOTONIC, &ts0);
        uint64_t r0 = __rdtsc();
        timespec req{ 0, static_cast<long>(sleep_ns) };
        nanosleep(&req, nullptr);
        uint64_t r1 = __rdtsc();
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        double wall_ns = static_cast<double>(ts1.tv_sec  - ts0.tv_sec)  * 1e9
                       + static_cast<double>(ts1.tv_nsec - ts0.tv_nsec);
#endif
        return static_cast<double>(r1 - r0) / wall_ns;
    }
};

#endif // CERMU_ARCH_X86

// ── Linux floating backend: CLOCK_THREAD_CPUTIME_ID ─────────────────────────

#ifdef CERMU_PLATFORM_LINUX

struct LinuxThreadCpuBackend {
    using sample_t = uint64_t; // nanoseconds

    static void pin_to_core(int) noexcept {} // no-op

    [[nodiscard]] static sample_t now() noexcept {
        timespec t;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
        return static_cast<uint64_t>(t.tv_sec) * 1'000'000'000ULL
             + static_cast<uint64_t>(t.tv_nsec);
    }

    [[nodiscard]] static double ticks_to_ns(sample_t ticks) noexcept {
        return static_cast<double>(ticks); // already nanoseconds
    }
};

#endif

// ── macOS floating backend: mach_absolute_time ──────────────────────────────

#ifdef CERMU_PLATFORM_MACOS

struct MachTimeBackend {
    using sample_t = uint64_t;

    static void pin_to_core(int) noexcept {} // no-op

    [[nodiscard]] static sample_t now() noexcept {
        return mach_absolute_time();
    }

    [[nodiscard]] static double ticks_to_ns(sample_t ticks) noexcept {
        return static_cast<double>(ticks) * timebase().numer
             / static_cast<double>(timebase().denom);
    }

private:
    static const mach_timebase_info_data_t& timebase() noexcept {
        static mach_timebase_info_data_t tb = [] {
            mach_timebase_info_data_t info{};
            mach_timebase_info(&info);
            return info;
        }();
        return tb;
    }
};

#endif

// ── Windows floating backend: QueryPerformanceCounter ───────────────────────

#ifdef CERMU_PLATFORM_WINDOWS

struct QPCBackend {
    using sample_t = int64_t;

    static void pin_to_core(int) noexcept {} // no-op

    [[nodiscard]] static sample_t now() noexcept {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }

    [[nodiscard]] static double ticks_to_ns(sample_t ticks) noexcept {
        return static_cast<double>(ticks) * 1e9 / static_cast<double>(freq());
    }

private:
    static int64_t freq() noexcept {
        static const int64_t f = [] {
            LARGE_INTEGER v;
            QueryPerformanceFrequency(&v);
            return v.QuadPart;
        }();
        return f;
    }
};

#endif

// ── Backend selector: choose best backend per platform × affinity policy ────

template<typename AffinityPolicy>
struct BackendSelector;

#if defined(CERMU_PLATFORM_LINUX)
template<> struct BackendSelector<AffinityPinned>   { using type = RdtscBackend;          };
template<> struct BackendSelector<AffinityFloating> { using type = LinuxThreadCpuBackend; };
#elif defined(CERMU_PLATFORM_MACOS)
template<> struct BackendSelector<AffinityPinned>   { using type = RdtscBackend;    };
template<> struct BackendSelector<AffinityFloating> { using type = MachTimeBackend; };
#elif defined(CERMU_PLATFORM_WINDOWS)
template<> struct BackendSelector<AffinityPinned>   { using type = RdtscBackend; };
template<> struct BackendSelector<AffinityFloating> { using type = QPCBackend;   };
#endif

} // namespace detail

// ── ThreadTimer<AffinityPolicy> ─────────────────────────────────────────────

template<typename AffinityPolicy>
class ThreadTimer {
    using Backend = typename detail::BackendSelector<AffinityPolicy>::type;

public:
    using sample_t = typename Backend::sample_t;

    // Call once on the emulation thread before any sampling.
    // core_index is only meaningful for AffinityPinned; ignored otherwise.
    static void init(int core_index = 1) noexcept {
        Backend::pin_to_core(core_index);
    }

    [[nodiscard]] static sample_t sample() noexcept {
        return Backend::now();
    }

    [[nodiscard]] static double delta_ns(sample_t t0, sample_t t1) noexcept {
        return Backend::ticks_to_ns(t1 - t0);
    }
};

// ── Convenience aliases ─────────────────────────────────────────────────────

using PinnedTimer   = ThreadTimer<AffinityPinned>;
using FloatingTimer = ThreadTimer<AffinityFloating>;

} // namespace cermu
