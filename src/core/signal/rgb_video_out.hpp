#pragma once

// RGB video stream — included by Amiga Denise, Atari ST shifter, etc.

#include "core/signal/video_out.hpp"
#include "core/signal/video_sample_types.hpp"

using RGBVideoOut     = VideoOut<RGBVideoSample>;
using NoRGBVideoOut = NoVideoOut<RGBVideoSample>;
