#include "app/menu/garage_scene.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/garage_racers.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/racer_edit_flow.hpp"
#include "app/race_settings.hpp"
#include "app/racer/racer_ref.hpp"
#include "core/log.hpp"
#include "core/utf16_text.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kMainMenuLine = 2;
constexpr std::size_t kMainPageLine = 3;
constexpr std::size_t kEditPageLine = 4;
constexpr std::size_t kDeleteQuestionLine = 0x76;
constexpr std::size_t kYesLine = 0x73;
constexpr std::size_t kNoLine = 0x74;
constexpr std::size_t kDisabledState = 0;
constexpr std::array<std::size_t, kGarageButtons> kButtonLines{0x28, 0x29, 0x2A, 0x2C,
                                                               0x2D, 9,    0x0A, 0x0B};
constexpr std::string_view kTestTrack = "TEST";
constexpr std::size_t kPickerControl = kGarageButtons + 1;

std::unique_ptr<RacerStand> LoadStand(render::Renderer& renderer, const assets::Vfs& vfs,
                                      const MenuFiles& files) {
    Result<std::unique_ptr<RacerStand>> stand =
        RacerStand::Load(renderer, vfs, files, kGarageScreen);
    if (!stand) {
        log::Warn("{}, the garage shows no racer", stand.error().message);
        return nullptr;
    }
    return std::move(*stand);
}

std::optional<DialogLook> LoadDialog(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<DialogLook> look = LoadDialogLook(renderer, vfs);
    if (!look) {
        log::Warn("{}, the garage deletes without asking", look.error().message);
        return std::nullopt;
    }
    return std::move(*look);
}

}

Result<std::unique_ptr<GarageScene>> GarageScene::Create(render::Renderer& renderer,
                                                         const assets::Vfs& vfs, MenuSounds sounds,
                                                         std::filesystem::path racersFile,
                                                         std::size_t racer) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kGarageScreen);
    Result<GarageLook> look = files.and_then(
        [&](const MenuFiles& loaded) { return LoadGarageLook(renderer, vfs, loaded); });
    if (!look) {
        return std::unexpected(std::move(look.error()));
    }
    Result<settings::RacerStore> store = settings::LoadRacers(racersFile);
    if (!store) {
        log::Warn("{}, the garage won't save", store.error().message);
    }
    Racers racers{.file = std::move(racersFile),
                  .isLoaded = store.has_value(),
                  .store = std::move(store).value_or(settings::RacerStore{}),
                  .shown = racer};
    Views views{.stand = LoadStand(renderer, vfs, *files), .dialog = LoadDialog(renderer, vfs)};
    return std::unique_ptr<GarageScene>(new GarageScene(renderer, vfs, sounds, std::move(*look),
                                                        std::move(views), std::move(racers)));
}

void GarageScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    if (dialog_) {
        dialog_->Measure(text_);
    }
    rules_ = {.respawn = RespawnRuleFrom(settings), .aiDriving = AiDrivingFrom(settings)};
}

std::optional<std::string> GarageScene::TakeNextScene() {
    return std::exchange(opened_, std::nullopt);
}

void GarageScene::Tick(const sim::InputFrame& input) {
    if (views_.stand) {
        views_.stand->Tick(kSimTick);
    }
    if (dialog_) {
        TickDialog(*dialog_, input);
        return;
    }
    const GarageButton focusBefore = menu_.Focus();
    AimPointer();
    PointerChoice pointed = pointer_.Tick(input.pointer, targets_);
    if (const std::optional<std::size_t> focus = pointed.Focus();
        focus && *focus < kPickerControl && !menu_.FocusOn(static_cast<GarageButton>(*focus))) {
        pointed = {};
    }
    const sim::InputFrame clicked = WithClick(input, pointed);
    const GarageRequest request = menu_.Tick(clicked);
    Handle(request);
    PlaySounds(clicked, request, focusBefore);
}

void GarageScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
    out.entityClocks.clear();
    if (views_.stand) {
        out.entityClocks.assign(views_.stand->Clocks().begin(), views_.stand->Clocks().end());
    }
}

void GarageScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    render::RenderView view;
    if (views_.stand) {
        DrawPicturesAround(std::span(&look_.backdrop, 1), views_.stand->Bounds(), sprites_);
        view = views_.stand->Render(interpolated.entityClocks, sprites_);
    } else {
        DrawPictures(std::span(&look_.backdrop, 1), sprites_);
    }
    const std::size_t title = menu_.IsEditing() ? kEditPageLine : kMainPageLine;
    DrawBanner(text_, look_.banner, text_.Line(title), look_.backdrop, look_.bannerTop, sprites_);
    DrawPicker();
    DrawButtons();
    if (dialog_) {
        dialog_->Draw(text_, sprites_);
    }
    view.sprites = sprites_;
    Result<void> drawn = renderer_->Draw(view);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

GarageScene::GarageScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                         GarageLook look, Views views, Racers racers)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      look_(std::move(look)),
      views_(std::move(views)),
      racers_(std::move(racers)),
      menu_(racers_.store.racers.size()) {
    for (const settings::Racer& racer : racers_.store.racers) {
        names_.push_back(Utf16FromUtf8(racer.name));
    }
    menu_.Refresh(racers_.store.racers.size(), racers_.shown, GarageButton::New);
    ShowRacer();
}

void GarageScene::Handle(GarageRequest request) {
    switch (request) {
        case GarageRequest::Step:
            ShowRacer();
            break;
        case GarageRequest::New:
            AddNew();
            break;
        case GarageRequest::Copy:
            Copy();
            break;
        case GarageRequest::Delete:
            AskDelete();
            break;
        case GarageRequest::TestDrive:
            TestDrive();
            break;
        case GarageRequest::Driver:
            opened_ = RacerScene(kEditDriverScene, menu_.Racer());
            break;
        case GarageRequest::Licence:
            opened_ = RacerScene(kLicenceScene, menu_.Racer());
            break;
        case GarageRequest::Car:
            opened_ = RacerScene(kEditCarScene, menu_.Racer());
            break;
        case GarageRequest::Leave:
            opened_ = std::string(kMainMenuScene);
            break;
        case GarageRequest::Page:
        case GarageRequest::None:
            break;
    }
}

void GarageScene::TickDialog(MessageDialog& dialog, const sim::InputFrame& input) {
    if (dialog.Tick(input, sounds_) == 0) {
        Delete();
    }
    if (dialog.IsClosed()) {
        dialog_.reset();
    }
}

void GarageScene::AddNew() {
    Result<settings::Racer> racer = NewRacer(*vfs_);
    if (!racer) {
        log::Warn("{}, the garage can't make a racer", racer.error().message);
        return;
    }
    names_.push_back(Utf16FromUtf8(racer->name));
    racers_.store.racers.push_back(std::move(*racer));
    const std::size_t added = racers_.store.racers.size() - 1;
    if (Changed(added, GarageButton::New)) {
        opened_ = EditorSceneName(RacerEditor::Driver, {.index = added, .isNew = true});
    }
}

void GarageScene::Copy() {
    const std::size_t copy = CopyRacer(racers_.store, menu_.Racer());
    names_.push_back(names_.at(menu_.Racer()));
    Changed(copy, GarageButton::Copy);
}

void GarageScene::AskDelete() {
    if (!views_.dialog) {
        Delete();
        return;
    }
    dialog_.emplace(*views_.dialog, kDeleteQuestionLine,
                    std::vector<DialogButton>{
                        {.line = kYesLine}, {.line = kNoLine, .style = DialogButtonStyle::Cross}});
    dialog_->Measure(text_);
}

void GarageScene::Delete() {
    const std::size_t racer = menu_.Racer();
    DeleteRacer(racers_.store, racer);
    names_.erase(names_.begin() + static_cast<std::ptrdiff_t>(racer));
    Changed(racer, GarageButton::Delete);
}

