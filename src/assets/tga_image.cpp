#include "assets/tga_image.hpp"

#include <cstdint>
#include <format>
#include <utility>

#include "assets/byte_reader.hpp"
#include "assets/little_endian.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kHeaderBytes = 18;
constexpr std::size_t kIdLengthOffset = 0;
constexpr std::size_t kColorMapTypeOffset = 1;
constexpr std::size_t kImageTypeOffset = 2;
constexpr std::size_t kWidthOffset = 12;
constexpr std::size_t kHeightOffset = 14;
constexpr std::size_t kDepthOffset = 16;
constexpr std::size_t kDescriptorOffset = 17;
constexpr std::uint8_t kTrueColorType = 2;
constexpr std::uint8_t kTopOriginFlag = 0x20;
constexpr std::size_t kBytesPerPixel = 4;
constexpr std::uint8_t kOpaque = 0xFF;
constexpr std::uint32_t kBitsPerByte = 8;

struct Header {
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    std::uint32_t bytesPerPixel = 0;
    bool isTopRowFirst = false;
    std::size_t idBytes = 0;
};

std::uint8_t ByteAt(std::span<const std::byte> header, std::size_t offset) {
    return std::to_integer<std::uint8_t>(header[offset]);
}

Result<Header> ReadHeader(std::span<const std::byte> bytes) {
    const std::span<const std::byte> raw = bytes.first(kHeaderBytes);
    const std::uint8_t imageType = ByteAt(raw, kImageTypeOffset);
    const std::uint8_t depth = ByteAt(raw, kDepthOffset);
    if (imageType != kTrueColorType || ByteAt(raw, kColorMapTypeOffset) != 0 ||
        (depth != 24 && depth != 32)) {
        return Fail(
            ErrorCode::InvalidData,
            std::format("unsupported TGA image type {} at {} bits per pixel", imageType, depth));
    }
    const Header header{
        .widthPx = LoadU16(raw, kWidthOffset),
        .heightPx = LoadU16(raw, kHeightOffset),
        .bytesPerPixel = depth / kBitsPerByte,
        .isTopRowFirst = (ByteAt(raw, kDescriptorOffset) & kTopOriginFlag) != 0,
        .idBytes = ByteAt(raw, kIdLengthOffset),
    };
    if (header.widthPx == 0 || header.heightPx == 0) {
        return Fail(ErrorCode::InvalidData, "TGA has no pixels");
    }
    return header;
}

ImageAsset ToRgba(const Header& header, std::span<const std::byte> pixels) {
    ImageAsset image{.widthPx = header.widthPx, .heightPx = header.heightPx};
    image.rgba.reserve(std::size_t{header.widthPx} * header.heightPx * kBytesPerPixel);
    const std::size_t rowBytes = std::size_t{header.widthPx} * header.bytesPerPixel;
    for (std::size_t y = 0; y < header.heightPx; ++y) {
        const std::size_t sourceRow = header.isTopRowFirst ? y : header.heightPx - 1 - y;
        const std::span<const std::byte> row = pixels.subspan(sourceRow * rowBytes, rowBytes);
        for (std::size_t x = 0; x < header.widthPx; ++x) {
            const std::span<const std::byte> bgr = row.subspan(x * header.bytesPerPixel);
            const std::byte alpha = header.bytesPerPixel == 4 ? bgr[3] : std::byte{kOpaque};
            image.rgba.insert(image.rgba.end(), {bgr[2], bgr[1], bgr[0], alpha});
        }
    }
    return image;
}

}

Result<ImageAsset> DecodeTga(std::span<const std::byte> bytes) {
    if (bytes.size() < kHeaderBytes) {
        return Fail(ErrorCode::InvalidData, "TGA header is cut short");
    }
    Result<Header> header = ReadHeader(bytes);
    if (!header) {
        return std::unexpected(std::move(header.error()));
    }
    ByteReader reader(bytes, kHeaderBytes + header->idBytes);
    const std::size_t pixelBytes =
        std::size_t{header->widthPx} * header->heightPx * header->bytesPerPixel;
    return reader.ReadBytes(pixelBytes).transform([&](std::span<const std::byte> pixels) {
        return ToRgba(*header, pixels);
    });
}

}
