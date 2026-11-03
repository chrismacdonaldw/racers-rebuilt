#include "app/hq_textures.hpp"

#include <array>
#include <utility>

namespace rr::app {
namespace {

constexpr std::array kImageExtensions{std::string_view(".BMP"), std::string_view(".TGA")};
constexpr std::string_view kPngExtension = ".PNG";

}

std::optional<std::string> PngSibling(std::string_view path) {
    for (const std::string_view extension : kImageExtensions) {
        if (path.ends_with(extension)) {
            path.remove_suffix(extension.size());
            return std::string(path).append(kPngExtension);
        }
    }
    return std::nullopt;
}

void HqTextures::DeclareSettings(settings::Registry& registry) const {
    registry.Declare(kHqTextures);
}

void HqTextures::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    hooks.Subscribe<mods::AssetResolving>([&settings](mods::AssetResolving& event) {
        if (!settings.Get(kHqTextures)) {
            return;
        }
        std::optional<std::string> png = PngSibling(event.request.path);
        if (png && event.vfs.Contains(*png)) {
            event.request.path = std::move(*png);
        }
    });
}

}
