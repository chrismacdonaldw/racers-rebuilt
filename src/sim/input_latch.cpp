#include "sim/input_latch.hpp"

namespace rr::sim {

void InputLatch::Press(Action action) {
    const std::size_t index = InputFrame::Index(action);
    if (pending_.held.test(index)) {
        return;
    }
    pending_.held.set(index);
    pending_.pressed.set(index);
}

void InputLatch::Release(Action action) {
    const std::size_t index = InputFrame::Index(action);
    if (!pending_.held.test(index)) {
        return;
    }
    pending_.held.reset(index);
    pending_.released.set(index);
}

void InputLatch::Press(MenuAction action) {
    pending_.menuPressed.set(InputFrame::Index(action));
}

void InputLatch::SetSteerLeftAxis(std::int8_t value) {
    pending_.steerLeftAxis = value;
}

void InputLatch::ReleaseAll() {
    pending_.released |= pending_.held;
    pending_.held.reset();
    pending_.steerLeftAxis = 0;
}

InputFrame InputLatch::TakeFrame() {
    const InputFrame frame = pending_;
    pending_.pressed.reset();
    pending_.released.reset();
    pending_.menuPressed.reset();
    return frame;
}

}
