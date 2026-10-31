#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace rr::overlay {

struct RacerAction {
    std::string name;
    std::function<void()> run;
};

/// Buttons that act on the chosen racer of the running scene. `selected` must outlive the frame.
struct RacerTools {
    std::vector<std::string> racers;
    std::size_t* selected = nullptr;
    std::vector<RacerAction> actions;
};

/// Draws into the open menu.
void DrawRacerTools(const RacerTools& tools);

}
