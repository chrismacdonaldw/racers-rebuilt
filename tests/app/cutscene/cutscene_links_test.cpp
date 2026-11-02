#include "app/cutscene/cutscene_links.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <vector>

#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"
#include "core/vector3.hpp"

namespace rr::app {
namespace {

using assets::EffectKind;

assets::CutsceneFrame Frame() {
    return {.transforms = {{.name = "camera"},
                           {.name = "snow",
                            .position = {.x = 1.0F, .y = 2.0F, .z = 3.0F},
                            .direction = {.y = 1.0F},
                            .up = {.z = 1.0F}}}};
}

constexpr std::array<CutsceneEventChange, 1> Changed(std::size_t transform, bool begins) {
    return {
        {{.event = {.kind = CutsceneEventKind::Transform, .index = transform}, .begins = begins}}};
}

TEST_CASE("a transform's begin and end fire their links where the transform is") {
    const std::vector<assets::CutsceneLink> links{
        {.transform = "snow", .kind = EffectKind::Particle, .effect = 1},
        {.transform = "snow", .kind = EffectKind::Sound, .effect = 0},
        {.transform = "snow", .onTransformEnd = true, .kind = EffectKind::Sound, .starts = false},
        {.transform = "camera", .kind = EffectKind::Fade},
    };
    std::vector<EffectCommand> commands;

    DispatchLinks(Frame(), links, Changed(1, true), commands);

    const assets::CutsceneAxes axes{.direction = {.y = 1.0F}, .up = {.z = 1.0F}};
    const Vector3 position{.x = 1.0F, .y = 2.0F, .z = 3.0F};
    CHECK(commands ==
          std::vector<EffectCommand>{
              {.kind = EffectKind::Particle, .effect = 1, .position = position, .axes = axes},
              {.kind = EffectKind::Sound, .position = position, .axes = axes}});

    commands.clear();
    DispatchLinks(Frame(), links, Changed(1, false), commands);
    CHECK(commands ==
          std::vector<EffectCommand>{
              {.kind = EffectKind::Sound, .starts = false, .position = position, .axes = axes}});
}

TEST_CASE("only transform events fire links") {
    const std::vector<assets::CutsceneLink> links{
        {.transform = "camera", .kind = EffectKind::Fade}};
    const std::array changes{CutsceneEventChange{
        .event = {.kind = CutsceneEventKind::Camera, .index = 0}, .begins = true}};
    std::vector<EffectCommand> commands;

    DispatchLinks(Frame(), links, changes, commands);

    CHECK(commands.empty());
}

}
}
