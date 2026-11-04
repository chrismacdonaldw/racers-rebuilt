#include "app/racer/racer_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/racer/built_racer.hpp"
#include "app/racer/racer_bounds.hpp"
#include "app/racer/racer_draws.hpp"
#include "app/track_space.hpp"
#include "app/voice_reactions.hpp"
#include "assets/race_catalog.hpp"
#include "assets/racer_catalog.hpp"
#include "core/utf16_text.hpp"
#include "sim/animation/part_clock.hpp"
#include "sim/physics/racer_chassis.hpp"
#include "sim/race/cheats.hpp"

namespace rr::app {
namespace {

constexpr float kLookRangeSquared = 200.0F * 200.0F;
constexpr float kSelfDistanceSquared = 2.0F;

Result<std::unique_ptr<RacerAssets>> LoadSlot(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                              const assets::RacerCatalog& catalog,
                                              std::string_view code, const settings::Racer* own,
                                              CarBody body) {
    Result<RacerAssets> racer = own != nullptr ? LoadBuiltRacer(vfs, loader, catalog, *own, body)
                                               : LoadRacer(vfs, loader, catalog, code);
    return racer.transform(
        [](RacerAssets assets) { return std::make_unique<RacerAssets>(std::move(assets)); });
}

Result<std::vector<std::string>> PlayersFirst(std::vector<std::string> codes,
                                              const RaceSceneSetup& setup) {
    const bool hasSecond = setup.players > 1;
    for (const std::size_t pick : {setup.racer, hasSecond ? setup.secondRacer : setup.racer}) {
        if (pick >= codes.size()) {
            return Fail(ErrorCode::InvalidData,
                        std::format("{} has no grid slot {} for a player", setup.track, pick));
        }
    }
    const std::string second = codes[hasSecond ? setup.secondRacer : 0];
    std::swap(codes.front(), codes[setup.racer]);
    if (hasSecond) {
        codes.at(1) = second;
    }
    if (setup.modifiers.rushesRocketRacer) {
        RushRocketRacer(codes, setup.players);
    }
    return codes;
}

Result<std::vector<std::string>> GridOf(const assets::Vfs& vfs, const RaceSceneSetup& setup) {
    return assets::RaceCatalog::Load(vfs).and_then([&](const assets::RaceCatalog& races) {
        const bool offCircuit = setup.circuit.empty() && !races.HomeCircuit(setup.track);
        if (offCircuit && !races.Circuits().empty()) {
            return Result<std::vector<std::string>>(races.Circuits().front().grid);
        }
        return races.Grid(setup.track, setup.circuit);
    });
}

const settings::Racer* OwnRacer(const RaceSceneSetup& setup, std::size_t slot) {
    if (slot == 0 && setup.playerRacer) {
        return &*setup.playerRacer;
    }
    if (slot == 1 && setup.players > 1 && setup.secondPlayerRacer) {
        return &*setup.secondPlayerRacer;
    }
    return nullptr;
}

RacerParts ShownParts(const sim::Cheats& cheats) {
    return {.wheels = !cheats.Has(sim::Cheat::NoWheels),
            .body = !cheats.Has(sim::Cheat::NoChassis),
            .driver = !cheats.Has(sim::Cheat::NoDriver)};
}

std::optional<Vector3> NearestRacer(std::span<const sim::EntitySnapshot> poses, std::size_t slot) {
    const Vector3 own = ToGame(poses[slot].transform.position);
    std::optional<Vector3> nearest;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const sim::EntitySnapshot& pose : poses) {
        const Vector3 other = ToGame(pose.transform.position);
        const float distance = Dot(other - own, other - own);
        if (distance >= kSelfDistanceSquared && distance <= kLookRangeSquared &&
            distance < nearestDistance) {
            nearest = other;
            nearestDistance = distance;
        }
    }
    return nearest;
}

void Dress(const Vector3& origin, const CarLook& look, std::span<render::MeshDraw> draws) {
    for (render::MeshDraw& draw : draws) {
        draw.transform.position = origin + ((draw.transform.position - origin) * look.scale);
        draw.scale *= look.scale;
        draw.material.vertexColors = look.colors;
    }
}

}

void RushRocketRacer(std::span<std::string> codes, std::size_t players) {
    std::ranges::fill(codes.subspan(std::min(players, codes.size())), std::string(kRocketRacer));
}

