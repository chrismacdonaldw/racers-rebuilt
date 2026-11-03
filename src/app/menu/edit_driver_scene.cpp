#include "app/menu/edit_driver_scene.hpp"

#include <cstdint>
#include <format>
#include <span>
#include <utility>

#include "app/circuit_awards.hpp"
#include "app/frame_loop.hpp"
#include "app/menu/garage_racers.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_widgets.hpp"
#include "assets/loader_pool.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kTitleLine = 9;
constexpr std::size_t kMixLine = 0x38;
constexpr std::size_t kBackLine = 0x1F;
constexpr std::size_t kDiscardLine = 0x7B;
constexpr std::size_t kContinueLine = 0x20;
constexpr std::size_t kAbandonLine = 0x77;
constexpr std::size_t kYesLine = 0x73;
constexpr std::size_t kNoLine = 0x74;
constexpr std::size_t kLoaderWorkers = 2;
constexpr std::uint64_t kMixSeed = 0xD15C0;
constexpr std::uint64_t kStateBase = 256;

std::unique_ptr<DriverStand> LoadStand(render::Renderer& renderer, const assets::Vfs& vfs,
                                       assets::TrackLoader& loader, const MenuFiles& files) {
    Result<std::unique_ptr<DriverStand>> stand =
        DriverStand::Load(renderer, vfs, loader, files, kEditDriverScreen, kBuilderStandSet);
    if (!stand) {
        log::Warn("{}, the driver editor shows no minifig", stand.error().message);
        return nullptr;
    }
    return std::move(*stand);
}

std::optional<DialogLook> LoadDialog(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<DialogLook> look = LoadDialogLook(renderer, vfs);
    if (!look) {
        log::Warn("{}, the driver editor discards changes without asking", look.error().message);
        return std::nullopt;
    }
    return std::move(*look);
}

}

Result<std::unique_ptr<EditDriverScene>> EditDriverScene::Create(render::Renderer& renderer,
                                                                 const assets::Vfs& vfs,
                                                                 MenuSounds sounds,
                                                                 const EditDriverFiles& files,
                                                                 EditedRacer racer) {
    assets::LoaderPool pool(kLoaderWorkers);
    assets::TrackLoader loader(vfs, pool);
    Result<settings::RacerStore> store = LoadRacerStore(files.racers, racer.index);
    Result<MenuFiles> menu = store.and_then(
        [&](const settings::RacerStore&) { return ReadMenuFiles(vfs, kEditDriverScreen); });
    Result<EditDriverLook> look = menu.and_then([&](const MenuFiles& loaded) {
        return LoadEditDriverLook(renderer, vfs, loaded,
                                  ButtonsOf(RacerEditor::Driver, racer.isNew));
    });
    Result<std::unique_ptr<DriverKit>> kit = look.and_then(
        [&](const EditDriverLook&) { return DriverKit::Load(renderer, vfs, loader); });
    if (!kit) {
        return Fail(kit.error().code, std::format("driver editor: {}", kit.error().message));
    }
    Views views{.kit = std::move(*kit),
                .stand = LoadStand(renderer, vfs, loader, *menu),
                .dialog = LoadDialog(renderer, vfs)};
    Racers racers{.file = files.racers,
                  .store = std::move(*store),
                  .racer = racer,
                  .progress = ReadProgress(files.progress)};
    return std::unique_ptr<EditDriverScene>(new EditDriverScene(
        renderer, vfs, sounds, std::move(*look), std::move(views), std::move(racers)));
}

EditDriverScene::EditDriverScene(render::Renderer& renderer, const assets::Vfs& vfs,
                                 MenuSounds sounds, EditDriverLook look, Views views, Racers racers)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      look_(std::move(look)),
      views_(std::move(views)),
      racers_(std::move(racers)),
      buttons_(ButtonsOf(RacerEditor::Driver, racers_.racer.isNew)),
      saved_(racers_.store.racers.at(racers_.racer.index).look),
      driver_(saved_),
      random_(kMixSeed) {
    for (std::size_t row = 0; row < kDriverSlots; ++row) {
        const auto slot = static_cast<DriverSlot>(row);
        const CarouselLayout& layout = look_.rows.at(row).carousel;
        carousels_.emplace_back(
            UnlockedParts(views_.kit->Catalog(), slot, PartOf(driver_, slot), racers_.progress),
            PartOf(driver_, slot),
            CarouselShape{.slots = layout.slots.size(), .focused = layout.focused}, look_.scroll);
        carouselViews_.emplace_back(layout, look_.itemColors);
    }
    LayOut();
    ShowDriver();
}

void EditDriverScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    if (dialog_) {
        dialog_->Measure(text_);
    }
}

std::optional<std::string> EditDriverScene::TakeNextScene() {
    return std::exchange(opened_, std::nullopt);
}

