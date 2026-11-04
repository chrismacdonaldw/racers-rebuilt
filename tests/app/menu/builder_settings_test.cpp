#include "app/menu/builder_settings.hpp"

#include <doctest/doctest.h>

#include <optional>

namespace rr::app {
namespace {

TEST_CASE("a hex colour reads as its channels, anything else as nothing") {
    const std::optional<render::Color> colour = ParseHexColour("#3A6EA5");

    REQUIRE(colour.has_value());
    CHECK(colour.value_or(render::Color{}).r == doctest::Approx(0x3A / 255.0F));
    CHECK(colour.value_or(render::Color{}).g == doctest::Approx(0x6E / 255.0F));
    CHECK(colour.value_or(render::Color{}).b == doctest::Approx(0xA5 / 255.0F));
    CHECK(colour.value_or(render::Color{}).a == doctest::Approx(1.0F));
    CHECK(ParseHexColour("3A6EA5") == std::nullopt);
    CHECK(ParseHexColour("#3A6EA") == std::nullopt);
    CHECK(ParseHexColour("#3A6EZ5") == std::nullopt);
    CHECK(ParseHexColour("#-A6EA5") == std::nullopt);
    CHECK(ParseHexColour(kBuilderColour.defaultValue).has_value());
}

}
}
