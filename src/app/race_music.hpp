#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "app/scene.hpp"
#include "sim/race/race_sounds.hpp"

namespace rr::app {

class RaceMusic {
public:
    /// `pick` chooses the race music when the list has more than one.
    RaceMusic(std::vector<std::string> list, std::uint32_t pick);

    /// `finishPlace` is the player's, once finished.
    void Hear(std::span<const sim::SoundEvent> sounds, std::size_t player,
              std::optional<int> finishPlace);

    [[nodiscard]] SceneMusic Current() const;

private:
    std::vector<std::string> list_;
    std::size_t raceTrack_ = 0;
    std::size_t current_ = 0;
    audio::Repeat repeat_ = audio::Repeat::Once;
};

}