void GarageScene::TestDrive() {
    RaceSceneSetup race = rules_;
    race.track = kTestTrack;
    race.opponents = 0;
    race.isTestDrive = true;
    race.player = RefOf(racers_.store.racers.at(menu_.Racer()));
    opened_ = RaceSceneName(race);
}

bool GarageScene::Changed(std::size_t racer, GarageButton focus) {
    menu_.Refresh(racers_.store.racers.size(), racer, focus);
    ShowRacer();
    if (!racers_.isLoaded) {
        return false;
    }
    if (Result<void> saved = settings::SaveRacers(racers_.store, racers_.file); !saved) {
        log::Warn("{}, the garage's change isn't saved", saved.error().message);
        return false;
    }
    return true;
}

void GarageScene::ShowRacer() {
    if (!views_.stand) {
        return;
    }
    if (menu_.Racers() == 0) {
        views_.stand->Clear();
        return;
    }
    views_.stand->Show(racers_.store.racers.at(menu_.Racer()));
}

void GarageScene::PlaySounds(const sim::InputFrame& input, GarageRequest request,
                             GarageButton focusBefore) const {
    if (menu_.Focus() != focusBefore && request != GarageRequest::Page) {
        sounds_.MoveHighlight(SoundsOf(focusBefore), SoundsOf(menu_.Focus()));
    }
    if (request == GarageRequest::Step) {
        sounds_.Scroll(look_.scrollSounds, input.WasPressed(sim::MenuAction::Left) ? -1 : 1);
    } else if (input.WasPressed(sim::MenuAction::Back)) {
        sounds_.Confirm(look_.backButton.sounds);
    } else if (request != GarageRequest::None) {
        sounds_.Confirm(SoundsOf(focusBefore));
    }
}

std::size_t GarageScene::StateOf(GarageButton button) const {
    if (!menu_.IsEnabled(button)) {
        return kDisabledState;
    }
    return menu_.Focus() == button ? kFocusedState : kNormalState;
}

const assets::SoundIds& GarageScene::SoundsOf(GarageButton button) const {
    return button == GarageButton::Back ? look_.backButton.sounds : look_.buttonSounds;
}

void GarageScene::AimPointer() {
    targets_.clear();
    for (const GarageButton button : menu_.Buttons()) {
        if (!menu_.IsEnabled(button)) {
            continue;
        }
        const std::size_t state = StateOf(button);
        const auto index = static_cast<std::size_t>(button);
        const render::Rect area =
            button == GarageButton::Back
                ? ImageButtonArea(text_, look_.backButton, state,
                                  text_.Line(menu_.IsEditing() ? kMainPageLine : kMainMenuLine),
                                  look_.back)
                : LabelArea(text_, look_.button.at(state), text_.Line(kButtonLines.at(index)),
                            look_.buttons.at(index));
        targets_.push_back({.area = area, .control = index});
    }
    if (!menu_.IsEditing()) {
        AddSelectorTargets(look_.picker, kPickerControl, targets_);
    }
}

void GarageScene::DrawButtons() {
    for (const GarageButton button : menu_.Buttons()) {
        const std::size_t state = StateOf(button);
        if (button == GarageButton::Back) {
            const std::size_t line = menu_.IsEditing() ? kMainPageLine : kMainMenuLine;
            DrawImageButton(text_, look_.backButton, state, text_.Line(line), look_.back, sprites_);
            continue;
        }
        const auto index = static_cast<std::size_t>(button);
        const MenuPoint at = look_.buttons.at(index);
        DrawLabel(text_, look_.button.at(state), text_.Line(kButtonLines.at(index)), at.x, at.y,
                  sprites_);
    }
}

void GarageScene::DrawPicker() {
    const bool isFocused = !menu_.IsEditing() && menu_.Racers() > 1;
    DrawSelectorArrows(look_.arrows, look_.picker, isFocused, sprites_);
    const UiFont* font = text_.Font(look_.label.font);
    if (font == nullptr || names_.empty()) {
        return;
    }
    DrawSelectorValue(*font, names_.at(menu_.Racer()), look_.picker, kSelectorValueColor, sprites_);
}

}
