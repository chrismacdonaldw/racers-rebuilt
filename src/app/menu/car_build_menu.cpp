#include "app/menu/car_build_menu.hpp"

#include <array>
#include <utility>

namespace rr::app {
namespace {

struct BuildKey {
    SDL_Keycode key = SDLK_UNKNOWN;
    BuildRequest request;
};

constexpr BuildRequest MoveTo(std::size_t direction) {
    return {.command = BuildCommand::Move, .direction = direction};
}

constexpr std::array kBuildKeys{
    BuildKey{.key = SDLK_UP, .request = MoveTo(0)},
    BuildKey{.key = SDLK_KP_9, .request = MoveTo(1)},
    BuildKey{.key = SDLK_RIGHT, .request = MoveTo(2)},
    BuildKey{.key = SDLK_KP_3, .request = MoveTo(3)},
    BuildKey{.key = SDLK_DOWN, .request = MoveTo(4)},
    BuildKey{.key = SDLK_KP_1, .request = MoveTo(5)},
    BuildKey{.key = SDLK_LEFT, .request = MoveTo(6)},
    BuildKey{.key = SDLK_KP_7, .request = MoveTo(7)},
    BuildKey{.key = SDLK_KP_5, .request = {.command = BuildCommand::Rotate}},
    BuildKey{.key = SDLK_KP_PLUS, .request = {.command = BuildCommand::Place}},
    BuildKey{.key = SDLK_PLUS, .request = {.command = BuildCommand::Place}},
    BuildKey{.key = SDLK_EQUALS, .request = {.command = BuildCommand::Place}},
    BuildKey{.key = SDLK_KP_MINUS, .request = {.command = BuildCommand::Undo}},
    BuildKey{.key = SDLK_MINUS, .request = {.command = BuildCommand::Undo}},
    BuildKey{.key = SDLK_INSERT, .request = {.command = BuildCommand::PreviousSet}},
    BuildKey{.key = SDLK_PAGEUP, .request = {.command = BuildCommand::NextSet}},
    BuildKey{.key = SDLK_DELETE, .request = {.command = BuildCommand::PreviousPiece}},
    BuildKey{.key = SDLK_PAGEDOWN, .request = {.command = BuildCommand::NextPiece}},
};

constexpr std::array<std::pair<sim::MenuAction, std::size_t>, 4> kPadDirections{{
    {sim::MenuAction::Up, 0},
    {sim::MenuAction::Right, 2},
    {sim::MenuAction::Down, 4},
    {sim::MenuAction::Left, 6},
}};

}

std::optional<BuildRequest> BuildKeyRequest(SDL_Keycode key) {
    for (const BuildKey& bound : kBuildKeys) {
        if (bound.key == key) {
            return bound.request;
        }
    }
    return std::nullopt;
}

BuildRequest CarBuildMenu::Tick(const sim::InputFrame& input) {
    if (isMoving_) {
        return TickMoving(input);
    }
    if (input.WasPressed(sim::MenuAction::Back)) {
        return {.command = BuildCommand::Done};
    }
    const auto row = static_cast<int>(focus_);
    if (input.WasPressed(sim::MenuAction::Up) && row > 0) {
        focus_ = static_cast<BuildRow>(row - 1);
    }
    if (input.WasPressed(sim::MenuAction::Down) && row + 1 < static_cast<int>(kBuildRows)) {
        focus_ = static_cast<BuildRow>(row + 1);
    }
    if (const BuildRequest step = Step(input); step.command != BuildCommand::None) {
        return step;
    }
    return input.WasPressed(sim::MenuAction::Confirm) ? Choose() : BuildRequest{};
}

BuildRequest CarBuildMenu::TickMoving(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Confirm) || input.WasPressed(sim::MenuAction::Back)) {
        isMoving_ = false;
        return {};
    }
    for (const auto& [action, direction] : kPadDirections) {
        if (input.WasPressed(action)) {
            return MoveTo(direction);
        }
    }
    return {};
}

BuildRequest CarBuildMenu::Step(const sim::InputFrame& input) const {
    const bool isLeft = input.WasPressed(sim::MenuAction::Left);
    if (!isLeft && !input.WasPressed(sim::MenuAction::Right)) {
        return {};
    }
    switch (focus_) {
        case BuildRow::Sets:
            return {.command = isLeft ? BuildCommand::PreviousSet : BuildCommand::NextSet};
        case BuildRow::Pieces:
            return {.command = isLeft ? BuildCommand::PreviousPiece : BuildCommand::NextPiece};
        default:
            return {};
    }
}

BuildRequest CarBuildMenu::Choose() {
    switch (focus_) {
        case BuildRow::Move:
            isMoving_ = true;
            return {};
        case BuildRow::Rotate:
            return {.command = BuildCommand::Rotate};
        case BuildRow::Place:
            return {.command = BuildCommand::Place};
        case BuildRow::Undo:
            return {.command = BuildCommand::Undo};
        case BuildRow::Done:
            return {.command = BuildCommand::Done};
        case BuildRow::Sets:
        case BuildRow::Pieces:
            return {};
    }
    return {};
}

}
