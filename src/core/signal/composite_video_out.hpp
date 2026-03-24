#pragma once

// Composite video stream — included by VIC-II, TED, NES PPU, VIC-20 VIC
// Chip includes this header; never sees VideoPort or anything downstream.

#include "core/signal/video_out.hpp"
#include "core/signal/video_sample_types.hpp"

using CompositeVideoOut     = VideoOut<CompositeVideoSample>;
using NullCompositeVideoOut = NullVideoOut<CompositeVideoSample>;
