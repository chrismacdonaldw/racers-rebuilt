#include "app/menu/menu_files.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/string_table.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kSharedStyles = "GSTYLES";
constexpr std::string_view kSharedImages = "GIMAGES";
constexpr std::string_view kMenuText = "MENUTEXT.SRF";

std::string MenuPath(std::string_view base, std::string_view extension) {
    return assets::JoinGamePath(kMenuFolder, std::format("{}.{}", base, extension));
}

template <typename Parse>
auto ReadFile(const assets::Vfs& vfs, std::string_view path, Parse parse) {
    return vfs.Read(path).and_then(
        [&](const assets::FileData& file) { return parse(file.Bytes()); });
}

template <typename T, typename Parse>
Result<void> ReadEach(const assets::Vfs& vfs, std::string_view screen, std::string_view shared,
                      std::string_view extension, Parse parse, std::vector<T>& out) {
    for (const std::string_view base : {shared, screen}) {
        Result<T> read = ReadFile(vfs, MenuPath(base, extension), parse);
        if (read) {
            out.push_back(std::move(*read));
        } else if (base == shared || read.error().code != ErrorCode::NotFound) {
            return std::unexpected(std::move(read.error()));
        }
    }
    return {};
}

bool IsSameName(std::string_view a, std::string_view b) {
    return assets::ToUpperAscii(a) == assets::ToUpperAscii(b);
}

template <typename Find>
auto FindInStyles(const std::vector<assets::MenuStyles>& styles, Find find)
    -> decltype(find(styles.front())) {
    for (const assets::MenuStyles& style : styles | std::views::reverse) {
        if (const auto* found = find(style)) {
            return found;
        }
    }
    return nullptr;
}

}

const assets::ImageStyle* MenuFiles::FindImageStyle(std::string_view name) const {
    return FindInStyles(styles,
                        [&](const assets::MenuStyles& style) { return style.FindImage(name); });
}

const assets::TextStyle* MenuFiles::FindTextStyle(std::string_view name) const {
    return FindInStyles(styles,
                        [&](const assets::MenuStyles& style) { return style.FindText(name); });
}

const assets::CarouselStyle* MenuFiles::FindCarouselStyle(std::string_view name) const {
    return FindInStyles(styles,
                        [&](const assets::MenuStyles& style) { return style.FindCarousel(name); });
}

const assets::SelectorStyle* MenuFiles::FindSelectorStyle(std::string_view name) const {
    return FindInStyles(styles,
                        [&](const assets::MenuStyles& style) { return style.FindSelector(name); });
}

const assets::GameButtonStyle* MenuFiles::FindGameButtonStyle(std::string_view name) const {
    return FindInStyles(
        styles, [&](const assets::MenuStyles& style) { return style.FindGameButton(name); });
}

const assets::ImageListEntry* MenuFiles::FindImage(std::string_view name) const {
    const auto image = std::ranges::find_if(
        images | std::views::reverse,
        [&](const assets::ImageListEntry& entry) { return IsSameName(entry.name, name); });
    return image == images.rend() ? nullptr : &*image;
}

Result<MenuFiles> ReadMenuFiles(const assets::Vfs& vfs, std::string_view screen) {
    MenuFiles files;
    Result<assets::MenuLayout> layout =
        ReadFile(vfs, MenuPath(screen, "MIB"), assets::ParseMenuLayout);
    if (!layout) {
        return std::unexpected(std::move(layout.error()));
    }
    files.layout = std::move(*layout);
    std::vector<std::vector<assets::ImageListEntry>> imageLists;
    Result<void> read =
        ReadEach(vfs, screen, kSharedStyles, "MSB", assets::ParseMenuStyles, files.styles)
            .and_then([&] {
                return ReadEach(vfs, screen, kSharedImages, "IDB", assets::ParseImageList,
                                imageLists);
            });
    if (!read) {
        return std::unexpected(std::move(read.error()));
    }
    files.images = imageLists | std::views::join | std::ranges::to<std::vector>();
    return files;
}

Result<std::vector<std::u16string>> ReadMenuText(const assets::Vfs& vfs,
                                                 std::string_view language) {
    return ReadFile(vfs, assets::JoinGamePath(MenuLanguageFolder(language), kMenuText),
                    assets::ParseStringTable);
}

std::string MenuLanguageFolder(std::string_view language) {
    return assets::JoinGamePath(kMenuFolder, language);
}

}
