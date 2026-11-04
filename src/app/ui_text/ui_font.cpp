#include "app/ui_text/ui_font.hpp"

#include <ranges>
#include <utility>
#include <vector>

#include "app/ui_text/font_list_fixes.hpp"
#include "app/ui_text/ui_pixels.hpp"
#include "assets/game_path.hpp"
#include "assets/texture_database.hpp"

namespace rr::app {

Result<UiFont> LoadUiFont(render::Renderer& renderer, const assets::Vfs& vfs,
                          std::string_view folder, const assets::FontDefinition& definition) {
    Result<UiPixels> image =
        ReadUiPixels(vfs, assets::JoinGamePath(folder, definition.FileName()), definition.fileKind);
    if (!image) {
        return std::unexpected(std::move(image.error()));
    }
    const assets::ImageAsset& layout = image->Layout();
    UiFont font{
        .widthPx = layout.widthPx,
        .heightPx = layout.heightPx,
        .spacingPx = definition.spacingPx,
        .glyphs = assets::ScanGlyphs(layout, definition.characters),
        .fixedGlyphs =
            FixedCharacters(definition).transform([&](const std::vector<char16_t>& characters) {
                return assets::ScanGlyphs(layout, characters);
            })};
    if (definition.colorKey && !image->original) {
        assets::ApplyColorKey(image->pixels, *definition.colorKey);
    }
    return renderer.UploadTexture(image->pixels).transform([&](render::TextureId texture) {
        font.texture = texture;
        return std::move(font);
    });
}

Result<UiFonts> LoadUiFonts(render::Renderer& renderer, const assets::Vfs& vfs,
                            std::string_view folder, std::string_view fontList) {
    Result<std::vector<assets::FontDefinition>> definitions =
        vfs.Read(assets::JoinGamePath(folder, fontList)).and_then([](const assets::FileData& file) {
            return assets::ParseFontList(file.Bytes());
        });
    if (!definitions) {
        return std::unexpected(std::move(definitions.error()));
    }
    UiFonts fonts;
    for (const assets::FontDefinition& definition : *definitions) {
        Result<UiFont> font = LoadUiFont(renderer, vfs, folder, definition);
        if (!font) {
            return std::unexpected(std::move(font.error()));
        }
        fonts.insert_or_assign(assets::ToUpperAscii(definition.name), std::move(*font));
    }
    return fonts;
}

const UiFont* FindUiFont(const UiFonts& fonts, std::string_view name) {
    const auto font = fonts.find(assets::ToUpperAscii(name));
    return font == fonts.end() ? nullptr : &font->second;
}

void UseTextFixes(UiFonts& fonts, TextFixes fixes) {
    for (UiFont& font : fonts | std::views::values) {
        font.fixes = fixes;
    }
}

}
