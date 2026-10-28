#include "assets/bmp_image.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <utility>
#include <vector>

#include "assets/byte_reader.hpp"
#include "assets/little_endian.hpp"
#include "assets/lzss.hpp"

namespace rr::assets {
namespace {

constexpr std::uint8_t kDepthMask = 0x3C;
constexpr std::uint8_t kNoPaletteFlag = 0x80;
constexpr std::uint16_t kWindowsMagic = 0x4D42;
constexpr std::size_t kWindowsPixelOffsetAt = 10;
constexpr std::size_t kWindowsInfoAt = 14;
constexpr std::size_t kWindowsWidthAt = 18;
constexpr std::size_t kWindowsHeightAt = 22;
constexpr std::size_t kWindowsDepthAt = 28;
constexpr std::size_t kWindowsCompressionAt = 30;
constexpr std::size_t kWindowsColorsUsedAt = 46;
constexpr std::uint32_t kMinInfoBytes = 40;
constexpr std::uint32_t kMaxPaletteColors = 256;
constexpr std::uint32_t kMaxDimensionPx = 0xFFFF;
constexpr std::size_t kWindowsRowAlignment = 4;
constexpr std::size_t kBytesPerPixel = 4;
constexpr std::uint8_t kOpaque = 0xFF;
constexpr std::uint8_t kNibbleMask = 0x0F;
constexpr int kNibbleBits = 4;
constexpr std::uint32_t kBitsPerByte = 8;

using Rgba = std::array<std::byte, kBytesPerPixel>;

struct Layout {
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    std::uint32_t bitsPerPixel = 0;
    std::vector<Rgba> palette;
};

struct Rows {
    std::span<const std::byte> bytes;
    std::size_t strideBytes = 0;
    bool isBottomFirst = false;
};

Result<void> CheckLayout(const Layout& layout) {
    if (layout.widthPx == 0 || layout.heightPx == 0) {
        return Fail(ErrorCode::InvalidData, "BMP has no pixels");
    }
    if (layout.widthPx > kMaxDimensionPx || layout.heightPx > kMaxDimensionPx) {
        return Fail(ErrorCode::InvalidData, std::format("BMP of {} by {} pixels is too large",
                                                        layout.widthPx, layout.heightPx));
    }
    switch (layout.bitsPerPixel) {
        case 4:
        case 8:
        case 24:
        case 32:
            return {};
        default:
            return Fail(ErrorCode::InvalidData,
                        std::format("unsupported BMP depth of {} bits", layout.bitsPerPixel));
    }
}

std::size_t PackedRowBytes(const Layout& layout) {
    return ((std::size_t{layout.widthPx} * layout.bitsPerPixel) + kBitsPerByte - 1) / kBitsPerByte;
}

Result<std::vector<Rgba>> ReadPalette(ByteReader& reader, std::size_t count,
                                      std::size_t entryBytes) {
    return reader.ReadBytes(count * entryBytes).transform([&](std::span<const std::byte> bgr) {
        std::vector<Rgba> palette(count);
        for (std::size_t i = 0; i < count; ++i) {
            const std::span<const std::byte> entry = bgr.subspan(i * entryBytes);
            palette[i] = {entry[2], entry[1], entry[0], std::byte{kOpaque}};
        }
        return palette;
    });
}

std::uint8_t PaletteIndex(const Layout& layout, std::span<const std::byte> row, std::size_t x) {
    const auto packed = std::to_integer<std::uint8_t>(row[x * layout.bitsPerPixel / kBitsPerByte]);
    if (layout.bitsPerPixel == 8) {
        return packed;
    }
    return x % 2 == 0 ? packed >> kNibbleBits : packed & kNibbleMask;
}

Result<Rgba> PixelAt(const Layout& layout, std::span<const std::byte> row, std::size_t x) {
    if (layout.bitsPerPixel >= 24) {
        const std::span<const std::byte> bgr = row.subspan(x * layout.bitsPerPixel / kBitsPerByte);
        const std::byte alpha = layout.bitsPerPixel == 32 ? bgr[3] : std::byte{kOpaque};
        return Rgba{bgr[2], bgr[1], bgr[0], alpha};
    }
    const std::uint8_t index = PaletteIndex(layout, row, x);
    if (index >= layout.palette.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("BMP palette index {} is past its {} colors", index,
                                layout.palette.size()));
    }
    return layout.palette[index];
}

Result<ImageAsset> ToRgba(const Layout& layout, const Rows& rows) {
    ImageAsset image{.widthPx = layout.widthPx, .heightPx = layout.heightPx};
    image.rgba.reserve(std::size_t{layout.widthPx} * layout.heightPx * kBytesPerPixel);
    for (std::size_t y = 0; y < layout.heightPx; ++y) {
        const std::size_t sourceRow = rows.isBottomFirst ? layout.heightPx - 1 - y : y;
        const std::span<const std::byte> row =
            rows.bytes.subspan(sourceRow * rows.strideBytes, PackedRowBytes(layout));
        for (std::size_t x = 0; x < layout.widthPx; ++x) {
            Result<Rgba> pixel = PixelAt(layout, row, x);
            if (!pixel) {
                return std::unexpected(std::move(pixel.error()));
            }
            image.rgba.insert(image.rgba.end(), pixel->begin(), pixel->end());
        }
    }
    return image;
}

Result<Layout> ReadGameHeader(ByteReader& reader) {
    Result<std::uint8_t> flags = reader.ReadU8();
    Result<std::uint8_t> lastColor = flags.and_then([&](auto) { return reader.ReadU8(); });
    Result<std::uint16_t> width = lastColor.and_then([&](auto) { return reader.ReadU16(); });
    Result<std::uint16_t> height = width.and_then([&](auto) { return reader.ReadU16(); });
    if (!height) {
        return std::unexpected(std::move(height.error()));
    }
    Layout layout{.widthPx = *width,
                  .heightPx = *height,
                  .bitsPerPixel = static_cast<std::uint32_t>(*flags & kDepthMask)};
    Result<void> checked = CheckLayout(layout);
    if (!checked) {
        return std::unexpected(std::move(checked.error()));
    }
    const bool hasPalette = layout.bitsPerPixel <= 8 && (*flags & kNoPaletteFlag) == 0;
    return ReadPalette(reader, hasPalette ? *lastColor + 1 : 0, 3)
        .transform([&](std::vector<Rgba> palette) {
            layout.palette = std::move(palette);
            return std::move(layout);
        });
}

Result<void> ReadChunk(ByteReader& reader, std::vector<std::byte>& rows, std::size_t totalBytes) {
    Result<std::uint16_t> unpackedSize = reader.ReadU16();
    Result<std::uint16_t> packedSize =
        unpackedSize.and_then([&](auto) { return reader.ReadU16(); });
    Result<std::span<const std::byte>> packed =
        packedSize.and_then([&](std::uint16_t size) { return reader.ReadBytes(size); });
    if (!packed) {
        return std::unexpected(std::move(packed.error()));
    }
    if (*unpackedSize == 0 || *unpackedSize > totalBytes - rows.size() ||
        *packedSize > *unpackedSize) {
        return Fail(ErrorCode::InvalidData,
                    std::format("bad BMP chunk of {} bytes packed into {} at offset {}",
                                *unpackedSize, *packedSize, reader.Position() - *packedSize));
    }
    const std::size_t start = rows.size();
    rows.resize(start + *unpackedSize);
    const std::span<std::byte> unpacked = std::span(rows).subspan(start);
    if (*packedSize == *unpackedSize) {
        std::ranges::copy(*packed, unpacked.begin());
        return {};
    }
    return DecodeLzss(*packed, unpacked);
}

Result<std::vector<std::byte>> ReadChunkedRows(ByteReader& reader, std::size_t totalBytes) {
    std::vector<std::byte> rows;
    while (rows.size() < totalBytes) {
        Result<void> chunk = ReadChunk(reader, rows, totalBytes);
        if (!chunk) {
            return std::unexpected(std::move(chunk.error()));
        }
    }
    return rows;
}

Result<ImageAsset> DecodeGameBmp(std::span<const std::byte> bytes) {
    ByteReader reader(bytes, 0);
    Result<Layout> layout = ReadGameHeader(reader);
    if (!layout) {
        return std::unexpected(std::move(layout.error()));
    }
    const std::size_t strideBytes = PackedRowBytes(*layout);
    return ReadChunkedRows(reader, strideBytes * layout->heightPx)
        .and_then([&](const std::vector<std::byte>& rows) {
            return ToRgba(*layout, {.bytes = rows, .strideBytes = strideBytes});
        });
}

struct WindowsInfo {
    std::uint32_t pixelOffset = 0;
    std::uint32_t infoBytes = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint16_t bitsPerPixel = 0;
    std::uint32_t compression = 0;
    std::uint32_t colorsUsed = 0;
};

Result<WindowsInfo> ReadWindowsInfo(std::span<const std::byte> bytes) {
    if (bytes.size() < kWindowsInfoAt + kMinInfoBytes) {
        return Fail(ErrorCode::InvalidData, "BMP header is cut short");
    }
    const WindowsInfo info{
        .pixelOffset = LoadU32(bytes, kWindowsPixelOffsetAt),
        .infoBytes = LoadU32(bytes, kWindowsInfoAt),
        .width = static_cast<std::int32_t>(LoadU32(bytes, kWindowsWidthAt)),
        .height = static_cast<std::int32_t>(LoadU32(bytes, kWindowsHeightAt)),
        .bitsPerPixel = LoadU16(bytes, kWindowsDepthAt),
        .compression = LoadU32(bytes, kWindowsCompressionAt),
        .colorsUsed = LoadU32(bytes, kWindowsColorsUsedAt),
    };
    if (info.infoBytes < kMinInfoBytes || info.compression != 0) {
        return Fail(ErrorCode::InvalidData, "only uncompressed Windows BMPs are supported");
    }
    return info;
}

std::uint32_t Magnitude(std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    return value < 0 ? 0U - bits : bits;
}

Result<ImageAsset> DecodeWindowsBmp(std::span<const std::byte> bytes) {
    Result<WindowsInfo> info = ReadWindowsInfo(bytes);
    if (!info) {
        return std::unexpected(std::move(info.error()));
    }
    Layout layout{.widthPx = info->width < 0 ? 0 : Magnitude(info->width),
                  .heightPx = Magnitude(info->height),
                  .bitsPerPixel = info->bitsPerPixel};
    Result<void> checked = CheckLayout(layout);
    const std::uint32_t paletteColors =
        info->colorsUsed != 0 ? info->colorsUsed : 1U << layout.bitsPerPixel;
    if (checked && layout.bitsPerPixel <= 8) {
        ByteReader reader(bytes, kWindowsInfoAt + info->infoBytes);
        checked = paletteColors > kMaxPaletteColors
                      ? Fail(ErrorCode::InvalidData, "BMP palette has over 256 colors")
                      : ReadPalette(reader, paletteColors, 4).transform([&](auto palette) {
                            layout.palette = std::move(palette);
                        });
    }
    if (!checked) {
        return std::unexpected(std::move(checked.error()));
    }
    const std::size_t strideBytes = (PackedRowBytes(layout) + kWindowsRowAlignment - 1) /
                                    kWindowsRowAlignment * kWindowsRowAlignment;
    ByteReader reader(bytes, info->pixelOffset);
    return reader.ReadBytes(strideBytes * layout.heightPx)
        .and_then([&](std::span<const std::byte> rows) {
            return ToRgba(
                layout,
                {.bytes = rows, .strideBytes = strideBytes, .isBottomFirst = info->height > 0});
        });
}

}

Result<ImageAsset> DecodeBmp(std::span<const std::byte> bytes) {
    const bool isWindows = bytes.size() >= 2 && LoadU16(bytes, 0) == kWindowsMagic;
    return isWindows ? DecodeWindowsBmp(bytes) : DecodeGameBmp(bytes);
}

}
