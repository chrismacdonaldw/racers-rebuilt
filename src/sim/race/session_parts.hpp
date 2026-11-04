#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "sim/race/race_session.hpp"

namespace rr::sim {

[[nodiscard]] BrickField BricksFor(const RaceCourse& course, const Cheats& cheats);

[[nodiscard]] std::size_t BrickCount(const RaceCourse& course);

[[nodiscard]] std::size_t UsesPerTick(const RaceSetup& setup, const RaceCourse& course);

[[nodiscard]] std::size_t WeaponEventCount(const RaceSetup& setup, const RaceCourse& course);

[[nodiscard]] std::size_t OutsideEventCount(const RaceSetup& setup, const RaceCourse& course);

/// Empty sources when the course has none.
[[nodiscard]] const TrackEventSources& EventSourcesFor(const RaceCourse& course);

[[nodiscard]] const TrackEntitySources& EntitySourcesFor(const RaceCourse& course);

[[nodiscard]] TrackSurfaces SurfacesFor(const RaceCourse& course);

/// One per racer sphere, the collision world it switches on.
[[nodiscard]] std::vector<std::optional<std::size_t>> SphereWorldsFor(const RaceCourse& course);

[[nodiscard]] std::span<const assets::RaceRoute> RoutesFor(const RaceCourse& course,
                                                           std::size_t slot);

/// Shaped like the course's routes, or empty when racers don't follow racing lines.
[[nodiscard]] std::vector<std::vector<std::shared_ptr<const RacingLine>>> RacingLinesFor(
    const RaceSetup& setup, const RaceCourse& course);

/// Empty when `lines` doesn't match `routes`.
[[nodiscard]] std::vector<RouteOption> RouteOptionsFor(
    std::span<const assets::RaceRoute> routes,
    std::span<const std::shared_ptr<const RacingLine>> lines);

}
