#include "app/hud/race_hud.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "app/hud/race_time_text.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr float kTopY = 7.0F;
constexpr float kCenterX = render::kUiWidth / 2.0F;
constexpr float kPlaceX = render::kUiWidth / 10.0F;
constexpr float kPlaceSuffixGap = 5.0F;
constexpr float kWrongWayY = render::kUiHeight / 5.0F;
constexpr float kLapTimeWidths = 11.0F / 8.0F;
constexpr float kLapNumberGap = 14.0F;
constexpr float kLapLabelGap = 6.0F;
constexpr float kBestLabelGap = 14.0F;
constexpr float kGapLabelGap = 4.0F;
constexpr float kBestRow = 7.0F / 8.0F;
constexpr float kGapRow = 21.0F / 8.0F;
constexpr float kResultsTop = 132.0F;
constexpr float kResultsRowHeight = 32.0F;
constexpr std::size_t kResultRows = 6;
constexpr float kRankX = 20.0F;
constexpr float kNameX = 50.0F;
constexpr float kStatusX = 500.0F;
constexpr float kBehindX = 520.0F;
constexpr render::Color kCountdownTint{.r = 1.0F, .g = 1.0F, .b = 0.0F};

float TimeWidth(const HudMetrics& metrics) {
    std::array<char16_t, kRaceTimeLength> zero{};
    return metrics.Width(HudFont::Text, WriteRaceTime(Milliseconds{0}, zero));
}

}

RaceHud::RaceHud(HudStrings strings, const HudMetrics& metrics)
    : strings_(std::move(strings)),
      metrics_(&metrics),
      lapTimeX_(render::kUiWidth - (kLapTimeWidths * TimeWidth(metrics))),
      bestLapY_(kTopY + (kBestRow * metrics.Height(HudFont::Text))),
      gapY_(kTopY + (kGapRow * metrics.Height(HudFont::Text))) {}

void RaceHud::Tick(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                   std::size_t racer, Milliseconds elapsed) {
    pulse_.Advance(ReadHudValues(clock, progress, racer).place, elapsed);
}

void RaceHud::Layout(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                     std::size_t racer, std::span<const std::u16string> names,
                     std::vector<HudText>& out) {
    out.clear();
    pool_.Clear();
    const HudValues values = ReadHudValues(clock, progress, racer);
    AddLap(values, out);
    AddPlace(values.place, out);
    if (values.isWrongWay) {
        out.push_back({.text = strings_.wrongWay,
                       .x = kCenterX - (metrics_->Width(HudFont::Text, strings_.wrongWay) / 2.0F),
                       .y = kWrongWayY});
    }
    if (values.banner) {
        AddBanner(*values.banner, out);
    }
    if (ShowsResults(clock)) {
        ReadResults(progress, results_);
        AddResults(names, out);
    }
}

void RaceHud::AddLap(const HudValues& values, std::vector<HudText>& out) {
    const std::u16string_view lap = pool_.Number(static_cast<std::size_t>(values.lap));
    const float lapX = lapTimeX_ - (metrics_->Width(HudFont::Text, lap) + kLapNumberGap);
    out.push_back({.text = strings_.lap,
                   .x = lapX - (metrics_->Width(HudFont::Text, strings_.lap) + kLapLabelGap),
                   .y = kTopY});
    out.push_back({.text = lap, .x = lapX, .y = kTopY});
    if (values.lapTime) {
        out.push_back({.text = pool_.Time(*values.lapTime), .x = lapTimeX_, .y = kTopY});
    }
    if (values.bestLap) {
        AddLabelledTime(strings_.best, kBestLabelGap, *values.bestLap, bestLapY_, out);
    }
    if (values.lapGap) {
        AddLabelledTime(strings_.gap, kGapLabelGap, *values.lapGap, gapY_, out);
    }
}

void RaceHud::AddLabelledTime(std::u16string_view label, float labelGap, Milliseconds time, float y,
                              std::vector<HudText>& out) {
    out.push_back({.text = label,
                   .x = lapTimeX_ - (metrics_->Width(HudFont::Text, label) + labelGap),
                   .y = y});
    out.push_back({.text = pool_.Time(time), .x = lapTimeX_, .y = y});
}

void RaceHud::AddPlace(int place, std::vector<HudText>& out) {
    if (place < 1 || std::cmp_greater(place, kShownPlaces)) {
        return;
    }
    out.push_back({.text = strings_.placeSuffixes[static_cast<std::size_t>(place - 1)],
                   .x = kPlaceX + kPlaceSuffixGap,
                   .y = kTopY});
    const std::u16string_view digit = pool_.Number(static_cast<std::size_t>(place));
    out.push_back({.font = HudFont::Numbers,
                   .text = digit,
                   .x = kPlaceX - metrics_->Width(HudFont::Numbers, digit),
                   .y = kTopY,
                   .scale = pulse_.Scale()});
}

void RaceHud::AddBanner(const BannerView& banner, std::vector<HudText>& out) const {
    const std::u16string_view text = BannerText(banner.text);
    const float width = metrics_->Width(HudFont::Numbers, text) * banner.scale;
    const float height = metrics_->Height(HudFont::Numbers) * banner.scale;
    out.push_back({.font = HudFont::Numbers,
                   .text = text,
                   .x = kCenterX - (width / 2.0F),
                   .y = gapY_ - (height / 4.0F),
                   .scale = banner.scale,
                   .tint = banner.text == HudBanner::Finish ? render::Color{} : kCountdownTint});
}

void RaceHud::AddResults(std::span<const std::u16string> names, std::vector<HudText>& out) {
    const std::size_t rows = std::min(results_.size(), kResultRows);
    for (std::size_t rank = 0; rank < rows; ++rank) {
        const ResultRow& row = results_[rank];
        const float y = kResultsTop + (static_cast<float>(rank) * kResultsRowHeight);
        out.push_back({.text = pool_.Number(rank + 1), .x = kRankX, .y = y});
        if (row.racer < names.size()) {
            out.push_back({.text = names[row.racer], .x = kNameX, .y = y});
        }
        switch (row.status) {
            case ResultStatus::Winner:
                out.push_back({.text = strings_.finished, .x = kStatusX, .y = y});
                break;
            case ResultStatus::Behind:
                out.push_back({.text = u"+", .x = kStatusX, .y = y});
                out.push_back({.text = pool_.Time(row.behind), .x = kBehindX, .y = y});
                break;
            case ResultStatus::Racing:
                out.push_back({.text = strings_.stillRacing, .x = kStatusX, .y = y});
                break;
        }
    }
}

std::u16string_view RaceHud::BannerText(HudBanner banner) const {
    switch (banner) {
        case HudBanner::Three:
            return u"3";
        case HudBanner::Two:
            return u"2";
        case HudBanner::One:
            return u"1";
        case HudBanner::Go:
            return strings_.go;
        case HudBanner::Finish:
            break;
    }
    return strings_.finish;
}

}
