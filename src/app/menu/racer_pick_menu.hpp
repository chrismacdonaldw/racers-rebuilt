#pragma once

#include <cstddef>
#include <cstdint>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class RacerPickRow : std::uint8_t { Racer, Config, Start, Back, Count };

enum class RacerPickRequest : std::uint8_t { None, Step, Config, Start, Leave };

/// Opens on START, as the original does. START does nothing while there's no racer.
class RacerPickMenu {
public:
    RacerPickMenu(std::size_t racers, std::size_t selected);

    [[nodiscard]] RacerPickRequest Tick(const sim::InputFrame& input);
    void FocusOn(RacerPickRow row) { focus_ = row; }

    [[nodiscard]] std::size_t Racer() const { return racer_; }
    [[nodiscard]] RacerPickRow Focus() const { return focus_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    [[nodiscard]] RacerPickRequest Step(std::size_t forward);
    [[nodiscard]] RacerPickRequest Choose() const;

    std::size_t racers_;
    std::size_t racer_;
    RacerPickRow focus_ = RacerPickRow::Start;
};

}
