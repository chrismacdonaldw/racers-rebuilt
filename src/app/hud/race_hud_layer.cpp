#include "app/hud/race_hud_layer.hpp"

#include <utility>

#include "app/hud/hud_assets.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

std::vector<std::u16string> RacerNames(std::span<const std::u16string> driverNames,
                                       std::span<const std::int32_t> nameIndexes) {
    std::vector<std::u16string> names;
    names.reserve(nameIndexes.size());
    for (const std::int32_t index : nameIndexes) {
        const auto line = static_cast<std::size_t>(index);
        names.push_back(index >= 0 && line < driverNames.size() ? driverNames[line]
                                                                : std::u16string{});
    }
    return names;
}

}

RaceHudLayer::RaceHudLayer(render::Renderer& renderer, const assets::Vfs& vfs, std::size_t racer,
                           std::vector<std::int32_t> nameIndexes)
    : renderer_(&renderer), vfs_(&vfs), racer_(racer), nameIndexes_(std::move(nameIndexes)) {}

void RaceHudLayer::SetLanguage(std::string_view language) {
    if (language == language_) {
        return;
    }
    language_ = language;
    hud_.reset();
    Result<HudFonts> found = LoadUiFonts(*renderer_, *vfs_, HudFolder(language_), kHudFontList)
                                 .and_then([&](UiFonts loaded) {
                                     fonts_ = std::move(loaded);
                                     return FindHudFonts(fonts_);
                                 });
    Result<HudLanguage> text = LoadHudLanguage(*vfs_, language_);
    if (!found || !text) {
        log::Error("the race HUD is hidden: {}", (found ? text.error() : found.error()).message);
        return;
    }
    names_ = RacerNames(text->driverNames, nameIndexes_);
    hud_.emplace(std::move(text->strings), *found);
}

void RaceHudLayer::Tick(const sim::RaceSession& session, Milliseconds elapsed) {
    if (hud_) {
        hud_->Tick(session.Clock(), session.Progress(), racer_, elapsed);
    }
}

std::span<const render::SpriteDraw> RaceHudLayer::Draw(const sim::RaceSession& session,
                                                       std::optional<std::size_t> pauseFocus) {
    sprites_.clear();
    if (hud_) {
        hud_->Layout(session.Clock(), session.Progress(), racer_, names_, texts_);
        if (pauseFocus) {
            hud_->LayoutPause(*pauseFocus, texts_);
        }
        DrawHudText(texts_, hud_->Fonts(), sprites_);
    }
    return sprites_;
}

}
