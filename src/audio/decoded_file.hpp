#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "audio/pcm_stream.hpp"
#include "core/result.hpp"

namespace rr::audio {

/// Decodes all of an OGG Vorbis, FLAC, MP3 or WAV file into memory. The format is picked by
/// content.
[[nodiscard]] Result<std::unique_ptr<PcmStream>> DecodeFile(const std::filesystem::path& path);

/// `samples` are interleaved for `format`'s channels.
[[nodiscard]] std::unique_ptr<PcmStream> SampleStream(PcmFormat format,
                                                      std::vector<std::int16_t> samples);

}
