#include "app/menu/edit_car_scene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <span>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/garage_racers.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_widgets.hpp"
#include "core/log.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kTitleLine = 0x0B;
constexpr std::array<std::size_t, kCarTextButtons> kTextLines{0x25, 0x3C, 0x3D};
constexpr std::array<CarRow, kCarTextButtons> kTextRows{CarRow::Build, CarRow::RemoveBricks,
                                                        CarRow::QuickBuild};
constexpr std::size_t kDoneLine = 0x72;
constexpr std::size_t kBackLine = 0x1F;
constexpr std::size_t kDiscardLine = 0x7B;
constexpr std::size_t kContinueLine = 0x20;
constexpr std::size_t kTooManyLine = 0xBA;
constexpr std::size_t kDisabledState = 0;
constexpr std::uint64_t kColourSeed = 0xCA2C0102;
constexpr std::uint32_t kChassisColours = 10;
constexpr float kTurnPerMs = 0.001F;
constexpr float kQuaternionPeriod = 4.0F * std::numbers::pi_v<float>;
constexpr Vector3 kSpot{.z = 1.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr sim::EntityId kCarId{1};
constexpr std::uint64_t kStateBase = 256;

std::unique_ptr<CarStand> LoadStand(render::Renderer& renderer, const assets::Vfs& vfs,
                                    const MenuFiles& files, const settings::Racer& racer) {
    Result<std::unique_ptr<CarStand>> stand =
        CarStand::Load(renderer, vfs, files, kEditCarScreen, racer);
    if (!stand) {
        log::Warn("{}, the car editor shows no car", stand.error().message);
        return nullptr;
    }
    return std::move(*stand);
}

std::optional<DialogLook> LoadDialog(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<DialogLook> look = LoadDialogLook(renderer, vfs);
    if (!look) {
        log::Warn("{}, the car editor discards changes without asking", look.error().message);
        return std::nullopt;
    }
    return std::move(*look);
}

}

Result<std::unique_ptr<EditCarScene>> EditCarScene::Create(render::Renderer& renderer,
                                                           const assets::Vfs& vfs,
                                                           MenuSounds sounds,
                                                           const std::filesystem::path& racersFile,
                                                           EditedRacer racer,
                                                           const std::set<std::int32_t>& partSets) {
    Result<settings::RacerStore> store = LoadRacerStore(racersFile, racer.index);
    Result<MenuFiles> menu = store.and_then(
        [&](const settings::RacerStore&) { return ReadMenuFiles(vfs, kEditCarScreen); });
    Result<EditCarLook> look = menu.and_then([&](MenuFiles& loaded) {
        return AddBuilderImages(vfs, loaded).and_then([&] {
            return LoadEditCarLook(renderer, vfs, loaded, ButtonsOf(RacerEditor::Car, racer.isNew));
        });
    });
    Result<assets::PieceCatalog> pieces = look.and_then([&](const EditCarLook&) {
        return assets::PieceCatalog::Load(vfs, assets::PieceDetail::Menu);
    });
    Result<QuickBuilds> quickBuilds =
        pieces.and_then([&](const assets::PieceCatalog&) { return LoadQuickBuilds(vfs); });
    if (!quickBuilds) {
        return Fail(quickBuilds.error().code,
                    std::format("car editor: {}", quickBuilds.error().message));
    }
    const settings::Racer& record = store->racers.at(racer.index);
    sim::BuiltCar built = LoadEditedCar(record.car, pieces->library);
    std::vector<std::size_t> sets = AvailablePartSets(partSets, built, *pieces);
    const std::size_t chassisSet =
        built.Pieces().empty() ? 0 : PartSetOfChassis(*pieces, record.chassis);
    const auto set = std::ranges::find(sets, chassisSet);
    const bool isReplaceable = built.Pieces().size() <= 1 || quickBuilds->Contains(record.car);
    Car car{.pieces = std::move(*pieces),
            .built = std::move(built),
            .quickBuilds = std::move(*quickBuilds),
            .sets = sets,
            .setIcons = SetIcons(*pieces, sets),
            .set = set == sets.end() ? 0 : static_cast<std::size_t>(set - sets.begin()),
            .isReplaceable = isReplaceable};
    Views views{.stand = LoadStand(renderer, vfs, *menu, record),
                .dialog = LoadDialog(renderer, vfs)};
    Edit edit{.racers = {.file = racersFile, .store = std::move(*store), .racer = racer},
              .car = std::move(car)};
    return std::unique_ptr<EditCarScene>(new EditCarScene(renderer, vfs, sounds, std::move(*look),
                                                          std::move(views), std::move(edit)));
}

