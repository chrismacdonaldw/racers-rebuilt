#include "app/powerup/powerup_models.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include "app/powerup/powerup_quads.hpp"
#include "assets/archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"

namespace rr::app {
namespace {

constexpr std::array kAnimated{"SHIELD0", "SHLDIN3", "TURBOL2", "TURB0F1", "TURB2F2",
                               "DMISSIL", "CURSE",   "MAGNET",  "MAGRING"};
constexpr std::array kStatic{"EXPLSN", "SPIKEXP", "BARREL", "GRAPPLE", "WARPPRT"};

std::optional<PowerupAssets> LoadRetail() {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    Result<PowerupAssets> loaded = LoadPowerupAssets(*vfs, loader);
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

bool IsJointedAndLoaded(const PowerupAssets& loaded, std::string_view name) {
    const auto entity = loaded.entities.find(name);
    return entity != loaded.entities.end() && entity->second.jointed.has_value() &&
           loaded.models.models.contains(assets::ToUpperAscii(entity->second.geometry));
}

TEST_CASE("the power-up world loads the animated models with their skeletons") {
    const std::optional<PowerupAssets> loaded = LoadRetail();
    if (!loaded) {
        return;
    }
    for (const std::string_view name : kAnimated) {
        CHECK_MESSAGE(IsJointedAndLoaded(*loaded, name), name);
    }
}

TEST_CASE("the power-up world holds every quad material") {
    const std::optional<PowerupAssets> loaded = LoadRetail();
    if (!loaded) {
        return;
    }
    for (const std::string_view name : PowerupQuadMaterials()) {
        CHECK_MESSAGE(loaded->models.materials.contains(name), name);
    }
}

TEST_CASE("the power-up world loads the still models") {
    const std::optional<PowerupAssets> loaded = LoadRetail();
    if (!loaded) {
        return;
    }
    for (const std::string_view name : kStatic) {
        CHECK_MESSAGE(loaded->entities.contains(name), name);
    }
}
}
}
