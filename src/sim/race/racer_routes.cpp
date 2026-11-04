#include "sim/race/racer_routes.hpp"

#include <algorithm>
#include <execution>
#include <optional>

#include "sim/ai/gate_path.hpp"

namespace rr::sim {
namespace {

struct RouteRef {
    std::size_t racer = 0;
    std::size_t variant = 0;
};

std::span<const assets::RaceRoute> RecordedFor(const RaceCourse& course, std::size_t slot) {
    if (course.routes == nullptr || slot >= course.routes->size()) {
        return {};
    }
    return (*course.routes)[slot];
}

bool HasRecordings(const RaceCourse& course) {
    return course.routes != nullptr &&
           std::ranges::any_of(*course.routes, [](const auto& slot) { return !slot.empty(); });
}

bool WantsGatePath(const RaceSetup& setup, bool hasRecordings) {
    return !hasRecordings || (setup.aiDriving == AiDriving::Physics && setup.aiCraft.gatePaths);
}

bool FollowsLines(const RaceSetup& setup, bool isGatePath) {
    return isGatePath || (setup.aiDriving == AiDriving::Physics && setup.aiCraft.racingLine);
}

}

RacerRoutes::RacerRoutes(const RaceSetup& setup, const RaceCourse& course)
    : recorded_(setup.racers.size()), gatePaths_(setup.racers.size()), lines_(setup.racers.size()) {
    std::optional<GateLoop> loop;
    bool hasTraced = false;
    std::vector<RouteRef> fits;
    const bool hasRecordings = HasRecordings(course);
    for (std::size_t i = 0; i < setup.racers.size(); ++i) {
        recorded_[i] = RecordedFor(course, setup.firstSlot + i);
        if (setup.racers[i].isPlayer || course.world == nullptr) {
            continue;
        }
        if (WantsGatePath(setup, hasRecordings) && course.checkpoints != nullptr) {
            if (!hasTraced) {
                loop = GateLoop::Trace(course.checkpoints->Checkpoints(), *course.world);
                hasTraced = true;
            }
            if (loop) {
                gatePaths_[i].push_back(loop->RouteFrom(course.grid[i], *course.world));
            }
        }
        if (FollowsLines(setup, IsGatePath(i))) {
            lines_[i].resize(Of(i).size());
            for (std::size_t variant = 0; variant < Of(i).size(); ++variant) {
                fits.push_back({.racer = i, .variant = variant});
            }
        }
    }
    std::for_each(std::execution::par, fits.begin(), fits.end(), [&](const RouteRef& ref) {
        lines_[ref.racer][ref.variant] = std::make_shared<const RacingLine>(
            RacingLine::Fit(Of(ref.racer)[ref.variant], *course.world));
    });
}

std::span<const assets::RaceRoute> RacerRoutes::Of(std::size_t racer) const {
    return IsGatePath(racer) ? std::span<const assets::RaceRoute>(gatePaths_[racer])
                             : recorded_[racer];
}

}
