#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/race_preview.hpp"
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

/// A single race's track, or a whole circuit when `track` is empty. `race` indexes the race list.
struct CourseChoice {
    std::string track;
    std::string circuit;
    std::size_t circuitIndex = 0;
    std::size_t race = 0;
    std::int32_t nameLine = -1;
    std::vector<RacerChoice> racers;
    std::vector<std::optional<RacePreview>> previews;
    bool isLocked = false;
};

/// Unmirrored circuit races in circuit order with their circuit's grid, then tracks only mods add.
[[nodiscard]] Result<std::vector<CourseChoice>> LoadTrackChoices(const assets::Vfs& vfs);

/// The unmirrored races of the first three circuits, locked with their circuit.
[[nodiscard]] Result<std::vector<CourseChoice>> LoadTrialChoices(
    const assets::Vfs& vfs, const std::set<std::string>& unlocked);

/// Every circuit with a race. The first is open even when `unlocked` doesn't name it.
[[nodiscard]] Result<std::vector<CourseChoice>> LoadCircuitChoices(
    const assets::Vfs& vfs, const std::set<std::string>& unlocked);

[[nodiscard]] Result<std::vector<std::u16string>> ReadRaceNames(const assets::Vfs& vfs,
                                                                std::string_view language);

[[nodiscard]] Result<std::vector<std::u16string>> ReadDriverNames(const assets::Vfs& vfs,
                                                                  std::string_view language);

enum class RaceSetupRow : std::uint8_t { Course, Race, Back, Count };

/// A single race, versus or circuit setup's course, then each player picks their own racer.
class RaceSetupMenu {
public:
    /// `courses` must not be empty, nor any course's racers.
    explicit RaceSetupMenu(std::vector<CourseChoice> courses, std::size_t players = 1);

    /// A chosen race keeps `rules` but for its course and racers. A locked course can't start.
    [[nodiscard]] std::optional<std::string> Tick(const sim::InputFrame& input,
                                                  const RaceSceneSetup& rules);

    [[nodiscard]] RaceSetupRow Focus() const { return focus_; }
    void FocusOn(RaceSetupRow row) { focus_ = row; }
    [[nodiscard]] const std::vector<CourseChoice>& Courses() const { return courses_; }
    [[nodiscard]] const CourseChoice& Course() const { return courses_[course_]; }
    [[nodiscard]] std::size_t CourseIndex() const { return course_; }

    [[nodiscard]] std::uint64_t StateHash() const;

private:
    [[nodiscard]] std::optional<std::string> Confirm(const RaceSceneSetup& rules);
    [[nodiscard]] std::string PickingRacers(std::string_view scenes) const;

    std::vector<CourseChoice> courses_;
    std::size_t course_ = 0;
    std::size_t players_ = 1;
    RaceSetupRow focus_ = RaceSetupRow::Course;
};

}
