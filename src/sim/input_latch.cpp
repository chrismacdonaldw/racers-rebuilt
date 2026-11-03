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

void InputLatch::PressPointer() {
    pending_.pointer.pressed = true;
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
    pending_.pointer.moved = false;
    pending_.pointer.pressed = false;
    pending_.pointer.released = false;
    return frame;
}

}
