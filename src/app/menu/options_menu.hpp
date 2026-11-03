#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class OptionsPage : std::uint8_t { Main, Game, Video, Audio };

enum class OptionsRow : std::uint8_t {
    Game,
    Video,
    Audio,
    Player1,
    Player2,
    Language,
    Credits,
    Back,
    Opponents,
    VersusLaps,
    Driver,
    MusicVolume,
    SoundVolume,
    Stereo,
};

inline constexpr int kMaxOpponents = 5;
inline constexpr int kMaxVersusLaps = 5;
inline constexpr int kMaxVolumeStep = 20;

struct OptionValues {
    int opponents = kMaxOpponents;
    int versusLaps = 3;
    int musicStep = kMaxVolumeStep;
    int soundStep = kMaxVolumeStep;
    bool isStereo = true;

    friend bool operator==(const OptionValues&, const OptionValues&) = default;
};

enum class OptionsRequest : std::uint8_t {
    None,
    Warn,
    ChangeValue,
    Player1Controls,
    Player2Controls,
    Language,
    Leave,
    Credits
};

/// Video and audio ask for a warning first, and open once `Proceed` runs.
class OptionsMenu {
public:
    explicit OptionsMenu(const OptionValues& values) : values_(values) {}

    [[nodiscard]] OptionsRequest Tick(const sim::InputFrame& input);
    void Proceed();
    void FocusRow(OptionsRow row);

    /// Keeps the page and focus.
    void Show(const OptionValues& values) { values_ = values; }

    [[nodiscard]] OptionsPage Page() const { return page_; }
    [[nodiscard]] std::span<const OptionsRow> Rows() const;
    [[nodiscard]] OptionsRow Focus() const { return Rows()[focus_]; }
    [[nodiscard]] const OptionValues& Values() const { return values_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    void Open(OptionsPage page);
    [[nodiscard]] OptionsRequest Choose();
    [[nodiscard]] bool Step(int step);

    OptionsPage page_ = OptionsPage::Main;
    OptionsPage warnedPage_ = OptionsPage::Main;
    std::size_t focus_ = 0;
    OptionValues values_;
};

}
