#include "app/ldraw_driver_prints.hpp"

#include <utility>

#include "assets/texture_database.hpp"
#include "core/log.hpp"
#include "platform/image_file.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPartFolder = "MENUDATA/PARTDB/";
constexpr std::string_view kImageExtension = ".BMP";

Result<std::vector<std::byte>> DrawPrint(assets::LdrawLibrary& library,
                                         const assets::LdrawPrint& print,
                                         const assets::FileData& file) {
    Result<assets::ImageAsset> torso =
        assets::DecodeImage(file.Bytes(), assets::TextureFileKind::Bmp);
    if (!torso) {
        return std::unexpected(torso.error());
    }
    return library.LoadPart(print.file).and_then([&](const assets::GeometryAsset& part) {
        const assets::ImageAsset image = assets::DrawTorsoPrint(*torso, part, kTorsoPrintSizePx);
        return platform::EncodePng(image.widthPx, image.heightPx, image.rgba);
    });
}

}

std::optional<assets::LdrawPrint> PrintOfImagePath(std::string_view path) {
    if (!path.starts_with(kPartFolder) || !path.ends_with(kImageExtension)) {
        return std::nullopt;
    }
    path.remove_prefix(kPartFolder.size());
    path.remove_suffix(kImageExtension.size());
    return assets::LdrawPrintFor(path);
}

void LdrawDriverPrints::DeclareSettings(settings::Registry& registry) const {
    registry.Declare(kLdrawPrintsSetting);
}

void LdrawDriverPrints::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    hooks.Subscribe<mods::AssetResolving>([this, &settings](mods::AssetResolving& event) {
        if (event.request.generated || !settings.Get(kLdrawPrintsSetting)) {
            return;
        }
        const std::optional<assets::LdrawPrint> print = PrintOfImagePath(event.request.path);
        if (!print) {
            return;
        }
        if (std::optional<std::vector<std::byte>> file =
                ImageFile(*print, event.vfs, event.request.path)) {
            event.request.generated = std::move(*file);
        }
    });
}

std::optional<std::vector<std::byte>> LdrawDriverPrints::ImageFile(const assets::LdrawPrint& print,
                                                                   const assets::Vfs& vfs,
                                                                   std::string_view path) {
    Result<std::vector<assets::FileData>> layers = vfs.ReadEveryLayer(path);
    if (!layers || layers->size() != 1) {
        return std::nullopt;
    }
    const std::scoped_lock lock(mutex_);
    const auto [found, isNew] = files_.try_emplace(std::string(print.torso));
    if (!isNew) {
        return found->second;
    }
    Result<std::vector<std::byte>> file = DrawPrint(library_, print, layers->front());
    if (!file) {
        log::Warn("Keeping torso {} because its LDraw print can't be drawn: {}", print.torso,
                  file.error().message);
        return std::nullopt;
    }
    found->second = std::move(*file);
    return found->second;
}

}
