#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/menu/car_build_screen.hpp"
#include "app/menu/grid_pick.hpp"

namespace rr::app {
namespace {

constexpr float kShellScale = 1.08F;
constexpr float kGlowLow = 0.15F;
constexpr float kGlowRange = 0.25F;
constexpr Milliseconds kPulsePeriod{900};
constexpr Milliseconds kFlashTime{600};
constexpr Milliseconds kFlashStep{100};
constexpr render::Color kBlockerGlow{.r = 0.7F, .g = 0.1F, .b = 0.05F};

float Pulse(Milliseconds at) {
    const float turn = static_cast<float>(at.count()) / static_cast<float>(kPulsePeriod.count());
    return 0.5F + (0.5F * std::sin(turn * 2.0F * std::numbers::pi_v<float>));
}

}

BuildOutcome CarBuildScreen::TickEdits(const GrabTick& grab) {
    BuildOutcome outcome = BuildOutcome::None;
    for (const PieceEditKey key : edits_) {
        if (key != PieceEditKey::Remove) {
            selected_.reset();
            outcome = std::max(outcome, Settle(StepHistory(builder_, key)));
        } else if (selected_) {
            const EditOutcome removed = RemoveSelected(builder_, *selected_);
            if (removed.isChanged) {
                selected_.reset();
            }
            outcome = std::max(outcome, Settle(removed));
        }
    }
    edits_.clear();
    if (grab.clicked) {
        selected_ = selected_ == grab.clicked ? std::nullopt : grab.clicked;
    }
    if (grab.lifted) {
        selected_.reset();
        const EditOutcome lifted = LiftSelected(builder_, *grab.lifted);
        if (lifted.isChanged) {
            hold_ = PaletteHold::Pointer;
            isAimed_ = false;
        } else {
            grab_.Forget();
        }
        outcome = std::max(outcome, Settle(lifted));
    }
    return outcome;
}

BuildOutcome CarBuildScreen::Settle(const EditOutcome& outcome) {
    if (outcome.sound != 0) {
        services_.sounds.Play(outcome.sound);
    }
    if (!outcome.blockers.empty()) {
        blockers_ = outcome.blockers;
        flash_ = kFlashTime;
    }
    if (!outcome.isChanged) {
        return BuildOutcome::None;
    }
    blockers_.clear();
    static_cast<void>(Refit());
    return BuildOutcome::CarChanged;
}

void CarBuildScreen::TickEditTimers() {
    flash_ = std::max(flash_ - kSimTick, Milliseconds{0});
    pulse_ = (pulse_ + kSimTick) % kPulsePeriod;
}

std::optional<std::size_t> CarBuildScreen::PieceUnder(const sim::MenuPointer& pointer) const {
    const render::Rect& view = look_.garage.interior;
    const bool isInside = pointer.isOver && pointer.x >= view.x && pointer.y >= view.y &&
                          pointer.x < view.x + view.width && pointer.y < view.y + view.height;
    return isInside ? PickPiece(GridOnScreen(), builder_.Car(), pointer.x, pointer.y)
                    : std::nullopt;
}

void CarBuildScreen::AddShell(const sim::CarPiece& piece, const render::Color& colour) {
    const PartPreview& model = models_.Model(*piece.piece, piece.colour, piece.placement.rotation);
    const Vector3 corner{.x = fit_.offset.x + static_cast<float>(piece.placement.x),
                         .y = fit_.offset.y + static_cast<float>(piece.placement.y),
                         .z = fit_.offset.z + (static_cast<float>(piece.height) * kPlateHeight)};
    for (render::MeshDraw draw : model.draws) {
        draw.transform = {.position = corner + (model.bounds.centre * (1.0F - kShellScale))};
        draw.scale = kShellScale;
        draw.material.blend = render::BlendMode::Additive;
        draw.material.tint = colour;
        extras_.push_back(draw);
    }
}

void CarBuildScreen::AddEditShells() {
    const auto& pieces = builder_.Car().Pieces();
    if (selected_ && *selected_ < pieces.size()) {
        const float glow = kGlowLow + (kGlowRange * Pulse(pulse_));
        AddShell(pieces[*selected_], {.r = glow, .g = glow, .b = glow});
    }
    if ((flash_ / kFlashStep) % 2 == 1) {
        for (const std::size_t blocker : blockers_) {
            if (blocker < pieces.size()) {
                AddShell(pieces[blocker], kBlockerGlow);
            }
        }
    }
}

}
