#pragma once

#include <filesystem>

#include "audio/looping_stream.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Reads `loop_start` and `loop_end` in samples. A missing file loops the whole track.
[[nodiscard]] Result<audio::LoopPoints> ReadLoopSidecar(const std::filesystem::path& path);

}
