#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "app/menu/car_build_screen.hpp"
#include "app/menu/grid_pick.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kSetControl = 0;
constexpr std::size_t kDoneControl = 1;
constexpr std::size_t kDoneButton = 5;
constexpr std::int32_t kBlockedSound = 8;
constexpr std::int32_t kRotateSound = 22;
constexpr int kTurns = 4;

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

void CarBuildScreen::UseControls(BuildControls controls) {
    if ((controls == BuildControls::Palette) == (palette_ != nullptr)) {
        return;
    }
    hold_ = PaletteHold::None;
    if (controls == BuildControls::Original) {
        palette_.reset();
        builder_.Choose(builder_.Choice());
        ShowSet();
        return;
    }
    palette_ = std::make_unique<PaletteControls>(*services_.catalog);
    ShowPaletteSet();
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
    const bool backs = input.WasPressed(sim::MenuAction::Back);
    const PaletteTick tick = motion_.IsBusy()
                                 ? PaletteTick{}
                                 : palette_->Tick(input, hold_ == PaletteHold::None && !movesByKeys,
                                                  backs && hold_ == PaletteHold::Pointer);
    if (tick.picked) {
        builder_.Pick(tick.picked->set, tick.picked->choice, palette_->ColourOf(*tick.picked));
        hold_ = tick.isDragged ? PaletteHold::Pointer : PaletteHold::Keys;
        isAimed_ = !tick.isDragged;
    } else if (hold_ == PaletteHold::Pointer) {
        DragPiece(input.pointer, tick.drag);
    } else if (hold_ == PaletteHold::Keys) {
        outcome = std::max(outcome, HoldByKeys(input, movesByKeys || backs));
    } else if (backs) {
        return BuildOutcome::Done;
    }
    const PaletteHold held =
        HoldAfterTick(hold_, builder_.Cursor().Piece() != nullptr, motion_.IsBusy());
    if (held != hold_) {
        hold_ = held;
        isAimed_ = true;
    }
    return std::max(outcome, PointPalette(input));
}

void CarBuildScreen::DragPiece(const sim::MenuPointer& pointer, const DragTick& drag) {
    const int turns = ((drag.turns % kTurns) + kTurns) % kTurns;
    for (int turn = 0; turn < turns; ++turn) {
        builder_.Rotate();
    }
    if (turns > 0) {
        services_.sounds.Play(kRotateSound);
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
    } else if (drag.step == DragStep::Dropped || drag.step == DragStep::Cancelled) {
        if (isAimed_) {
            services_.sounds.Play(kBlockedSound);
        }
        hold_ = PaletteHold::None;
        builder_.Release();
    }
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
    if (hold_ != PaletteHold::Pointer) {
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
    const StateButton& done = look_.buttons.at(kDoneButton);
    sprites.push_back({.texture = done.images.at(kNormalState), .bounds = done.bounds});
}

}
