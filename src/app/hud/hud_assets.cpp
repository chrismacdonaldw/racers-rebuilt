#include "app/hud/hud_assets.hpp"

#include <format>
#include <span>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/string_table.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kGameStrings = "GAME.SRF";
constexpr std::string_view kDriverNames = "DRIVERS.SRF";
constexpr std::string_view kImageList = "LEGOIMGS.IDB";
constexpr std::string_view kTextFont = "font_ths";
constexpr std::string_view kNumberFont = "ignum";

Result<std::vector<std::u16string>> ReadStrings(const assets::Vfs& vfs, std::string_view folder,
                                                std::string_view name) {
    return vfs.Read(assets::JoinGamePath(folder, name)).and_then([](const assets::FileData& file) {
        return assets::ParseStringTable(file.Bytes());
    });
}

}

std::string HudFolder(std::string_view language) {
    return assets::JoinGamePath(kCommonFolder, language);
}

Result<HudLanguage> LoadHudLanguage(const assets::Vfs& vfs, std::string_view language) {
    const std::string folder = HudFolder(language);
    Result<HudStrings> strings =
        ReadStrings(vfs, folder, kGameStrings)
            .and_then([](const std::vector<std::u16string>& game) { return ReadHudStrings(game); });
    Result<std::vector<std::u16string>> names = ReadStrings(vfs, folder, kDriverNames);
    if (!strings || !names) {
        return std::unexpected(std::move(strings ? names.error() : strings.error()));
    }
    return HudLanguage{.strings = std::move(*strings), .driverNames = std::move(*names)};
}

Result<HudFonts> FindHudFonts(const UiFonts& fonts) {
    const HudFonts found{.text = FindUiFont(fonts, kTextFont),
                         .numbers = FindUiFont(fonts, kNumberFont)};
    if (found.text == nullptr || found.numbers == nullptr) {
        return Fail(ErrorCode::NotFound,
                    std::format("the HUD needs the {} and {} fonts", kTextFont, kNumberFont));
    }
    return found;
}

Result<std::vector<UiImage>> LoadHudImages(render::Renderer& renderer, const assets::Vfs& vfs) {
    return LoadUiImages(renderer, vfs, kCommonFolder, kImageList);
}

}
