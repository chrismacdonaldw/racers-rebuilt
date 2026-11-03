#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "assets/image_asset.hpp"
#include "platform/image_file.hpp"

namespace rr::test {

inline constexpr std::size_t kTgaHeaderBytes = 18;

[[nodiscard]] inline assets::ImageAsset SolidImage(std::uint32_t widthPx, std::uint32_t heightPx,
                                                   std::byte alpha) {
    assets::ImageAsset image{.widthPx = widthPx, .heightPx = heightPx, .rgba = {}};
    for (std::uint32_t i = 0; i < widthPx * heightPx; ++i) {
        image.rgba.insert(image.rgba.end(),
                          {std::byte{0xFF}, std::byte{0x00}, std::byte{0xFF}, alpha});
    }
    return image;
}

inline void WritePng(const std::filesystem::path& path, const assets::ImageAsset& image) {
    std::filesystem::create_directories(path.parent_path());
    REQUIRE(platform::SavePng(path, image.widthPx, image.heightPx, image.rgba).has_value());
}

/// Uncompressed 32-bit, top row first.
[[nodiscard]] inline std::vector<std::byte> TgaBytes(const assets::ImageAsset& image) {
    std::vector<std::byte> bytes(kTgaHeaderBytes);
    bytes[2] = std::byte{2};
    bytes[12] = static_cast<std::byte>(image.widthPx);
    bytes[14] = static_cast<std::byte>(image.heightPx);
    bytes[16] = std::byte{32};
    bytes[17] = std::byte{0x28};
    for (std::size_t i = 0; i + 4 <= image.rgba.size(); i += 4) {
        bytes.insert(bytes.end(),
                     {image.rgba[i + 2], image.rgba[i + 1], image.rgba[i], image.rgba[i + 3]});
    }
    return bytes;
}

inline void WriteTga(const std::filesystem::path& path, const assets::ImageAsset& image) {
    std::filesystem::create_directories(path.parent_path());
    const std::vector<std::byte> bytes = TgaBytes(image);
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
}

[[nodiscard]] inline std::vector<std::byte> ReadFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    for (std::size_t i = 0; i < chars.size(); ++i) {
        bytes[i] = static_cast<std::byte>(chars[i]);
    }
    return bytes;
}

}