EditCarScene::EditCarScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                           EditCarLook look, Views views, Edit edit)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      look_(std::move(look)),
      views_(std::move(views)),
      racers_(std::move(edit.racers)),
      buttons_(ButtonsOf(RacerEditor::Car, racers_.racer.isNew)),
      car_(std::move(edit.car)),
      setStrip_(look_.setSlide),
      shown_(racers_.store.racers.at(racers_.racer.index)),
      random_(kColourSeed) {
    LayOut();
    setStrip_.Snap(car_.set, car_.sets.size());
    if (car_.built.Pieces().empty()) {
        ResetCar();
    } else {
        ShowCar();
    }
}

void EditCarScene::OpenBuilder() {
    if (!views_.stand) {
        return;
    }
    Result<std::unique_ptr<CarBuildScreen>> build =
        CarBuildScreen::Load(*renderer_, *vfs_,
                             {.vfs = vfs_,
                              .stand = views_.stand.get(),
                              .catalog = &car_.pieces,
                              .text = &text_,
                              .sounds = sounds_},
                             car_.built, car_.sets);
    if (!build) {
        log::Warn("{}, the car editor stays open", build.error().message);
        return;
    }
    build_ = std::move(*build);
}

void EditCarScene::HandleEvent(const SDL_Event& event) {
    if (build_ && !dialog_ && event.type == SDL_EVENT_KEY_DOWN) {
        build_->HandleKey(event.key.key);
    }
}

void EditCarScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    if (dialog_) {
        dialog_->Measure(text_);
    }
}

std::optional<std::string> EditCarScene::TakeNextScene() {
    return std::exchange(opened_, std::nullopt);
}

void EditCarScene::Tick(const sim::InputFrame& input) {
    if (views_.stand) {
        views_.stand->Tick(kSimTick);
    }
    turn_ =
        std::fmod(turn_ + (kTurnPerMs * static_cast<float>(kSimTick.count())), kQuaternionPeriod);
    setStrip_.Tick(kSimTick);
    if (dialog_) {
        TickDialog(*dialog_, input);
        return;
    }
    if (build_) {
        TickBuild(input);
        return;
    }
    const CarRow focusBefore = menu_.Focus();
    AimPointer();
    PointerChoice pointed = pointer_.Tick(input.pointer, targets_);
    if (const std::optional<std::size_t> focus = pointed.Focus();
        focus && !menu_.FocusOn(static_cast<CarRow>(*focus))) {
        pointed = {};
    }
    const sim::InputFrame clicked = WithClick(input, pointed);
    CarRequest request = menu_.Tick(clicked);
    const bool isSet = request == CarRequest::PreviousSet || request == CarRequest::NextSet;
    if (isSet && !setStrip_.Slide(request == CarRequest::NextSet ? 1 : -1, car_.sets.size())) {
        request = CarRequest::None;
    }
    Handle(request);
    PlaySounds(clicked, request, focusBefore);
}

void EditCarScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.assign(
        1, {.id = kCarId, .transform = {.position = kSpot, .rotation = FromAxisAngle(kUp, turn_)}});
    out.camera = {};
    out.simStateHash = (static_cast<std::uint64_t>(menu_.Focus()) * kStateBase) + car_.set;
    out.entityClocks.clear();
    if (views_.stand) {
        out.entityClocks.assign(views_.stand->Clocks().begin(), views_.stand->Clocks().end());
    }
}

void EditCarScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    if (build_) {
        RenderBuild();
        return;
    }
    render::RenderView view;
    const Transform car = interpolated.entities.empty() ? Transform{.position = kSpot}
                                                        : interpolated.entities.front().transform;
    if (views_.stand) {
        DrawPicturesAround(under_, views_.stand->Framing().interior, sprites_);
        view = views_.stand->Render(interpolated.entityClocks, car);
        DrawMenuFrameBorder(views_.stand->Framing().look, views_.stand->Framing().frame, sprites_);
    } else {
        DrawPictures(under_, sprites_);
    }
    DrawControls();
    if (dialog_) {
        dialog_->Draw(text_, sprites_);
    }
    view.sprites = sprites_;
    Result<void> drawn = renderer_->Draw(view);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

void EditCarScene::Handle(CarRequest request) {
    switch (request) {
        case CarRequest::PreviousSet:
            StepSet(car_.sets.size() - 1);
            break;
        case CarRequest::NextSet:
            StepSet(1);
            break;
        case CarRequest::RemoveBricks:
        case CarRequest::QuickBuild:
            if (car_.isReplaceable) {
                Apply(request);
            } else {
                Confirm(request);
            }
            break;
        case CarRequest::Done:
            Done();
            break;
        case CarRequest::Leave:
            if (HasChanges() && !racers_.racer.isNew) {
                Confirm(request);
            } else {
                Apply(request);
            }
            break;
        case CarRequest::Build:
            OpenBuilder();
            break;
        case CarRequest::None:
            break;
    }
}

