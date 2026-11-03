#include "assets/png_image.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <vector>

#include <stb_image.h>

namespace rr::assets {
namespace {

constexpr std::array kSignature{std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
                                std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A}};
constexpr int kRgbaChannels = 4;

struct FreePixels {
    void operator()(stbi_uc* pixels) const { stbi_image_free(pixels); }
};

}

bool IsPng(std::span<const std::byte> bytes) {
    return bytes.size() >= kSignature.size() &&
           std::ranges::equal(bytes.first(kSignature.size()), kSignature);
}

Result<ImageAsset> DecodePng(std::span<const std::byte> bytes) {
    if (!IsPng(bytes) || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return Fail(ErrorCode::InvalidData, "not a PNG image");
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    const std::unique_ptr<stbi_uc, FreePixels> pixels(stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width,
        &height, &channels, kRgbaChannels));
    if (!pixels) {
        return Fail(ErrorCode::InvalidData,
                    std::format("the PNG doesn't decode: {}", stbi_failure_reason()));
    }
    const std::size_t byteCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kRgbaChannels;
    const auto* first = reinterpret_cast<const std::byte*>(pixels.get());
    return ImageAsset{.widthPx = static_cast<std::uint32_t>(width),
                      .heightPx = static_cast<std::uint32_t>(height),
                      .rgba = std::vector<std::byte>(first, first + byteCount)};
}

}
