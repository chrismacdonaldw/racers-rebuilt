#include "app/menu/workshop/bench_drag.hpp"

#include <cmath>
#include <utility>

namespace rr::app {
namespace {

constexpr float kLiftTravel = 6.0F;

bool LetGo(const sim::MenuPointer& pointer) {
    return pointer.released && !pointer.IsHeld(sim::PointerButton::Left);
}

}

BenchDragTick BenchDrag::Tick(const sim::MenuPointer& pointer, std::optional<BenchPiece> under,
                              bool cancels) {
    if (isHolding_) {
        if (cancels || pointer.rightPressed) {
            isHolding_ = false;
            return {.step = BenchStep::Cancelled};
        }
        if (LetGo(pointer)) {
            isHolding_ = false;
            return {.step = BenchStep::Dropped};
        }
        return {};
    }
    if (!pressed_) {
        if (!pointer.LeftPressed() || !under) {
            return {};
        }
        pressed_ = under;
        travelled_ = 0.0F;
    }
    if (cancels || pointer.rightPressed) {
        Forget();
        return {};
    }
    if (LetGo(pointer)) {
        return {.step = BenchStep::Clicked, .piece = std::exchange(pressed_, std::nullopt)};
    }
    travelled_ += std::abs(pointer.dx) + std::abs(pointer.dy);
    if (travelled_ < kLiftTravel) {
        return {};
    }
    isHolding_ = true;
    return {.step = BenchStep::Lifted, .piece = std::exchange(pressed_, std::nullopt)};
}

void BenchDrag::Hold() {
    pressed_.reset();
    isHolding_ = true;
}

void BenchDrag::Forget() {
    pressed_.reset();
    isHolding_ = false;
}

}
