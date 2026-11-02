#include "sim/physics/racer_chassis.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "sim/car_build/retail_cars.hpp"

namespace rr::sim {
namespace {

TEST_CASE("a prebuilt racer weighs what its champion does") {
    const assets::ChampionEntry champion{.mass = 4500.0F, .centreOfMass = {.x = -1.0F, .z = -1.0F}};
    assets::ChassisEntry chassis{.builtCentreOfMass = {.x = -1.0F, .z = -0.5F},
                                 .baseMass = 2500.0F,
                                 .shadowWidth = 8.0F,
                                 .shadowLength = 9.0F,
                                 .handling = 70,
                                 .topSpeed = 60,
                                 .acceleration = 100};
    chassis.wheelPositions[0] = {.x = 1.5F, .y = -2.0F};

    const ChassisSpec spec = PrebuiltChassis(champion, chassis);
    const BodyConstants constants = MakeBodyConstants(spec, CarBuild{});

    CHECK(constants.mass == 4500.0F);
    CHECK(constants.centreOfMass == Vector3{.x = -1.0F, .z = -1.0F});
    CHECK(spec.shadowLength == 9.0F);
    CHECK(spec.wheelPositions[0] == Vector3{.x = 1.5F, .y = -2.0F});
    CHECK(spec.handling == 70);
    CHECK(spec.topSpeed == 60);
    CHECK(spec.acceleration == 100);
}

TEST_CASE("a built racer adds its bricks to the chassis's own mass and centre of mass") {
    const assets::ChassisEntry chassis{
        .builtCentreOfMass = {.x = -1.0F, .z = -0.5F}, .baseMass = 2500.0F, .handling = 70};

    const ChassisSpec spec = BuiltChassis(chassis);
    const CarBuild build =
        BuiltCarBuild({.cells = 40, .centroid = {.x = -2.0F, .y = 1.0F, .z = 6.0F}});
    const BodyConstants constants = MakeBodyConstants(spec, build);

    CHECK(constants.mass == 2540.0F);
    CHECK(constants.centreOfMass == Vector3{.x = -1.5F, .y = 0.25F, .z = -0.5F});
    CHECK(MakeStatMultipliers(spec, build).handling ==
          MakeStatMultipliers(spec, CarBuild{.highPieceCount = 40}).handling);
    CHECK(spec.handling == 70);
}

struct RetailChassis {
    assets::ChassisEntry chassis;
    BrickVolume bricks;
};

std::optional<RetailChassis> LoadJoanOfKart() {
    const std::optional<assets::Vfs> vfs = rr::test::OpenRetailVfs();
    const std::optional<test::RetailCars> cars = test::LoadRetailCars(assets::PieceDetail::Race);
    if (!vfs || !cars) {
        return std::nullopt;
    }
    const Result<std::vector<assets::ChassisEntry>> chassis =
        vfs->Read("GAMEDATA/COMMON/CHASSIS.CMB").and_then([](const assets::FileData& data) {
            return assets::ParseChassis(data.Bytes());
        });
    REQUIRE(chassis.has_value());
    const assets::SavedRacer& joan = cars->racers.front();
    REQUIRE(joan.name == u"JOAN OF KART");
    const auto entry = std::ranges::find(*chassis, joan.chassis, &assets::ChassisEntry::name);
    REQUIRE(entry != chassis->end());
    const Result<BuiltCar> car = BuiltCar::Load(joan.car, cars->catalog.library);
    REQUIRE(car.has_value());
    return RetailChassis{.chassis = *entry, .bricks = car->Bricks()};
}

TEST_CASE("JOAN OF KART weighs its chassis plus 214 brick cells") {
    const std::optional<RetailChassis> joan = LoadJoanOfKart();
    if (!joan) {
        return;
    }

    const BodyConstants constants =
        MakeBodyConstants(BuiltChassis(joan->chassis), BuiltCarBuild(joan->bricks));

    CHECK(constants.mass == 2714.0F);
    CHECK(constants.centreOfMass.x == doctest::Approx(-1.2044F).epsilon(1e-4));
    CHECK(constants.centreOfMass.y == doctest::Approx(-0.0234F).epsilon(1e-2));
    CHECK(constants.centreOfMass.z == -0.5F);
}

}
}
