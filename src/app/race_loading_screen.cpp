#include "app/race_loading_screen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/hud/hud_assets.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/game_path.hpp"
#include "assets/image_list.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreenFile = "LOADSCRN.LSB";
constexpr std::string_view kImageList = "LOADSCRN.IDB";
constexpr std::string_view kTickImage = "TICK";
constexpr float kNameY = 25.0F;
constexpr float kTickSize = 16.0F;

float TickCentre(float share, float size) {
    return static_cast<float>(std::trunc(static_cast<double>(share) * static_cast<double>(size)));
}

Result<assets::FileData> ReadFile(const assets::Vfs& vfs, std::string_view folder,
                                  std::string_view name) {
    return vfs.Read(assets::JoinGamePath(folder, name));
}

Result<UiImage> LoadTick(render::Renderer& renderer, const assets::Vfs& vfs,
                         std::string_view folder) {
    return ReadFile(vfs, folder, kImageList)
        .and_then([](const assets::FileData& file) { return assets::ParseImageList(file.Bytes()); })
        .and_then([&](const std::vector<assets::ImageListEntry>& entries) -> Result<UiImage> {
            const auto tick = std::ranges::find(entries, kTickImage, [](const auto& entry) {
                return assets::ToUpperAscii(entry.name);
            });
            if (tick == entries.end()) {
                return Fail(ErrorCode::NotFound, std::format("{} has no tick image", kImageList));
            }
            return LoadUiImage(renderer, vfs, folder, *tick);
        });
}

Result<std::u16string> TrackName(const assets::Vfs& vfs, std::string_view language,
                                 std::int32_t index) {
    return ReadGameStrings(vfs, language)
        .and_then([&](std::vector<std::u16string> strings) -> Result<std::u16string> {
            if (index < 0 || std::cmp_greater_equal(index, strings.size())) {
                return Fail(ErrorCode::InvalidData, std::format("GAME.SRF has no line {}", index));
            }
            return std::move(strings[static_cast<std::size_t>(index)]);
        });
}

Result<LoadingScreenParts> LoadParts(render::Renderer& renderer, const assets::Vfs& vfs,
                                     std::string_view track, std::string_view language) {
    const std::string folder = assets::JoinGamePath("GAMEDATA", track);
    Result<assets::LoadingScreenAsset> screen =
        ReadFile(vfs, folder, kScreenFile).and_then([](const assets::FileData& file) {
            return assets::ParseLsb(file.Bytes());
        });
    Result<UiImage> background = screen.and_then([&](const assets::LoadingScreenAsset& asset) {
        return LoadUiImage(renderer, vfs, folder,
                           {.name = asset.image, .fileKind = assets::TextureFileKind::Bmp});
    });
    Result<UiImage> tick =
        background.and_then([&](const UiImage&) { return LoadTick(renderer, vfs, folder); });
    Result<std::u16string> name =
        tick.and_then([&](const UiImage&) { return TrackName(vfs, language, screen->nameIndex); });
    if (!name) {
        return std::unexpected(std::move(name.error()));
    }
    return LoadingScreenParts{.background = background->texture,
                              .tick = tick->texture,
                              .ticks = std::move(screen->ticks),
                              .trackName = std::move(*name)};
}

}

void LayoutLoadingScreen(const LoadingScreenParts& parts, const HudFonts& fonts, float progress,
                         std::vector<render::SpriteDraw>& out) {
    out.push_back({.texture = parts.background,
                   .bounds = {.width = render::kUiWidth, .height = render::kUiHeight}});
    const float nameWidth = fonts.Width(HudFont::Text, parts.trackName);
    const std::array name{HudText{.text = parts.trackName,
                                  .x = std::floor((render::kUiWidth - nameWidth) / 2.0F),
                                  .y = kNameY}};
    DrawHudText(name, fonts, out);
    const auto shown = static_cast<std::size_t>(static_cast<float>(parts.ticks.size()) *
                                                std::clamp(progress, 0.0F, 1.0F));
    for (std::size_t i = 0; i < shown; ++i) {
        const auto [x, y] = parts.ticks[i];
        out.push_back({.texture = parts.tick,
                       .bounds = {.x = TickCentre(x, render::kUiWidth) - (kTickSize / 2.0F),
                                  .y = TickCentre(y, render::kUiHeight) - (kTickSize / 2.0F),
                                  .width = kTickSize,
                                  .height = kTickSize}});
    }
}

RaceLoadingScreen::RaceLoadingScreen(render::Renderer& renderer, UiFonts fonts,
                                     LoadingScreenParts parts)
    : renderer_(&renderer),
      fonts_(std::move(fonts)),
      hudFonts_(FindHudFonts(fonts_).value_or(HudFonts{})),
      parts_(std::move(parts)) {}

Result<std::unique_ptr<RaceLoadingScreen>> RaceLoadingScreen::Load(render::Renderer& renderer,
                                                                   const assets::Vfs& vfs,
                                                                   std::string_view track,
                                                                   std::string_view language) {
    Result<UiFonts> fonts = LoadUiFonts(renderer, vfs, HudFolder(language), kHudFontList);
    Result<LoadingScreenParts> parts =
        fonts.and_then([](const UiFonts& loaded) { return FindHudFonts(loaded); })
            .and_then([&](const HudFonts&) { return LoadParts(renderer, vfs, track, language); });
    if (!parts) {
        return std::unexpected(std::move(parts.error()));
    }
    return std::unique_ptr<RaceLoadingScreen>(
        new RaceLoadingScreen(renderer, std::move(*fonts), std::move(*parts)));
}

UiFonts RaceLoadingScreen::TakeFonts() && {
    return std::move(fonts_);
}

void RaceLoadingScreen::Show(float progress) {
    sprites_.clear();
    LayoutLoadingScreen(parts_, hudFonts_, progress, sprites_);
    const Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

std::unique_ptr<RaceLoadingScreen> OpenLoadingScreen(render::Renderer& renderer,
                                                     const assets::Vfs& vfs, std::string_view track,
                                                     std::string_view language) {
    Result<std::unique_ptr<RaceLoadingScreen>> screen =
        RaceLoadingScreen::Load(renderer, vfs, track, language);
    if (!screen) {
        log::Warn("{}: {}, loading without a loading screen", track, screen.error().message);
        return nullptr;
    }
    (*screen)->Show(0.0F);
    return std::move(*screen);
}

}
