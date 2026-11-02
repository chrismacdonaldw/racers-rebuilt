#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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
#include "render/ui_layout.hpp"
#include "sim/race/race_clock.hpp"
#include "sim/race/race_progress.hpp"

namespace rr::app {

enum class PauseDialog : std::uint8_t { Menu, Confirm };

struct PauseView {
    PauseDialog dialog = PauseDialog::Menu;
    std::size_t focus = 0;
};

/// In UI units. Rows measure their y from `top`.
struct HudPlacement {
    float top = 0.0F;
    float bottom = render::kUiHeight;
    float textScale = 1.0F;
    float bannerScale = 1.0F;
};

/// Full screen for one player. With two, player 1 takes the top half and player 2 the bottom.
[[nodiscard]] HudPlacement PlacementFor(std::size_t player, std::size_t players);

/// One racer's HUD, placed as the original places it on a 640×480 screen.
class RaceHud {
public:
    RaceHud(HudStrings strings, HudFonts fonts, HudPlacement placement = {});

    /// Runs once per tick, after the session's.
    void Tick(const sim::RaceClock& clock, const sim::RaceProgress& progress, std::size_t racer,
              Milliseconds elapsed);

    /// Replaces `out` without allocating once warm.
    void Layout(const sim::RaceClock& clock, const sim::RaceProgress& progress, std::size_t racer,
                std::vector<HudText>& out);

    void TickPause(Milliseconds elapsed);

    /// `circuitPoints` is empty outside a circuit.
    void LayoutResults(const sim::RaceClock& clock, const sim::RaceProgress& progress,
                       std::size_t racer, std::span<const std::u16string> names,
                       std::span<const std::int32_t> circuitPoints, std::vector<HudText>& out);

    void LayoutPause(PauseView view, bool restartsCircuit, std::vector<HudText>& out) const;

    [[nodiscard]] const HudFonts& Fonts() const { return fonts_; }

private:
    void AddLap(const HudValues& values, std::vector<HudText>& out);
    void AddLabelledTime(std::u16string_view label, float labelGap, Milliseconds time, float y,
                         std::vector<HudText>& out);
    void AddPlace(int place, std::vector<HudText>& out);
    void AddBanner(const BannerView& banner, std::vector<HudText>& out) const;
    void AddDialog(std::u16string_view title, std::span<const std::u16string_view> rows,
                   std::size_t focus, std::vector<HudText>& out) const;
    [[nodiscard]] std::optional<render::Color> FocusTint() const;
    [[nodiscard]] std::u16string_view BannerText(HudBanner banner) const;
    [[nodiscard]] HudText Centered(std::u16string_view text, float y, render::Color tint,
                                   float scale = 1.0F) const;
    [[nodiscard]] float TextWidth(HudFont font, std::u16string_view text) const;

    HudStrings strings_;
    HudFonts fonts_;
    HudPlacement placement_;
    PlacePulse pulse_;
    TextPool pool_;
    std::vector<ResultRow> results_;
    float lapTimeX_ = 0.0F;
    float topY_ = 0.0F;
    float bestLapY_ = 0.0F;
    float gapY_ = 0.0F;
    Milliseconds dialogTime_{0};
};

}
