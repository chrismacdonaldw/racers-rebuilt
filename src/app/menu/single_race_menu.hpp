#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/race_scene_name.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// `nameIndex` is a `DRIVERS.SRF` line, or -1.
struct RacerChoice {
    std::string code;
    std::int32_t nameIndex = -1;
};

struct TrackChoice {
    std::string track;
    std::vector<RacerChoice> racers;
};

/// Every race track whose grid loads. Fails when there are none.
[[nodiscard]] Result<std::vector<TrackChoice>> LoadTrackChoices(const assets::Vfs& vfs);

[[nodiscard]] Result<std::vector<std::u16string>> ReadDriverNames(const assets::Vfs& vfs,
                                                                  std::string_view language);

enum class SingleRaceRow : std::uint8_t { Track, Racer, Race, Back, Count };

/// The single race setup's choices, moved through with the menu actions.
class SingleRaceMenu {
public:
    /// `tracks` must not be empty, nor any track's racers.
    explicit SingleRaceMenu(std::vector<TrackChoice> tracks);

    /// The scene the input chose, if any. A chosen race keeps `rules` but for its track and racer.
    [[nodiscard]] std::optional<std::string> Tick(const sim::InputFrame& input,
                                                  const RaceSceneSetup& rules);

    [[nodiscard]] SingleRaceRow Focus() const { return focus_; }
    [[nodiscard]] const std::vector<TrackChoice>& Tracks() const { return tracks_; }
    [[nodiscard]] const TrackChoice& Track() const { return tracks_[track_]; }
    [[nodiscard]] std::size_t TrackIndex() const { return track_; }
    [[nodiscard]] std::size_t Racer() const { return racer_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    void MoveFocus(int step);
    void Change(int step);
    [[nodiscard]] std::optional<std::string> Confirm(const RaceSceneSetup& rules);

    std::vector<TrackChoice> tracks_;
    std::size_t track_ = 0;
    std::size_t racer_ = 0;
    SingleRaceRow focus_ = SingleRaceRow::Track;
};

}
