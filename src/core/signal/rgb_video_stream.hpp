#pragma once

// RGB video stream — included by Amiga Denise, Atari ST shifter, etc.

#include "core/signal/video_stream.hpp"
#include "core/signal/video_sample_types.hpp"

using RGBVideoStream     = VideoStream<RGBVideoSample>;
using NullRGBVideoStream = NullVideoStream<RGBVideoSample>;
