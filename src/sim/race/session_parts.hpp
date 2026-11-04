#pragma once

#include <cstddef>
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

}
