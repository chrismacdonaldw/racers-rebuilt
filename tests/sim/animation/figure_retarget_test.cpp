#include "sim/animation/figure_retarget.hpp"

#include <doctest/doctest.h>

#include <numbers>
#include <span>
#include <vector>

#include "sim/animation/skeleton_pose.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kChest = 1;
constexpr std::size_t kShoulder = 2;
constexpr std::size_t kElbow = 3;
constexpr std::size_t kHand = 4;
constexpr Vector3 kTorsoPivot{.z = 0.5F};
constexpr Vector3 kSocket{.y = 1.0F, .z = 1.2F};
constexpr Vector3 kBend{.y = 1.0F, .z = 0.6F};
constexpr Vector3 kWrist{.y = 1.0F};
constexpr float kNear = 0.01F;

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(kNear));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(kNear));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(kNear));
}

assets::SkeletonAsset Figure() {
    assets::SkeletonAsset skeleton{
        .nodes = {{.name = "root"},
                  {.name = "chest", .position = {.z = 1.0F}, .parent = 0},
                  {.name = "shoulder", .position = {.y = 1.0F, .z = 0.5F}, .parent = kChest},
                  {.name = "elbow", .position = {.z = -1.0F}, .parent = kShoulder},
                  {.name = "hand", .position = {.z = -1.0F}, .parent = kElbow}},
        .rig = assets::FigureRig{.pivots = {{.node = kChest, .position = kTorsoPivot},
                                            {.node = kShoulder, .position = kSocket},
                                            {.node = kElbow, .position = kBend},
                                            {.node = kHand, .position = kWrist}},
                                 .limbs = {{.nodes = {kShoulder, kElbow, kHand}}}}};
    skeleton.rig = BoundRig(*skeleton.rig, skeleton);
    return skeleton;
}

struct Posed {
    std::vector<Transform> rest;
    std::vector<Transform> retail;
    std::vector<Transform> figure;
};

Posed Pose(const assets::SkeletonAsset& skeleton, std::size_t node, const Quaternion& turn) {
    SkeletonPose pose(skeleton, 1.0F);
    const std::span<const Transform> rest = pose.Solve();
    Posed posed{.rest = {rest.begin(), rest.end()}, .retail = {}, .figure = {}};
    pose.Locals()[node].rotation = turn * pose.Locals()[node].rotation;
    const std::span<const Transform> retail = pose.Solve();
    posed.retail.assign(retail.begin(), retail.end());
    posed.figure = posed.retail;
    FigureRetargeter{}.Run(skeleton, 1.0F, posed.figure);
    return posed;
}

Vector3 Moved(const Posed& posed, std::size_t node, const Vector3& point) {
    return Apply(Compose(posed.figure[node], Inverse(posed.rest[node])), point);
}

Vector3 Across(const Vector3& v, const Vector3& along) {
    return v - (along * Dot(v, along));
}

TEST_CASE("in the rest pose every rig piece stays where it was fitted") {
    const Posed posed = Pose(Figure(), 0, {});

    CheckNear(Moved(posed, kChest, kTorsoPivot), kTorsoPivot);
    CheckNear(Moved(posed, kShoulder, kSocket), kSocket);
    CheckNear(Moved(posed, kHand, kWrist), kWrist);
}

TEST_CASE("a raised arm reaches toward the original hand as far as the piece goes") {
    const Quaternion raise = FromAxisAngle({.x = 1.0F}, std::numbers::pi_v<float> / 2.0F);

    const Posed posed = Pose(Figure(), kShoulder, raise);

    const Vector3 pointing = Normalized(posed.retail[kHand].position - kSocket, {});
    CheckNear(Moved(posed, kShoulder, kSocket), kSocket);
    CheckNear(Moved(posed, kHand, kWrist), kSocket + (pointing * Length(kWrist - kSocket)));
}

TEST_CASE("a hand within reach lands where the original hand is") {
    const Quaternion bend = FromAxisAngle({.x = 1.0F}, std::numbers::pi_v<float> * 0.8F);

    const Posed posed = Pose(Figure(), kElbow, bend);

    CheckNear(Moved(posed, kHand, kWrist), posed.retail[kHand].position);
}

TEST_CASE("a bent original elbow bends the piece's elbow the same way") {
    const Quaternion bend = FromAxisAngle({.x = 1.0F}, std::numbers::pi_v<float> * 2.0F / 3.0F);

    const Posed posed = Pose(Figure(), kElbow, bend);

    const Vector3 elbow = Moved(posed, kElbow, kBend);
    const Vector3 wrist = Moved(posed, kHand, kWrist);
    const Vector3 along = Normalized(wrist - kSocket, {});
    const Vector3 retailSide =
        Across(posed.retail[kElbow].position - posed.retail[kShoulder].position,
               Normalized(posed.retail[kHand].position - posed.retail[kShoulder].position, {}));
    CHECK(Length(elbow - kSocket) == doctest::Approx(Length(kBend - kSocket)));
    CHECK(Length(wrist - elbow) == doctest::Approx(Length(kWrist - kBend)));
    CHECK(Dot(Across(elbow - kSocket, along), retailSide) > 0.0F);
}

TEST_CASE("a bent original wrist keeps the hand on the forearm's line") {
    const Quaternion bend = FromAxisAngle({.x = 1.0F}, std::numbers::pi_v<float> / 2.0F);
    const Vector3 fingers = kWrist + Vector3{.z = -0.2F};

    const Posed posed = Pose(Figure(), kHand, bend);

    CheckNear(Moved(posed, kHand, fingers), fingers);
}

TEST_CASE("a turned chest turns the torso about its own pivot and carries the arm along") {
    const Quaternion lean = FromAxisAngle({.y = 1.0F}, std::numbers::pi_v<float> / 4.0F);

    const Posed posed = Pose(Figure(), kChest, lean);

    const Vector3 socket = Moved(posed, kChest, kSocket);
    CheckNear(Moved(posed, kChest, kTorsoPivot), kTorsoPivot);
    CheckNear(Moved(posed, kShoulder, kSocket), socket);
}

TEST_CASE("a rig bound to another skeleton turns its pieces from the pose they were fitted in") {
    const assets::SkeletonAsset body = Figure();
    assets::SkeletonAsset shown = Figure();
    shown.nodes[kShoulder].rotation = FromAxisAngle({.x = 1.0F}, std::numbers::pi_v<float> / 2.0F);
    shown.rig = BoundRig(*body.rig, body);
    SkeletonPose bodyPose(body, 1.0F);
    const std::vector<Transform> bind(bodyPose.Solve().begin(), bodyPose.Solve().end());
    SkeletonPose shownPose(shown, 1.0F);
    std::vector<Transform> figure(shownPose.Solve().begin(), shownPose.Solve().end());
    const Vector3 hand = figure[kHand].position;

    FigureRetargeter{}.Run(shown, 1.0F, figure);

    const Vector3 pointing = Normalized(hand - kSocket, {});
    CheckNear(Apply(Compose(figure[kHand], Inverse(bind[kHand])), kWrist),
              kSocket + (pointing * Length(kWrist - kSocket)));
}

TEST_CASE("a skeleton without a rig keeps its own poses") {
    assets::SkeletonAsset skeleton = Figure();
    skeleton.rig.reset();
    const Quaternion raise = FromAxisAngle({.x = 1.0F}, 1.0F);

    const Posed posed = Pose(skeleton, kShoulder, raise);

    for (std::size_t node = 0; node < posed.figure.size(); ++node) {
        CheckNear(posed.figure[node].position, posed.retail[node].position);
    }
}

}
}
