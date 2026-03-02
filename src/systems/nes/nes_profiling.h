#pragma once
/*
 * nes_profiling.h — Optional rdtsc-based component profiling for NES.
 *
 * Enabled with -DNES_PROFILING at compile time.
 * Zero overhead when disabled — all macros expand to no-ops.
 */

#ifdef NES_PROFILING

#include <cstdint>
#include <cstdio>
#include <x86intrin.h>

struct NesProfileCounters {
    uint64_t ppu_clock_cycles = 0;       // PPU clock() total
    uint64_t cpu_phi2_cycles = 0;        // CPU PHI2 tick
    uint64_t bus_dispatch_cycles = 0;    // Bus read/write dispatch
    uint64_t cpu_phi1_cycles = 0;        // CPU PHI1 tick (includes APU)
    uint64_t irq_nmi_cycles = 0;         // NMI/IRQ handling
    uint64_t dma_cycles = 0;             // DMA transfer overhead
    uint64_t total_ticks = 0;            // Total tick() invocations
    uint64_t cpu_ticks = 0;              // Ticks where CPU actually ran

    void report() const {
        if (total_ticks == 0) return;
        uint64_t total = ppu_clock_cycles + cpu_phi2_cycles +
                         bus_dispatch_cycles + cpu_phi1_cycles +
                         irq_nmi_cycles + dma_cycles;
        auto pct = [total](uint64_t c) {
            return total ? 100.0 * c / total : 0.0;
        };
        printf("\n  === Component Cycle Breakdown (rdtsc) ===\n");
        printf("  PPU clock():     %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               ppu_clock_cycles, pct(ppu_clock_cycles),
               (double)ppu_clock_cycles / total_ticks);
        printf("  CPU PHI2:        %12lu cycles  %5.1f%%  (%.1f cy/tick)\n",
               cpu_phi2_cycles, pct(cpu_phi2_cycles),
               cpu_ticks ? (double)cpu_phi2_cycles / cpu_ticks : 0);
        printf("  Bus dispatch:    %12lu cycles  %5.1f%%  (%.1f cy/tick)\n",
               bus_dispatch_cycles, pct(bus_dispatch_cycles),
               cpu_ticks ? (double)bus_dispatch_cycles / cpu_ticks : 0);
        printf("  IRQ/NMI:         %12lu cycles  %5.1f%%  (%.1f cy/tick)\n",
               irq_nmi_cycles, pct(irq_nmi_cycles),
               cpu_ticks ? (double)irq_nmi_cycles / cpu_ticks : 0);
        printf("  CPU PHI1 (APU):  %12lu cycles  %5.1f%%  (%.1f cy/tick)\n",
               cpu_phi1_cycles, pct(cpu_phi1_cycles),
               cpu_ticks ? (double)cpu_phi1_cycles / cpu_ticks : 0);
        printf("  DMA:             %12lu cycles  %5.1f%%\n",
               dma_cycles, pct(dma_cycles));
        printf("  -----------------------------------------\n");
        printf("  Total measured:  %12lu rdtsc cycles\n", total);
        printf("  Total dots:      %12lu\n", total_ticks);
        printf("  CPU ticks:       %12lu (ratio %.2f:1)\n",
               cpu_ticks, cpu_ticks ? (double)total_ticks / cpu_ticks : 0);
    }

    void reset() { *this = {}; }
};

extern NesProfileCounters g_nes_profile;

#define NES_PROF_START(var)       uint64_t var##_t0 = __rdtsc()
#define NES_PROF_END(counter, var) g_nes_profile.counter += __rdtsc() - var##_t0
#define NES_PROF_TICK()           g_nes_profile.total_ticks++
#define NES_PROF_CPU_TICK()       g_nes_profile.cpu_ticks++

#else  // !NES_PROFILING

#define NES_PROF_START(var)       ((void)0)
#define NES_PROF_END(counter, var) ((void)0)
#define NES_PROF_TICK()           ((void)0)
#define NES_PROF_CPU_TICK()       ((void)0)

#endif // NES_PROFILING
