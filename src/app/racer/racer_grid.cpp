#include "app/racer/racer_grid.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

#include "app/racer/racer_bounds.hpp"
#include "app/racer/racer_draws.hpp"
#include "app/track_space.hpp"
#include "assets/circuit_database.hpp"
#include "assets/racer_catalog.hpp"
#include "sim/physics/prebuilt_chassis.hpp"

namespace rr::app {
namespace {

Result<std::unique_ptr<RacerAssets>> LoadSlot(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                              const assets::RacerCatalog& catalog,
                                              std::string_view code) {
    return LoadRacer(vfs, loader, catalog, code).transform([](RacerAssets assets) {
        return std::make_unique<RacerAssets>(std::move(assets));
    });
}

void ScaleAbout(const Vector3& origin, float scale, std::span<render::MeshDraw> draws) {
    for (render::MeshDraw& draw : draws) {
        draw.transform.position = origin + ((draw.transform.position - origin) * scale);
        draw.scale *= scale;
    }
}

}

Result<RacerGrid> RacerGrid::Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                  assets::TrackLoader& loader, std::string_view track,
                                  std::size_t slots, std::size_t playerRacer) {
    Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(vfs);
    Result<std::vector<std::string>> codes =
        catalog.and_then([&](const assets::RacerCatalog&) { return assets::LoadGrid(vfs, track); });
    if (!codes) {
        return std::unexpected(std::move(codes.error()));
    }
    slots = std::min(slots, codes->size());
    if (playerRacer >= slots) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} has no grid slot {} for the player", track, playerRacer));
    }
    std::swap(codes->front(), codes->at(playerRacer));
    std::vector<Slot> loaded;
    for (const std::string& code : std::span(*codes).first(slots)) {
        Result<std::unique_ptr<RacerAssets>> assets = LoadSlot(vfs, loader, *catalog, code);
        Result<BoundingSphere> bounds = assets.and_then(
            [](const std::unique_ptr<RacerAssets>& racer) { return RacerBounds(*racer); });
        Result<RacerVisuals> visuals = bounds.and_then([&](const BoundingSphere&) {
            return UploadRacer(renderer, **assets).and_then([&](RacerDraws draws) {
                return RacerVisuals::Create(**assets, std::move(draws));
            });
        });
        if (!visuals) {
            return Fail(visuals.error().code,
                        std::format("racer {}: {}", code, visuals.error().message));
        }
        loaded.push_back(
            {.assets = std::move(*assets), .visuals = std::move(*visuals), .localBounds = *bounds});
    }
    return RacerGrid(std::move(loaded));
}

RacerGrid::RacerGrid(std::vector<Slot> slots) : slots_(std::move(slots)) {
    for (const Slot& slot : slots_) {
        draws_.insert(draws_.end(), slot.visuals.Draws().begin(), slot.visuals.Draws().end());
    }
}

sim::ChassisSpec RacerGrid::Chassis(std::size_t slot) const {
    const assets::RacerFiles& files = slots_.at(slot).assets->files;
    return sim::PrebuiltChassis(files.champion, files.chassis);
}

sim::PowerupHabits RacerGrid::Habits(std::size_t slot) const {
    return sim::HabitsOf(slots_.at(slot).assets->files.driver);
}

std::int32_t RacerGrid::NameIndex(std::size_t slot) const {
    return slots_.at(slot).assets->files.driver.nameIndex;
}

std::int32_t RacerGrid::VoiceBank(std::size_t slot) const {
    return slots_.at(slot).assets->files.driver.voiceBank;
}

void RacerGrid::Update(std::span<const sim::EntitySnapshot> poses,
                       std::span<const sim::RacerSnapshot> motion, std::span<const float> scales,
                       float elapsedMs) {
    auto next = draws_.begin();
    for (std::size_t i = 0; i < slots_.size() && i < poses.size() && i < motion.size(); ++i) {
        RacerVisuals& visuals = slots_[i].visuals;
        visuals.Update(TransformToGame(poses[i].transform), motion[i], elapsedMs);
        const auto first = next;
        next = std::ranges::copy(visuals.Draws(), next).out;
        if (motion[i].isGhost) {
            std::ranges::for_each(first, next, [](render::MeshDraw& draw) { draw.indexCount = 0; });
        } else if (i < scales.size() && scales[i] != 1.0F) {
            ScaleAbout(poses[i].transform.position, scales[i], std::span(first, next));
        }
    }
}

}
