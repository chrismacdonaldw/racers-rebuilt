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

struct RacerActionGroup {
    std::string name;
    std::vector<RacerAction> actions;
};

/// Buttons that act on the chosen racer of the running scene. `selected` must outlive the frame.
struct RacerTools {
    std::vector<std::string> racers;
    std::size_t* selected = nullptr;
    std::vector<RacerActionGroup> groups;
};

/// Draws into the open menu.
void DrawRacerTools(const RacerTools& tools);

}
