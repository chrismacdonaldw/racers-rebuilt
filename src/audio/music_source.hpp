#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "assets/vfs.hpp"
#include "audio/looping_stream.hpp"
#include "audio/pcm_stream.hpp"
#include "core/result.hpp"

namespace rr::audio {

struct MusicTrack {
    std::unique_ptr<PcmStream> stream;
    LoopPoints loop;
};

class MusicSource {
public:
    virtual ~MusicSource() = default;

    [[nodiscard]] virtual std::string Name() const = 0;

    /// `NotFound` means this source has no such track. Any other error is a broken track.
    [[nodiscard]] virtual Result<MusicTrack> Open(std::string_view id) const = 0;
};

/// The install's `<id>.tun`. `vfs` must outlive the source.
class OriginalTrackSource final : public MusicSource {
public:
    explicit OriginalTrackSource(const assets::Vfs& vfs) : vfs_(&vfs) {}

    [[nodiscard]] std::string Name() const override { return "original music"; }
    [[nodiscard]] Result<MusicTrack> Open(std::string_view id) const override;

private:
    const assets::Vfs* vfs_;
};

}
