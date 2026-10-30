#pragma once

#include <cstddef>
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
    /// `metrics` must outlive the HUD.
    RaceHud(HudStrings strings, const HudMetrics& metrics);

    /// Runs once per tick, after the session's.
    void Tick(const sim::RaceClock& clock, const sim::RaceProgress& progress, std::size_t racer,
              Milliseconds elapsed);

    /// Replaces `out` without allocating once warm. `names` holds one name per racer.
    void Layout(const sim::RaceClock& clock, const sim::RaceProgress& progress, std::size_t racer,
                std::span<const std::u16string> names, std::vector<HudText>& out);

private:
    void AddLap(const HudValues& values, std::vector<HudText>& out);
    void AddLabelledTime(std::u16string_view label, float labelGap, Milliseconds time, float y,
                         std::vector<HudText>& out);
    void AddPlace(int place, std::vector<HudText>& out);
    void AddBanner(const BannerView& banner, std::vector<HudText>& out) const;
    [[nodiscard]] std::u16string_view BannerText(HudBanner banner) const;

    HudStrings strings_;
    const HudMetrics* metrics_;
    PlacePulse pulse_;
    TextPool pool_;
    std::vector<ResultRow> results_;
    float lapTimeX_ = 0.0F;
    float bestLapY_ = 0.0F;
    float gapY_ = 0.0F;
};

}
