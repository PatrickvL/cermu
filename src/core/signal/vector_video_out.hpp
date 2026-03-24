#pragma once

// Vector video output — included by vector display chips (Vectrex, etc.)

#include "core/signal/video_out.hpp"
#include "core/signal/video_sample_types.hpp"

using VectorVideoOut     = VideoOut<VectorVideoSample>;
using NoVectorVideoOut = NoVideoOut<VectorVideoSample>;
