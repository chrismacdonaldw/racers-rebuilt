#pragma once

#include <doctest/doctest.h>

#include <cstdint>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "app/track_space.hpp"
#include "core/quaternion.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::test {

inline void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

[[nodiscard]] inline assets::AnimationPart Part(std::string name, std::uint32_t firstTrack) {
    return {.name = std::move(name),
            .firstTrack = firstTrack,
            .frameCount = 10,
            .loopFrames = 11,
            .framesPerSecond = 30,
            .velocity = {},
            .bounds = {}};
}

/// Part `frwrd` turns the axle, 64 units ahead of the root, a quarter turn about Y by frame 10.
[[nodiscard]] inline app::JointedAssets Wheels() {
    const assets::KeyRun turning{.firstValue = 0, .firstFrame = 0, .count = 2};
    return {
        .skeleton =
            {.nodes = {{.name = "root", .position = {}, .rotation = {}, .parent = {}},
                       {.name = "axle", .position = {.x = 64.0F}, .rotation = {}, .parent = 0}}},
        .animations = {.positionKeys = {},
                       .rotationKeys = {Quaternion{},
                                        FromAxisAngle({.y = 1.0F},
                                                      std::numbers::pi_v<float> / 2.0F)},
                       .keyFrames = {0, 10},
                       .tracks = {{}, {.rotations = turning, .positions = {}}, {}, {}},
                       .parts = {Part("frwrd", 0), Part("rvrse", 2)}},
    };
}

/// Part `steer-l` moves the pelvis 64 units along Y, and `power1` along Z.
[[nodiscard]] inline app::JointedAssets Driver() {
    return {
        .skeleton = {.nodes = {{.name = "alpelvis", .position = {}, .rotation = {}, .parent = {}}}},
        .animations = {.positionKeys = {{.y = 64.0F}, {.z = 64.0F}},
                       .rotationKeys = {},
                       .keyFrames = {},
                       .tracks = {{},
                                  {},
                                  {.rotations = {}, .positions = {.count = 1}},
                                  {.rotations = {}, .positions = {.firstValue = 1, .count = 1}}},
                       .parts = {Part("default", 0), Part("power1", 3), Part("steer-r", 1),
                                 Part("steer-l", 2)}},
    };
}

[[nodiscard]] inline app::ModelDraws Model(std::vector<std::uint16_t> joints) {
    return {.draws = std::vector<render::MeshDraw>(joints.size()),
            .joints = std::move(joints),
            .scale = 1.0F / 64.0F};
}

[[nodiscard]] inline Transform Placed(const render::MeshDraw& draw, std::uint16_t joint) {
    return joint < draw.joints.size() ? Compose(draw.transform, draw.joints[joint])
                                      : draw.transform;
}

[[nodiscard]] inline Vector3 GamePosition(const render::MeshDraw& draw, std::uint16_t joint = 0) {
    const Vector3 engine = Placed(draw, joint).position;
    return {.x = engine.x, .y = -engine.z, .z = engine.y};
}

[[nodiscard]] inline Vector3 GameAxis(const render::MeshDraw& draw, const Vector3& axis,
                                      std::uint16_t joint = 0) {
    const Vector3 engine = Rotate(Placed(draw, joint).rotation, app::ToEngine(axis));
    return {.x = engine.x, .y = -engine.z, .z = engine.y};
}

}
