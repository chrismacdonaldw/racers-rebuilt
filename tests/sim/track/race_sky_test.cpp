#include "sim/track/race_sky.hpp"

#include <doctest/doctest.h>

#include "assets/event_table.hpp"
#include "assets/sky_file.hpp"

namespace rr::sim {
namespace {

constexpr assets::Rgb8 kBlack{0, 0, 0};
constexpr assets::Rgb8 kGrey{200, 100, 50};

assets::SkyKeyframe Solid(const assets::Rgb8& color, Milliseconds length = Milliseconds{1000}) {
    return {.length = length, .horizon = color, .middle = color, .top = color};
}

assets::SkyFile TwoSkies() {
    return {.skies = {{.name = "daylight",
                       .keyframes = {{.horizon = {250, 250, 120},
                                      .middle = {100, 100, 255},
                                      .top = {25, 25, 255}}}},
                      {.name = "flash", .keyframes = {Solid(kBlack)}}}};
}

TEST_CASE("the sky starts on its initial colours") {
    const assets::SkyFile file = TwoSkies();
    const RaceSky sky(file);

    CHECK(sky.Colors() ==
          SkyColors{.horizon = {250, 250, 120}, .middle = {100, 100, 255}, .top = {25, 25, 255}});
    CHECK(sky.ShowsDome());
    CHECK(sky.ShowsWorld());
}

TEST_CASE("a fade blends from the old sky and truncates each channel") {
    const assets::SkyFile file = TwoSkies();
    RaceSky sky(file);

    sky.FadeTo("FLASH", Milliseconds{100});
    sky.Tick(Milliseconds{25});

    CHECK(sky.Colors().horizon == assets::Rgb8{187, 187, 90});
    CHECK(sky.Colors().top == assets::Rgb8{18, 18, 191});
}

TEST_CASE("a fade ends once its time has passed") {
    const assets::SkyFile file = TwoSkies();
    RaceSky sky(file);

    sky.FadeTo("flash", Milliseconds{100});
    sky.Tick(Milliseconds{50});
    CHECK(sky.Colors().horizon != kBlack);
    sky.Tick(Milliseconds{51});

    CHECK(sky.Colors() == SkyColors{.horizon = kBlack, .middle = kBlack, .top = kBlack});
}

TEST_CASE("an unknown sky changes nothing") {
    const assets::SkyFile file = TwoSkies();
    RaceSky sky(file);

    sky.FadeTo("castle", Milliseconds{100});
    sky.Tick(Milliseconds{10});

    CHECK(sky.Colors().top == assets::Rgb8{25, 25, 255});
}

TEST_CASE("fading to the current sky mid-fade jumps to it") {
    const assets::SkyFile file = TwoSkies();
    RaceSky sky(file);

    sky.FadeTo("flash", Milliseconds{100});
    sky.Tick(Milliseconds{10});
    sky.FadeTo("flash", Milliseconds{100});
    sky.Tick(Milliseconds{10});

    CHECK(sky.Colors().horizon == kBlack);
}

TEST_CASE("a cycling sky blends towards its next keyframe and wraps") {
    const assets::SkyFile file{.skies = {{.name = "cycle",
                                          .keyframes = {Solid(kBlack, Milliseconds{100}),
                                                        Solid(kGrey, Milliseconds{100})}}}};
    RaceSky sky(file);

    sky.Tick(Milliseconds{50});
    CHECK(sky.Colors().horizon == assets::Rgb8{100, 50, 25});
    sky.Tick(Milliseconds{60});
    CHECK(sky.Colors().horizon == kGrey);
    sky.Tick(Milliseconds{101});
    CHECK(sky.Colors().horizon == kBlack);
}

TEST_CASE("hiding the dome hides the sky world too") {
    const assets::SkyFile file = TwoSkies();
    RaceSky sky(file);

    sky.Apply({.hidesDome = true});
    CHECK_FALSE(sky.ShowsDome());
    CHECK_FALSE(sky.ShowsWorld());

    sky.Apply({.showsDome = true, .hidesSkyWorld = true});
    CHECK(sky.ShowsDome());
    CHECK_FALSE(sky.ShowsWorld());
}

TEST_CASE("a state that shows and hides at once hides") {
    const assets::SkyFile file = TwoSkies();
    RaceSky sky(file);

    sky.Apply({.hidesDome = true, .showsDome = true});

    CHECK_FALSE(sky.ShowsDome());
}

TEST_CASE("a state fades to its named sky") {
    const assets::SkyFile file = TwoSkies();
    RaceSky sky(file);

    sky.Apply({.sky = "flash", .fade = Milliseconds{0}});
    sky.Tick(Milliseconds{1});

    CHECK(sky.Colors().top == kBlack);
}

TEST_CASE("a sky file without skies shows nothing") {
    const assets::SkyFile file;
    RaceSky sky(file);

    sky.FadeTo("daylight", Milliseconds{0});
    sky.Tick(Milliseconds{10});

    CHECK_FALSE(sky.ShowsDome());
}

}
}
