#include "app/ui_text/ui_image.hpp"

#include <utility>

#include "assets/game_path.hpp"
#include "assets/texture_database.hpp"

namespace rr::app {

Result<UiImage> LoadUiImage(render::Renderer& renderer, const assets::Vfs& vfs,
                            std::string_view folder, const assets::ImageListEntry& entry) {
    Result<assets::ImageAsset> image =
        vfs.Read(assets::JoinGamePath(folder, entry.FileName()))
            .and_then([&](const assets::FileData& file) {
                return assets::DecodeImage(file.Bytes(), entry.fileKind);
            });
    if (!image) {
        return std::unexpected(std::move(image.error()));
    }
    if (entry.colorKey) {
        assets::ApplyColorKey(*image, *entry.colorKey);
    }
    return renderer.UploadTexture(*image).transform([&](render::TextureId texture) {
        return UiImage{.texture = texture, .widthPx = image->widthPx, .heightPx = image->heightPx};
    });
}

Result<std::vector<UiImage>> LoadUiImages(render::Renderer& renderer, const assets::Vfs& vfs,
                                          std::string_view folder, std::string_view imageList) {
    Result<std::vector<assets::ImageListEntry>> entries =
        vfs.Read(assets::JoinGamePath(folder, imageList))
            .and_then(
                [](const assets::FileData& file) { return assets::ParseImageList(file.Bytes()); });
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    std::vector<UiImage> images;
    images.reserve(entries->size());
    for (const assets::ImageListEntry& entry : *entries) {
        Result<UiImage> image = LoadUiImage(renderer, vfs, folder, entry);
        if (!image) {
            return std::unexpected(std::move(image.error()));
        }
        images.push_back(*image);
    }
    return images;
}

}
