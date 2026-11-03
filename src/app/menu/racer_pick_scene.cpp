#include "app/menu/racer_pick_scene.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/racer_pick_list.hpp"
#include "app/race_scene_name.hpp"
#include "app/racer/racer_lookup.hpp"
#include "app/racer/racer_ref.hpp"
#include "core/log.hpp"
#include "core/utf16_text.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kTitleLine = 0x14;
constexpr std::size_t kFirstPlayerTitleLine = 0x15;
constexpr std::size_t kConfigLine = 0xB9;
constexpr std::size_t kStartLine = 0x72;
constexpr std::size_t kBackLine = 0x1F;

bool IsVersus(std::string_view scene) {
    return IsRaceScene(scene) && ParseRaceScene(scene).value_or(RaceSceneSetup{}).players > 1;
}

std::string_view SetupSceneOf(std::string_view scenes) {
    for (const auto part : std::views::split(scenes, kSceneChainMark)) {
        const std::string_view scene(part.begin(), part.end());
        if (IsVersus(scene)) {
            return kVersusRaceScene;
        }
        if (IsTrialScene(scene)) {
            return kTimeTrialScene;
        }
        if (IsCircuitScene(scene)) {
            return kCircuitRaceScene;
        }
    }
    return kSingleRaceScene;
}

std::vector<settings::Racer> DefaultRacers(const assets::Vfs& vfs) {
    Result<std::vector<settings::Racer>> racers = ReadDefaultRacers(vfs);
    if (!racers) {
        log::Warn("{}, the racer pick shows only your own racers", racers.error().message);
        return {};
    }
    return std::move(*racers);
}

}

Result<std::unique_ptr<RacerPickScene>> RacerPickScene::Create(render::Renderer& renderer,
                                                               const assets::Vfs& vfs,
                                                               MenuSounds sounds,
                                                               std::filesystem::path racersFile,
                                                               RacerPickRoute route) {
    Result<RacerPickLook> look = LoadRacerPickLook(renderer, vfs);
    if (!look) {
        return std::unexpected(std::move(look.error()));
    }
    Result<settings::RacerStore> store = settings::LoadRacers(racersFile);
    if (!store) {
        log::Warn("{}, the racer pick shows only the game's racers", store.error().message);
    }
    Racers racers{.file = std::move(racersFile),
                  .isLoaded = store.has_value(),
                  .store = std::move(store).value_or(settings::RacerStore{})};
    racers.pickable = PickableRacers(racers.store, DefaultRacers(vfs));
    for (const settings::Racer& racer : racers.pickable) {
        racers.names.push_back(Utf16FromUtf8(racer.name));
    }
    return std::unique_ptr<RacerPickScene>(new RacerPickScene(
        renderer, vfs, sounds, std::move(*look), std::move(route), std::move(racers)));
}

void RacerPickScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
}

std::optional<std::string> RacerPickScene::TakeNextScene() {
    return std::exchange(opened_, std::nullopt);
}

void RacerPickScene::Tick(const sim::InputFrame& input) {
    const RacerPickRow focusBefore = menu_.Focus();
    const RacerPickRequest request = menu_.Tick(input);
    switch (request) {
        case RacerPickRequest::Start:
            Start();
            break;
        case RacerPickRequest::Config:
            opened_ = ChainedScene(route_.player == 0 ? kControlsScene : kPlayer2ControlsScene,
                                   Reopened());
            break;
        case RacerPickRequest::Leave:
            opened_ = Left();
            break;
        case RacerPickRequest::Step:
        case RacerPickRequest::None:
            break;
    }
    PlaySounds(input, request, focusBefore);
}

void RacerPickScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
}

void RacerPickScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(std::span(&look_.backdrop, 1), sprites_);
    const std::size_t title = isVersus_ ? kFirstPlayerTitleLine + route_.player : kTitleLine;
    DrawBanner(text_, look_.banner, text_.Line(title), look_.backdrop, look_.bannerTop, sprites_);
    DrawPicker();
    DrawButtons();
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

