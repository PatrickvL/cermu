// Combined waveform lookup tables for MOS 6581/8580 SID emulation.
// Tables indexed as: model_wave[chip_model][waveform_index][accumulator_index]
//   chip_model: 0 = MOS6581, 1 = MOS8580
//   waveform_index: lower 3 bits of waveform selector (0-7, without noise bit)
//     0: no waveform (0xFFF mask)
//     1: triangle
//     2: sawtooth
//     3: sawtooth + triangle (combined, from hardware measurements)
//     4: pulse (0xFFF mask, pulse handled via comparator)
//     5: pulse + triangle (combined, from hardware measurements)
//     6: pulse + sawtooth (combined, from hardware measurements)
//     7: pulse + sawtooth + triangle (combined, from hardware measurements)
//   accumulator_index: upper 12 bits of accumulator (0-4095)
//
// Combined waveform data from reSID by Dag Lem, sampled from real hardware.
// Pure waveforms (0, 1, 2, 4) are computed mathematically.

#ifndef SID_WAVEFORM_TABLES_H
#define SID_WAVEFORM_TABLES_H

#include <cstdint>

namespace sid_tables {

// The waveform table: [2 models][8 waveform combos][4096 entries]
extern uint16_t model_wave[2][8][4096];

// Initialize the waveform tables. Must be called once before use.
void init_waveform_tables();

} // namespace sid_tables

#endif // SID_WAVEFORM_TABLES_H
