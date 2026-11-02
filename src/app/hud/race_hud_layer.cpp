#include "app/hud/race_hud_layer.hpp"

#include <utility>
#include <variant>

#include "app/hud/hud_assets.hpp"
#include "app/hud/powerup_panel.hpp"
#include "app/screen_fill.hpp"
#include "core/log.hpp"
#include "sim/physics/racer.hpp"

namespace rr::app {
namespace {

constexpr render::Color kDialogBackdrop{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 64.0F / 255.0F};

std::u16string NameOf(std::span<const std::u16string> driverNames, const DriverName& driver) {
    if (const auto* own = std::get_if<std::u16string>(&driver)) {
        return *own;
    }
    const std::int32_t index = std::get<std::int32_t>(driver);
    const auto line = static_cast<std::size_t>(index);
    return index >= 0 && line < driverNames.size() ? driverNames[line] : std::u16string{};
}

std::vector<std::u16string> RacerNames(std::span<const std::u16string> driverNames,
                                       std::span<const DriverName> drivers) {
    std::vector<std::u16string> names;
    names.reserve(drivers.size());
    for (const DriverName& driver : drivers) {
        names.push_back(NameOf(driverNames, driver));
    }
    return names;
}

}

RaceHudLayer::RaceHudLayer(render::Renderer& renderer, const assets::Vfs& vfs, std::size_t players,
                           std::vector<DriverName> drivers)
    : renderer_(&renderer), vfs_(&vfs), players_(players), drivers_(std::move(drivers)) {}

void RaceHudLayer::SetLanguage(std::string_view language) {
    if (language == language_) {
        return;
    }
    Result<UiFonts> fonts = LoadUiFonts(*renderer_, *vfs_, HudFolder(language), kHudFontList);
    if (!fonts) {
        language_ = language;
        huds_.clear();
        log::Error("the race HUD is hidden: {}", fonts.error().message);
        return;
    }
    SetLanguage(language, std::move(*fonts));
}

void RaceHudLayer::SetLanguage(std::string_view language, UiFonts fonts) {
    language_ = language;
    huds_.clear();
    fonts_ = std::move(fonts);
    Result<HudFonts> found = FindHudFonts(fonts_);
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
    names_ = RacerNames(text->driverNames, drivers_);
    for (std::size_t player = 0; player < players_; ++player) {
        huds_.emplace_back(text->strings, *found, PlacementFor(player, players_));
    }
}

void RaceHudLayer::SetCircuitPoints(std::span<const std::int32_t> before) {
    circuitPoints_.assign(before.begin(), before.end());
}

void RaceHudLayer::SetMap(std::optional<TrackMap> map) {
    gadgets_.assign(players_, HudGadget(map));
}

void RaceHudLayer::SetTimeTrial(const sim::TimeTrialGhosts& ghosts) {
    trial_ = &ghosts;
}

std::u16string_view RaceHudLayer::RacerName(std::size_t slot) const {
    return slot < names_.size() ? std::u16string_view(names_[slot]) : std::u16string_view{};
}

void RaceHudLayer::CycleGadget(std::size_t player) {
    if (player < gadgets_.size()) {
        gadgets_[player].Cycle();
    }
}

void RaceHudLayer::Tick(const sim::RaceSession& session, Milliseconds elapsed) {
    for (std::size_t player = 0; player < gadgets_.size(); ++player) {
        const sim::Racer& racer = session.Racers()[player];
        if (!racer.Status().isGhost) {
            gadgets_[player].Tick(racer.Body().derived.forwardSpeed);
        }
    }
    for (std::size_t player = 0; player < huds_.size(); ++player) {
        huds_[player].Tick(session.Clock(), session.Progress(), player, elapsed);
    }
}

void RaceHudLayer::TickPause(Milliseconds elapsed) {
    for (RaceHud& hud : huds_) {
        hud.TickPause(elapsed);
    }
}

std::span<const render::SpriteDraw> RaceHudLayer::Draw(
    const sim::RaceSession& session, std::span<const sim::RacerSnapshot> racers,
    std::span<const MapView> maps, std::span<const std::optional<render::SpriteDraw>> covers,
    std::optional<PauseView> pause) {
    sprites_.clear();
    for (std::size_t player = 0; player < players_; ++player) {
        const HudPlacement placement = PlacementFor(player, players_);
        if (player < huds_.size()) {
            RaceHud& hud = huds_[player];
            const bool isGhost = racers[player].isGhost;
            hud.Layout(session.Clock(), session.Progress(),
                       {.slot = player, .isGhost = isGhost, .trial = trial_}, texts_);
            DrawHudText(texts_, hud.Fonts(), sprites_);
            if (!isGhost) {
                LayoutPowerupPanel(racers[player], images_, sprites_, placement.bottom);
            }
            if (!isGhost && player < gadgets_.size() && player < maps.size()) {
                gadgets_[player].Draw(maps[player], images_, placement, sprites_);
            }
        }
        if (player < covers.size() && covers[player]) {
            render::SpriteDraw cover = *covers[player];
            if (players_ > 1) {
                cover.bounds.y = placement.top;
                cover.bounds.height = placement.bottom - placement.top;
            }
            sprites_.push_back(cover);
        }
    }
    if (huds_.empty()) {
        return sprites_;
    }
    RaceHud& first = huds_.front();
    texts_.clear();
    if (players_ == 1 && trial_ == nullptr) {
        first.LayoutResults(session.Clock(), session.Progress(), 0, names_, circuitPoints_, texts_);
    }
    DrawHudText(texts_, first.Fonts(), sprites_);
    if (pause) {
        sprites_.push_back(ScreenFill(kDialogBackdrop));
        texts_.clear();
        first.LayoutPause(*pause, !circuitPoints_.empty(), texts_);
        DrawHudText(texts_, first.Fonts(), sprites_);
    }
    return sprites_;
}

}
