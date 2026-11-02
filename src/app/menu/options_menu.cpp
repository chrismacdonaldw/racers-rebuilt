#include "app/menu/options_menu.hpp"

#include <algorithm>
#include <array>

namespace rr::app {
namespace {

constexpr std::uint64_t kHashBase = 32;

constexpr std::array kMainRows{OptionsRow::Game,    OptionsRow::Video,   OptionsRow::Audio,
                               OptionsRow::Player1, OptionsRow::Player2, OptionsRow::Language,
                               OptionsRow::Credits, OptionsRow::Back};
constexpr std::array kGameRows{OptionsRow::Opponents, OptionsRow::VersusLaps, OptionsRow::Back};
constexpr std::array kVideoRows{OptionsRow::Driver, OptionsRow::Back};
constexpr std::array kAudioRows{OptionsRow::MusicVolume, OptionsRow::SoundVolume,
                                OptionsRow::Stereo, OptionsRow::Back};

bool StepWithin(int& value, int step, int low, int high) {
    const int stepped = std::clamp(value + step, low, high);
    const bool changed = stepped != value;
    value = stepped;
    return changed;
}

}

OptionsRequest OptionsMenu::Tick(const sim::InputFrame& input) {
    const auto rowCount = Rows().size();
    if (input.WasPressed(sim::MenuAction::Up)) {
        focus_ = (focus_ + rowCount - 1) % rowCount;
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        focus_ = (focus_ + 1) % rowCount;
    }
    if (input.WasPressed(sim::MenuAction::Back)) {
        focus_ = rowCount - 1;
        return Choose();
    }
    if (input.WasPressed(sim::MenuAction::Confirm)) {
        return Choose();
    }
    const int step = (input.WasPressed(sim::MenuAction::Right) ? 1 : 0) -
                     (input.WasPressed(sim::MenuAction::Left) ? 1 : 0);
    return step != 0 && Step(step) ? OptionsRequest::ChangeValue : OptionsRequest::None;
}

void OptionsMenu::Proceed() {
    Open(warnedPage_);
}

std::span<const OptionsRow> OptionsMenu::Rows() const {
    switch (page_) {
        case OptionsPage::Game:
            return kGameRows;
        case OptionsPage::Video:
            return kVideoRows;
        case OptionsPage::Audio:
            return kAudioRows;
        case OptionsPage::Main:
            break;
    }
    return kMainRows;
}

std::uint64_t OptionsMenu::StateHash() const {
    auto hash = static_cast<std::uint64_t>(page_);
    for (const int part : {static_cast<int>(focus_), values_.opponents, values_.versusLaps,
                           values_.musicStep, values_.soundStep, values_.isStereo ? 1 : 0}) {
        hash = (hash * kHashBase) + static_cast<std::uint64_t>(part);
    }
    return hash;
}

void OptionsMenu::Open(OptionsPage page) {
    page_ = page;
    focus_ = 0;
}

OptionsRequest OptionsMenu::Choose() {
    switch (Focus()) {
        case OptionsRow::Game:
            Open(OptionsPage::Game);
            return OptionsRequest::None;
        case OptionsRow::Video:
            warnedPage_ = OptionsPage::Video;
            return OptionsRequest::Warn;
        case OptionsRow::Audio:
            warnedPage_ = OptionsPage::Audio;
            return OptionsRequest::Warn;
        case OptionsRow::Player1:
            return OptionsRequest::Player1Controls;
        case OptionsRow::Player2:
            return OptionsRequest::Player2Controls;
        case OptionsRow::Language:
        case OptionsRow::Credits:
            return OptionsRequest::NotBuilt;
        case OptionsRow::Back:
            if (page_ == OptionsPage::Main) {
                return OptionsRequest::Leave;
            }
            Open(OptionsPage::Main);
            return OptionsRequest::None;
        default:
            return OptionsRequest::None;
    }
}

bool OptionsMenu::Step(int step) {
    switch (Focus()) {
        case OptionsRow::Opponents:
            return StepWithin(values_.opponents, step, 0, kMaxOpponents);
        case OptionsRow::VersusLaps:
            return StepWithin(values_.versusLaps, step, 1, kMaxVersusLaps);
        case OptionsRow::MusicVolume:
            return StepWithin(values_.musicStep, step, 0, kMaxVolumeStep);
        case OptionsRow::SoundVolume:
            return StepWithin(values_.soundStep, step, 0, kMaxVolumeStep);
        case OptionsRow::Stereo: {
            const bool isStereo = step < 0;
            const bool changed = isStereo != values_.isStereo;
            values_.isStereo = isStereo;
            return changed;
        }
        default:
            return false;
    }
}

}
