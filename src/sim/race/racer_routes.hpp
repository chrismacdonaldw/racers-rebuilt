#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "assets/race_route.hpp"
#include "sim/ai/racing_line.hpp"
#include "sim/race/race_course.hpp"
#include "sim/race/race_setup.hpp"

namespace rr::sim {

/// The routes and racing lines each racer may drive, by racer.
class RacerRoutes {
public:
    RacerRoutes(const RaceSetup& setup, const RaceCourse& course);

    /// A computer racer on a gate path drives it instead of its slot's recordings.
    [[nodiscard]] std::span<const assets::RaceRoute> Of(std::size_t racer) const;

    [[nodiscard]] std::span<const assets::RaceRoute> RecordedOf(std::size_t racer) const {
        return recorded_[racer];
    }

    /// Traced from the checkpoints and collision, on tracks without recordings or when asked.
    [[nodiscard]] bool IsGatePath(std::size_t racer) const { return !gatePaths_[racer].empty(); }

    /// One per route when the racer follows racing lines, otherwise empty.
    [[nodiscard]] std::span<const std::shared_ptr<const RacingLine>> LinesOf(
        std::size_t racer) const {
        return lines_[racer];
    }

private:
    std::vector<std::span<const assets::RaceRoute>> recorded_;
    std::vector<std::vector<assets::RaceRoute>> gatePaths_;
    std::vector<std::vector<std::shared_ptr<const RacingLine>>> lines_;
};

}
