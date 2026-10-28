#pragma once

#include "audio/pcm_stream.hpp"
#include "core/result.hpp"

#include <miniaudio.h>

namespace rr::audio {

/// `stream` must outlive the source.
struct StreamDataSource {
    ma_data_source_base base;
    PcmStream* stream = nullptr;
};

[[nodiscard]] Result<void> InitStreamDataSource(StreamDataSource& source, PcmStream& stream);

void UninitStreamDataSource(StreamDataSource& source);

}
