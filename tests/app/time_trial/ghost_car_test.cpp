#include "app/time_trial/ghost_car.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <numbers>
#include <optional>
#include <span>
#include <utility>

#include "../racer/racer_rig.hpp"

namespace rr::app {
namespace {

using test::CheckNear;
using test::GameAxis;
using test::GamePosition;
using test::Model;

constexpr float kFrameMs = 16.0F;
constexpr std::size_t kDefaultPart = 0;
constexpr std::size_t kPowerPart = 1;

struct Rig {
    JointedAssets wheels = test::Wheels();
    JointedAssets driver = test::Driver();

    [[nodiscard]] GhostCarModels Models(std::size_t driverPart) const {
        return {.chassis = Model({1}),
                .chassisJoints = &wheels,
                .body = Model({0}),
                .driver = Model({0}),
                .driverJoints = &driver,
                .seat = {.x = -2.0F, .z = 1.0F},
                .driverPart = driverPart};
    }

    [[nodiscard]] GhostCar Car(std::size_t driverPart) const {
        Result<GhostCar> car = GhostCar::Create(Models(driverPart));
        REQUIRE(car.has_value());
        return std::move(*car);
    }
};

sim::GhostFrame At(float x, float chassisAnimationRate = 0.0F) {
    return {.pose = {.position = {.x = x}}, .chassisAnimationRate = chassisAnimationRate};
}

TEST_CASE("a ghost car sits at its frame, with its driver in the seat playing the given part") {
    const Rig rig;
    GhostCar car = rig.Car(kPowerPart);

    car.Update(At(10.0F), kFrameMs);
    const std::span<const render::MeshDraw> draws = car.Draws();

    REQUIRE(draws.size() == 3);
    CheckNear(GamePosition(draws[0]), {.x = 11.0F});
    CheckNear(GamePosition(draws[1]), {.x = 10.0F});
    CheckNear(GamePosition(draws[2]), {.x = 8.0F, .z = 2.0F});
}

TEST_CASE("a ghost car hides while it has no frame") {
    const Rig rig;
    GhostCar car = rig.Car(kDefaultPart);

    CHECK(car.Draws().empty());
    car.Update(At(0.0F), kFrameMs);
    CHECK(car.Draws().size() == 3);
    car.Update(std::nullopt, kFrameMs);
    CHECK(car.Draws().empty());
}

TEST_CASE("a ghost car draws at alpha 127 and keeps its colour key") {
    const Rig rig;
    GhostCar car = rig.Car(kDefaultPart);

    car.Update(At(0.0F), kFrameMs);

    for (const render::MeshDraw& draw : car.Draws()) {
        CHECK(draw.material.blend == render::BlendMode::Alpha);
        CHECK(draw.material.tint.a == doctest::Approx(127.0F / 255.0F));
        CHECK(draw.material.alphaCutoff == doctest::Approx(0.5F * 127.0F / 255.0F));
    }
}

TEST_CASE("a ghost's wheels turn at the frame's rate in frames per second") {
    const Rig rig;
    GhostCar car = rig.Car(kDefaultPart);

    car.Update(At(0.0F, 10.0F), 500.0F);

    const float half = std::numbers::sqrt2_v<float> / 2.0F;
    CheckNear(GameAxis(car.Draws()[0], {.x = 1.0F}), {.x = half, .z = -half});
}

TEST_CASE("a ghost driver part past the animations fails") {
    const Rig rig;

    CHECK_FALSE(GhostCar::Create(rig.Models(4)).has_value());
}

}
}