void EditDriverScene::Tick(const sim::InputFrame& input) {
    if (views_.stand) {
        views_.stand->Tick(kSimTick);
    }
    for (PartCarousel& carousel : carousels_) {
        carousel.Tick(kSimTick);
    }
    if (leaving_) {
        if (!views_.stand || !views_.stand->IsLeaving()) {
            opened_ = std::exchange(leaving_, std::nullopt);
        }
        return;
    }
    if (dialog_) {
        TickDialog(*dialog_, input);
        return;
    }
    const DriverRow focusBefore = menu_.Focus();
    AimPointer();
    const PointerChoice pointed = pointer_.Tick(input.pointer, targets_);
    if (const std::optional<std::size_t> focus = pointed.Focus()) {
        menu_.FocusOn(static_cast<DriverRow>(*focus));
    }
    const sim::InputFrame clicked = WithClick(input, pointed);
    const DriverRequest request = menu_.Tick(clicked);
    const CarouselStep step = request == DriverRequest::Step ? Step() : CarouselStep::Ignored;
    Handle(request);
    PlaySounds(clicked, request, focusBefore, step);
}

void EditDriverScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    auto hash = static_cast<std::uint64_t>(menu_.Focus());
    for (const PartCarousel& carousel : carousels_) {
        hash = (hash * kStateBase) + carousel.Part();
    }
    out.simStateHash = hash;
    out.entityClocks.clear();
    if (views_.stand) {
        out.entityClocks.assign(views_.stand->Clocks().begin(), views_.stand->Clocks().end());
    }
}

void EditDriverScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    views3d_.clear();
    DrawPicturesAround(under_, holes_, sprites_);
    if (views_.stand) {
        views3d_.push_back(views_.stand->Render(interpolated.entityClocks));
        const StandFraming& framing = views_.stand->Framing();
        DrawMenuFrameBorder(framing.look, framing.frame, sprites_);
    }
    for (std::size_t row = 0; row < kDriverSlots; ++row) {
        const bool isFocused = menu_.Slot() == static_cast<DriverSlot>(row);
        carouselViews_.at(row).Render(carousels_.at(row),
                                      views_.kit->Previews(static_cast<DriverSlot>(row)), isFocused,
                                      under_, views3d_);
    }
    DrawControls();
    if (dialog_) {
        dialog_->Draw(text_, sprites_);
    }
    views3d_.front().sprites = sprites_;
    Result<void> drawn = renderer_->Draw(views3d_);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

void EditDriverScene::Handle(DriverRequest request) {
    switch (request) {
        case DriverRequest::Mix:
            Mix();
            break;
        case DriverRequest::Done:
            Done();
            break;
        case DriverRequest::Leave:
            AskLeave();
            break;
        case DriverRequest::Step:
        case DriverRequest::None:
            break;
    }
}

void EditDriverScene::TickDialog(MessageDialog& dialog, const sim::InputFrame& input) {
    if (dialog.Tick(input, sounds_) == 0) {
        GoBack();
    }
    if (dialog.IsClosed()) {
        dialog_.reset();
    }
}

CarouselStep EditDriverScene::Step() {
    const std::optional<DriverSlot> slot = menu_.Slot();
    if (!slot) {
        return CarouselStep::Ignored;
    }
    PartCarousel& carousel = carousels_.at(static_cast<std::size_t>(*slot));
    const CarouselStep step = carousel.Step(menu_.Direction());
    if (step == CarouselStep::Moved) {
        PartOf(driver_, *slot) = carousel.Part();
        ShowDriver();
        if (views_.stand) {
            views_.stand->React(*slot);
        }
    }
    return step;
}

void EditDriverScene::Mix() {
    for (std::size_t row = 0; row < kDriverSlots; ++row) {
        PartCarousel& carousel = carousels_.at(row);
        carousel.Select(random_.Below(static_cast<std::uint32_t>(carousel.Count())));
        PartOf(driver_, static_cast<DriverSlot>(row)) = carousel.Part();
    }
    ShowDriver();
}

void EditDriverScene::Done() {
    racers_.store.racers.at(racers_.racer.index).look = driver_;
    if (Result<void> saved = settings::SaveRacers(racers_.store, racers_.file); !saved) {
        log::Warn("{}, the driver isn't saved", saved.error().message);
    }
    Leave(NextScene(RacerEditor::Driver, racers_.racer), true);
}

void EditDriverScene::AskLeave() {
    if (!views_.dialog || (!racers_.racer.isNew && driver_ == saved_)) {
        GoBack();
    } else if (racers_.racer.isNew) {
        Ask(kAbandonLine, kYesLine, kNoLine);
    } else {
        Ask(kDiscardLine, kContinueLine, kBackLine);
    }
}

void EditDriverScene::Ask(std::size_t question, std::size_t yes, std::size_t no) {
    dialog_.emplace(
        *views_.dialog, question,
        std::vector<DialogButton>{{.line = yes}, {.line = no, .style = DialogButtonStyle::Cross}});
    dialog_->Measure(text_);
}

