#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "app/menu/build_help.hpp"
#include "app/menu/build_sounds.hpp"
#include "app/menu/car_build_screen.hpp"
#include "app/menu/grid_pick.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kSetControl = 0;
constexpr std::size_t kDoneControl = 1;
constexpr std::size_t kDoneButton = 5;
constexpr int kTurns = 4;
constexpr std::u16string_view kExtraSetLabel = u"LDraw";

std::optional<std::size_t> DirectionOf(const sim::InputFrame& input) {
    constexpr std::array kDirections{std::pair{sim::MenuAction::Up, std::size_t{0}},
                                     std::pair{sim::MenuAction::Right, std::size_t{2}},
                                     std::pair{sim::MenuAction::Down, std::size_t{4}},
                                     std::pair{sim::MenuAction::Left, std::size_t{6}}};
    for (const auto& [action, direction] : kDirections) {
        if (input.WasPressed(action)) {
            return direction;
        }
    }
    return std::nullopt;
}

}

bool CarBuildScreen::UseControls(BuildMode mode) {
    if (mode.controls == BuildControls::Workshop) {
        return false;
    }
    const bool usesPalette = mode.controls == BuildControls::Palette;
    OfferExtraPieces(usesPalette && mode.offersExtraPieces);
    if (usesPalette == (palette_ != nullptr)) {
        return true;
    }
    builder_.Release();
    hold_ = PaletteHold::None;
    selected_.reset();
    blockers_.clear();
    grab_.Forget();
    builder_.RecordEdits(usesPalette);
    if (!usesPalette) {
        palette_.reset();
        builder_.Choose(builder_.Choice());
        ShowSet();
        return true;
    }
    palette_ = std::make_unique<PaletteControls>(*services_.catalog);
    ShowPaletteSet();
    return true;
}

void CarBuildScreen::ShowPaletteSet() {
    if (!palette_) {
        return;
    }
    palette_->ShowSet(builder_.Set());
    if (hold_ == PaletteHold::None) {
        builder_.Release();
    }
}

void CarBuildScreen::OfferExtraPieces(bool offers) {
    const std::size_t extra = services_.catalog->ExtraSet();
    const bool isOffered = !sets_.empty() && sets_.back() == extra;
    if (offers == isOffered || services_.catalog->extras.empty()) {
        return;
    }
    if (offers) {
        services_.catalog->ReadExtraModels(*services_.vfs);
        sets_.push_back(extra);
        setIcons_.push_back(look_.setImages.size());
    } else {
        sets_.pop_back();
        setIcons_.pop_back();
    }
    if (!offers && builder_.Set() == extra && !sets_.empty()) {
        builder_.Release();
        hold_ = PaletteHold::None;
        set_ = 0;
        builder_.SelectSet(sets_.front());
        ShowSet();
        ShowPaletteSet();
    }
    setStrip_.Snap(set_, sets_.size());
}

void CarBuildScreen::DrawExtraSetLabel(std::vector<render::SpriteDraw>& sprites) const {
    const UiFont* font = services_.text->Font(kBuildHelpFont);
    for (const std::optional<StripPage>& page : setStrip_.Pages()) {
        if (font != nullptr && page && page->shift == 0.0F && page->item < sets_.size() &&
            sets_[page->item] == services_.catalog->ExtraSet()) {
            DrawSelectorValue(*font, kExtraSetLabel, look_.sets, kFocusedSelectorValueColor,
                              sprites);
        }
    }
}

std::vector<PointerTarget> CarBuildScreen::PaletteTargets() const {
    std::vector<PointerTarget> targets;
    AddSelectorTargets(look_.sets, kSetControl, targets);
    targets.push_back({.area = look_.buttons.at(kDoneButton).bounds, .control = kDoneControl});
    return targets;
}

BuildOutcome CarBuildScreen::TickPalette(const sim::InputFrame& input) {
    BuildOutcome outcome = BuildOutcome::None;
    const bool movesByKeys = std::ranges::any_of(
        keys_, [](const BuildRequest& key) { return key.command == BuildCommand::Move; });
    for (const BuildRequest& key : keys_) {
        if (hold_ == PaletteHold::Keys || key.command != BuildCommand::Move) {
            outcome = std::max(outcome, Apply(key));
        }
    }
    TickEditTimers();
    const bool backs = input.WasPressed(sim::MenuAction::Back);
    const bool cancels = backs && hold_ == PaletteHold::Pointer;
    const PaletteTick tick =
        motion_.IsBusy()
            ? PaletteTick{}
            : palette_->Tick(input, hold_ == PaletteHold::None && !movesByKeys, cancels);
    const GrabTick grab = GrabFor(input.pointer, tick, cancels);
    if (tick.swatch) {
        outcome = std::max(outcome, Paint(*tick.swatch));
    }
    if (tick.picked) {
        selected_.reset();
        builder_.Pick(tick.picked->set, tick.picked->choice, palette_->ColourOf(*tick.picked));
        hold_ = tick.isDragged ? PaletteHold::Pointer : PaletteHold::Keys;
        isAimed_ = !tick.isDragged;
    } else if (hold_ == PaletteHold::Pointer) {
        outcome = std::max(outcome,
                           DragPiece(input.pointer, builder_.IsLifted() ? grab.drag : tick.drag));
    } else if (hold_ == PaletteHold::Keys) {
        outcome = std::max(outcome, HoldByKeys(input, movesByKeys || backs));
    } else {
        outcome = std::max(outcome, TickEdits(grab));
        if (backs && !std::exchange(selected_, std::nullopt)) {
            return BuildOutcome::Done;
        }
    }
    const PaletteHold held =
        HoldAfterTick(hold_, builder_.Cursor().Piece() != nullptr, motion_.IsBusy());
    if (held != hold_) {
        hold_ = held;
        isAimed_ = true;
    }
    return std::max(outcome, PointPalette(input));
}

