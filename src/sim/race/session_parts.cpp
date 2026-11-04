#include "sim/race/session_parts.hpp"

#include <algorithm>
#include <execution>
#include <memory>
#include <string>
#include <string_view>

#include "assets/game_path.hpp"
#include "sim/powerup/powerup_cheats.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kMostWeaponEventsPerEffect = 2;
constexpr std::size_t kMostShotCastsPerEffect = 2;
constexpr std::size_t kEventsPerShotCast = 2;

struct RouteRef {
    std::size_t slot = 0;
    std::size_t variant = 0;
};

std::size_t EffectSlots(const RaceSetup& setup) {
    std::size_t slots = 0;
    for (std::size_t kind = 0; kind < kPowerupKindCount; ++kind) {
        slots += setup.effects.at(kind) ? SpecFor(static_cast<PowerupKind>(kind)).poolSize : 0;
    }
    return slots;
}

std::optional<std::size_t> FindWorld(std::span<const NamedWorld> worlds, std::string_view name) {
    const std::string wanted = assets::ToUpperAscii(name);
    const auto found = std::ranges::find_if(worlds, [&](const NamedWorld& world) {
        return assets::ToUpperAscii(world.name) == wanted;
    });
    if (found == worlds.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - worlds.begin());
}

}

BrickField BricksFor(const RaceCourse& course, const Cheats& cheats) {
    return course.bricks == nullptr ? BrickField()
                                    : BrickField(*course.bricks, CheatBrickColour(cheats));
}

std::size_t BrickCount(const RaceCourse& course) {
    return course.bricks == nullptr ? 0
                                    : course.bricks->coloured.size() + course.bricks->white.size();
}

std::size_t UsesPerTick(const RaceSetup& setup, const RaceCourse& course) {
    return setup.racers.size() + Hazards::MostUsesPerTick(course.hazards, setup.racers.size());
}

std::size_t WeaponEventCount(const RaceSetup& setup, const RaceCourse& course) {
    return (EffectSlots(setup) * kMostWeaponEventsPerEffect) +
           (UsesPerTick(setup, course) * kMaxLaunches);
}

const TrackEventSources& EventSourcesFor(const RaceCourse& course) {
    static const TrackEventSources kNone;
    return course.events == nullptr ? kNone : *course.events;
}

std::size_t OutsideEventCount(const RaceSetup& setup, const RaceCourse& course) {
    return (EffectSlots(setup) * kMostShotCastsPerEffect * kEventsPerShotCast) +
           (setup.racers.size() * SurfaceEvents::MostEventsPerRacer()) +
           EventResources::MostFiredPerTick(EventSourcesFor(course).table) +
           Hazards::MostEventsPerTick(course.hazards, setup.racers.size()) +
           (setup.racers.size() * kEventsPerTaunt);
}

TrackSurfaces SurfacesFor(const RaceCourse& course) {
    return course.collision == nullptr ? TrackSurfaces()
                                       : TrackSurfaces(course.collision->surfaces);
}

std::vector<std::optional<std::size_t>> SphereWorldsFor(const RaceCourse& course) {
    const std::vector<assets::TriggerSphere>& spheres = EventSourcesFor(course).racerSpheres;
    std::vector<std::optional<std::size_t>> worlds(spheres.size());
    for (std::size_t i = 0; course.collision != nullptr && i < spheres.size(); ++i) {
        if (spheres[i].collisionWorld) {
            worlds[i] = FindWorld(course.collision->worlds, *spheres[i].collisionWorld);
        }
    }
    return worlds;
}

const TrackEntitySources& EntitySourcesFor(const RaceCourse& course) {
    static const TrackEntitySources kNone;
    return course.entities == nullptr ? kNone : *course.entities;
}

std::span<const assets::RaceRoute> RoutesFor(const RaceCourse& course, std::size_t slot) {
    if (course.routes == nullptr || slot >= course.routes->size()) {
        return {};
    }
    return (*course.routes)[slot];
}

std::vector<std::vector<std::shared_ptr<const RacingLine>>> RacingLinesFor(
    const RaceSetup& setup, const RaceCourse& course) {
    std::vector<std::vector<std::shared_ptr<const RacingLine>>> lines;
    if (setup.aiDriving != AiDriving::Physics || !setup.aiCraft.racingLine ||
        course.routes == nullptr || course.world == nullptr) {
        return lines;
    }
    std::vector<RouteRef> routes;
    for (std::size_t slot = 0; slot < course.routes->size(); ++slot) {
        lines.emplace_back((*course.routes)[slot].size());
        for (std::size_t variant = 0; variant < (*course.routes)[slot].size(); ++variant) {
            routes.push_back({.slot = slot, .variant = variant});
        }
    }
    std::for_each(std::execution::par, routes.begin(), routes.end(), [&](const RouteRef& ref) {
        lines[ref.slot][ref.variant] = std::make_shared<const RacingLine>(
            RacingLine::Fit((*course.routes)[ref.slot][ref.variant], *course.world));
    });
    return lines;
}

}
