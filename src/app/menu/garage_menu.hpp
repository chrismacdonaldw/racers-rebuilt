#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "sim/input_frame.hpp"

namespace rr::app {

inline constexpr std::size_t kGarageCapacity = 100;

enum class GarageButton : std::uint8_t {
    New,
    Edit,
    Copy,
    Delete,
    TestDrive,
    Driver,
    Licence,
    Car,
    Back,
};

enum class GarageRequest : std::uint8_t {
    None,
    Step,
    Page,
    New,
    Copy,
    Delete,
    TestDrive,
    Driver,
    Licence,
    Car,
    Leave,
};

/// Left and right step the racer on the main page. TEST DRIVE stays off until TEST races.
class GarageMenu {
public:
    explicit GarageMenu(std::size_t racers);

    [[nodiscard]] GarageRequest Tick(const sim::InputFrame& input);

    /// After the store changes: shows `racer` and moves focus to `focus`, or the first button on.
    void Refresh(std::size_t racers, std::size_t racer, GarageButton focus);

    [[nodiscard]] std::span<const GarageButton> Buttons() const;
    [[nodiscard]] bool IsEnabled(GarageButton button) const;
    [[nodiscard]] bool IsEditing() const { return isEditing_; }
    [[nodiscard]] std::size_t Racers() const { return racers_; }
    [[nodiscard]] std::size_t Racer() const { return racer_; }
    [[nodiscard]] GarageButton Focus() const { return focus_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    void MoveFocus(int step);
    void Open(bool isEditing);
    [[nodiscard]] GarageRequest Step(std::size_t forward);
    [[nodiscard]] GarageRequest Choose();

    std::size_t racers_;
    std::size_t racer_ = 0;
    bool isEditing_ = false;
    GarageButton focus_ = GarageButton::New;
};

}
