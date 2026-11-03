#include "app/cutscene/award_car.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"

namespace rr::app {
namespace {

assets::TrackAssets Stage() {
    assets::WorldAsset world{.models = {"box"}};
    world.modelEntities.push_back({.name = "carbody", .lods = {{.model = 0}}});
    assets::TrackAssets stage{
        .worlds = {std::make_shared<const assets::WorldAsset>(std::move(world))},
        .models = {{"BOX", std::make_shared<const assets::GeometryAsset>(
                               assets::GeometryAsset{.materials = {"box"}})}}};
    stage.materials.emplace("RED", assets::MaterialAsset{.name = "stage"});
    return stage;
}

TEST_CASE("the award car takes carbody's model and its materials win") {
    assets::TrackAssets stage = Stage();
    const AwardCar car{.model = {.materials = {"red"}},
                       .looks = {.materials = {{"RED", assets::MaterialAsset{.name = "car"}}}}};
    REQUIRE(HasCarBody(stage));

    DressAwardCar(car, stage);

    CHECK(stage.models.at("BOX")->materials == std::vector<std::string>{"red"});
    CHECK(stage.materials.at("RED").name == "car");
}

TEST_CASE("a stage without carbody has no car to dress") {
    assets::TrackAssets stage = Stage();
    stage.models.clear();
    assets::WorldAsset world{.models = {"box"}};
    world.modelEntities.push_back({.name = "guy1", .lods = {{.model = 0}}});
    stage.worlds = {std::make_shared<const assets::WorldAsset>(std::move(world))};

    CHECK_FALSE(HasCarBody(stage));
}

TEST_CASE("the award car is the player's racer, or none, off the car win screen") {
    const assets::Vfs vfs;
    const Result<std::optional<settings::Racer>> none = AwardCarRacer(vfs, {});
    REQUIRE(none.has_value());
    CHECK_FALSE(none->has_value());
    const settings::Racer mine{.name = "MINE"};
    const Result<std::optional<settings::Racer>> own = AwardCarRacer(vfs, {.playerRacer = mine});
    REQUIRE(own.has_value());
    CHECK(*own == std::optional(mine));
}

std::optional<settings::Racer> ChampionCar(const assets::Vfs& vfs, std::string_view champion) {
    return AwardCarRacer(vfs, {.driver = std::string(champion), .showsChampionCar = true})
        .value_or(std::nullopt);
}

bool IsFrozenAndDressed(const AwardCar& car) {
    const bool isFrozen = std::ranges::all_of(
        car.model.vertices, [](const assets::GeometryVertex& vertex) { return vertex.joint == 0; });
    const bool isDressed = std::ranges::all_of(car.model.materials, [&](const std::string& name) {
        return car.looks.materials.contains(assets::ToUpperAscii(name));
    });
    return isFrozen && isDressed && !car.model.indices.empty();
}

TEST_CASE("every retail champion hands over a CHAMP quick build on their chassis") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    std::vector<std::string> found;
    for (const std::string_view champion : {"CR", "KK", "BB", "JT", "GM", "BVB"}) {
        const std::optional<settings::Racer> car = ChampionCar(*vfs, champion);
        found.push_back(car ? std::format("{} {}", car->name, assets::ToUpperAscii(car->chassis))
                            : std::string("none"));
    }
    CHECK(found == std::vector<std::string>{"CHAMP CRCHAS0", "CHAMP KKCHAS0", "CHAMP BBCHAS0",
                                            "CHAMP JTCHAS0", "CHAMP GM_CHAS0", "CHAMP BVBCHA0"});
}

TEST_CASE("a retail champion's car loads on its menu chassis at rest") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const std::optional<settings::Racer> racer = ChampionCar(*vfs, "KK");
    REQUIRE(racer.has_value());
    const Result<std::optional<AwardCar>> car = LoadAwardCar(*vfs, loader, *racer);
    REQUIRE_MESSAGE(car.has_value(), car.error().message);
    REQUIRE(car->has_value());
    CHECK(IsFrozenAndDressed(**car));
}

TEST_CASE("a racer without pieces keeps the stand-in car") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const Result<std::optional<AwardCar>> car = LoadAwardCar(*vfs, loader, {.chassis = "XXJMW"});
    REQUIRE_MESSAGE(car.has_value(), car.error().message);
    CHECK_FALSE(car->has_value());
}

}
}
