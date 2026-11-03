#include "app/menu/racer_pick_scene.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/menu_files.hpp"
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

std::unique_ptr<RacerStand> LoadStand(render::Renderer& renderer, const assets::Vfs& vfs,
                                      const MenuFiles& files) {
    Result<std::unique_ptr<RacerStand>> stand =
        RacerStand::Load(renderer, vfs, files, kRacerPickScreen);
    if (!stand) {
        log::Warn("the racer pick shows no racer: {}", stand.error().message);
        return nullptr;
    }
    return std::move(*stand);
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
    Result<MenuFiles> files = ReadMenuFiles(vfs, kRacerPickScreen);
    Result<RacerPickLook> look = files.and_then(
        [&](const MenuFiles& read) { return LoadRacerPickLook(renderer, vfs, read); });
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
    auto scene = std::unique_ptr<RacerPickScene>(new RacerPickScene(
        renderer, vfs, sounds, std::move(*look), std::move(route), std::move(racers)));
    scene->stand_ = LoadStand(renderer, vfs, *files);
    scene->ShowPicked();
    return scene;
}

void RacerPickScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
}

std::optional<std::string> RacerPickScene::TakeNextScene() {
    return std::exchange(opened_, std::nullopt);
}

void RacerPickScene::Tick(const sim::InputFrame& input) {
    if (stand_) {
        stand_->Tick(kSimTick);
    }
    if (cheered_) {
        if (!stand_ || !stand_->IsCheering()) {
            opened_ = std::exchange(cheered_, std::nullopt);
        }
        return;
    }
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
            ShowPicked();
            break;
        case RacerPickRequest::None:
            break;
    }
    PlaySounds(input, request, focusBefore);
}

void RacerPickScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
    out.entityClocks.clear();
    if (stand_) {
        out.entityClocks.assign(stand_->Clocks().begin(), stand_->Clocks().end());
    }
}

void RacerPickScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    render::RenderView view;
    if (stand_) {
        DrawPicturesAround(std::span(&look_.backdrop, 1), stand_->Bounds(), sprites_);
        view = stand_->Render(interpolated.entityClocks, sprites_);
    } else {
        DrawPictures(std::span(&look_.backdrop, 1), sprites_);
    }
    const std::size_t title = isVersus_ ? kFirstPlayerTitleLine + route_.player : kTitleLine;
    DrawBanner(text_, look_.banner, text_.Line(title), look_.backdrop, look_.bannerTop, sprites_);
    DrawPicker();
    DrawButtons();
    view.sprites = sprites_;
    Result<void> drawn = renderer_->Draw(view);
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
      menu_(racers_.pickable.size(),
            route_.player == 0 ? SelectedPick(racers_.pickable, racers_.store) : 0) {}

void RacerPickScene::Start() {
    const std::size_t picked = menu_.Racer();
    const RacerRef ref = RefOf(racers_.pickable.at(picked));
    if (route_.player != 0) {
        CheerInto(WithSecondPlayer(route_.next, ref));
        return;
    }
    if (racers_.isLoaded) {
        SelectPicked(racers_.store, racers_.pickable, picked);
        if (Result<void> saved = settings::SaveRacers(racers_.store, racers_.file); !saved) {
            log::Warn("{}, the pick isn't saved", saved.error().message);
        }
    }
    CheerInto(WithPlayer(route_.next, ref));
}

void RacerPickScene::CheerInto(std::string next) {
    if (stand_) {
        stand_->Cheer();
    }
    cheered_ = std::move(next);
}

void RacerPickScene::ShowPicked() {
    if (stand_ && !racers_.pickable.empty()) {
        stand_->Show(racers_.pickable.at(menu_.Racer()));
    }
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
