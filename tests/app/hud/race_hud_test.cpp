#include "app/hud/race_hud.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ring_race.hpp"

namespace rr::app {
namespace {

constexpr float kCharWidth = 10.0F;
constexpr float kNumberWidth = 20.0F;
constexpr float kTextHeight = 16.0F;
constexpr float kNumberHeight = 40.0F;

class FixedMetrics final : public HudMetrics {
public:
    [[nodiscard]] float Width(HudFont font, std::u16string_view text) const override {
        return static_cast<float>(text.size()) *
               (font == HudFont::Numbers ? kNumberWidth : kCharWidth);
    }

    [[nodiscard]] float Height(HudFont font) const override {
        return font == HudFont::Numbers ? kNumberHeight : kTextHeight;
    }
};

HudStrings EnglishStrings() {
    return {.placeSuffixes = {u"ST", u"ND", u"RD", u"TH", u"TH", u"TH", u"TH", u"TH"},
            .gap = u"-",
            .wrongWay = u"WRONG WAY",
            .best = u"BEST",
            .lap = u"LAP",
            .finish = u"FINISH",
            .go = u"GO",
            .finished = u"FINISHED",
            .stillRacing = u"STILL RACING"};
}

std::vector<std::u16string> Names() {
    return {u"ROCKET RACER", u"ANN DROID", u"ISLANDER"};
}

std::optional<HudText> Find(const std::vector<HudText>& texts, std::u16string_view text) {
    const auto found =
        std::ranges::find_if(texts, [&](const HudText& item) { return item.text == text; });
    return found == texts.end() ? std::nullopt : std::optional(*found);
}

float XOf(const std::vector<HudText>& texts, std::u16string_view text) {
    return Find(texts, text).transform([](const HudText& item) { return item.x; }).value_or(-1.0F);
}

float YOf(const std::vector<HudText>& texts, std::u16string_view text) {
    return Find(texts, text).transform([](const HudText& item) { return item.y; }).value_or(-1.0F);
}

HudText LastNumbers(const std::vector<HudText>& texts) {
    const auto found = std::ranges::find_last_if(
        texts, [](const HudText& item) { return item.font == HudFont::Numbers; });
    return found.empty() ? HudText{} : found.front();
}

TEST_CASE("the lap counter and lap time sit top right, right of their labels") {
    const FixedMetrics metrics;
    RaceHud hud(EnglishStrings(), metrics);
    const test::RingRace race(3, 3);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 0, Names(), texts);

    const float lapTimeX = 640.0F - (11.0F / 8.0F * 70.0F);
    CHECK(XOf(texts, u"0:00:00") == doctest::Approx(lapTimeX));
    CHECK(YOf(texts, u"0:00:00") == doctest::Approx(7.0F));
    CHECK(XOf(texts, u"1") == doctest::Approx(lapTimeX - 24.0F));
    CHECK(XOf(texts, u"LAP") == doctest::Approx(lapTimeX - 60.0F));
    CHECK_FALSE(Find(texts, u"BEST").has_value());
}

TEST_CASE("the place digit ends where its suffix starts, top left") {
    const FixedMetrics metrics;
    RaceHud hud(EnglishStrings(), metrics);
    test::RingRace race(3, 3);
    const std::array<Vector3, 3> positions{Vector3{.x = -9.0F}, Vector3{.x = -8.0F},
                                           Vector3{.x = -7.0F}};
    race.progress.Rank(race.graph, positions);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 0, Names(), texts);

    const auto digit = std::ranges::find_if(
        texts, [](const HudText& item) { return item.font == HudFont::Numbers; });
    REQUIRE(digit != texts.end());
    CHECK(digit->text == u"3");
    CHECK(digit->x == doctest::Approx(44.0F));
    CHECK(XOf(texts, u"RD") == doctest::Approx(69.0F));
}

TEST_CASE("the countdown is centred, yellow and in the number font") {
    const FixedMetrics metrics;
    RaceHud hud(EnglishStrings(), metrics);
    test::RingRace race(1, 3);
    race.clock.Advance(Milliseconds{2001}, false);
    race.clock.Advance(Milliseconds{2500}, false);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 0, Names(), texts);

    const HudText one = LastNumbers(texts);
    const float scale = 0.9F;
    CHECK(one.text == u"1");
    CHECK(one.scale == doctest::Approx(scale));
    CHECK(one.x == doctest::Approx(320.0F - (kNumberWidth * scale / 2.0F)));
    CHECK(one.y == doctest::Approx(7.0F + 42.0F - (kNumberHeight * scale / 4.0F)));
    CHECK(one.tint.b == 0.0F);
}

TEST_CASE("the results table lists each racer with its gap to the winner") {
    const FixedMetrics metrics;
    RaceHud hud(EnglishStrings(), metrics);
    test::RingRace race(3, 1);
    race.StartRacing();
    race.DriveLap(1);
    race.DriveLap(1);
    race.DriveLap(0);
    race.Wait(0, Milliseconds{2000});
    race.DriveLap(0);
    race.clock.Advance(Milliseconds{12}, true);
    race.clock.Advance(Milliseconds{5001}, false);
    const std::array<Vector3, 3> positions{};
    race.progress.Rank(race.graph, positions);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 1, Names(), texts);

    CHECK(YOf(texts, u"ANN DROID") == doctest::Approx(132.0F));
    CHECK(XOf(texts, u"ANN DROID") == doctest::Approx(50.0F));
    CHECK(YOf(texts, u"FINISHED") == doctest::Approx(132.0F));
    CHECK(YOf(texts, u"ROCKET RACER") == doctest::Approx(164.0F));
    CHECK(XOf(texts, u"0:02:00") == doctest::Approx(520.0F));
    CHECK(YOf(texts, u"STILL RACING") == doctest::Approx(196.0F));
    CHECK_FALSE(Find(texts, u"FINISH").has_value());
}

}
}
