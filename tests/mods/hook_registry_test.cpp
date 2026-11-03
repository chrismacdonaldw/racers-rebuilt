#include "mods/hook_registry.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "assets/vfs.hpp"

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

TEST_CASE("asset handlers chain and serve reads through the vfs") {
    HookRegistry hooks;
    hooks.Subscribe<AssetResolving>(
        [](AssetResolving& event) { event.request.path = "MENUDATA/LOGO.BMP"; });
    std::string seen;
    hooks.Subscribe<AssetResolving>([&seen](AssetResolving& event) {
        seen = event.request.path;
        event.request.generated = std::vector<std::byte>{std::byte{7}};
    });
    assets::Vfs vfs;
    ResolveThroughHooks(vfs, hooks);

    const Result<assets::FileData> data = vfs.Read("gamedata/a.txt");

    CHECK(seen == "MENUDATA/LOGO.BMP");
    REQUIRE(data.has_value());
    CHECK(data->Bytes().size() == 1);
    CHECK(data->Bytes()[0] == std::byte{7});
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