void EditDriverScene::GoBack() {
    if (racers_.racer.isNew && racers_.racer.index + 1 == racers_.store.racers.size()) {
        DeleteRacer(racers_.store, racers_.racer.index);
        if (Result<void> saved = settings::SaveRacers(racers_.store, racers_.file); !saved) {
            log::Warn("{}, the abandoned racer isn't removed", saved.error().message);
        }
    }
    Leave(PreviousScene(RacerEditor::Driver, racers_.racer), false);
}

void EditDriverScene::Leave(std::string next, bool isCheering) {
    leaving_ = std::move(next);
    if (isCheering && views_.stand) {
        views_.stand->Leave();
    }
}

void EditDriverScene::ShowDriver() {
    if (!views_.stand) {
        return;
    }
    Result<void> shown = views_.kit->Minifig(driver_).and_then(
        [&](KitMinifig minifig) { return views_.stand->Show(std::move(minifig)); });
    if (!shown) {
        log::Warn("{}, the driver editor's minifig isn't updated", shown.error().message);
    }
}

void EditDriverScene::PlaySounds(const sim::InputFrame& input, DriverRequest request,
                                 DriverRow focusBefore, CarouselStep step) const {
    if (menu_.Focus() != focusBefore) {
        sounds_.MoveHighlight(SoundsOf(focusBefore), SoundsOf(menu_.Focus()));
    }
    if (step == CarouselStep::Moved) {
        sounds_.Scroll(look_.scrollSounds, menu_.Direction());
    } else if (step == CarouselStep::AtEnd) {
        sounds_.Play(look_.scrollSounds.back());
    } else if (input.WasPressed(sim::MenuAction::Back)) {
        sounds_.Confirm(look_.back.sounds);
    } else if (request != DriverRequest::None && request != DriverRequest::Step) {
        sounds_.Confirm(SoundsOf(focusBefore));
    }
}

const assets::SoundIds& EditDriverScene::SoundsOf(DriverRow row) const {
    switch (row) {
        case DriverRow::Mix:
            return look_.mixSounds;
        case DriverRow::Done:
            return look_.done.sounds;
        case DriverRow::Back:
            return look_.back.sounds;
        default:
            return look_.rowSounds;
    }
}

std::size_t EditDriverScene::StateOf(DriverRow row) const {
    return menu_.Focus() == row ? kFocusedState : kNormalState;
}

void EditDriverScene::LayOut() {
    under_.push_back(look_.backdrop);
    for (const PartRow& row : look_.rows) {
        if (look_.box.fill) {
            under_.push_back(
                {.bounds = FrameInterior(look_.box, row.selector.bounds), .tint = *look_.box.fill});
        }
        DrawMenuFrameBorder(look_.box, row.selector.bounds, sprites_);
        holes_.push_back(row.carousel.bounds);
    }
    for (const render::SpriteDraw& border : sprites_) {
        under_.push_back({.texture = border.texture, .bounds = border.bounds, .tint = border.tint});
    }
    sprites_.clear();
    if (views_.stand) {
        holes_.push_back(views_.stand->Framing().interior);
    }
}

void EditDriverScene::AimPointer() {
    targets_.clear();
    for (std::size_t row = 0; row < kDriverSlots; ++row) {
        AddSelectorTargets(look_.rows.at(row).selector, row, targets_);
    }
    targets_.push_back({.area = LabelArea(text_, look_.mixButton.at(StateOf(DriverRow::Mix)),
                                          text_.Line(kMixLine), look_.mix),
                        .control = static_cast<std::size_t>(DriverRow::Mix)});
    targets_.push_back({.area = ImageButtonArea(text_, look_.done, StateOf(DriverRow::Done),
                                                text_.Line(buttons_.done.line), look_.donePlace),
                        .control = static_cast<std::size_t>(DriverRow::Done)});
    targets_.push_back({.area = ImageButtonArea(text_, look_.back, StateOf(DriverRow::Back),
                                                text_.Line(buttons_.back.line), look_.backPlace),
                        .control = static_cast<std::size_t>(DriverRow::Back)});
}

void EditDriverScene::DrawControls() {
    for (std::size_t row = 0; row < kDriverSlots; ++row) {
        DrawSelectorArrows(look_.arrows, look_.rows.at(row).selector,
                           menu_.Slot() == static_cast<DriverSlot>(row), sprites_);
    }
    DrawBanner(text_, look_.banner, text_.Line(kTitleLine), look_.backdrop, look_.bannerTop,
               sprites_);
    DrawLabel(text_, look_.mixButton.at(StateOf(DriverRow::Mix)), text_.Line(kMixLine), look_.mix.x,
              look_.mix.y, sprites_);
    DrawImageButton(text_, look_.done, StateOf(DriverRow::Done), text_.Line(buttons_.done.line),
                    look_.donePlace, sprites_);
    DrawImageButton(text_, look_.back, StateOf(DriverRow::Back), text_.Line(buttons_.back.line),
                    look_.backPlace, sprites_);
}

}
