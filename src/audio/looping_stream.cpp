#include "audio/looping_stream.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace rr::audio {

LoopingStream::LoopingStream(std::unique_ptr<PcmStream> stream, LoopPoints loop)
    : stream_(std::move(stream)), loop_(loop) {}

std::uint64_t LoopingStream::Read(std::span<std::int16_t> interleaved) noexcept {
    const std::size_t channelCount = stream_->Format().channelCount;
    const std::uint64_t loopEnd =
        loop_.endFrame.value_or(std::numeric_limits<std::uint64_t>::max());
    std::uint64_t written = 0;
    bool hasLooped = false;
    while (written * channelCount < interleaved.size()) {
        const std::uint64_t untilLoopEnd = loopEnd - std::min(loopEnd, stream_->Cursor());
        const std::size_t wanted = static_cast<std::size_t>(
            std::min<std::uint64_t>(untilLoopEnd, (interleaved.size() / channelCount) - written));
        const std::uint64_t read =
            wanted == 0
                ? 0
                : stream_->Read(interleaved.subspan(
                      static_cast<std::size_t>(written) * channelCount, wanted * channelCount));
        if (read == 0) {
            if (hasLooped) {
                break;
            }
            stream_->Seek(loop_.startFrame);
            hasLooped = true;
            continue;
        }
        written += read;
        hasLooped = false;
    }
    return written;
}

}
