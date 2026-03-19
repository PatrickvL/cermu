#pragma once

// Vector video stream — included by vector display chips (Vectrex, etc.)

#include "core/signal/video_stream.hpp"
#include "core/signal/video_sample_types.hpp"

using VectorVideoStream     = VideoStream<VectorVideoSample>;
using NullVectorVideoStream = NullVideoStream<VectorVideoSample>;
