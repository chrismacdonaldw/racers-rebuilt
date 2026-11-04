#include "app/video_settings.hpp"

#include <doctest/doctest.h>

#include <array>

#include "app/startup_settings.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

settings::Registry VideoRegistry() {
    settings::Registry registry;
    DeclareVideoSettings(registry);
    return registry;
}

TEST_CASE("fresh settings pace with VSync") {
    CHECK(PacingFromSettings(VideoRegistry()).mode == platform::PresentMode::Vsync);
}

TEST_CASE("pacing round-trips through the settings") {
    settings::Registry registry = VideoRegistry();

    StorePacing(registry, {.mode = platform::PresentMode::Capped, .fpsCap = 60});
    const platform::PacingConfig capped = PacingFromSettings(registry);
    StorePacing(registry, {.mode = platform::PresentMode::Uncapped});
    const platform::PacingConfig uncapped = PacingFromSettings(registry);

    CHECK(capped.mode == platform::PresentMode::Capped);
    CHECK(capped.fpsCap == 60);
    CHECK(uncapped.mode == platform::PresentMode::Uncapped);
}

TEST_CASE("turning VSync back on keeps the cap for later") {
    settings::Registry registry = VideoRegistry();

    StorePacing(registry, {.mode = platform::PresentMode::Capped, .fpsCap = 90});
    StorePacing(registry, {.mode = platform::PresentMode::Vsync});

    CHECK(PacingFromSettings(registry).mode == platform::PresentMode::Vsync);
    CHECK(registry.Get(kFpsCapSetting) == 90);
}

TEST_CASE("HUD widths name the area the HUD keeps to") {
    CHECK_FALSE(HudAspect("Window").has_value());
    CHECK(HudAspect("21:9") == doctest::Approx(43.0F / 18.0F));
    CHECK(HudAspect("16:9") == doctest::Approx(16.0F / 9.0F));
}

TEST_CASE("command-line overrides reach the pacing") {
    const test::TempFolder folder;
    const std::array overrides{settings::SettingOverride{.key = "video.vsync", .value = "false"},
                               settings::SettingOverride{.key = "video.fps_cap", .value = "0"}};

    const Result<settings::Registry> registry =
        LoadStartupSettings(folder.Path() / "settings.toml", overrides, Extensions{});

    REQUIRE(registry.has_value());
    CHECK(PacingFromSettings(*registry).mode == platform::PresentMode::Uncapped);
}

}
}
