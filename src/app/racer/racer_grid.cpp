#include "app/racer/racer_grid.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

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
    if (codes->size() < slots) {
        return Fail(ErrorCode::InvalidData, std::format("{} has {} grid drivers for {} slots",
                                                        track, codes->size(), slots));
    }
    if (playerRacer >= slots) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} has no grid slot {} for the player", track, playerRacer));
    }
    std::swap(codes->front(), codes->at(playerRacer));
    std::vector<Slot> loaded;
    for (const std::string& code : std::span(*codes).first(slots)) {
        Result<std::unique_ptr<RacerAssets>> assets = LoadSlot(vfs, loader, *catalog, code);
        Result<RacerVisuals> visuals =
            assets.and_then([&](const std::unique_ptr<RacerAssets>& racer) {
                return UploadRacer(renderer, *racer).and_then([&](RacerDraws draws) {
                    return RacerVisuals::Create(*racer, std::move(draws));
                });
            });
        if (!visuals) {
            return Fail(visuals.error().code,
                        std::format("racer {}: {}", code, visuals.error().message));
        }
        loaded.push_back({.assets = std::move(*assets), .visuals = std::move(*visuals)});
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

void RacerGrid::Update(std::span<const sim::EntitySnapshot> poses,
                       std::span<const sim::RacerSnapshot> motion, float elapsedMs) {
    auto next = draws_.begin();
    for (std::size_t i = 0; i < slots_.size() && i < poses.size() && i < motion.size(); ++i) {
        RacerVisuals& visuals = slots_[i].visuals;
        visuals.Update(TransformToGame(poses[i].transform), motion[i], elapsedMs);
        next = std::ranges::copy(visuals.Draws(), next).out;
    }
}

}
