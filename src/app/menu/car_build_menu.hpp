#pragma once

#include <SDL3/SDL_keycode.h>

#include <cstddef>
#include <cstdint>
#include <optional>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class BuildRow : std::uint8_t { Sets, Pieces, Move, Rotate, Place, Undo, Done };

inline constexpr std::size_t kBuildRows = 7;

enum class BuildCommand : std::uint8_t {
    None,
    PreviousSet,
    NextSet,
    PreviousPiece,
    NextPiece,
    Move,
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

/// The original's builder keys: arrows and the keypad move, keypad 5 turns, + and - place and undo.
[[nodiscard]] std::optional<BuildRequest> BuildKeyRequest(SDL_Keycode key);

/// Confirm on the move pad holds it, so the directions move the piece until Confirm or Back.
class CarBuildMenu {
public:
    [[nodiscard]] BuildRequest Tick(const sim::InputFrame& input);

    [[nodiscard]] BuildRow Focus() const { return focus_; }
    [[nodiscard]] bool IsMoving() const { return isMoving_; }

private:
    [[nodiscard]] BuildRequest TickMoving(const sim::InputFrame& input);
    [[nodiscard]] BuildRequest Step(const sim::InputFrame& input) const;
    [[nodiscard]] BuildRequest Choose();

    BuildRow focus_ = BuildRow::Sets;
    bool isMoving_ = false;
};

}