RacerPickScene::RacerPickScene(render::Renderer& renderer, const assets::Vfs& vfs,
                               MenuSounds sounds, RacerPickLook look, RacerPickRoute route,
                               Racers racers)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      look_(std::move(look)),
      route_(std::move(route)),
      isVersus_(
          std::ranges::any_of(std::views::split(route_.next, kSceneChainMark),
                              [](const auto part) { return IsVersus(std::string_view(part)); })),
      racers_(std::move(racers)),
      menu_(racers_.pickable.size(), route_.player == 0 ? racers_.store.selected.value_or(0) : 0) {}

void RacerPickScene::Start() {
    const std::size_t picked = menu_.Racer();
    const RacerRef ref = RefOf(racers_.pickable.at(picked));
    if (route_.player != 0) {
        opened_ = WithSecondPlayer(route_.next, ref);
        return;
    }
    if (racers_.isLoaded) {
        SelectPicked(racers_.store, racers_.pickable, picked);
        if (Result<void> saved = settings::SaveRacers(racers_.store, racers_.file); !saved) {
            log::Warn("{}, the pick isn't saved", saved.error().message);
        }
    }
    opened_ = WithPlayer(route_.next, ref);
}

std::string RacerPickScene::Reopened() const {
    return ChainedScene(route_.player == 0 ? kPickRacerScene : kPickSecondRacerScene, route_.next);
}

std::string RacerPickScene::Left() const {
    return route_.player == 0 ? std::string(SetupSceneOf(route_.next))
                              : ChainedScene(kPickRacerScene, Reopened());
}

void RacerPickScene::PlaySounds(const sim::InputFrame& input, RacerPickRequest request,
                                RacerPickRow focusBefore) const {
    if (menu_.Focus() != focusBefore) {
        sounds_.MoveHighlight(SoundsOf(focusBefore), SoundsOf(menu_.Focus()));
    }
    if (request == RacerPickRequest::Step) {
        sounds_.Scroll(look_.scrollSounds, input.WasPressed(sim::MenuAction::Left) ? -1 : 1);
    }
    if (request == RacerPickRequest::Leave && input.WasPressed(sim::MenuAction::Back)) {
        sounds_.Confirm(look_.backButton.sounds);
    } else if (request != RacerPickRequest::None && request != RacerPickRequest::Step) {
        sounds_.Confirm(SoundsOf(menu_.Focus()));
    }
}

const assets::SoundIds& RacerPickScene::SoundsOf(RacerPickRow row) const {
    switch (row) {
        case RacerPickRow::Config:
            return look_.configSounds;
        case RacerPickRow::Start:
            return look_.startButton.sounds;
        case RacerPickRow::Back:
            return look_.backButton.sounds;
        default:
            return look_.pickerSounds;
    }
}

void RacerPickScene::DrawPicker() {
    const bool isFocused = menu_.Focus() == RacerPickRow::Racer;
    DrawSelectorArrows(look_.arrows, look_.picker, isFocused, sprites_);
    const UiFont* font = text_.Font(look_.label.font);
    if (font == nullptr || racers_.names.empty()) {
        return;
    }
    DrawSelectorValue(*font, racers_.names.at(menu_.Racer()), look_.picker,
                      isFocused ? kFocusedSelectorValueColor : kSelectorValueColor, sprites_);
}

void RacerPickScene::DrawButtons() {
    const auto stateOf = [&](RacerPickRow row) {
        return menu_.Focus() == row ? kFocusedState : kNormalState;
    };
    DrawLabel(text_, look_.configButton.at(stateOf(RacerPickRow::Config)), text_.Line(kConfigLine),
              look_.config.x, look_.config.y, sprites_);
    DrawImageButton(text_, look_.startButton, stateOf(RacerPickRow::Start), text_.Line(kStartLine),
                    look_.start, sprites_);
    DrawImageButton(text_, look_.backButton, stateOf(RacerPickRow::Back), text_.Line(kBackLine),
                    look_.back, sprites_);
}

}
