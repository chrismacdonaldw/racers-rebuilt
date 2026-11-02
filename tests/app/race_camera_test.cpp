#include "app/race_camera.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kTickMs = 12.0F;
constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

float Sine(float degrees) {
    return std::sin(degrees * std::numbers::pi_v<float> / 180.0F);
}

float Cosine(float degrees) {
    return std::cos(degrees * std::numbers::pi_v<float> / 180.0F);
}

CameraSubject CarFacing(float headingRadians) {
    return {.pose = {.rotation = FromAxisAngle({.z = 1.0F}, headingRadians)}};
}

Vector3 Eye(const RaceCamera& camera) {
    return ToGame(camera.Pose().position);
}

Vector3 Gaze(const RaceCamera& camera) {
    return ToGame(Rotate(camera.Pose().rotation, {.z = -1.0F}));
}

void Drive(RaceCamera& camera, const CameraSubject& subject, float ms) {
    const auto ticks = static_cast<int>(std::ceil(ms / kTickMs));
    for (int tick = 0; tick < ticks; ++tick) {
        camera.Follow(subject, kTickMs);
    }
}

float CutDistance(RaceCamera& camera, const CameraSubject& subject, float ms) {
    camera.Cut();
    camera.Follow(subject, ms);
    return -Eye(camera).x / Cosine(5.0F);
}

void CheckNear(const Vector3& actual, const Vector3& expected, double epsilon = 0.01) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(epsilon));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(epsilon));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(epsilon));
}

}

TEST_CASE("the race camera starts behind and above the car, looking down at it") {
    const RaceCamera camera(CarFacing(0.0F));

    CheckNear(Eye(camera), {.x = -20.0F * Cosine(5.0F), .z = 20.0F * (Sine(5.0F) + Sine(35.0F))});
    CheckNear(Gaze(camera), {.x = Cosine(5.0F), .z = -Sine(5.0F)});
}

TEST_CASE("each race camera view keeps its own distance and height") {
    RaceCamera camera(CarFacing(0.0F));

    camera.SetView(CameraView::Far, false);
    camera.Cut();
    camera.Follow(CarFacing(0.0F), kTickMs);
    CheckNear(Eye(camera), {.x = -30.0F * Cosine(8.0F), .z = 30.0F * (Sine(8.0F) + Sine(25.0F))});

    camera.SetView(CameraView::Standard, true);
    camera.Cut();
    camera.Follow(CarFacing(0.0F), kTickMs);
    CheckNear(Eye(camera), {.x = -22.0F * Cosine(8.0F), .z = 22.0F * (Sine(8.0F) + Sine(20.0F))});
}

TEST_CASE("the race camera eases into a new view") {
    RaceCamera camera(CarFacing(0.0F));
    const float before = Eye(camera).x;

    camera.SetView(CameraView::Far, false);
    camera.Follow(CarFacing(0.0F), kTickMs);
    const float during = Eye(camera).x;
    Drive(camera, CarFacing(0.0F), 3000.0F);

    CHECK(during < before);
    CHECK(during > Eye(camera).x);
    CHECK(Eye(camera).x == doctest::Approx(-30.0F * Cosine(8.0F)).epsilon(0.01));
}

TEST_CASE("cycling race camera views steps through all but the finish view") {
    RaceCamera camera(CarFacing(0.0F));

    camera.CycleView();
    CHECK(camera.View() == CameraView::Far);
    camera.CycleView();
    CHECK(camera.View() == CameraView::Close);
    camera.CycleView();
    CHECK(camera.View() == CameraView::Cockpit);
    camera.CycleView();
    CHECK(camera.View() == CameraView::Standard);
}

TEST_CASE("the cockpit view rides above the car and looks where it faces") {
    RaceCamera camera(CarFacing(kQuarterTurn));

    camera.SetView(CameraView::Cockpit, false);
    camera.Cut();
    camera.Follow(CarFacing(kQuarterTurn), kTickMs);
    CheckNear(Eye(camera), {.z = 10.0F});
    CheckNear(Gaze(camera), {.y = 1.0F});

    camera.SetView(CameraView::Cockpit, true);
    camera.Cut();
    camera.Follow(CarFacing(kQuarterTurn), kTickMs);
    CheckNear(Eye(camera), {.z = 6.0F});
}

