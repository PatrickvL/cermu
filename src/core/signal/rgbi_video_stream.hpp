#pragma once

// RGBI video stream — included by C128 VDC, etc.

#include "core/signal/video_stream.hpp"
#include "core/signal/video_sample_types.hpp"

using RGBIVideoStream     = VideoStream<RGBIVideoSample>;
using NullRGBIVideoStream = NullVideoStream<RGBIVideoSample>;
