#include "app/menu/piece_grab.hpp"

#include <cmath>
#include <utility>

namespace rr::app {
namespace {

constexpr float kLiftTravel = 6.0F;

}

GrabTick PieceGrab::Tick(const sim::MenuPointer& pointer, std::optional<std::size_t> piece,
                         bool cancels) {
    if (drag_.Entry()) {
        return {.drag = drag_.Tick(pointer, std::nullopt, cancels)};
    }
    if (!pressed_) {
        if (!pointer.pressed || pointer.rightPressed || !piece) {
            return {};
        }
        pressed_ = piece;
        travelled_ = 0.0F;
        if (!pointer.released) {
            return {};
        }
    }
    if (cancels || pointer.rightPressed) {
        Forget();
        return {};
    }
    if (pointer.released) {
        return {.clicked = std::exchange(pressed_, std::nullopt)};
    }
    travelled_ += std::abs(pointer.dx) + std::abs(pointer.dy);
    if (travelled_ < kLiftTravel) {
        return {};
    }
    drag_.Hold(*pressed_);
    return {.lifted = std::exchange(pressed_, std::nullopt)};
}

void PieceGrab::Forget() {
    pressed_.reset();
    drag_.Forget();
}

}
