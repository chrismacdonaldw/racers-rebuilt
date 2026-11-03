#include "app/ui_text/ui_pixels.hpp"

#include <utility>

#include "assets/png_image.hpp"

namespace rr::app {
namespace {

Result<assets::ImageAsset> Decode(const assets::FileData& file, assets::TextureFileKind kind) {
    return assets::DecodeImage(file.Bytes(), kind);
}

}

Result<UiPixels> ReadUiPixels(const assets::Vfs& vfs, std::string_view path,
                              assets::TextureFileKind kind) {
    const Result<assets::FileData> file = vfs.Read(path);
    if (!file) {
        return std::unexpected(file.error());
    }
    Result<assets::ImageAsset> pixels = Decode(*file, kind);
    if (!pixels) {
        return std::unexpected(std::move(pixels.error()));
    }
    UiPixels read{.pixels = std::move(*pixels), .original = std::nullopt};
    if (!assets::IsPng(file->Bytes())) {
        return read;
    }
    Result<assets::ImageAsset> original = vfs.ReadOriginal(path).and_then(
        [&](const assets::FileData& data) { return Decode(data, kind); });
    if (!original) {
        return std::unexpected(std::move(original.error()));
    }
    read.original = std::move(*original);
    return read;
}

}
