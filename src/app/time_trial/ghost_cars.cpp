#include "app/time_trial/ghost_cars.hpp"

#include <algorithm>
#include <format>
#include <initializer_list>
#include <string_view>
#include <utility>

#include "app/racer/far_cars.hpp"
#include "app/track_space.hpp"
#include "core/log.hpp"
#include "core/vector3.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kGhostWorld = "GHOST.WDB";
constexpr std::string_view kRecordChassis = "GHOST2";
constexpr std::string_view kRecordBody = "GHOSTB2";
constexpr std::string_view kRecordDriver = "GHOSTDR";
constexpr Vector3 kRecordSeat{.x = -2.131681F, .y = 0.01123F, .z = 1.608F};
constexpr std::size_t kRecordDriverPart = 12;
constexpr std::size_t kBestDriverPart = 13;

Result<GhostCarModels> RecordModels(const EffectModels& models) {
    const auto chassis = models.find(kRecordChassis);
    const auto body = models.find(kRecordBody);
    const auto driver = models.find(kRecordDriver);
    if (chassis == models.end() || body == models.end() || driver == models.end()) {
        return Fail(ErrorCode::NotFound, std::format("{} lacks {}, {} or {}", kGhostWorld,
                                                     kRecordChassis, kRecordBody, kRecordDriver));
    }
    return GhostCarModels{.chassis = chassis->second.draws,
                          .chassisJoints = chassis->second.jointed,
                          .body = body->second.draws,
                          .driver = driver->second.draws,
                          .driverJoints = driver->second.jointed,
                          .seat = kRecordSeat,
                          .driverPart = kRecordDriverPart};
}

float RecordRangeSquared(const EffectModels& models) {
    float range = 0.0F;
    for (const std::string_view part : {kRecordChassis, kRecordBody, kRecordDriver}) {
        range = std::max(range, models.find(part)->second.rangeSquared);
    }
    return range;
}

GhostCarModels BestModels(const RacerGrid& racers, std::size_t playerSlot) {
    const RacerAssets& assets = racers.Assets(playerSlot);
    const RacerDraws& models = racers.Models(playerSlot);
    return {.chassis = models.wheels,
            .chassisJoints = &assets.wheels,
            .body = models.body,
            .driver = models.driver,
            .driverJoints = &assets.driver,
            .seat = assets.files.chassis.seat,
            .driverPart = kBestDriverPart};
}

}

std::optional<GhostCars> GhostCars::Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                         assets::TrackLoader& loader, const RacerGrid& racers,
                                         std::size_t playerSlot) {
    Result<EffectModelSet> record = LoadEffectModels(renderer, vfs, loader, kGhostWorld);
    Result<GhostCar> recordCar =
        record.and_then([](const EffectModelSet& set) { return RecordModels(set.models); })
            .and_then(GhostCar::Create);
    Result<GhostCar> bestCar = recordCar.and_then(
        [&](const GhostCar&) { return GhostCar::Create(BestModels(racers, playerSlot)); });
    if (!bestCar) {
        log::Warn("ghost cars: {}", bestCar.error().message);
        return std::nullopt;
    }
    return GhostCars(std::move(record->assets), std::move(*recordCar), std::move(*bestCar),
                     RecordRangeSquared(record->models));
}

void GhostCars::Update(const sim::GhostSnapshot& ghosts, float elapsedMs) {
    bestCar_.Update(ghosts.best, elapsedMs);
    recordCar_.Update(ghosts.record, elapsedMs);
}

void GhostCars::AppendDraws(std::vector<render::MeshDraw>& out) const {
    out.insert(out.end(), bestCar_.Draws().begin(), bestCar_.Draws().end());
    out.insert(out.end(), recordCar_.Draws().begin(), recordCar_.Draws().end());
}

void GhostCars::AddFarCars(const sim::GhostSnapshot& ghosts, std::size_t firstDraw,
                           FarModels& out) const {
    if (ghosts.best) {
        out.Add(FarKind::Car, {.position = ToEngine(ghosts.best->pose.position),
                               .draws = {.first = firstDraw, .count = bestCar_.Draws().size()},
                               .rangeSquared = kCarRangeSquared});
    }
    if (ghosts.record) {
        out.Add(FarKind::Car, {.position = ToEngine(ghosts.record->pose.position),
                               .draws = {.first = firstDraw + bestCar_.Draws().size(),
                                         .count = recordCar_.Draws().size()},
                               .rangeSquared = recordRangeSquared_});
    }
}

void AppendGhostMarkers(const sim::GhostSnapshot& ghosts, std::vector<MapMarker>& out) {
    if (ghosts.best) {
        out.push_back({.position = ghosts.best->pose.position});
    }
    if (ghosts.record) {
        out.push_back({.position = ghosts.record->pose.position, .isChampion = true});
    }
}

}
