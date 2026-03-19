#pragma once

// Composite video stream — included by VIC-II, TED, NES PPU, VIC-20 VIC
// Chip includes this header; never sees VideoPort or anything downstream.

#include "core/signal/video_stream.hpp"
#include "core/signal/video_sample_types.hpp"

using CompositeVideoStream     = VideoStream<CompositeVideoSample>;
using NullCompositeVideoStream = NullVideoStream<CompositeVideoSample>;
