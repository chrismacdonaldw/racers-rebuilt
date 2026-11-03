#include "app/hud/race_hud.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

#include "app/hud/race_time_text.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr float kTopY = 7.0F;
constexpr float kCenterX = render::kUiWidth / 2.0F;
constexpr float kPlaceX = render::kUiWidth / 10.0F;
constexpr float kPlaceSuffixGap = 5.0F;
constexpr float kWrongWayFraction = 1.0F / 5.0F;
constexpr float kLapTimeWidths = 11.0F / 8.0F;
constexpr float kLapNumberGap = 14.0F;
constexpr float kLapLabelGap = 6.0F;
constexpr float kBestLabelGap = 14.0F;
constexpr float kGapLabelGap = 4.0F;
constexpr float kBestRow = 7.0F / 8.0F;
constexpr float kTotalRow = 14.0F / 8.0F;
constexpr float kGapRow = 21.0F / 8.0F;
constexpr float kCenterY = render::kUiHeight / 2.0F;
constexpr float kDialogRowGap = 2.0F;
constexpr Milliseconds kDialogBlinkPeriod{500};
constexpr Milliseconds kDialogBlinkLit{250};
constexpr float kDialogFocusAlpha = 191.0F / 255.0F;
constexpr float kDialogFocusSwing = 64.0F / 255.0F;
constexpr render::Color kDialogFocusTint{.r = 1.0F, .g = 1.0F, .b = 36.0F / 255.0F};
constexpr render::Color kDialogRowTint{
    .r = 127.0F / 255.0F, .g = 127.0F / 255.0F, .b = 18.0F / 255.0F};
constexpr float kCircuitTitleY = 60.0F;
constexpr Milliseconds kBlinkPeriod{400};
constexpr Milliseconds kBlinkLit{250};
constexpr render::Color kCountdownTint{.r = 1.0F, .g = 1.0F, .b = 0.0F};
constexpr Milliseconds kDemoBlinkPeriod{1000};
constexpr Milliseconds kDemoBlinkLit{750};
constexpr float kDemoBottomGap = 5.0F;

float TimeWidth(const HudFonts& fonts) {
    std::array<char16_t, kRaceTimeLength> zero{};
    return fonts.Width(HudFont::Text, WriteRaceTime(Milliseconds{0}, zero));
}

}

RaceHud::RaceHud(HudStrings strings, HudFonts fonts, HudPlacement placement)
    : strings_(std::move(strings)),
      fonts_(fonts),
      placement_(placement),
      lapTimeX_(render::kUiWidth - (kLapTimeWidths * TimeWidth(fonts) * placement.textScale)),
      topY_(placement.top + kTopY),
      bestLapY_(topY_ + (kBestRow * fonts.Height(HudFont::Text) * placement.textScale)),
      totalY_(topY_ + (kTotalRow * fonts.Height(HudFont::Text) * placement.textScale)),
      gapY_(topY_ + (kGapRow * fonts.Height(HudFont::Text) * placement.textScale)) {}

void RaceHud::Tick(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                   std::size_t racer, Milliseconds elapsed) {
    pulse_.Advance(ReadHudValues(clock, progress, racer).place, elapsed);
}

void RaceHud::Layout(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                     const HudRacer& racer, std::vector<HudText>& out) {
    out.clear();
    pool_.Clear();
    const HudValues values = ReadHudValues(clock, progress, racer.slot, racer.trial);
    const bool showsRaceInfo = !racer.isGhost && !racer.hidesRaceInfo;
    if (showsRaceInfo) {
        AddLap(values, out);
        if (values.total) {
            AddLabelledTime(strings_.total, kBestLabelGap, *values.total, totalY_, out);
        } else {
            AddPlace(values.place, out);
        }
    }
    if (values.isWrongWay && showsRaceInfo) {
        const float wrongWayY =
            placement_.top + ((placement_.bottom - placement_.top) * kWrongWayFraction);
        out.push_back(Centered(strings_.wrongWay, wrongWayY, {}, placement_.textScale));
    }
    if (values.banner) {
        AddBanner(*values.banner, out);
    }
}

void RaceHud::LayoutResults(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                            std::size_t racer, std::span<const std::u16string> names,
                            std::span<const std::int32_t> circuitPoints,
                            std::vector<HudText>& out) {
    if (!ShowsResults(clock)) {
        return;
    }
    const ResultLabels labels{.finished = strings_.finished, .stillRacing = strings_.stillRacing};
    if (circuitPoints.empty()) {
        ReadResults(progress, results_);
        app::LayoutResults(results_, names, labels, std::nullopt, pool_, out);
        return;
    }
    const bool isLit = clock.PhaseTime() % kBlinkPeriod < kBlinkLit;
    if (clock.Phase() == sim::RacePhase::Results) {
        out.push_back(Centered(strings_.circuit, kCircuitTitleY, {}));
        ReadCircuitTotals(progress, circuitPoints, results_);
    } else {
        ReadCircuitRace(progress, results_);
    }
    app::LayoutResults(results_, names, labels, isLit ? std::nullopt : std::optional(racer), pool_,
                       out);
}

void RaceHud::LayoutDemo(Milliseconds shown, std::vector<HudText>& out) const {
    if (shown % kDemoBlinkPeriod < kDemoBlinkLit) {
        const float y = render::kUiHeight - fonts_.Height(HudFont::Text) - kDemoBottomGap;
        out.push_back(Centered(strings_.demo, y, {}));
    }
}

void RaceHud::TickPause(Milliseconds elapsed) {
    dialogTime_ += elapsed;
    if (dialogTime_ >= kDialogBlinkPeriod) {
        dialogTime_ = Milliseconds{0};
    }
}

