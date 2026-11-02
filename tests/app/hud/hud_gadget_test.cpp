#include "app/hud/hud_gadget.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "sprite_corners.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kImageCount = 11;
constexpr std::uint32_t kDialSize = 128;
constexpr std::uint32_t kCapSize = 16;
constexpr float kHubX = 586.0F;
constexpr float kHubY = 426.0F;
constexpr float kNeedleLength = 45.0F;

std::vector<UiImage> GadgetImages(std::uint32_t count = kImageCount) {
    std::vector<UiImage> images;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t size = i == 0 ? kDialSize : kCapSize;
        images.push_back({.texture = render::TextureId{i + 1}, .widthPx = size, .heightPx = size});
    }
    return images;
}

TrackMap Map() {
    return {.minX = -512.0F, .maxY = 512.0F, .maxX = 512.0F, .minY = -512.0F};
}

MapView View() {
    return {.playerForward = {.x = 1.0F}, .viewDirection = {.y = 1.0F}};
}

std::vector<render::SpriteDraw> Drawn(const HudGadget& gadget,
                                      const std::vector<UiImage>& images = GadgetImages()) {
    std::vector<render::SpriteDraw> out;
    gadget.Draw(View(), images, {}, out);
    return out;
}

HudGadget Speedometer() {
    HudGadget gadget(Map());
    gadget.Cycle();
    return gadget;
}

void CheckNeedleAt(const std::vector<render::SpriteDraw>& out, float angle) {
    REQUIRE(out.size() == 4);
    const render::SpriteCorner tip = test::CornersOf(out[2])[2];
    CHECK(tip.x == doctest::Approx(kHubX + (std::cos(angle) * kNeedleLength)));
    CHECK(tip.y == doctest::Approx(kHubY + (std::sin(angle) * kNeedleLength)));
}

}

TEST_CASE("a race starts on the fixed map and cycles through every gadget") {
    HudGadget gadget(Map());
    CHECK(gadget.Mode() == HudGadgetMode::FixedMap);

    gadget.Cycle();
    CHECK(gadget.Mode() == HudGadgetMode::Speedometer);
    gadget.Cycle();
    CHECK(gadget.Mode() == HudGadgetMode::RotatingMap);
    gadget.Cycle();
    CHECK(gadget.Mode() == HudGadgetMode::None);
    CHECK(Drawn(gadget).empty());
    gadget.Cycle();
    CHECK(gadget.Mode() == HudGadgetMode::FixedMap);
}

TEST_CASE("a track without a map draws neither map") {
    HudGadget gadget;
    CHECK(Drawn(gadget).empty());

    gadget.Cycle();
    gadget.Cycle();
    CHECK(gadget.Mode() == HudGadgetMode::RotatingMap);
    CHECK(Drawn(gadget).empty());
}

TEST_CASE("the gadget draws nothing while its images are missing") {
    CHECK(Drawn(Speedometer(), GadgetImages(kImageCount - 1)).empty());
}

TEST_CASE("the speedometer draws its dial, a two-tone needle and the hub cap") {
    const std::vector<render::SpriteDraw> out = Drawn(Speedometer());

    REQUIRE(out.size() == 4);
    CHECK(out[0].texture == render::TextureId{1});
    CHECK(out[0].bounds.x == 510.0F);
    CHECK(out[0].bounds.y == 350.0F);
    CHECK(out[1].texture == render::kWhiteTexture);
    CHECK(test::CornersOf(out[1])[2].tint.r == doctest::Approx(200.0F / 255.0F));
    CHECK(test::CornersOf(out[2])[0].tint.r == 1.0F);
    CHECK(out[3].texture == render::TextureId{2});
    CHECK(out[3].bounds.x == 578.0F);
    CHECK(out[3].bounds.y == 418.0F);
    CheckNeedleAt(out, 2.25F);
}

TEST_CASE("a split screen speedometer shrinks to 0.8 in its half's corner") {
    std::vector<render::SpriteDraw> out;

    Speedometer().Draw(View(), GadgetImages(), PlacementFor(0, 2), out);

    REQUIRE(out.size() == 4);
    CHECK(out[0].bounds.x == 536.0F);
    CHECK(out[0].bounds.y == 136.0F);
    CHECK(out[0].bounds.width == 102.0F);
    const render::SpriteCorner tip = test::CornersOf(out[2])[2];
    CHECK(tip.x == doctest::Approx(597.0F + (std::cos(2.25F) * 36.0F)));
    CHECK(tip.y == doctest::Approx(197.0F + (std::sin(2.25F) * 36.0F)));
    CHECK(out[3].bounds.width == 12.0F);
}

TEST_CASE("the needle eases a fifth of the way to the new speed each tick") {
    HudGadget gadget = Speedometer();

    gadget.Tick(0.1F);

    CheckNeedleAt(Drawn(gadget), 2.25F + (0.02F * 18.0F));
}

TEST_CASE("the needle stops at the end of the dial") {
    HudGadget gadget = Speedometer();
    for (int i = 0; i < 100; ++i) {
        gadget.Tick(1.0F);
    }

    CheckNeedleAt(Drawn(gadget), 2.25F + 3.4F);
}

TEST_CASE("the needle rests at zero when reversing") {
    HudGadget gadget = Speedometer();
    gadget.Tick(-0.1F);

    CheckNeedleAt(Drawn(gadget), 2.25F);
}

}
