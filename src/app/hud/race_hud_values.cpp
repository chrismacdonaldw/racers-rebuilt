#include "app/hud/race_hud_values.hpp"

#include <algorithm>
#include <numeric>
#include <span>

namespace rr::app {
namespace {

constexpr Milliseconds kBannerStep{1000};
constexpr Milliseconds kGoTime{2000};
constexpr float kBannerScale = 1.8F;
constexpr Milliseconds kLapFlashTime{3000};
constexpr Milliseconds kLapFlashPeriod{250};
constexpr Milliseconds kGapTime{2000};

float ShrinkingScale(Milliseconds left) {
    return kBannerScale * static_cast<float>(left.count()) /
           static_cast<float>(kBannerStep.count());
}

std::optional<BannerView> ReadBanner(const sim::RaceClock& clock, bool hasFinished) {
    if (clock.IsTestDrive() && clock.Phase() != sim::RacePhase::Finished) {
        return std::nullopt;
    }
    switch (clock.Phase()) {
        case sim::RacePhase::Intro:
        case sim::RacePhase::Results:
            return std::nullopt;
        case sim::RacePhase::Countdown: {
            const std::int64_t step = std::min<std::int64_t>(clock.PhaseTime() / kBannerStep, 2);
            return BannerView{
                .text = static_cast<HudBanner>(step),
                .scale = ShrinkingScale(((step + 1) * kBannerStep) - clock.PhaseTime())};
        }
        case sim::RacePhase::Racing:
            if (clock.RaceTime() >= kGoTime) {
                return std::nullopt;
            }
            return BannerView{.text = HudBanner::Go,
                              .scale = clock.RaceTime() < kBannerStep
                                           ? kBannerScale
                                           : ShrinkingScale(kGoTime - clock.RaceTime())};
        case sim::RacePhase::Finished:
            if (!hasFinished) {
                return std::nullopt;
            }
            return BannerView{.text = HudBanner::Finish};
    }
    return std::nullopt;
}

bool IsFlashedOff(Milliseconds sinceLap) {
    return sinceLap < kLapFlashTime && (sinceLap / kLapFlashPeriod) % 2 == 1;
}

std::optional<Milliseconds> ReadLapTime(const sim::RaceClock& clock,
                                        const sim::RacerProgress& progress, int lap, int lapCount) {
    const std::span<const Milliseconds> times = progress.lapTimes;
    if (lap < lapCount) {
        const Milliseconds current = times[static_cast<std::size_t>(lap)];
        if (lap == 0 || current >= kLapFlashTime) {
            return current;
        }
        if (IsFlashedOff(current)) {
            return std::nullopt;
        }
        return times[static_cast<std::size_t>(lap - 1)];
    }
    const Milliseconds sinceFinish =
        clock.Phase() == sim::RacePhase::Finished ? clock.PhaseTime() : kLapFlashTime;
    if (IsFlashedOff(sinceFinish)) {
        return std::nullopt;
    }
    return times.back();
}

std::optional<Milliseconds> ReadBestLap(const sim::RacerProgress& progress, int completedLaps) {
    const auto completed =
        std::span(progress.lapTimes).first(static_cast<std::size_t>(completedLaps));
    if (completed.empty()) {
        return std::nullopt;
    }
    return std::ranges::min(completed);
}

std::optional<Milliseconds> ReadLapGap(const sim::RacerProgress& progress, int lap, int lapCount,
                                       Milliseconds gap) {
    if (lap == 0 || lap >= lapCount || gap <= Milliseconds{0} ||
        progress.lapTimes[static_cast<std::size_t>(lap)] >= kGapTime) {
        return std::nullopt;
    }
    return gap;
}

std::optional<Milliseconds> ReadTotal(const sim::RacerProgress& progress,
                                      const sim::TimeTrialGhosts* trial) {
    if (trial == nullptr) {
        return std::nullopt;
    }
    return std::accumulate(progress.lapTimes.begin(), progress.lapTimes.end(), Milliseconds{0});
}

int ReadPlace(const sim::RaceProgress& progress, std::size_t racer) {
    if (const std::optional<int> place = progress.OfRacer(racer).finishPlace) {
        return *place;
    }
    const std::span<const std::size_t> standings = progress.Standings();
    return static_cast<int>(std::ranges::find(standings, racer) - standings.begin()) + 1;
}

}

HudValues ReadHudValues(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                        std::size_t racer, const sim::TimeTrialGhosts* trial) {
    const sim::RacerProgress& own = progress.OfRacer(racer);
    const int lapCount = progress.LapCount();
    const int lap = std::min(own.laps.CompletedLaps(), lapCount);
    const Milliseconds gap =
        trial != nullptr
            ? trial->TimeBehind(std::span(own.lapTimes).first(static_cast<std::size_t>(lap)))
            : own.lapGap;
    return {.banner = ReadBanner(clock, own.finishPlace.has_value()),
            .lap = std::min(lap + 1, lapCount),
            .lapTime = ReadLapTime(clock, own, lap, lapCount),
            .bestLap = ReadBestLap(own, lap),
            .lapGap = ReadLapGap(own, lap, lapCount, gap),
            .total = ReadTotal(own, trial),
            .place = ReadPlace(progress, racer),
            .isWrongWay = own.laps.IsWrongWay()};
}

bool ShowsResults(const sim::RaceClock& clock) {
    return clock.Phase() == sim::RacePhase::Results ||
           (clock.Phase() == sim::RacePhase::Finished && clock.PhaseTime() > kResultsDelay);
}

}
