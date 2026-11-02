#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
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

/// A single race's track, or a whole circuit when `track` is empty.
struct CourseChoice {
    std::string track;
    std::string circuit;
    std::size_t circuitIndex = 0;
    std::int32_t nameLine = -1;
    std::vector<RacerChoice> racers;
    bool isLocked = false;
};

/// Unmirrored circuit races in circuit order, each with its circuit's grid.
[[nodiscard]] Result<std::vector<CourseChoice>> LoadTrackChoices(const assets::Vfs& vfs);

/// Leaves out mirrored circuits. The first circuit is open even when `unlocked` doesn't name it.
[[nodiscard]] Result<std::vector<CourseChoice>> LoadCircuitChoices(
    const assets::Vfs& vfs, const std::set<std::string>& unlocked);

[[nodiscard]] Result<std::vector<std::u16string>> ReadRaceNames(const assets::Vfs& vfs,
                                                                std::string_view language);

[[nodiscard]] Result<std::vector<std::u16string>> ReadDriverNames(const assets::Vfs& vfs,
                                                                  std::string_view language);

enum class RaceSetupRow : std::uint8_t { Course, Racer, Race, Back, Count };

/// A single race or circuit setup's choices, moved through with the menu actions.
class RaceSetupMenu {
public:
    /// `courses` must not be empty, nor any course's racers.
    explicit RaceSetupMenu(std::vector<CourseChoice> courses);

    /// A chosen race keeps `rules` but for its course and racer. A locked course can't start.
    [[nodiscard]] std::optional<std::string> Tick(const sim::InputFrame& input,
                                                  const RaceSceneSetup& rules);

    [[nodiscard]] RaceSetupRow Focus() const { return focus_; }
    [[nodiscard]] const std::vector<CourseChoice>& Courses() const { return courses_; }
    [[nodiscard]] const CourseChoice& Course() const { return courses_[course_]; }
    [[nodiscard]] std::size_t CourseIndex() const { return course_; }
    [[nodiscard]] std::size_t Racer() const { return racer_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    void MoveFocus(int step);
    void Change(int step);
    [[nodiscard]] std::optional<std::string> Confirm(const RaceSceneSetup& rules);

    std::vector<CourseChoice> courses_;
    std::size_t course_ = 0;
    std::size_t racer_ = 0;
    RaceSetupRow focus_ = RaceSetupRow::Course;
};

}
