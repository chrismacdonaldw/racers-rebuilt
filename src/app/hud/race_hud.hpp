#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/hud/hud_strings.hpp"
#include "app/hud/hud_text.hpp"
#include "app/hud/place_pulse.hpp"
#include "app/hud/race_hud_values.hpp"
#include "app/hud/race_results.hpp"
#include "app/hud/text_pool.hpp"
#include "core/duration.hpp"
#include "sim/race/race_clock.hpp"
#include "sim/race/race_progress.hpp"

namespace rr::app {

/// One racer's full-screen HUD, placed as the original places it on a 640×480 screen.
class RaceHud {
public:
    RaceHud(HudStrings strings, HudFonts fonts);

    /// Runs once per tick, after the session's.
    void Tick(const sim::RaceClock& clock, const sim::RaceProgress& progress, std::size_t racer,
              Milliseconds elapsed);

    /// Replaces `out` without allocating once warm. `circuitPoints` is empty outside a circuit.
    void Layout(const sim::RaceClock& clock, const sim::RaceProgress& progress, std::size_t racer,
                std::span<const std::u16string> names, std::span<const std::int32_t> circuitPoints,
                std::vector<HudText>& out);

    /// Appends the pause menu's title and rows, highlighting row `focus`.
    void LayoutPause(std::size_t focus, std::vector<HudText>& out) const;

    [[nodiscard]] const HudFonts& Fonts() const { return fonts_; }

private:
    void AddLap(const HudValues& values, std::vector<HudText>& out);
    void AddLabelledTime(std::u16string_view label, float labelGap, Milliseconds time, float y,
                         std::vector<HudText>& out);
    void AddPlace(int place, std::vector<HudText>& out);
    void AddBanner(const BannerView& banner, std::vector<HudText>& out) const;
    void AddResults(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                    std::size_t racer, std::span<const std::u16string> names,
                    std::span<const std::int32_t> circuitPoints, std::vector<HudText>& out);
    [[nodiscard]] std::u16string_view BannerText(HudBanner banner) const;
    [[nodiscard]] HudText Centered(std::u16string_view text, float y, render::Color tint) const;

    HudStrings strings_;
    HudFonts fonts_;
    PlacePulse pulse_;
    TextPool pool_;
    std::vector<ResultRow> results_;
    float lapTimeX_ = 0.0F;
    float bestLapY_ = 0.0F;
    float gapY_ = 0.0F;
};

}
