#include "app/hud/race_hud.hpp"

#include <array>
#include <cmath>
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
constexpr float kCenterY = render::kUiHeight / 2.0F;
constexpr float kDialogRowGap = 2.0F;
constexpr float kCircuitTitleY = 60.0F;
constexpr Milliseconds kBlinkPeriod{400};
constexpr Milliseconds kBlinkLit{250};
constexpr render::Color kCountdownTint{.r = 1.0F, .g = 1.0F, .b = 0.0F};
constexpr render::Color kFocusTint = kCountdownTint;

float TimeWidth(const HudFonts& fonts) {
    std::array<char16_t, kRaceTimeLength> zero{};
    return fonts.Width(HudFont::Text, WriteRaceTime(Milliseconds{0}, zero));
}

}

RaceHud::RaceHud(HudStrings strings, HudFonts fonts)
    : strings_(std::move(strings)),
      fonts_(fonts),
      lapTimeX_(render::kUiWidth - (kLapTimeWidths * TimeWidth(fonts))),
      bestLapY_(kTopY + (kBestRow * fonts.Height(HudFont::Text))),
      gapY_(kTopY + (kGapRow * fonts.Height(HudFont::Text))) {}

void RaceHud::Tick(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                   std::size_t racer, Milliseconds elapsed) {
    pulse_.Advance(ReadHudValues(clock, progress, racer).place, elapsed);
}

void RaceHud::Layout(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                     std::size_t racer, std::span<const std::u16string> names,
                     std::span<const std::int32_t> circuitPoints, std::vector<HudText>& out) {
    out.clear();
    pool_.Clear();
    const HudValues values = ReadHudValues(clock, progress, racer);
    AddLap(values, out);
    AddPlace(values.place, out);
    if (values.isWrongWay) {
        out.push_back(Centered(strings_.wrongWay, kWrongWayY, {}));
    }
    if (values.banner) {
        AddBanner(*values.banner, out);
    }
    if (ShowsResults(clock)) {
        AddResults(clock, progress, racer, names, circuitPoints, out);
    }
}

void RaceHud::AddResults(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                         std::size_t racer, std::span<const std::u16string> names,
                         std::span<const std::int32_t> circuitPoints, std::vector<HudText>& out) {
    const ResultLabels labels{.finished = strings_.finished, .stillRacing = strings_.stillRacing};
    if (circuitPoints.empty()) {
        ReadResults(progress, results_);
        LayoutResults(results_, names, labels, std::nullopt, pool_, out);
        return;
    }
    const bool isLit = clock.PhaseTime() % kBlinkPeriod < kBlinkLit;
    if (clock.Phase() == sim::RacePhase::Results) {
        out.push_back(Centered(strings_.circuit, kCircuitTitleY, {}));
        ReadCircuitTotals(progress, circuitPoints, results_);
    } else {
        ReadCircuitRace(progress, results_);
    }
    LayoutResults(results_, names, labels, isLit ? std::nullopt : std::optional(racer), pool_, out);
}

void RaceHud::LayoutPause(PauseView view, bool restartsCircuit, std::vector<HudText>& out) const {
    if (view.dialog == PauseDialog::Confirm) {
        const std::array<std::u16string_view, 2> rows{strings_.yes, strings_.no};
        AddDialog(strings_.sure, rows, view.focus, out);
        return;
    }
    const std::array<std::u16string_view, 3> rows{
        strings_.resume, restartsCircuit ? strings_.restartCircuit : strings_.restart,
        strings_.quit};
    AddDialog(strings_.paused, rows, view.focus, out);
}

void RaceHud::AddDialog(std::u16string_view title, std::span<const std::u16string_view> rows,
                        std::size_t focus, std::vector<HudText>& out) const {
    const float rowHeight = fonts_.Height(HudFont::Text) + kDialogRowGap;
    const float firstRowY =
        kCenterY - std::floor(static_cast<float>(rows.size()) * rowHeight / 2.0F);
    out.push_back(Centered(title, firstRowY - rowHeight, {}));
    for (std::size_t row = 0; row < rows.size(); ++row) {
        out.push_back(Centered(rows[row], firstRowY + (static_cast<float>(row) * rowHeight),
                               row == focus ? kFocusTint : render::Color{}));
    }
}

void RaceHud::AddLap(const HudValues& values, std::vector<HudText>& out) {
    const std::u16string_view lap = pool_.Number(static_cast<std::size_t>(values.lap));
    const float lapX = lapTimeX_ - (fonts_.Width(HudFont::Text, lap) + kLapNumberGap);
    out.push_back({.text = strings_.lap,
                   .x = lapX - (fonts_.Width(HudFont::Text, strings_.lap) + kLapLabelGap),
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
    out.push_back(
        {.text = label, .x = lapTimeX_ - (fonts_.Width(HudFont::Text, label) + labelGap), .y = y});
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
                   .x = kPlaceX - fonts_.Width(HudFont::Numbers, digit),
                   .y = kTopY,
                   .scale = pulse_.Scale()});
}

void RaceHud::AddBanner(const BannerView& banner, std::vector<HudText>& out) const {
    const std::u16string_view text = BannerText(banner.text);
    const float width = fonts_.Width(HudFont::Numbers, text) * banner.scale;
    const float height = fonts_.Height(HudFont::Numbers) * banner.scale;
    out.push_back({.font = HudFont::Numbers,
                   .text = text,
                   .x = kCenterX - (width / 2.0F),
                   .y = gapY_ - (height / 4.0F),
                   .scale = banner.scale,
                   .tint = banner.text == HudBanner::Finish ? render::Color{} : kCountdownTint});
}

HudText RaceHud::Centered(std::u16string_view text, float y, render::Color tint) const {
    return {.text = text,
            .x = kCenterX - (fonts_.Width(HudFont::Text, text) / 2.0F),
            .y = y,
            .tint = tint};
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
