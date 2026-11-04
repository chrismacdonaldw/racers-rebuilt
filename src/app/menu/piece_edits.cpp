#include "app/menu/piece_edits.hpp"

#include <expected>

#include "app/menu/build_sounds.hpp"
#include "app/menu/car_build_menu.hpp"

namespace rr::app {
namespace {

EditOutcome Changed(std::int32_t sound) {
    return {.sound = sound, .isChanged = true};
}

EditOutcome Refused(const CarBuilder& builder, std::size_t index, sim::PlacementFailure failure) {
    EditOutcome refused{.sound = kBuildRefusedSound};
    if (failure == sim::PlacementFailure::HoldsPieces) {
        refused.blockers = builder.Car().Supports(index);
    }
    return refused;
}

EditOutcome Settled(const CarBuilder& builder, std::size_t index,
                    const std::expected<void, sim::PlacementFailure>& edited, std::int32_t sound) {
    return edited ? Changed(sound) : Refused(builder, index, edited.error());
}

}

std::optional<PieceEditKey> PieceEditKeyOf(SDL_Keycode key, SDL_Keymod mod) {
    const bool control = (mod & SDL_KMOD_CTRL) != 0;
    if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
        return PieceEditKey::Remove;
    }
    if (control && key == SDLK_Z) {
        return (mod & SDL_KMOD_SHIFT) != 0 ? PieceEditKey::Redo : PieceEditKey::Undo;
    }
    if (control && key == SDLK_Y) {
        return PieceEditKey::Redo;
    }
    const std::optional<BuildRequest> request = BuildKeyRequest(key);
    if (request && request->command == BuildCommand::Undo) {
        return PieceEditKey::Undo;
    }
    return std::nullopt;
}

EditOutcome RemoveSelected(CarBuilder& builder, std::size_t index) {
    return Settled(builder, index, builder.Edit(sim::RemovePiece{.index = index}), kBuildUndoSound);
}

EditOutcome RecolourSelected(CarBuilder& builder, std::size_t index, std::uint8_t colour) {
    if (index < builder.Car().Pieces().size() && builder.Car().Pieces()[index].colour == colour) {
        return {};
    }
    return Settled(builder, index,
                   builder.Edit(sim::RecolourPiece{.index = index, .colour = colour}),
                   kBuildPlacedSound);
}

EditOutcome LiftSelected(CarBuilder& builder, std::size_t index) {
    return Settled(builder, index, builder.Lift(index), kBuildMoveSound);
}

EditOutcome DropOffCar(CarBuilder& builder) {
    return builder.RemoveLifted() ? Changed(kBuildUndoSound)
                                  : EditOutcome{.sound = kBuildRefusedSound};
}

EditOutcome StepHistory(CarBuilder& builder, PieceEditKey step) {
    const bool isRedo = step == PieceEditKey::Redo;
    if (isRedo ? builder.RedoEdit() : builder.UndoEdit()) {
        return Changed(isRedo ? kBuildPlacedSound : kBuildUndoSound);
    }
    return {.sound = kBuildRefusedSound};
}

}
