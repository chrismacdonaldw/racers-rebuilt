#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class CarRow : std::uint8_t { PartSet, Build, RemoveBricks, QuickBuild, Done, Back };

inline constexpr std::size_t kCarRows = 6;

enum class CarRequest : std::uint8_t {
    None,
    PreviousSet,
    NextSet,
    Build,
    RemoveBricks,
    QuickBuild,
    Done,
    Leave,
};

class EditCarMenu {
public:
    [[nodiscard]] CarRequest Tick(const sim::InputFrame& input);

    /// Moves focus down to the next row on when the focused row turns off.
    void Enable(CarRow row, bool isEnabled);

    [[nodiscard]] bool IsEnabled(CarRow row) const;
    [[nodiscard]] CarRow Focus() const { return focus_; }
    /// False, leaving the focus, for a row that's off.
    [[nodiscard]] bool FocusOn(CarRow row);

private:
    void MoveFocus(int step);
    [[nodiscard]] CarRequest Choose() const;

    std::array<bool, kCarRows> enabled_{true, true, true, true, true, true};
    CarRow focus_ = CarRow::Build;
};

}
