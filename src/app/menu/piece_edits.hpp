#pragma once

#include <SDL3/SDL_keycode.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "app/menu/car_builder.hpp"

namespace rr::app {

enum class PieceEditKey : std::uint8_t { Remove, Undo, Redo };

/// Unset for keys the palette builder leaves to the original controls.
[[nodiscard]] std::optional<PieceEditKey> PieceEditKeyOf(SDL_Keycode key, SDL_Keymod mod);

/// `blockers` are the pieces resting on one that couldn't come off.
struct EditOutcome {
    std::int32_t sound = 0;
    bool isChanged = false;
    std::vector<std::size_t> blockers;
};

[[nodiscard]] EditOutcome RemoveSelected(CarBuilder& builder, std::size_t index);
[[nodiscard]] EditOutcome RecolourSelected(CarBuilder& builder, std::size_t index,
                                           std::uint8_t colour);
[[nodiscard]] EditOutcome LiftSelected(CarBuilder& builder, std::size_t index);
[[nodiscard]] EditOutcome DropOffCar(CarBuilder& builder);
[[nodiscard]] EditOutcome StepHistory(CarBuilder& builder, PieceEditKey step);

}