void EditCarScene::TickBuild(const sim::InputFrame& input) {
    switch (build_->Tick(input)) {
        case BuildOutcome::CarChanged:
            car_.built = build_->Car();
            ShowCar();
            break;
        case BuildOutcome::TooManyPieces:
            if (views_.dialog) {
                dialog_.emplace(*views_.dialog, kTooManyLine,
                                std::vector<DialogButton>{{.line = kDoneLine}});
                dialog_->Measure(text_);
            }
            break;
        case BuildOutcome::Done:
            CloseBuilder();
            break;
        case BuildOutcome::None:
            break;
    }
}

void EditCarScene::CloseBuilder() {
    car_.built = build_->Car();
    build_.reset();
    car_.isReplaceable =
        car_.built.Pieces().size() <= 1 || car_.quickBuilds.Contains(car_.built.Save());
    ShowCar();
}

void EditCarScene::RenderBuild() {
    views3d_.clear();
    build_->Render(views3d_, sprites_);
    if (dialog_) {
        dialog_->Draw(text_, sprites_);
    }
    views3d_.front().sprites = sprites_;
    if (Result<void> drawn = renderer_->Draw(views3d_); !drawn) {
        log::Error("{}", drawn.error().message);
    }
}

void EditCarScene::TickDialog(MessageDialog& dialog, const sim::InputFrame& input) {
    if (dialog.Tick(input, sounds_) == 0 && asked_) {
        Apply(*asked_);
    }
    if (dialog.IsClosed()) {
        dialog_.reset();
        asked_.reset();
    }
}

void EditCarScene::Confirm(CarRequest request) {
    if (!views_.dialog) {
        Apply(request);
        return;
    }
    asked_ = request;
    dialog_.emplace(
        *views_.dialog, kDiscardLine,
        std::vector<DialogButton>{{.line = kContinueLine},
                                  {.line = kBackLine, .style = DialogButtonStyle::Cross}});
    dialog_->Measure(text_);
}

void EditCarScene::Apply(CarRequest request) {
    switch (request) {
        case CarRequest::RemoveBricks:
            ResetCar();
            break;
        case CarRequest::QuickBuild:
            QuickBuild();
            break;
        case CarRequest::Leave:
            opened_ = PreviousScene(RacerEditor::Car, racers_.racer);
            break;
        default:
            break;
    }
}

void EditCarScene::StepSet(std::size_t forward) {
    car_.set = (car_.set + forward) % car_.sets.size();
    ResetCar();
}

void EditCarScene::ResetCar() {
    const std::size_t set = car_.sets.at(car_.set);
    const auto colour = static_cast<std::uint8_t>(random_.Below(kChassisColours));
    if (Result<void> reset = ResetToChassis(car_.built, car_.pieces, set, colour); !reset) {
        log::Warn("{}, the car stays as it was", reset.error().message);
        return;
    }
    shown_.chassis = car_.pieces.partSets.at(set).chassisName;
    car_.isReplaceable = true;
    ShowCar();
}

void EditCarScene::QuickBuild() {
    const assets::SavedRacer* build = car_.quickBuilds.Next(shown_.chassis);
    if (build == nullptr) {
        return;
    }
    Result<sim::BuiltCar> loaded = sim::BuiltCar::Load(build->car, car_.pieces.library);
    if (!loaded) {
        log::Warn("{}, the car stays as it was", loaded.error().message);
        return;
    }
    car_.built = std::move(*loaded);
    car_.isReplaceable = true;
    ShowCar();
}

void EditCarScene::Done() {
    settings::Racer& record = racers_.store.racers.at(racers_.racer.index);
    record.chassis = shown_.chassis;
    record.car = shown_.car;
    if (racers_.racer.isNew) {
        racers_.store.selected = racers_.racer.index;
    }
    if (Result<void> saved = settings::SaveRacers(racers_.store, racers_.file); !saved) {
        log::Warn("{}, the car isn't saved", saved.error().message);
    }
    opened_ = NextScene(RacerEditor::Car, racers_.racer);
}

void EditCarScene::ShowCar() {
    shown_.car = car_.built.Save();
    const bool hasBricks = car_.built.Pieces().size() > 1;
    menu_.Enable(CarRow::RemoveBricks, hasBricks);
    menu_.Enable(CarRow::PartSet, !hasBricks || car_.isReplaceable);
    if (views_.stand) {
        views_.stand->ShowCar(shown_);
    }
}

