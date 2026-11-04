#include "app/hud/powerup_panel.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::app {
namespace {

constexpr std::uint32_t kImageCount = 24;
constexpr std::uint32_t kFirstTexture = 100;
constexpr std::uint32_t kHolderSize = 64;
constexpr std::uint32_t kItemSize = 32;
constexpr std::uint32_t kBadgeSize = 16;

render::TextureId ImageTexture(std::uint32_t index) {
    return render::TextureId{index + kFirstTexture};
}

std::uint32_t ImageSize(std::uint32_t index) {
    if (index == 11 || index == 12) {
        return kHolderSize;
    }
    return index >= 13 && index <= 15 ? kBadgeSize : kItemSize;
}

std::vector<UiImage> PanelImages() {
    std::vector<UiImage> images;
    images.reserve(kImageCount);
    for (std::uint32_t i = 0; i < kImageCount; ++i) {
        images.push_back(
            {.texture = ImageTexture(i), .widthPx = ImageSize(i), .heightPx = ImageSize(i)});
    }
    return images;
}

std::vector<render::SpriteDraw> Layout(const sim::RacerSnapshot& racer,
                                       const std::vector<UiImage>& images) {
    std::vector<render::SpriteDraw> out;
    LayoutPowerupPanel(racer, sim::kOriginalSlots, images, out);
    return out;
}

TEST_CASE("the power-up panel is hidden while nothing is held") {
    CHECK(Layout({}, PanelImages()).empty());
}

TEST_CASE("the power-up panel shows the held colour's item and white level") {
    const std::vector<render::SpriteDraw> out =
        Layout({.heldColour = assets::BrickColour::Red, .whiteCount = 2}, PanelImages());

    REQUIRE(out.size() == 4);
    CHECK(out[0].texture == ImageTexture(11));
    CHECK(out[0].bounds.x == doctest::Approx(20.0F));
    CHECK(out[0].bounds.y == doctest::Approx(406.0F));
    CHECK(out[0].bounds.width == doctest::Approx(64.0F));
    CHECK(out[1].texture == ImageTexture(12));
    CHECK(out[1].tint.g == doctest::Approx(0.0F));
    CHECK(out[1].tint.b == doctest::Approx(0.0F));
    CHECK(out[2].texture == ImageTexture(4));
    CHECK(out[2].bounds.x == doctest::Approx(35.0F));
    CHECK(out[2].bounds.y == doctest::Approx(428.0F));
    CHECK(out[3].texture == ImageTexture(14));
    CHECK(out[3].bounds.x == doctest::Approx(44.0F));
    CHECK(out[3].bounds.y == doctest::Approx(406.0F));
}

TEST_CASE("a split-screen panel sits at the bottom of its player's half") {
    std::vector<render::SpriteDraw> out;

    LayoutPowerupPanel({.heldColour = assets::BrickColour::Red}, sim::kOriginalSlots, PanelImages(),
                       out, 240.0F);

    REQUIRE_FALSE(out.empty());
    CHECK(out[0].bounds.y == doctest::Approx(166.0F));
}

TEST_CASE("each colour picks its own items and tint") {
    const std::vector<UiImage> images = PanelImages();

    const std::vector<render::SpriteDraw> yellow =
        Layout({.heldColour = assets::BrickColour::Yellow}, images);
    const std::vector<render::SpriteDraw> blue =
        Layout({.heldColour = assets::BrickColour::Blue, .whiteCount = 3}, images);
    const std::vector<render::SpriteDraw> green =
        Layout({.heldColour = assets::BrickColour::Green, .whiteCount = 1}, images);

    REQUIRE(yellow.size() == 3);
    CHECK(yellow[2].texture == ImageTexture(6));
    CHECK(yellow[1].tint.b == doctest::Approx(0.0F));
    REQUIRE(blue.size() == 4);
    CHECK(blue[2].texture == ImageTexture(19));
    CHECK(blue[3].texture == ImageTexture(15));
    CHECK(blue[1].tint.r == doctest::Approx(80.0F / 255.0F));
    REQUIRE(green.size() == 4);
    CHECK(green[2].texture == ImageTexture(21));
    CHECK(green[1].tint.r == doctest::Approx(0.0F));
}

TEST_CASE("a remapped slot shows its power-up's original icon in the brick's tint") {
    sim::PowerupSlots slots = sim::kOriginalSlots;
    slots[static_cast<std::size_t>(assets::BrickColour::Yellow)][3] = sim::PowerupKind::Lightning;
    slots[static_cast<std::size_t>(assets::BrickColour::Blue)][1] = sim::PowerupKind::Turbo;
    std::vector<render::SpriteDraw> yellow;
    std::vector<render::SpriteDraw> blue;

    LayoutPowerupPanel({.heldColour = assets::BrickColour::Yellow, .whiteCount = 3}, slots,
                       PanelImages(), yellow);
    LayoutPowerupPanel({.heldColour = assets::BrickColour::Blue, .whiteCount = 1}, slots,
                       PanelImages(), blue);

    REQUIRE(yellow.size() == 4);
    CHECK(yellow[2].texture == ImageTexture(4));
    CHECK(yellow[1].tint.g == doctest::Approx(1.0F));
    REQUIRE(blue.size() == 4);
    CHECK(blue[2].texture == ImageTexture(21));
}

TEST_CASE("white bricks alone show an untinted holder and the level") {
    const std::vector<render::SpriteDraw> out = Layout({.whiteCount = 1}, PanelImages());

    REQUIRE(out.size() == 3);
    CHECK(out[1].tint.r == doctest::Approx(1.0F));
    CHECK(out[1].tint.b == doctest::Approx(1.0F));
    CHECK(out[2].texture == ImageTexture(13));
}

TEST_CASE("the power-up panel is hidden without its images") {
    std::vector<UiImage> images = PanelImages();
    images.pop_back();

    CHECK(Layout({.heldColour = assets::BrickColour::Red}, images).empty());
}

}
}
