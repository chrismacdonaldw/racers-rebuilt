#include "sim/interpolation.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

EntitySnapshot EntityAt(std::uint32_t id, float x, SnapshotFlags flags = SnapshotFlags::None) {
    return {.id = EntityId{id}, .transform = {.position = {.x = x}}, .flags = flags};
}

TEST_CASE("positions blend by alpha") {
    const RenderSnapshot previous{.entities = {EntityAt(1, 0.0F)}};
    const RenderSnapshot current{.tick = 1, .entities = {EntityAt(1, 10.0F)}};
    RenderSnapshot out;

    Interpolate(previous, current, 0.25F, out);

    CHECK(out.tick == 1);
    REQUIRE(out.entities.size() == 1);
    CHECK(out.entities[0].transform.position.x == doctest::Approx(2.5F));
}

TEST_CASE("a teleported entity snaps to its current transform") {
    const RenderSnapshot previous{.entities = {EntityAt(1, 0.0F)}};
    const RenderSnapshot current{.entities = {EntityAt(1, 10.0F, SnapshotFlags::Teleported)}};
    RenderSnapshot out;

    Interpolate(previous, current, 0.5F, out);

    CHECK(out.entities[0].transform.position.x == 10.0F);
}

TEST_CASE("an entity missing from the previous tick uses its current transform") {
    const RenderSnapshot previous{.entities = {EntityAt(1, 0.0F)}};
    const RenderSnapshot current{.entities = {EntityAt(1, 4.0F), EntityAt(2, 8.0F)}};
    RenderSnapshot out;

    Interpolate(previous, current, 0.5F, out);

    REQUIRE(out.entities.size() == 2);
    CHECK(out.entities[0].transform.position.x == doctest::Approx(2.0F));
    CHECK(out.entities[1].transform.position.x == 8.0F);
}

TEST_CASE("a camera cut skips camera interpolation") {
    const RenderSnapshot previous{.camera = {.transform = {.position = {.y = 0.0F}}}};
    const RenderSnapshot current{
        .camera = {.transform = {.position = {.y = 6.0F}}, .flags = SnapshotFlags::Teleported}};
    RenderSnapshot out;

    Interpolate(previous, current, 0.5F, out);

    CHECK(out.camera.transform.position.y == 6.0F);
}

TEST_CASE("other players' cameras blend unless they cut") {
    const RenderSnapshot previous{.otherPlayerCameras = {{.transform = {.position = {.x = 0.0F}}},
                                                         {.transform = {.position = {.x = 0.0F}}}}};
    const RenderSnapshot current{.otherPlayerCameras = {{.transform = {.position = {.x = 4.0F}}},
                                                        {.transform = {.position = {.x = 4.0F}},
                                                         .flags = SnapshotFlags::Teleported}}};
    RenderSnapshot out;

    Interpolate(previous, current, 0.25F, out);

    REQUIRE(out.otherPlayerCameras.size() == 2);
    CHECK(out.otherPlayerCameras[0].transform.position.x == doctest::Approx(1.0F));
    CHECK(out.otherPlayerCameras[1].transform.position.x == 4.0F);
}

TEST_CASE("effects blend by slot unless they started this tick") {
    const RenderSnapshot previous{.effects = {{.slot = 3, .transform = {.position = {.x = 0.0F}}},
                                              {.slot = 4, .transform = {.position = {.x = 0.0F}}}}};
    const RenderSnapshot current{.effects = {{.slot = 4,
                                              .transform = {.position = {.x = 10.0F}},
                                              .flags = SnapshotFlags::Teleported},
                                             {.slot = 3, .transform = {.position = {.x = 10.0F}}}}};
    RenderSnapshot out;

    Interpolate(previous, current, 0.5F, out);

    REQUIRE(out.effects.size() == 2);
    CHECK(out.effects[0].transform.position.x == 10.0F);
    CHECK(out.effects[1].transform.position.x == doctest::Approx(5.0F));
}

TEST_CASE("an effect's time left blends within a phase and jumps to a new one") {
    const RenderSnapshot previous{.effects = {{.slot = 1, .phase = 2, .phaseLeftMs = 100.0F},
                                              {.slot = 2, .phase = 1, .phaseLeftMs = 10.0F}}};
    const RenderSnapshot current{.effects = {{.slot = 1, .phase = 2, .phaseLeftMs = 88.0F},
                                             {.slot = 2, .phase = 2, .phaseLeftMs = 1500.0F}}};
    RenderSnapshot out;

    Interpolate(previous, current, 0.5F, out);

    REQUIRE(out.effects.size() == 2);
    CHECK(out.effects[0].phaseLeftMs == doctest::Approx(94.0F));
    CHECK(out.effects[1].phaseLeftMs == 1500.0F);
}

}
}