Result<RacerGrid> RacerGrid::Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                  assets::TrackLoader& loader, const RaceSceneSetup& setup,
                                  std::size_t slots) {
    Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(vfs);
    Result<std::vector<std::string>> codes =
        catalog.and_then([&](const assets::RacerCatalog&) { return GridOf(vfs, setup); })
            .and_then([&](std::vector<std::string> grid) {
                return PlayersFirst(std::move(grid), setup);
            });
    if (!codes) {
        return std::unexpected(std::move(codes.error()));
    }
    slots = std::min(slots, codes->size());
    const RacerParts shown = ShownParts(setup.cheats);
    std::vector<Slot> loaded;
    for (const std::string& code : std::span(*codes).first(slots)) {
        const settings::Racer* own = OwnRacer(setup, loaded.size());
        Result<std::unique_ptr<RacerAssets>> assets =
            LoadSlot(vfs, loader, *catalog, code, own, setup.carBody);
        Result<BoundingSphere> bounds = assets.and_then(
            [&](const std::unique_ptr<RacerAssets>& racer) { return RacerBounds(*racer, shown); });
        RacerDraws models;
        Result<RacerVisuals> visuals = bounds.and_then([&](const BoundingSphere&) {
            return UploadRacer(renderer, **assets).and_then([&](RacerDraws draws) {
                models = draws;
                return RacerVisuals::Create(**assets, ShowOnly(std::move(draws), shown),
                                            sim::Random(loaded.size()));
            });
        });
        if (!visuals) {
            return Fail(visuals.error().code,
                        std::format("racer {}: {}", own != nullptr ? own->name : code,
                                    visuals.error().message));
        }
        loaded.push_back({.assets = std::move(*assets),
                          .models = std::move(models),
                          .visuals = std::move(*visuals),
                          .localBounds = *bounds});
    }
    return RacerGrid(std::move(loaded));
}

RacerGrid::RacerGrid(std::vector<Slot> slots) : slots_(std::move(slots)) {
    for (const Slot& slot : slots_) {
        draws_.insert(draws_.end(), slot.visuals.Draws().begin(), slot.visuals.Draws().end());
    }
}

sim::ChassisSpec RacerGrid::Chassis(std::size_t slot) const {
    const RacerAssets& racer = *slots_.at(slot).assets;
    return racer.build ? sim::BuiltChassis(racer.files.chassis)
                       : sim::PrebuiltChassis(racer.files.champion, racer.files.chassis);
}

sim::CarBuild RacerGrid::Build(std::size_t slot) const {
    return slots_.at(slot).assets->build.value_or(sim::CarBuild{});
}

const assets::SkidMarks& RacerGrid::Skids(std::size_t slot) const {
    return slots_.at(slot).assets->files.chassis.skids;
}

sim::PowerupHabits RacerGrid::Habits(std::size_t slot) const {
    return sim::HabitsOf(slots_.at(slot).assets->files.driver);
}

Milliseconds RacerGrid::SuddenStopLength(std::size_t slot) const {
    const std::vector<assets::AnimationPart>& parts =
        slots_.at(slot).assets->driver.animations.parts;
    const auto stop =
        std::ranges::find(parts, kDriverPartNames[std::to_underlying(DriverPart::SuddenStop)],
                          &assets::AnimationPart::name);
    if (stop == parts.end()) {
        return Milliseconds{0};
    }
    return Milliseconds{static_cast<std::int64_t>(std::ceil(sim::OnceLengthMs(*stop)))};
}

DriverName RacerGrid::Name(std::size_t slot) const {
    const RacerAssets& racer = *slots_.at(slot).assets;
    if (racer.build) {
        return Utf16FromUtf8(racer.files.driver.name);
    }
    return racer.files.driver.nameIndex;
}

std::int32_t RacerGrid::Voice(std::size_t slot) const {
    const RacerAssets& racer = *slots_.at(slot).assets;
    return racer.build ? BuiltCarVoice(slot) : DriverVoice(racer.files.driver.voiceBank);
}

float RacerGrid::EnginePitch(std::size_t slot) const {
    const float pitch = slots_.at(slot).assets->files.chassis.enginePitchScale;
    return std::isfinite(pitch) && pitch > 0.0F ? pitch : 1.0F;
}

void RacerGrid::Update(std::span<const sim::EntitySnapshot> poses,
                       std::span<const sim::RacerSnapshot> motion, std::span<const CarLook> looks,
                       float elapsedMs) {
    auto next = draws_.begin();
    for (std::size_t i = 0; i < slots_.size() && i < poses.size() && i < motion.size(); ++i) {
        RacerVisuals& visuals = slots_[i].visuals;
        visuals.Update(TransformToGame(poses[i].transform), motion[i], NearestRacer(poses, i),
                       elapsedMs);
        const auto first = next;
        next = std::ranges::copy(visuals.Draws(), next).out;
        if (motion[i].isGhost) {
            std::ranges::for_each(first, next, [](render::MeshDraw& draw) { draw.indexCount = 0; });
        } else if (i < looks.size()) {
            Dress(poses[i].transform.position, looks[i], std::span(first, next));
        }
    }
}

}
