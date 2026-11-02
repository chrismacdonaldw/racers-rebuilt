#include "app/hud/race_hud_layer.hpp"

#include <utility>

#include "app/hud/hud_assets.hpp"
#include "app/hud/powerup_panel.hpp"
#include "app/screen_fill.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr render::Color kDialogBackdrop{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 64.0F / 255.0F};

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
    if (images_.empty()) {
        Result<std::vector<UiImage>> images = LoadHudImages(*renderer_, *vfs_);
        if (!images) {
            log::Error("the race HUD is hidden: {}", images.error().message);
            return;
        }
        images_ = std::move(*images);
    }
    names_ = RacerNames(text->driverNames, nameIndexes_);
    hud_.emplace(std::move(text->strings), *found);
}

void RaceHudLayer::SetCircuitPoints(std::span<const std::int32_t> before) {
    circuitPoints_.assign(before.begin(), before.end());
}

void RaceHudLayer::Tick(const sim::RaceSession& session, Milliseconds elapsed) {
    if (hud_) {
        hud_->Tick(session.Clock(), session.Progress(), racer_, elapsed);
    }
}

void RaceHudLayer::TickPause(Milliseconds elapsed) {
    if (hud_) {
        hud_->TickPause(elapsed);
    }
}

std::span<const render::SpriteDraw> RaceHudLayer::Draw(const sim::RaceSession& session,
                                                       const sim::RacerSnapshot& racer,
                                                       std::optional<render::SpriteDraw> cover,
                                                       std::optional<PauseView> pause) {
    sprites_.clear();
    if (hud_) {
        hud_->Layout(session.Clock(), session.Progress(), racer_, names_, circuitPoints_, texts_);
        DrawHudText(texts_, hud_->Fonts(), sprites_);
        LayoutPowerupPanel(racer, images_, sprites_);
    }
    if (cover) {
        sprites_.push_back(*cover);
    }
    if (hud_ && pause) {
        sprites_.push_back(ScreenFill(kDialogBackdrop));
        texts_.clear();
        hud_->LayoutPause(*pause, !circuitPoints_.empty(), texts_);
        DrawHudText(texts_, hud_->Fonts(), sprites_);
    }
    return sprites_;
}

}
