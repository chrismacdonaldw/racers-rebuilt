#include "app/race_music.hpp"

#include <utility>

namespace rr::app {
namespace {

constexpr std::size_t kIntro = 0;
constexpr std::size_t kRaceMusic = 1;
constexpr std::size_t kLose = 2;
constexpr std::size_t kWin = 3;
constexpr std::size_t kMoreRaceMusic = 4;

std::size_t PickRaceTrack(std::size_t count, std::uint32_t pick) {
    if (count <= kMoreRaceMusic) {
        return kRaceMusic;
    }
    const std::size_t choice = pick % (count - kWin);
    return choice == 0 ? kRaceMusic : choice + kWin;
}

}

RaceMusic::RaceMusic(std::vector<std::string> list, std::uint32_t pick)
    : list_(std::move(list)), raceTrack_(PickRaceTrack(list_.size(), pick)), current_(kIntro) {}

void RaceMusic::Hear(std::span<const sim::SoundEvent> sounds, std::size_t player,
                     std::optional<int> finishPlace) {
    for (const sim::SoundEvent& sound : sounds) {
        if (sound.sound == sim::RaceSound::Go) {
            current_ = raceTrack_;
            repeat_ = audio::Repeat::Loop;
        } else if (sound.sound == sim::RaceSound::Finish && sound.racer == player) {
            current_ = finishPlace == 1 ? kWin : kLose;
            repeat_ = audio::Repeat::Once;
        }
    }
}

SceneMusic RaceMusic::Current() const {
    if (current_ >= list_.size()) {
        return {};
    }
    return {.track = list_[current_], .repeat = repeat_};
}

}