void RaceHud::LayoutPause(PauseView view, bool restartsCircuit, std::vector<HudText>& out) const {
    const std::array<std::u16string_view, 2> answers{strings_.yes, strings_.no};
    const std::array<std::u16string_view, 3> rows{
        strings_.resume, restartsCircuit ? strings_.restartCircuit : strings_.restart,
        strings_.quit};
    switch (view.dialog) {
        case PauseDialog::Menu:
            AddDialog(std::array<std::u16string_view, 1>{strings_.paused}, rows, view.focus, out);
            return;
        case PauseDialog::Confirm:
            AddDialog(std::array<std::u16string_view, 1>{strings_.sure}, answers, view.focus, out);
            return;
        case PauseDialog::RecordStands:
            AddDialog(std::array<std::u16string_view, 2>{strings_.recordStands[0],
                                                         strings_.recordStands[1]},
                      answers, view.focus, out);
            return;
        case PauseDialog::RecordBeaten:
            AddDialog(std::array<std::u16string_view, 2>{strings_.recordBeaten[0],
                                                         strings_.recordBeaten[1]},
                      answers, view.focus, out);
            return;
    }
}

void RaceHud::AddDialog(std::span<const std::u16string_view> titles,
                        std::span<const std::u16string_view> rows, std::size_t focus,
                        std::vector<HudText>& out) const {
    const float rowHeight = fonts_.Height(HudFont::Text) + kDialogRowGap;
    const float firstRowY =
        kCenterY - std::floor(static_cast<float>(rows.size()) * rowHeight / 2.0F);
    const float lead = titles.size() > 1 ? std::floor(rowHeight / 2.0F) : 0.0F;
    float titleY = firstRowY - rowHeight - lead;
    for (const std::u16string_view title : titles) {
        out.push_back(Centered(title, titleY, {}));
        titleY += rowHeight;
    }
    const std::optional<render::Color> focusTint = FocusTint();
    for (std::size_t row = 0; row < rows.size(); ++row) {
        const float y = titleY + (static_cast<float>(row) * rowHeight);
        if (row != focus) {
            out.push_back(Centered(rows[row], y, kDialogRowTint));
        } else if (focusTint) {
            out.push_back(Centered(rows[row], y, *focusTint));
        }
    }
}

std::optional<render::Color> RaceHud::FocusTint() const {
    if (dialogTime_ >= kDialogBlinkLit) {
        return std::nullopt;
    }
    const float turn =
        static_cast<float>(dialogTime_.count()) / static_cast<float>(kDialogBlinkPeriod.count());
    render::Color tint = kDialogFocusTint;
    tint.a =
        kDialogFocusAlpha + (kDialogFocusSwing * std::sin(2.0F * std::numbers::pi_v<float> * turn));
    return tint;
}

void RaceHud::AddLap(const HudValues& values, std::vector<HudText>& out) {
    const float scale = placement_.textScale;
    const std::u16string_view lap = pool_.Number(static_cast<std::size_t>(values.lap));
    const float lapX = lapTimeX_ - (TextWidth(HudFont::Text, lap) + kLapNumberGap);
    out.push_back({.text = strings_.lap,
                   .x = lapX - (TextWidth(HudFont::Text, strings_.lap) + kLapLabelGap),
                   .y = topY_,
                   .scale = scale});
    out.push_back({.text = lap, .x = lapX, .y = topY_, .scale = scale});
    if (values.lapTime) {
        out.push_back(
            {.text = pool_.Time(*values.lapTime), .x = lapTimeX_, .y = topY_, .scale = scale});
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
                   .x = lapTimeX_ - (TextWidth(HudFont::Text, label) + labelGap),
                   .y = y,
                   .scale = placement_.textScale});
    out.push_back(
        {.text = pool_.Time(time), .x = lapTimeX_, .y = y, .scale = placement_.textScale});
}

void RaceHud::AddPlace(int place, std::vector<HudText>& out) {
    if (place < 1 || std::cmp_greater(place, kShownPlaces)) {
        return;
    }
    out.push_back({.text = strings_.placeSuffixes[static_cast<std::size_t>(place - 1)],
                   .x = kPlaceX + kPlaceSuffixGap,
                   .y = topY_,
                   .scale = placement_.textScale});
    const std::u16string_view digit = pool_.Number(static_cast<std::size_t>(place));
    out.push_back({.font = HudFont::Numbers,
                   .text = digit,
                   .x = kPlaceX - TextWidth(HudFont::Numbers, digit),
                   .y = topY_,
                   .scale = pulse_.Scale() * placement_.textScale});
}

void RaceHud::AddBanner(const BannerView& banner, std::vector<HudText>& out) const {
    const std::u16string_view text = BannerText(banner.text);
    const float scale = banner.scale * placement_.bannerScale;
    const float width = fonts_.Width(HudFont::Numbers, text) * scale;
    const float height = fonts_.Height(HudFont::Numbers) * scale;
    out.push_back({.font = HudFont::Numbers,
                   .text = text,
                   .x = kCenterX - (width / 2.0F),
                   .y = gapY_ - (height / 4.0F),
                   .scale = scale,
                   .tint = banner.text == HudBanner::Finish ? render::Color{} : kCountdownTint});
}

HudText RaceHud::Centered(std::u16string_view text, float y, render::Color tint,
                          float scale) const {
    return {.text = text,
            .x = kCenterX - (fonts_.Width(HudFont::Text, text) / 2.0F),
            .y = y,
            .scale = scale,
            .tint = tint};
}

float RaceHud::TextWidth(HudFont font, std::u16string_view text) const {
    return fonts_.Width(font, text) * placement_.textScale;
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
