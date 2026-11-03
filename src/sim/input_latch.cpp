#include "sim/input_latch.hpp"

namespace rr::sim {

void InputLatch::Press(std::size_t player, Action action) {
    PlayerInput& input = pending_.players.at(player);
    const std::size_t index = PlayerInput::Index(action);
    if (input.held.test(index)) {
        return;
    }
    input.held.set(index);
    input.pressed.set(index);
}

void InputLatch::Release(std::size_t player, Action action) {
    PlayerInput& input = pending_.players.at(player);
    const std::size_t index = PlayerInput::Index(action);
    if (!input.held.test(index)) {
        return;
    }
    input.held.reset(index);
    input.released.set(index);
}

void InputLatch::Press(MenuAction action) {
    pending_.menuPressed.set(InputFrame::Index(action));
}

void InputLatch::SetSteerLeftAxis(std::size_t player, std::int8_t value) {
    pending_.players.at(player).steerLeftAxis = value;
}

void InputLatch::MovePointer(float x, float y, bool isOver) {
    MenuPointer& pointer = pending_.pointer;
    pointer.moved = pointer.moved || x != pointer.x || y != pointer.y || isOver != pointer.isOver;
    pointer.x = x;
    pointer.y = y;
    pointer.isOver = isOver;
}

void InputLatch::AddPointerMotion(float dx, float dy) {
    pending_.pointer.dx += dx;
    pending_.pointer.dy += dy;
}

void InputLatch::PressPointer(PointerButton button) {
    pending_.pointer.pressed = true;
    pending_.pointer.rightPressed = pending_.pointer.rightPressed || button == PointerButton::Right;
}

void InputLatch::ReleasePointer() {
    pending_.pointer.released = true;
}

void InputLatch::ReleaseAll() {
    for (PlayerInput& input : pending_.players) {
        input.released |= input.held;
        input.held.reset();
        input.steerLeftAxis = 0;
    }
}

InputFrame InputLatch::TakeFrame() {
    const InputFrame frame = pending_;
    for (PlayerInput& input : pending_.players) {
        input.pressed.reset();
        input.released.reset();
    }
    pending_.menuPressed.reset();
    pending_.pointer.dx = 0.0F;
    pending_.pointer.dy = 0.0F;
    pending_.pointer.moved = false;
    pending_.pointer.pressed = false;
    pending_.pointer.rightPressed = false;
    pending_.pointer.released = false;
    return frame;
}

}
