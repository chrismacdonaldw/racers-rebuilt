#pragma once

#include "sim/input_frame.hpp"

namespace rr::sim {

class InputLatch {
public:
    void Press(Action action);
    void Release(Action action);

    [[nodiscard]] InputFrame TakeFrame();

private:
    InputFrame pending_;
};

}