GrabTick CarBuildScreen::GrabFor(const sim::MenuPointer& pointer, const PaletteTick& tick,
                                 bool cancels) {
    if (motion_.IsBusy() || hold_ == PaletteHold::Keys || palette_->IsDragging() || tick.picked) {
        return {};
    }
    const bool picks = hold_ == PaletteHold::None && pointer.pressed;
    return grab_.Tick(pointer, picks ? PieceUnder(pointer) : std::nullopt, cancels);
}

BuildOutcome CarBuildScreen::Paint(std::uint8_t colour) {
    if (!selected_) {
        palette_->ChooseColour(colour);
        return BuildOutcome::None;
    }
    return Settle(RecolourSelected(builder_, *selected_, colour));
}

BuildOutcome CarBuildScreen::DragPiece(const sim::MenuPointer& pointer, const DragTick& drag) {
    const int turns = ((drag.turns % kTurns) + kTurns) % kTurns;
    for (int turn = 0; turn < turns; ++turn) {
        builder_.Rotate();
    }
    if (turns > 0) {
        services_.sounds.Play(kBuildRotateSound);
    }
    const std::optional<GridCellPick> cell =
        pointer.isOver ? PickGridCell(GridOnScreen(), builder_.Car().Grid(), pointer.x, pointer.y)
                       : std::nullopt;
    isAimed_ = cell.has_value();
    if (const assets::PieceRecord* piece = builder_.Cursor().Piece(); piece != nullptr && cell) {
        const unsigned rotation = builder_.Cursor().Placement().rotation;
        builder_.PutAt(CentredOn(*cell, piece->shape.Footprint(rotation), rotation));
    }
    if (drag.step == DragStep::Dropped && isAimed_ && builder_.Check() != PlaceResult::Refused) {
        hold_ = PaletteHold::None;
        motion_.Drop(RestHeight());
        return BuildOutcome::None;
    }
    if (drag.step != DragStep::Dropped && drag.step != DragStep::Cancelled) {
        return BuildOutcome::None;
    }
    hold_ = PaletteHold::None;
    const bool isLifted = builder_.IsLifted();
    if (drag.step == DragStep::Dropped && !isAimed_ && isLifted) {
        return Settle(DropOffCar(builder_));
    }
    if (isAimed_) {
        services_.sounds.Play(kBuildBlockedSound);
    }
    builder_.Release();
    return isLifted ? Settle({.isChanged = true}) : BuildOutcome::None;
}

BuildOutcome CarBuildScreen::HoldByKeys(const sim::InputFrame& input, bool skipsMenu) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        hold_ = PaletteHold::None;
        builder_.Release();
        return BuildOutcome::None;
    }
    if (skipsMenu) {
        return BuildOutcome::None;
    }
    if (const std::optional<std::size_t> direction = DirectionOf(input)) {
        Move(*direction);
    }
    return input.WasPressed(sim::MenuAction::Confirm) ? Place() : BuildOutcome::None;
}

BuildOutcome CarBuildScreen::PointPalette(const sim::InputFrame& input) {
    const std::vector<PointerTarget> targets = PaletteTargets();
    const PointerChoice choice = paletteButtons_.Tick(input.pointer, targets);
    if (choice.clicked == kDoneControl) {
        return BuildOutcome::Done;
    }
    if (choice.clicked == kSetControl && choice.click) {
        StepSet(choice.Clicks(sim::MenuAction::Right) ? 1 : -1);
    }
    if (hold_ != PaletteHold::Pointer && !grab_.IsBusy()) {
        const std::optional<BuildPad> held = menu_.Held();
        const SceneMouse mouse = drag_.Tick(input.pointer, Regions(), menu_);
        static_cast<void>(camera_.Swing(mouse.turn));
        static_cast<void>(camera_.Lift(mouse.tilt));
        ChangePad(held);
        return Apply(mouse.request);
    }
    return BuildOutcome::None;
}

GridView CarBuildScreen::GridOnScreen() const {
    return {.camera = ViewCamera(),
            .view = look_.garage.interior,
            .car = CarTransform(),
            .corner = fit_.offset};
}

void CarBuildScreen::RenderPalette(std::vector<render::RenderView>& views,
                                   std::vector<render::SpriteDraw>& sprites) {
    const std::array holes{palette_->Area(), look_.garage.interior};
    DrawPicturesAround(under_, holes, sprites);
    RenderScene(hold_ != PaletteHold::None && isAimed_, views);
    DrawMenuFrameBorder(look_.garage.look, look_.garage.frame, sprites);
    palette_->Render(models_, under_, views, sprites);
    DrawSelectorArrows(look_.arrows, look_.sets, true, sprites);
    DrawStrip(setStrip_, look_.setImages, setIcons_, look_.setBox, sprites);
    DrawExtraSetLabel(sprites);
    const StateButton& done = look_.buttons.at(kDoneButton);
    sprites.push_back({.texture = done.images.at(kNormalState), .bounds = done.bounds});
}

}
