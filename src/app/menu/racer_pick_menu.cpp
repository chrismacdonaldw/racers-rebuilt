#include "app/menu/racer_pick_menu.hpp"

namespace rr::app {
namespace {

constexpr auto kRowCount = static_cast<std::size_t>(RacerPickRow::Count);
constexpr std::uint64_t kHashBase = 8;

RacerPickRow Moved(RacerPickRow row, std::size_t forward) {
    return static_cast<RacerPickRow>((static_cast<std::size_t>(row) + forward) % kRowCount);
}

}

RacerPickMenu::RacerPickMenu(std::size_t racers, std::size_t selected)
    : racers_(racers), racer_(selected < racers ? selected : 0) {}

RacerPickRequest RacerPickMenu::Tick(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        return RacerPickRequest::Leave;
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        focus_ = Moved(focus_, kRowCount - 1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        focus_ = Moved(focus_, 1);
    }
    if (focus_ == RacerPickRow::Racer && input.WasPressed(sim::MenuAction::Left)) {
        return Step(racers_ - 1);
    }
    if (focus_ == RacerPickRow::Racer && input.WasPressed(sim::MenuAction::Right)) {
        return Step(1);
    }
    return input.WasPressed(sim::MenuAction::Confirm) ? Choose() : RacerPickRequest::None;
}

std::uint64_t RacerPickMenu::StateHash() const {
    return (racer_ * kHashBase) + static_cast<std::uint64_t>(focus_);
}

RacerPickRequest RacerPickMenu::Step(std::size_t forward) {
    if (racers_ < 2) {
        return RacerPickRequest::None;
    }
    racer_ = (racer_ + forward) % racers_;
    return RacerPickRequest::Step;
}

RacerPickRequest RacerPickMenu::Choose() const {
    switch (focus_) {
        case RacerPickRow::Config:
            return RacerPickRequest::Config;
        case RacerPickRow::Start:
            return racers_ == 0 ? RacerPickRequest::None : RacerPickRequest::Start;
        case RacerPickRow::Back:
            return RacerPickRequest::Leave;
        default:
            return RacerPickRequest::None;
    }
}

}