bool EditCarScene::HasChanges() const {
    return shown_.car != racers_.store.racers.at(racers_.racer.index).car;
}

void EditCarScene::PlaySounds(const sim::InputFrame& input, CarRequest request,
                              CarRow focusBefore) const {
    if (menu_.Focus() != focusBefore) {
        sounds_.MoveHighlight(SoundsOf(focusBefore), SoundsOf(menu_.Focus()));
    } else if (request == CarRequest::PreviousSet || request == CarRequest::NextSet) {
        sounds_.Scroll(look_.scrollSounds, request == CarRequest::NextSet ? 1 : -1);
    } else if (input.WasPressed(sim::MenuAction::Back)) {
        sounds_.Confirm(look_.back.sounds);
    } else if (request != CarRequest::None) {
        sounds_.Confirm(SoundsOf(focusBefore));
    }
}

const assets::SoundIds& EditCarScene::SoundsOf(CarRow row) const {
    switch (row) {
        case CarRow::PartSet:
            return look_.pickerSounds;
        case CarRow::Build:
        case CarRow::RemoveBricks:
        case CarRow::QuickBuild:
            return look_.textSounds.at(static_cast<std::size_t>(row) - 1);
        case CarRow::Done:
            return look_.done.sounds;
        case CarRow::Back:
            return look_.back.sounds;
    }
    return look_.back.sounds;
}

std::size_t EditCarScene::StateOf(CarRow row) const {
    if (!menu_.IsEnabled(row)) {
        return kDisabledState;
    }
    return menu_.Focus() == row ? kFocusedState : kNormalState;
}

void EditCarScene::LayOut() {
    under_.push_back(look_.backdrop);
    if (look_.box.fill) {
        under_.push_back(
            {.bounds = FrameInterior(look_.box, look_.picker.bounds), .tint = *look_.box.fill});
    }
    DrawMenuFrameBorder(look_.box, look_.picker.bounds, sprites_);
    for (const render::SpriteDraw& border : sprites_) {
        under_.push_back({.texture = border.texture, .bounds = border.bounds, .tint = border.tint});
    }
    sprites_.clear();
}

void EditCarScene::AimPointer() {
    targets_.clear();
    if (menu_.IsEnabled(CarRow::PartSet)) {
        AddSelectorTargets(look_.picker, static_cast<std::size_t>(CarRow::PartSet), targets_);
    }
    for (std::size_t button = 0; button < kCarTextButtons; ++button) {
        const CarRow row = kTextRows.at(button);
        if (menu_.IsEnabled(row)) {
            targets_.push_back(
                {.area = LabelArea(text_, look_.textButton.at(StateOf(row)),
                                   text_.Line(kTextLines.at(button)), look_.textPlaces.at(button)),
                 .control = static_cast<std::size_t>(row)});
        }
    }
    targets_.push_back({.area = ImageButtonArea(text_, look_.done, StateOf(CarRow::Done),
                                                text_.Line(buttons_.done.line), look_.donePlace),
                        .control = static_cast<std::size_t>(CarRow::Done)});
    targets_.push_back({.area = ImageButtonArea(text_, look_.back, StateOf(CarRow::Back),
                                                text_.Line(buttons_.back.line), look_.backPlace),
                        .control = static_cast<std::size_t>(CarRow::Back)});
}

void EditCarScene::DrawControls() {
    if (menu_.IsEnabled(CarRow::PartSet)) {
        DrawSelectorArrows(look_.arrows, look_.picker, menu_.Focus() == CarRow::PartSet, sprites_);
    }
    DrawStrip(setStrip_, look_.sets, car_.setIcons, look_.setBox, sprites_);
    DrawBanner(text_, look_.banner, text_.Line(kTitleLine), look_.backdrop, look_.bannerTop,
               sprites_);
    for (std::size_t button = 0; button < kCarTextButtons; ++button) {
        const MenuPoint& at = look_.textPlaces.at(button);
        DrawLabel(text_, look_.textButton.at(StateOf(kTextRows.at(button))),
                  text_.Line(kTextLines.at(button)), at.x, at.y, sprites_);
    }
    DrawImageButton(text_, look_.done, StateOf(CarRow::Done), text_.Line(buttons_.done.line),
                    look_.donePlace, sprites_);
    DrawImageButton(text_, look_.back, StateOf(CarRow::Back), text_.Line(buttons_.back.line),
                    look_.backPlace, sprites_);
}

}
