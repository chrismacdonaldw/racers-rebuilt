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

}
}
