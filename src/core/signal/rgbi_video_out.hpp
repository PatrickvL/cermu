#pragma once

// RGBI video stream — included by C128 VDC, etc.

#include "core/signal/video_out.hpp"
#include "core/signal/video_sample_types.hpp"

using RGBIVideoOut     = VideoOut<RGBIVideoSample>;
using NullRGBIVideoOut = NullVideoOut<RGBIVideoSample>;
