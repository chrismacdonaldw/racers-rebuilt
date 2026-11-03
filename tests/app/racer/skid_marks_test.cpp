#include "app/racer/skid_marks.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <ranges>
#include <vector>

#include "../decal_ground.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr render::TextureId kSkidTexture{1};
constexpr render::TextureId kTurboTexture{2};
constexpr float kFrameMs = 100.0F;

assets::NameMap<render::Material> Materials() {
    return {{"SKID", {.texture = kSkidTexture}}, {"TRBSKID", {.texture = kTurboTexture}}};
}

constexpr assets::SkidMarks kWheels{
    .frontWidth = 0.3F,
    .rearWidth = 0.5F,
    .wheelOffsets = {Vector3{.x = 2.7F, .y = -1.9F}, Vector3{.x = 2.7F, .y = 1.9F},
                     Vector3{.x = -3.2F, .y = -1.9F}, Vector3{.x = -3.2F, .y = 1.9F}}};

class Car {
public:
    Car() : marks_(Materials(), kWheels) {}

    void Drive(const sim::RacerSnapshot& racer, int frames) {
        for (int i = 0; i < frames; ++i) {
            x_ += 1.0F;
            marks_.Update({.transform = TransformToEngine({.position = {.x = x_, .z = 1.0F}})},
                          racer, kFrameMs);
        }
    }

    [[nodiscard]] std::vector<render::QuadDraw> Marks() const {
        const DecalSurface surface = test::FlatDecalGround();
        GroundDecals ground(surface);
        std::vector<render::QuadDraw> quads;
        marks_.Append(ground, quads);
        return quads;
    }

    [[nodiscard]] float X() const { return x_; }

private:
    SkidMarks marks_;
    float x_ = 0.0F;
};

Vector3 Centre(const render::QuadDraw& quad) {
    Vector3 sum;
    for (const Vector3& corner : quad.corners) {
        sum = sum + ToGame(corner);
    }
    return sum * 0.25F;
}

float Width(const render::QuadDraw& quad) {
    const auto [narrowest, widest] =
        std::ranges::minmax(quad.corners | std::views::transform([](const Vector3& corner) {
                                return ToGame(corner).y;
                            }));
    return (widest - narrowest) * 0.99F;
}

bool AllUse(const std::vector<render::QuadDraw>& marks, render::TextureId texture) {
    return std::ranges::all_of(
        marks, [&](const render::QuadDraw& mark) { return mark.texture == texture; });
}

bool AllWithin(const std::vector<render::QuadDraw>& marks, float minX, float maxX, float minY,
               float maxY) {
    return std::ranges::all_of(marks, [&](const render::QuadDraw& mark) {
        const Vector3 centre = Centre(mark);
        return centre.x >= minX && centre.x <= maxX && centre.y >= minY && centre.y <= maxY;
    });
}

sim::RacerSnapshot With(sim::SkidSnapshot skid) {
    return {.skid = skid};
}

TEST_CASE("a drifting car marks the wheels on the side it slides toward") {
    Car car;
    car.Drive({}, 1);

    car.Drive(With({.leftwardSpeed = -1.0F, .isDrifting = true}), 4);

    const std::vector<render::QuadDraw> marks = car.Marks();
    REQUIRE(marks.size() == 2);
    CHECK(AllUse(marks, kSkidTexture));
    CHECK(AllWithin(marks, -10.0F, 10.0F, -1.91F, -1.89F));
    CHECK(Width(marks[0]) == doctest::Approx(0.3F).epsilon(test::kClipEpsilon));
    CHECK(Width(marks[1]) == doctest::Approx(0.5F).epsilon(test::kClipEpsilon));
}

TEST_CASE("a strong turbo marks the rear wheels in its own material, which fade when it ends") {
    Car car;
    car.Drive({}, 1);

    car.Drive(With({.turbo = sim::TurboLevel::Strong, .turboCount = 1}), 3);
    const std::vector<render::QuadDraw> boosting = car.Marks();
    REQUIRE(boosting.size() == 2);
    CHECK(AllUse(boosting, kTurboTexture));
    CHECK(AllWithin(boosting, 0.0F, car.X() - 3.0F, -2.0F, 2.0F));
    car.Drive(With({.turboCount = 1}), 5);
    CHECK(car.Marks().size() == 2);
    car.Drive(With({.turboCount = 1}), 6);
    CHECK(car.Marks().empty());
}

TEST_CASE("a weak turbo leaves no marks") {
    Car car;
    car.Drive({}, 1);

    car.Drive(With({.turbo = sim::TurboLevel::Weak, .turboCount = 1}), 3);

    CHECK(car.Marks().empty());
}

TEST_CASE("a spinning car marks all four wheels with short trails, three segments each") {
    Car car;
    car.Drive({}, 1);

    car.Drive(With({.isSpinning = true}), 12);

    CHECK(car.Marks().size() == 12);
}

TEST_CASE("a ghost's marks are hidden, and a halted car stops marking") {
    Car car;
    car.Drive(With({.isMagnetHeld = true}), 3);
    REQUIRE(car.Marks().size() == 2);

    sim::RacerSnapshot ghost = With({.isMagnetHeld = true});
    ghost.isGhost = true;
    car.Drive(ghost, 1);
    CHECK(car.Marks().empty());

    Car halted;
    halted.Drive(With({.isMagnetHeld = true}), 3);
    halted.Drive(With({.isMagnetHeld = true, .isHalted = true}), 12);
    CHECK(halted.Marks().empty());
}

}
}
