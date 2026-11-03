#pragma once

#include <cstddef>
#include <cstdint>

#include "sim/input_frame.hpp"

namespace rr::sim {

class InputLatch {
public:
    void Press(std::size_t player, Action action);
    void Release(std::size_t player, Action action);
    void Press(MenuAction action);
    void SetSteerLeftAxis(std::size_t player, std::int8_t value);
    void MovePointer(float x, float y, bool isOver);
    void AddPointerMotion(float dx, float dy);
    void PressPointer(PointerButton button);
    void ReleasePointer();
    void ReleaseAll();

    [[nodiscard]] InputFrame TakeFrame();

private:
    InputFrame pending_;
};

}
