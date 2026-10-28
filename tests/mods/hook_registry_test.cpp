#include "mods/hook_registry.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rr::mods {
namespace {

TEST_CASE("handlers run in subscription order and see earlier changes") {
    HookRegistry hooks;
    std::vector<float> seen;
    hooks.Subscribe<FrameRendering>([&](FrameRendering& frame) {
        seen.push_back(frame.camera.position.x);
        frame.camera.position.x = 1.0F;
    });
    hooks.Subscribe<FrameRendering>([&](FrameRendering& frame) {
        seen.push_back(frame.camera.position.x);
        frame.camera.position.x = 2.0F;
    });
    Transform camera;

    hooks.Emit(FrameRendering{.camera = camera, .elapsed = FloatSeconds{0.0}});

    CHECK(seen == std::vector<float>{0.0F, 1.0F});
    CHECK(camera.position.x == 2.0F);
}

TEST_CASE("an event only reaches handlers of its own type") {
    HookRegistry hooks;
    int started = 0;
    hooks.Subscribe<TickStarted>([&](const TickStarted&) { ++started; });

    hooks.Emit(TickEnded{.tick = 1});
    hooks.Emit(TickStarted{.tick = 2});

    CHECK(started == 1);
}

}
}
