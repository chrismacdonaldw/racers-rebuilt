#include "app/racer/champion_car.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/racer/built_racer.hpp"
#include "app/racer/racer_lookup.hpp"
#include "assets/archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"

namespace rr::app {
namespace {

struct Retail {
    assets::Vfs vfs;
    assets::RacerCatalog catalog;
};

std::optional<Retail> OpenRetail() {
    std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(*vfs);
    REQUIRE_MESSAGE(catalog.has_value(), catalog.error().message);
    return Retail{.vfs = std::move(*vfs), .catalog = std::move(*catalog)};
}

settings::Racer QuickBuild(const Retail& retail, std::string_view chassis) {
    Result<settings::Racer> racer = FindChampionCar(retail.vfs, chassis);
    REQUIRE_MESSAGE(racer.has_value(), racer.error().message);
    return racer.value_or(settings::Racer{});
}

std::string ChampionName(const Retail& retail, const settings::Racer& racer) {
    const Result<std::optional<assets::ChampionEntry>> champion =
        QuickBuiltChampion(retail.vfs, retail.catalog, racer);
    REQUIRE_MESSAGE(champion.has_value(), champion.error().message);
    const std::optional<assets::ChampionEntry> found = champion.value_or(std::nullopt);
    return found ? found->name + " " + found->body.geometry : std::string("none");
}

struct RacedCar {
    std::string body;
    bool hasBricks = false;
    float mass = 0.0F;
    int bricks = 0;
};

RacedCar Race(const Retail& retail, assets::TrackLoader& loader, const settings::Racer& racer,
              CarBody body) {
    const Result<RacerAssets> loaded =
        LoadBuiltRacer(retail.vfs, loader, retail.catalog, racer, body);
    REQUIRE_MESSAGE(loaded.has_value(), loaded.error().message);
    const RacerAssets& assets = *loaded;
    return {.body = assets.files.champion.body.geometry,
            .hasBricks = assets.models.models.contains("BUILT_CAR"),
            .mass = assets.files.champion.mass,
            .bricks = assets.build.value_or(sim::CarBuild{}).highPieceCount};
}

}

TEST_CASE("each champion's quick build is that champion's car") {
    const std::optional<Retail> retail = OpenRetail();
    if (!retail) {
        return;
    }
    std::vector<std::string> found;
    for (const std::string_view chassis : {"crchas0", "kkchas0", "bbchas0", "jtchas0", "gm_chas0",
                                           "bvbcha0", "rrchas0", "vvchas0"}) {
        found.push_back(ChampionName(*retail, QuickBuild(*retail, chassis)));
    }
    CHECK(found == std::vector<std::string>{"cr crcm", "kk kkcm", "bb bbcm", "jt jtcm", "gm gmcm",
                                            "bvb bvbcm", "rr rrcm", "vv vvcm"});
}

TEST_CASE("a changed quick build or another racer's car has no champion") {
    const std::optional<Retail> retail = OpenRetail();
    if (!retail) {
        return;
    }
    settings::Racer changed = QuickBuild(*retail, "kkchas0");
    constexpr std::size_t kFirstBrickColour = 2 + 8 + 5;
    changed.car.at(kFirstBrickColour) = std::byte{1};
    const Result<settings::Racer> joan = FindGameRacer(retail->vfs, {.name = "JOAN OF KART"});
    REQUIRE(joan.has_value());

    CHECK(ChampionName(*retail, changed) == "none");
    CHECK(ChampionName(*retail, joan.value_or(settings::Racer{})) == "none");
}

TEST_CASE("bricks keep a champion's quick build as built") {
    const std::optional<Retail> retail = OpenRetail();
    if (!retail) {
        return;
    }
    const settings::Racer racer = QuickBuild(*retail, "kkchas0");
    const Result<std::optional<assets::ChampionEntry>> bricks =
        ChampionBodyOf(retail->vfs, retail->catalog, racer, CarBody::Bricks);
    REQUIRE(bricks.has_value());
    CHECK_FALSE(bricks->has_value());
}

TEST_CASE("a racer in King Kahuka's quick build races his body on its own bricks' physics") {
    const std::optional<Retail> retail = OpenRetail();
    if (!retail) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(retail->vfs, pool);
    const settings::Racer racer = QuickBuild(*retail, "kkchas0");
    const RacedCar bricks = Race(*retail, loader, racer, CarBody::Bricks);
    const RacedCar champion = Race(*retail, loader, racer, CarBody::Champion);

    CHECK(std::pair(bricks.body, bricks.hasBricks) == std::pair(std::string("BUILT_CAR"), true));
    CHECK(std::pair(champion.body, champion.hasBricks) == std::pair(std::string("kkcm"), false));
    CHECK(std::pair(champion.mass, champion.bricks) == std::pair(bricks.mass, bricks.bricks));
}

}
