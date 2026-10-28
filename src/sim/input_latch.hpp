#pragma once

#include <cstdint>

#include "sim/input_frame.hpp"

namespace rr::sim {

class InputLatch {
public:
    void Press(Action action);
    void Release(Action action);
    void SetSteerLeftAxis(std::int8_t value);

    [[nodiscard]] InputFrame TakeFrame();

private:
    InputFrame pending_;
};

}