TEST_CASE("looking back mirrors the chase camera through the car") {
    RaceCamera camera(CarFacing(0.0F));

    camera.SetLookBack(true);

    CheckNear(Eye(camera), {.x = 20.0F * Cosine(5.0F), .z = 20.0F * (Sine(5.0F) + Sine(35.0F))});
    CheckNear(Gaze(camera), {.x = -Cosine(5.0F), .z = -Sine(5.0F)});
    camera.SetView(CameraView::Standard, false);
    CHECK(Eye(camera).x < 0.0F);
}

TEST_CASE("looking back from the cockpit turns the view around") {
    RaceCamera camera(CarFacing(0.0F));
    camera.SetView(CameraView::Cockpit, false);
    camera.Cut();
    camera.Follow(CarFacing(0.0F), kTickMs);

    camera.SetLookBack(true);

    CheckNear(Eye(camera), {.z = 10.0F});
    CheckNear(Gaze(camera), {.x = -1.0F});
}

TEST_CASE("a turbo pulls the race camera in, then lets it drift back out") {
    RaceCamera camera(CarFacing(0.0F));
    CameraSubject boosting = CarFacing(0.0F);
    boosting.turbo = sim::TurboLevel::Hover;

    for (int step = 0; step < 4; ++step) {
        CHECK(CutDistance(camera, boosting, 100.0F) == doctest::Approx(12.0F).epsilon(0.001));
    }
    CHECK(CutDistance(camera, boosting, 100.0F) == doctest::Approx(16.0F).epsilon(0.001));
    CHECK(CutDistance(camera, boosting, 100.0F) == doctest::Approx(20.0F).epsilon(0.001));
    CHECK(CutDistance(camera, boosting, 100.0F) == doctest::Approx(20.0F).epsilon(0.001));
}

TEST_CASE("the race camera only zooms when a turbo starts") {
    CameraSubject boosting = CarFacing(0.0F);
    boosting.turbo = sim::TurboLevel::Weak;
    RaceCamera camera(boosting);

    CHECK(CutDistance(camera, boosting, kTickMs) == doctest::Approx(20.0F).epsilon(0.001));
    CHECK(CutDistance(camera, CarFacing(0.0F), kTickMs) == doctest::Approx(20.0F).epsilon(0.001));
    CHECK(CutDistance(camera, boosting, kTickMs) == doctest::Approx(12.0F).epsilon(0.001));
}

TEST_CASE("the race camera leads into a turn") {
    RaceCamera camera(CarFacing(0.0F));
    CameraSubject turning = CarFacing(0.0F);
    turning.forwardSpeed = 0.1F;
    turning.turnRadius = 20.0F;

    Drive(camera, turning, 6000.0F);

    const float lead = std::atan(std::sin(0.3F));
    CHECK(Eye(camera).y == doctest::Approx(-20.0F * Cosine(5.0F) * std::sin(lead)).epsilon(0.005));
}

TEST_CASE("the race camera holds its heading while the car spins") {
    RaceCamera camera(CarFacing(0.0F));
    CameraSubject spinning = CarFacing(kQuarterTurn);
    spinning.isSpinning = true;

    Drive(camera, spinning, 1000.0F);

    CheckNear(Eye(camera), {.x = -20.0F * Cosine(5.0F), .z = 20.0F * (Sine(5.0F) + Sine(35.0F))});
}

TEST_CASE("the finish view swings round to face the car") {
    RaceCamera camera(CarFacing(0.0F));

    camera.SetView(CameraView::Finish, false);
    Drive(camera, CarFacing(0.0F), 1000.0F);
    const Vector3 halfway = Eye(camera);
    Drive(camera, CarFacing(0.0F), 5000.0F);

    CHECK(std::abs(halfway.y) > 10.0F);
    CheckNear(Eye(camera), {.x = 32.0F * Cosine(20.0F), .z = 32.0F * (Sine(20.0F) + Sine(15.0F))});
    CheckNear(Gaze(camera), {.x = -Cosine(20.0F), .z = -Sine(20.0F)});
}

}
