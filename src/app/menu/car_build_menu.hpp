#pragma once

#include <SDL3/SDL_keycode.h>

#include <cstddef>
#include <cstdint>
#include <optional>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class BuildRow : std::uint8_t { Sets, Pieces, Move, View, Rotate, Place, Undo, Done };

inline constexpr std::size_t kBuildRows = 8;

enum class BuildPad : std::uint8_t { Move, View };

enum class BuildCommand : std::uint8_t {
    None,
    PreviousSet,
    NextSet,
    PreviousPiece,
    NextPiece,
    Move,
    TurnLeft,
    TurnRight,
    TiltUp,
    TiltDown,
    ResetView,
    Rotate,
    Place,
    Undo,
    Done,
};

struct BuildRequest {
    BuildCommand command = BuildCommand::None;
    std::size_t direction = 0;

    friend bool operator==(const BuildRequest&, const BuildRequest&) = default;
};

/// The original's builder keys: arrows and keypad move, keypad 5 turns, + and - place and undo.
[[nodiscard]] std::optional<BuildRequest> BuildKeyRequest(SDL_Keycode key);

/// Confirm on a pad holds it, so the directions work it until Confirm or Back.
class CarBuildMenu {
public:
    [[nodiscard]] BuildRequest Tick(const sim::InputFrame& input);

    [[nodiscard]] BuildRow Focus() const { return focus_; }

    /// False, leaving the focus, while a pad is held.
    [[nodiscard]] bool FocusOn(BuildRow row);

    [[nodiscard]] std::optional<BuildPad> Held() const { return held_; }

private:
    [[nodiscard]] BuildRequest TickHeld(const sim::InputFrame& input);
    [[nodiscard]] BuildRequest Step(const sim::InputFrame& input) const;
    [[nodiscard]] BuildRequest Choose();

    BuildRow focus_ = BuildRow::Sets;
    std::optional<BuildPad> held_;
};

}
