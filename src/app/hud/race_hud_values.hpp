#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/duration.hpp"
#include "sim/race/race_clock.hpp"
#include "sim/race/race_progress.hpp"
#include "sim/time_trial/time_trial_ghosts.hpp"

namespace rr::app {

enum class HudBanner : std::uint8_t { Three, Two, One, Go, Finish };

struct BannerView {
    HudBanner text = HudBanner::Go;
    float scale = 1.0F;
};

struct HudValues {
    std::optional<BannerView> banner;
    int lap = 1;
    std::optional<Milliseconds> lapTime;
    std::optional<Milliseconds> bestLap;
    std::optional<Milliseconds> lapGap;
    std::optional<Milliseconds> total;
    int place = 1;
    bool isWrongWay = false;
};

/// What `racer`'s HUD shows, against `trial`'s ghosts when given. Missing times are hidden.
[[nodiscard]] HudValues ReadHudValues(const sim::RaceClock& clock,
                                      const sim::RaceProgress& progress, std::size_t racer,
                                      const sim::TimeTrialGhosts* trial = nullptr);

/// The results table replaces the finish banner this long after the finish.
inline constexpr Milliseconds kResultsDelay{5000};

[[nodiscard]] bool ShowsResults(const sim::RaceClock& clock);

}
