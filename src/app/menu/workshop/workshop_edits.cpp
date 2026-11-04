#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/build_sounds.hpp"
#include "app/menu/car_edit.hpp"
#include "app/menu/grid_pick.hpp"
#include "app/menu/workshop/workshop_screen.hpp"
#include "app/track_space.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "sim/car_build/loose_bricks.hpp"

namespace rr::app {
namespace {

constexpr float kShellScale = 1.06F;
constexpr float kHeldAlpha = 0.65F;
constexpr render::Color kFits{.r = 0.6F, .g = 1.0F, .b = 0.6F, .a = kHeldAlpha};
constexpr render::Color kBlocked{.r = 1.0F, .g = 0.35F, .b = 0.35F, .a = kHeldAlpha};
constexpr float kGlowLow = 0.15F;
constexpr float kGlowRange = 0.25F;
constexpr float kPulseMs = 1000.0F;
constexpr int kTurns = 4;
constexpr Milliseconds kFlashTime{600};
constexpr Milliseconds kFlashStep{100};
constexpr render::Color kBlockerGlow{.r = 0.7F, .g = 0.1F, .b = 0.05F};

float Pulse(Milliseconds time) {
    const float phase = static_cast<float>(time.count()) / kPulseMs;
    return 0.5F + (0.5F * std::sin(2.0F * std::numbers::pi_v<float> * phase));
}

sim::CarPiece Placed(const HeldPiece& held, sim::PiecePlacement placement) {
    return {.piece = held.piece,
            .placement = placement,
            .colour = held.colour,
            .partType = held.partType};
}

}

BenchView WorkshopScreen::ViewNow() const {
    const Vector3 ground = GroundCorner();
    return {.camera = ViewCamera(),
            .view = panels_.Layout().scene,
            .carCorner = CarCorner(),
            .groundCorner = ground};
}

void WorkshopScreen::TickEdits(const sim::MenuPointer& pointer, bool isOverPanel) {
    pulse_ += kSimTick;
    flash_ = std::max(flash_ - kSimTick, Milliseconds{0});
    const std::optional<BenchPiece> under =
        drag_.IsHolding() || isOverPanel ? std::nullopt
                                         : PickBenchPiece(ViewNow(), bench_, pointer.x, pointer.y);
    if (pointer.LeftPressed() && !under && !drag_.IsHolding()) {
        selected_.reset();
    }
    const BenchDragTick tick = drag_.Tick(pointer, under, false);
    switch (tick.step) {
        case BenchStep::Clicked:
            selected_ = selected_ == tick.piece ? std::nullopt : tick.piece;
            break;
        case BenchStep::Lifted:
            Lift(*tick.piece);
            break;
        case BenchStep::Dropped:
            if (Contains(panels_.Layout().drawer, pointer.x, pointer.y)) {
                Discard();
            } else if (isOverPanel) {
                PutBack();
            } else {
                Drop();
            }
            break;
        case BenchStep::Cancelled:
            PutBack();
            break;
        case BenchStep::None:
            break;
    }
    if (held_) {
        Aim(pointer, isOverPanel);
    }
}

void WorkshopScreen::Act(const PanelTick& panel) {
    if (panel.grabbed) {
        Grab(*panel.grabbed);
    }
    if (panel.swatch) {
        ChooseSwatch(*panel.swatch);
    }
    if (panel.background) {
        ChooseBackground(*panel.background);
    }
    if (!panel.action) {
        return;
    }
    switch (*panel.action) {
        case PanelAction::Done:
            isDone_ = true;
            break;
        case PanelAction::Undo:
            StepHistory(true);
            break;
        case PanelAction::Redo:
            StepHistory(false);
            break;
        case PanelAction::Turn:
            Turn(1);
            break;
        case PanelAction::Delete:
            DeleteSelected();
            break;
        case PanelAction::Frame:
            FrameCar();
            break;
        case PanelAction::Background:
            break;
    }
}

void WorkshopScreen::Grab(const PaletteEntry& entry) {
    const assets::PieceCatalog& catalog = *services_.catalog;
    held_ = HeldPiece{
        .piece = &catalog.library.pieces.at(catalog.Choices(entry.set)[entry.choice].piece),
        .colour = panels_.ColourOf(entry),
        .partType = PartTypeOf(catalog, entry.set)};
    drag_.Hold();
    selected_.reset();
    services_.sounds.Play(kBuildPieceViewSound);
}

void WorkshopScreen::ChooseSwatch(std::uint8_t colour) {
    if (selected_) {
        Paint(colour);
    } else {
        panels_.ChooseColour(colour);
    }
}

BenchPiece WorkshopScreen::Newest(sim::BenchArea area) const {
    const std::size_t count =
        area == sim::BenchArea::Car ? bench_.car.Pieces().size() : bench_.ground.Pieces().size();
    return {.area = area, .index = count - 1};
}

void WorkshopScreen::Lift(const BenchPiece& piece) {
    const sim::CarPiece* lifted = PieceOf(bench_, piece);
    const bool isFixed =
        lifted == nullptr || lifted->piece->IsChassis() ||
        (piece.area == sim::BenchArea::Car && !bench_.car.Supports(piece.index).empty());
    if (isFixed) {
        drag_.Forget();
        services_.sounds.Play(kBuildRefusedSound);
        FlashBlockers(piece);
        return;
    }
    held_ = HeldPiece{.piece = lifted->piece,
                      .colour = lifted->colour,
                      .partType = lifted->partType,
                      .rotation = lifted->placement.rotation,
                      .from = piece};
    beforeLift_ = bench_;
    const bool isLifted = piece.area == sim::BenchArea::Car
                              ? bench_.car.Remove(piece.index).has_value()
                              : bench_.ground.Take(piece.index).has_value();
    if (!isLifted) {
        PutBack();
        return;
    }
    selected_.reset();
    services_.sounds.Play(kBuildPieceViewSound);
}

void WorkshopScreen::Aim(const sim::MenuPointer& pointer, bool isOverPanel) {
    const std::optional<BenchCell> picked =
        isOverPanel ? std::nullopt : PickBenchCell(ViewNow(), bench_, pointer.x, pointer.y);
    if (!held_ || !picked) {
        aim_.reset();
        return;
    }
    const BenchCell cell = *picked;
    const assets::PieceRecord& piece = *held_->piece;
    const unsigned rotation = held_->rotation;
    const sim::PiecePlacement placement = PlaceOn(cell, piece.shape.Footprint(rotation), rotation);
    std::expected<int, sim::PlacementFailure> rest = cell.area == sim::BenchArea::Car
                                                         ? bench_.car.Test(piece, placement)
                                                         : bench_.ground.Test(piece, placement);
    if (rest && !Allows(piece, cell.area)) {
        rest = std::unexpected(sim::PlacementFailure::Collision);
    }
    aim_ = BenchAim{.cell = cell, .placement = placement, .rest = rest};
}

void WorkshopScreen::Drop() {
    if (!aim_ || !aim_->rest) {
        isOverfull_ = aim_ && aim_->rest.error() == sim::PlacementFailure::TooManyPieces;
        services_.sounds.Play(kBuildBlockedSound);
        PutBack();
        return;
    }
    const HeldPiece held = *std::exchange(held_, std::nullopt);
    const BenchAim aim = *std::exchange(aim_, std::nullopt);
    if (beforeLift_) {
        bench_ = *std::exchange(beforeLift_, std::nullopt);
    }
    const bool isDone =
        held.from
            ? Apply(sim::ShiftPiece{.from = held.from->area,
                                    .index = held.from->index,
                                    .to = aim.cell.area,
                                    .placement = aim.placement})
            : Apply(sim::PutPiece{.area = aim.cell.area, .piece = Placed(held, aim.placement)});
    if (isDone) {
        selected_ = Newest(aim.cell.area);
        services_.sounds.Play(kBuildPlacedSound);
    }
}

void WorkshopScreen::PutBack() {
    if (beforeLift_) {
        bench_ = *std::exchange(beforeLift_, std::nullopt);
    }
    held_.reset();
    aim_.reset();
    drag_.Forget();
}

void WorkshopScreen::Discard() {
    const std::optional<BenchPiece> from = held_ ? held_->from : std::nullopt;
    PutBack();
    if (from && Apply(sim::LiftPiece{.area = from->area, .index = from->index})) {
        services_.sounds.Play(kBuildUndoSound);
    }
}

void WorkshopScreen::Turn(int quarters) {
    const auto turned = [&](unsigned rotation) {
        return static_cast<unsigned>((static_cast<int>(rotation) + (quarters % kTurns) + kTurns) %
                                     kTurns);
    };
    if (held_) {
        held_->rotation = turned(held_->rotation);
        services_.sounds.Play(kBuildRotateSound);
        return;
    }
    if (!selected_) {
        return;
    }
    const BenchPiece at = *selected_;
    const sim::CarPiece* piece = PieceOf(bench_, at);
    if (piece == nullptr) {
        return;
    }
    sim::PiecePlacement placement = piece->placement;
    placement.rotation = turned(placement.rotation);
    if (Apply(sim::ShiftPiece{
            .from = at.area, .index = at.index, .to = at.area, .placement = placement})) {
        selected_ = Newest(at.area);
        services_.sounds.Play(kBuildRotateSound);
    } else {
        FlashBlockers(at);
    }
}

void WorkshopScreen::FlashBlockers(const BenchPiece& piece) {
    if (piece.area == sim::BenchArea::Car) {
        blockers_ = bench_.car.Supports(piece.index);
        flash_ = kFlashTime;
    }
}

bool WorkshopScreen::Apply(const sim::BenchEdit& edit) {
    if (!history_.Do(bench_, edit)) {
        services_.sounds.Play(kBuildRefusedSound);
        return false;
    }
    return true;
}

void WorkshopScreen::Paint(std::uint8_t colour) {
    if (selected_ && colour < services_.catalog->colours.size() &&
        Apply(sim::PaintPiece{
            .area = selected_->area, .index = selected_->index, .colour = colour})) {
        services_.sounds.Play(kBuildPlacedSound);
    }
}

void WorkshopScreen::DeleteSelected() {
    if (!selected_ || held_) {
        return;
    }
    if (Apply(sim::LiftPiece{.area = selected_->area, .index = selected_->index})) {
        selected_.reset();
        services_.sounds.Play(kBuildUndoSound);
    } else {
        FlashBlockers(*selected_);
    }
}

void WorkshopScreen::StepHistory(bool undoes) {
    if (held_) {
        return;
    }
    const bool isStepped = undoes ? history_.Undo(bench_) : history_.Redo(bench_);
    services_.sounds.Play(isStepped ? kBuildUndoSound : kBuildRefusedSound);
    if (isStepped) {
        selected_.reset();
    }
}

bool WorkshopScreen::Allows(const assets::PieceRecord& piece, sim::BenchArea area) const {
    return area == sim::BenchArea::Ground || services_.mode.offersExtraPieces ||
           !assets::IsExtraPieceType(piece.type);
}

BuildOutcome WorkshopScreen::Outcome() {
    if (std::exchange(isDone_, false)) {
        PutBack();
        return BuildOutcome::Done;
    }
    if (std::exchange(isOverfull_, false)) {
        return BuildOutcome::TooManyPieces;
    }
    const assets::CarData car = bench_.car.Save();
    if (car == shownCar_) {
        return BuildOutcome::None;
    }
    shownCar_ = car;
    return BuildOutcome::CarChanged;
}

Vector3 WorkshopScreen::CornerOf(sim::BenchArea area, sim::PiecePlacement placement,
                                 int height) const {
    const Vector3 corner = area == sim::BenchArea::Car ? CarCorner() : GroundCorner();
    return corner + Vector3{.x = static_cast<float>(placement.x),
                            .y = static_cast<float>(placement.y),
                            .z = static_cast<float>(height) * kPlateHeight};
}

void WorkshopScreen::AddShell(const sim::CarPiece& piece, const Vector3& corner,
                              const render::Color& tint, render::BlendMode blend) {
    const PartPreview& model = models_.Model(*piece.piece, piece.colour, piece.placement.rotation);
    const float scale = blend == render::BlendMode::Additive ? kShellScale : 1.0F;
    for (render::MeshDraw draw : model.draws) {
        draw.transform =
            TransformToEngine({.position = corner + (model.bounds.centre * (1.0F - scale))});
        draw.scale = scale;
        draw.material.blend = blend;
        draw.material.tint = draw.material.tint * tint;
        draws_.push_back(draw);
    }
}

void WorkshopScreen::AddHeld() {
    if (!held_ || !aim_) {
        return;
    }
    const int height = aim_->rest.value_or(
        aim_->cell.area == sim::BenchArea::Car
            ? bench_.car.Grid().At(aim_->cell.cell.x, aim_->cell.cell.y).height
            : bench_.ground.Grid().At(aim_->cell.cell.x, aim_->cell.cell.y).height);
    AddShell(Placed(*held_, aim_->placement), CornerOf(aim_->cell.area, aim_->placement, height),
             aim_->rest ? kFits : kBlocked, render::BlendMode::Alpha);
}

void WorkshopScreen::AddSelection() {
    if (!selected_) {
        return;
    }
    const BenchPiece at = *selected_;
    const sim::CarPiece* piece = PieceOf(bench_, at);
    if (piece == nullptr) {
        return;
    }
    const float glow = kGlowLow + (kGlowRange * Pulse(pulse_));
    AddShell(*piece, CornerOf(at.area, piece->placement, piece->height),
             {.r = glow, .g = glow, .b = glow}, render::BlendMode::Additive);
}

void WorkshopScreen::AddBlockers() {
    if ((flash_ / kFlashStep) % 2 == 0) {
        return;
    }
    const std::span<const sim::CarPiece> pieces = bench_.car.Pieces();
    for (const std::size_t blocker : blockers_) {
        if (blocker < pieces.size()) {
            const sim::CarPiece& piece = pieces[blocker];
            AddShell(piece, CornerOf(sim::BenchArea::Car, piece.placement, piece.height),
                     kBlockerGlow, render::BlendMode::Additive);
        }
    }
}

}
