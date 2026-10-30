#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio/mixer.hpp"
#include "audio/music_source.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"

namespace rr::audio {

inline constexpr Milliseconds kMusicCrossfade{1000};

enum class Repeat : std::uint8_t { Loop, Once };

/// Asks each source in order and plays the first working copy of a track. `mixer` must outlive
/// the player.
class MusicPlayer {
public:
    MusicPlayer(Mixer& mixer, std::vector<std::unique_ptr<MusicSource>> sources);

    /// Replaying the current track does nothing.
    [[nodiscard]] Result<void> Play(std::string_view id, Repeat repeat = Repeat::Loop);
    void Stop();

    [[nodiscard]] const std::string& Current() const { return current_; }

private:
    [[nodiscard]] Result<MusicTrack> Find(std::string_view id) const;

    Mixer* mixer_;
    std::vector<std::unique_ptr<MusicSource>> sources_;
    std::string current_;
};

}
