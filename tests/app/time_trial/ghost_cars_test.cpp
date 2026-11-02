#include "app/time_trial/ghost_cars.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "app/powerup/powerup_models.hpp"
#include "assets/archive_survey.hpp"
#include "assets/loader_pool.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kRecordDriverPart = 12;
constexpr std::size_t kBestDriverPart = 13;

const JointedAssets* JointsOf(const EffectAssets& loaded, std::string_view entity) {
    const auto found = loaded.entities.find(entity);
    return found == loaded.entities.end() || !found->second.jointed ? nullptr
                                                                    : &*found->second.jointed;
}

std::optional<EffectAssets> LoadGhostWorld() {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    Result<EffectAssets> loaded = LoadEffectAssets(*vfs, loader, "GHOST.WDB");
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

sim::GhostFrame At(float x) {
    return {.pose = {.position = {.x = x}}};
}

TEST_CASE("the minimap marks the best ghost as a racer and the record ghost as the champion") {
    std::vector<MapMarker> markers;

    AppendGhostMarkers({.best = At(1.0F), .record = At(2.0F)}, markers);

    REQUIRE(markers.size() == 2);
    CHECK(markers[0].position.x == 1.0F);
    CHECK_FALSE(markers[0].isChampion);
    CHECK(markers[1].position.x == 2.0F);
    CHECK(markers[1].isChampion);
}

TEST_CASE("ghosts that aren't drawn have no minimap marker") {
    std::vector<MapMarker> markers;

    AppendGhostMarkers({.best = std::nullopt, .record = At(2.0F)}, markers);
    AppendGhostMarkers({}, markers);

    REQUIRE(markers.size() == 1);
    CHECK(markers[0].isChampion);
}

TEST_CASE("GHOST.WDB holds the record ghost's chassis, body and driver") {
    const std::optional<EffectAssets> ghost = LoadGhostWorld();
    if (!ghost) {
        return;
    }

    CHECK(JointsOf(*ghost, "GHOST2") != nullptr);
    CHECK(ghost->entities.contains("GHOSTB2"));
    CHECK(JointsOf(*ghost, "GHOSTDR") != nullptr);
}

TEST_CASE("the record ghost's driver loops loser_lp and the best ghost's winner1") {
    const std::optional<EffectAssets> ghost = LoadGhostWorld();
    if (!ghost) {
        return;
    }
    const JointedAssets* driver = JointsOf(*ghost, "GHOSTDR");
    REQUIRE(driver != nullptr);
    const std::vector<assets::AnimationPart>& parts = driver->animations.parts;

    REQUIRE(parts.size() > kBestDriverPart);
    CHECK(parts[kRecordDriverPart].name == "loser_lp");
    CHECK(parts[kBestDriverPart].name == "winner1");
}

}
}
