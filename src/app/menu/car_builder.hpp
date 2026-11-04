#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

#include "assets/piece_catalog.hpp"
#include "sim/car_build/build_history.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/car_build/piece_cursor.hpp"

namespace rr::app {

inline constexpr std::size_t kBuildDirections = 8;
inline constexpr std::size_t kBuildViews = 8;

enum class PlaceResult : std::uint8_t { Placed, Refused, TooManyPieces };

/// `catalog` must outlive it.
class CarBuilder {
public:
    CarBuilder(const assets::PieceCatalog& catalog, sim::BuiltCar car, std::size_t set);

    void SelectSet(std::size_t set);
    void Choose(std::size_t choice);

    /// Holds the set's choice in `colour` instead of its own.
    void Pick(std::size_t set, std::size_t choice, std::uint8_t colour);
    void PutAt(sim::PiecePlacement placement);
    /// Empties the hand, putting a lifted piece back where it was.
    void Release();

    /// `direction` counts eighth turns clockwise from up on screen, `view` the camera's slot.
    bool Move(std::size_t direction, std::size_t view);
    void Rotate();
    PlaceResult Place();

    /// What `Place` would do, without placing.
    [[nodiscard]] PlaceResult Check() const;

    [[nodiscard]] int RestPlates() const;

    /// Puts the newest piece back in the cursor, reselecting the set and choice it came from.
    bool Undo();

    /// Sends every change through an undo history from now on, or stops and forgets it.
    void RecordEdits(bool records);

    /// Takes a placed piece off the car into the hand. `Place` then moves it.
    std::expected<void, sim::PlacementFailure> Lift(std::size_t index);
    [[nodiscard]] bool IsLifted() const { return lifted_.has_value(); }
    bool RemoveLifted();

    std::expected<void, sim::PlacementFailure> Edit(const sim::BuildEdit& edit);
    bool UndoEdit() { return history_.Undo(car_); }
    bool RedoEdit() { return history_.Redo(car_); }

    [[nodiscard]] const sim::BuiltCar& Car() const { return car_; }
    [[nodiscard]] const sim::PieceCursor& Cursor() const { return cursor_; }
    [[nodiscard]] std::size_t Set() const { return set_; }
    [[nodiscard]] std::size_t Choice() const { return choice_; }
    [[nodiscard]] std::uint8_t Colour() const { return colour_; }

private:
    struct Lifted {
        std::size_t index = 0;
        sim::CarPiece piece;
    };

    std::optional<Lifted> PutBack();

    const assets::PieceCatalog* catalog_;
    sim::BuiltCar car_;
    sim::BuildHistory history_;
    bool recordsEdits_ = false;
    std::optional<Lifted> lifted_;
    sim::PieceCursor cursor_;
    std::size_t set_ = 0;
    std::size_t choice_ = 0;
    std::uint8_t colour_ = 0;
};

}
