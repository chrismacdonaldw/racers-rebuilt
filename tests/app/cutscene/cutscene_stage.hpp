#pragma once

#include <array>
#include <numbers>
#include <vector>

#include "app/track_model.hpp"
#include "assets/world_database.hpp"
#include "core/quaternion.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app::test {

inline sim::TrackEntitySources StageEntities() {
    return {
        .skeletons = {{.nodes = {{.name = "pivot"},
                                 {.name = "tip", .position = {.x = 10.0F}, .parent = 0}}}},
        .animations =
            {{.positionKeys = {},
              .rotationKeys = {Quaternion{},
                               FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F)},
              .keyFrames = {0, 10},
              .tracks = {{.rotations = {.firstValue = 0, .firstFrame = 0, .count = 2}}, {}},
              .parts =
                  {{.name = "swing", .frameCount = 21, .loopFrames = 22, .framesPerSecond = 100}}}},
        .entities = {{.name = "guy1", .startPart = 0}, {.name = "Camera01"}},
    };
}

inline assets::WorldAsset StageWorld() {
    return {.modelEntities = {{.name = "set"}},
            .animatedEntities = {{.name = "guy1"}, {.name = "Camera01"}},
            .sprites = {{.materialName = "star",
                         .position = {.x = 7.0F},
                         .width = 4.0F,
                         .height = 2.0F,
                         .maxDistance = 50.0F}},
            .cameras = {{.name = "Camera01",
                         .trackedEntity = 1,
                         .trackedNode = 1,
                         .nearClip = 2.0F,
                         .farClip = 300.0F,
                         .fovDegrees = 50.0F},
                        {.name = "wide",
                         .position = {.x = -5.0F},
                         .forward = {.y = 1.0F},
                         .target = Vector3{}}}};
}

inline std::vector<NamedDraws> StageStatics() {
    return {{.entity = "SET", .span = {.first = 0, .count = 2}}};
}

}
