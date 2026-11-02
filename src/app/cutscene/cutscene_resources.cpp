#include "app/cutscene/cutscene_resources.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "assets/game_path.hpp"
#include "assets/image_list.hpp"
#include "assets/install.hpp"
#include "assets/string_table.hpp"

namespace rr::app {
namespace {

using StringTable = std::vector<std::u16string>;

Result<StringTable> ReadTable(const assets::Vfs& vfs, std::string_view folder,
                              std::string_view name) {
    const std::string file = std::format("{}/{}.SRF", folder, assets::ToUpperAscii(name));
    return vfs.Read(file).and_then(
        [](const assets::FileData& data) { return assets::ParseStringTable(data.Bytes()); });
}

Result<std::vector<StringTable>> ReadTables(const assets::Vfs& vfs, std::string_view folder,
                                            std::string_view language,
                                            const assets::CutsceneEffectsAsset& effects) {
    std::vector<StringTable> tables;
    for (const std::string& name : effects.stringTables) {
        const bool isLocalized = tables.empty() && language != assets::kDefaultLanguage;
        Result<StringTable> table = ReadTable(vfs, folder, isLocalized ? language : name);
        if (!table && isLocalized) {
            table = ReadTable(vfs, folder, name);
        }
        if (!table) {
            return std::unexpected(std::move(table.error()));
        }
        tables.push_back(std::move(*table));
    }
    return tables;
}

Result<std::vector<OverlayText>> ResolveTexts(const assets::CutsceneEffectsAsset& effects,
                                              const std::vector<StringTable>& tables,
                                              const UiFonts& fonts) {
    std::vector<OverlayText> texts;
    for (const assets::CutsceneText& text : effects.texts) {
        if (text.stringTable >= tables.size() || text.line >= tables[text.stringTable].size()) {
            return Fail(ErrorCode::NotFound, std::format("text {} shows line {} of table {}, "
                                                         "which doesn't exist",
                                                         text.name, text.line, text.stringTable));
        }
        const UiFont* font = FindUiFont(fonts, text.font);
        if (font == nullptr) {
            return Fail(ErrorCode::NotFound, std::format("unable to find font {}", text.font));
        }
        texts.push_back({.text = tables[text.stringTable][text.line], .font = font});
    }
    return texts;
}

Result<std::vector<assets::ImageListEntry>> ReadImageLists(
    const assets::Vfs& vfs, std::string_view folder, const assets::CutsceneEffectsAsset& effects) {
    std::vector<assets::ImageListEntry> entries;
    for (const std::string& list : effects.imageLists) {
        Result<std::vector<assets::ImageListEntry>> read =
            vfs.Read(std::format("{}/{}.IDB", folder, assets::ToUpperAscii(list)))
                .and_then([](const assets::FileData& data) {
                    return assets::ParseImageList(data.Bytes());
                });
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
        entries.insert(entries.end(), read->begin(), read->end());
    }
    return entries;
}

Result<std::vector<UiImage>> LoadImages(render::Renderer& renderer, const assets::Vfs& vfs,
                                        std::string_view folder,
                                        const assets::CutsceneEffectsAsset& effects) {
    Result<std::vector<assets::ImageListEntry>> entries = ReadImageLists(vfs, folder, effects);
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    std::vector<UiImage> images;
    for (const assets::CutsceneImage& image : effects.images) {
        const auto entry = std::ranges::find_if(*entries, [&](const assets::ImageListEntry& e) {
            return assets::ToUpperAscii(e.name) == assets::ToUpperAscii(image.image);
        });
        if (entry == entries->end()) {
            return Fail(ErrorCode::NotFound, std::format("unable to find image {}", image.image));
        }
        Result<UiImage> loaded = LoadUiImage(renderer, vfs, folder, *entry);
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        images.push_back(*loaded);
    }
    return images;
}

}

Result<CutsceneResources> LoadCutsceneResources(render::Renderer& renderer, const assets::Vfs& vfs,
                                                std::string_view folder, std::string_view language,
                                                const assets::CutsceneEffectsAsset& effects) {
    CutsceneResources resources;
    if (!effects.texts.empty()) {
        Result<UiFonts> fonts =
            LoadUiFonts(renderer, vfs, MenuLanguageFolder(language), kMenuFontList);
        if (!fonts) {
            return std::unexpected(std::move(fonts.error()));
        }
        resources.fonts = std::move(*fonts);
    }
    Result<std::vector<OverlayText>> texts =
        ReadTables(vfs, folder, language, effects).and_then([&](const auto& tables) {
            return ResolveTexts(effects, tables, resources.fonts);
        });
    Result<std::vector<UiImage>> images =
        texts.and_then([&](const auto&) { return LoadImages(renderer, vfs, folder, effects); });
    if (!images) {
        return std::unexpected(std::move(images.error()));
    }
    resources.overlay = {.texts = std::move(*texts), .images = std::move(*images)};
    return resources;
}

}
