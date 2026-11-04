#include "app/menu/builder_settings.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>

namespace rr::app {
namespace {

constexpr std::size_t kHexDigits = 6;
constexpr int kHexBase = 16;
constexpr unsigned kChannelBits = 8;
constexpr float kChannelMax = 255.0F;

float Channel(std::uint32_t rgb, unsigned shift) {
    return static_cast<float>((rgb >> shift) & 0xFFU) / kChannelMax;
}

}

std::optional<render::Color> ParseHexColour(std::string_view text) {
    if (text.size() != kHexDigits + 1 || text.front() != '#') {
        return std::nullopt;
    }
    std::uint32_t rgb = 0;
    const std::string_view digits = text.substr(1);
    const auto [end, error] =
        std::from_chars(digits.data(), digits.data() + digits.size(), rgb, kHexBase);
    if (error != std::errc{} || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return render::Color{
        .r = Channel(rgb, 2 * kChannelBits), .g = Channel(rgb, kChannelBits), .b = Channel(rgb, 0)};
}

}
