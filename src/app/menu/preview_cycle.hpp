#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/duration.hpp"

namespace rr::app {

inline constexpr Milliseconds kPreviewRaceTime{2000};
inline constexpr Milliseconds kPreviewFadeTime{250};

/// The circuit screen fades each shown preview in, and cycles through its races when `cycles`.
struct PreviewStyle {
    bool fades = false;
    bool cycles = false;
};

/// A race pick screen's turn through its races' previews, fading to black before each switch.
class PreviewCycle {
public:
    void Restart(PreviewStyle style);

    /// The race to show next, when this update moves on.
    [[nodiscard]] std::optional<std::size_t> Advance(Milliseconds elapsed);

    /// Call after showing a race. `restartsTimer` when the frame changed or the race is missing.
    void Shown(bool restartsTimer);

    [[nodiscard]] std::size_t Race() const { return race_; }

    /// From 0 for clear to 1 for black.
    [[nodiscard]] float FadeAlpha() const;

private:
    enum class Fade : std::uint8_t { None, ToBlack, FromBlack };

    void StartFade(Fade fade);

    PreviewStyle style_;
    std::size_t race_ = 0;
    Milliseconds timer_{0};
    Fade fade_ = Fade::None;
    Milliseconds fadeAge_{0};
};

}
