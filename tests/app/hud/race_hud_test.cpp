#include "app/hud/race_hud.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ring_race.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kTextWidth = 10;
constexpr std::uint32_t kTextHeight = 16;
constexpr std::uint32_t kNumberWidth = 20;
constexpr std::uint32_t kNumberHeight = 40;

UiFont MonospaceFont(std::uint32_t width, std::uint32_t height) {
    UiFont font{.widthPx = width * 64, .heightPx = height, .glyphs = {.spaceWidthPx = width}};
    for (char16_t character = u'!'; character <= u'Z'; ++character) {
        font.glyphs.glyphs.push_back({.character = character, .leftPx = 0, .widthPx = width});
    }
    return font;
}

struct Fonts {
    UiFont text = MonospaceFont(kTextWidth, kTextHeight);
    UiFont numbers = MonospaceFont(kNumberWidth, kNumberHeight);

    [[nodiscard]] HudFonts Hud() const { return {.text = &text, .numbers = &numbers}; }
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
            .stillRacing = u"STILL RACING",
            .circuit = u"CIRCUIT",
            .paused = u"GAME PAUSED",
            .resume = u"CONTINUE",
            .restart = u"RESTART RACE",
            .restartCircuit = u"RESTART CIRCUIT",
            .quit = u"EXIT",
            .sure = u"ARE YOU SURE?",
            .yes = u"YES",
            .no = u"NO"};
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

float BlueOf(const std::vector<HudText>& texts, std::u16string_view text) {
    return Find(texts, text)
        .transform([](const HudText& item) { return item.tint.b; })
        .value_or(-1.0F);
}

HudText LastNumbers(const std::vector<HudText>& texts) {
    const auto found = std::ranges::find_last_if(
        texts, [](const HudText& item) { return item.font == HudFont::Numbers; });
    return found.empty() ? HudText{} : found.front();
}

TEST_CASE("the pause menu's rows are centred on screen under its title, the focused one tinted") {
    const Fonts fonts;
    const RaceHud hud(EnglishStrings(), fonts.Hud());
    std::vector<HudText> texts;

    hud.LayoutPause({.dialog = PauseDialog::Menu, .focus = 1}, false, texts);

    REQUIRE(texts.size() == 4);
    CHECK(XOf(texts, u"GAME PAUSED") == doctest::Approx(320.0F - 55.0F));
    CHECK(XOf(texts, u"EXIT") == doctest::Approx(320.0F - 20.0F));
    CHECK(YOf(texts, u"GAME PAUSED") == doctest::Approx(195.0F));
    CHECK(YOf(texts, u"CONTINUE") == doctest::Approx(213.0F));
    CHECK(YOf(texts, u"RESTART RACE") == doctest::Approx(231.0F));
    CHECK(YOf(texts, u"EXIT") == doctest::Approx(249.0F));
    CHECK(BlueOf(texts, u"RESTART RACE") == doctest::Approx(36.0F / 255.0F));
    CHECK(BlueOf(texts, u"EXIT") == doctest::Approx(18.0F / 255.0F));
    CHECK(BlueOf(texts, u"GAME PAUSED") == 1.0F);
}

TEST_CASE("a circuit race's pause menu restarts the circuit") {
    const Fonts fonts;
    const RaceHud hud(EnglishStrings(), fonts.Hud());
    std::vector<HudText> texts;

    hud.LayoutPause({}, true, texts);

    CHECK(Find(texts, u"RESTART CIRCUIT").has_value());
    CHECK_FALSE(Find(texts, u"RESTART RACE").has_value());
}

TEST_CASE("the pause confirm asks are you sure over yes and no") {
    const Fonts fonts;
    const RaceHud hud(EnglishStrings(), fonts.Hud());
    std::vector<HudText> texts;

    hud.LayoutPause({.dialog = PauseDialog::Confirm, .focus = 1}, false, texts);

    REQUIRE(texts.size() == 3);
    CHECK(YOf(texts, u"ARE YOU SURE?") == doctest::Approx(204.0F));
    CHECK(YOf(texts, u"YES") == doctest::Approx(222.0F));
    CHECK(YOf(texts, u"NO") == doctest::Approx(240.0F));
    CHECK(XOf(texts, u"NO") == doctest::Approx(320.0F - 10.0F));
    CHECK(BlueOf(texts, u"YES") == doctest::Approx(18.0F / 255.0F));
    CHECK(BlueOf(texts, u"NO") == doctest::Approx(36.0F / 255.0F));
    CHECK_FALSE(Find(texts, u"GAME PAUSED").has_value());
}

float FocusAlphaAfter(Milliseconds paused) {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
    for (Milliseconds time{0}; time < paused; time += Milliseconds{10}) {
        hud.TickPause(Milliseconds{10});
    }
    std::vector<HudText> texts;
    hud.LayoutPause({}, false, texts);
    return Find(texts, u"CONTINUE")
        .transform([](const HudText& item) { return item.tint.a; })
        .value_or(0.0F);
}

TEST_CASE("the focused dialog row brightens and fades for 250 ms of every 500") {
    CHECK(FocusAlphaAfter(Milliseconds{0}) == doctest::Approx(191.0F / 255.0F));
    CHECK(FocusAlphaAfter(Milliseconds{130}) == doctest::Approx(1.0F).epsilon(0.01));
    CHECK(FocusAlphaAfter(Milliseconds{250}) == 0.0F);
    CHECK(FocusAlphaAfter(Milliseconds{500}) == doctest::Approx(191.0F / 255.0F));
}

TEST_CASE("the lap counter and lap time sit top right, right of their labels") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
    const test::RingRace race(3, 3);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 0, texts);

    const float lapTimeX = 640.0F - (11.0F / 8.0F * 70.0F);
    CHECK(XOf(texts, u"0:00:00") == doctest::Approx(lapTimeX));
    CHECK(YOf(texts, u"0:00:00") == doctest::Approx(7.0F));
    CHECK(XOf(texts, u"1") == doctest::Approx(lapTimeX - 24.0F));
    CHECK(XOf(texts, u"LAP") == doctest::Approx(lapTimeX - 60.0F));
    CHECK_FALSE(Find(texts, u"BEST").has_value());
}

TEST_CASE("the place digit ends where its suffix starts, top left") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
    test::RingRace race(3, 3);
    const std::array<Vector3, 3> positions{Vector3{.x = -9.0F}, Vector3{.x = -8.0F},
                                           Vector3{.x = -7.0F}};
    race.progress.Rank(race.graph, positions);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 0, texts);

    const auto digit = std::ranges::find_if(
        texts, [](const HudText& item) { return item.font == HudFont::Numbers; });
    REQUIRE(digit != texts.end());
    CHECK(digit->text == u"3");
    CHECK(digit->x == doctest::Approx(44.0F));
    CHECK(XOf(texts, u"RD") == doctest::Approx(69.0F));
}

TEST_CASE("the countdown is centred, yellow and in the number font") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
    test::RingRace race(1, 3);
    race.clock.Advance(Milliseconds{2001}, false);
    race.clock.Advance(Milliseconds{2500}, false);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 0, texts);

    const HudText one = LastNumbers(texts);
    const float scale = 0.9F;
    CHECK(one.text == u"1");
    CHECK(one.scale == doctest::Approx(scale));
    CHECK(one.x == doctest::Approx(320.0F - (static_cast<float>(kNumberWidth) * scale / 2.0F)));
    CHECK(one.y ==
          doctest::Approx(7.0F + 42.0F - (static_cast<float>(kNumberHeight) * scale / 4.0F)));
    CHECK(one.tint.b == 0.0F);
}

TEST_CASE("one player's HUD fills the screen and two players' split it top and bottom") {
    const HudPlacement full = PlacementFor(0, 1);
    const HudPlacement top = PlacementFor(0, 2);
    const HudPlacement bottom = PlacementFor(1, 2);

    CHECK(full.top == 0.0F);
    CHECK(full.bottom == 480.0F);
    CHECK(full.textScale == 1.0F);
    CHECK(top.top == 0.0F);
    CHECK(top.bottom == 240.0F);
    CHECK(bottom.top == 240.0F);
    CHECK(bottom.bottom == 480.0F);
    CHECK(bottom.textScale == doctest::Approx(0.9F));
    CHECK(bottom.bannerScale == doctest::Approx(0.8F));
}

struct SplitLayout {
    SplitLayout() {
        test::RingRace race(3, 3);
        const std::array<Vector3, 3> positions{Vector3{.x = -9.0F}, Vector3{.x = -8.0F},
                                               Vector3{.x = -7.0F}};
        race.progress.Rank(race.graph, positions);
        hud.Layout(race.clock, race.progress, 0, texts);
    }

    Fonts fonts;
    RaceHud hud{EnglishStrings(), fonts.Hud(), PlacementFor(1, 2)};
    std::vector<HudText> texts;
};

TEST_CASE("a split HUD's lap row shrinks and measures y from the half's top") {
    const SplitLayout split;
    const std::vector<HudText>& texts = split.texts;

    const float lapTimeX = 640.0F - (11.0F / 8.0F * 70.0F * 0.9F);
    CHECK(XOf(texts, u"0:00:00") == doctest::Approx(lapTimeX));
    CHECK(YOf(texts, u"0:00:00") == doctest::Approx(247.0F));
    CHECK(Find(texts, u"0:00:00").value_or(HudText{}).scale == doctest::Approx(0.9F));
    CHECK(XOf(texts, u"LAP") == doctest::Approx(lapTimeX - 56.0F));
}

TEST_CASE("a split HUD's place shrinks and keeps its suffix's x") {
    const SplitLayout split;
    const std::vector<HudText>& texts = split.texts;

    CHECK(XOf(texts, u"RD") == doctest::Approx(69.0F));
    CHECK(YOf(texts, u"RD") == doctest::Approx(247.0F));
    const auto digit = std::ranges::find_if(
        texts, [](const HudText& item) { return item.font == HudFont::Numbers; });
    REQUIRE(digit != texts.end());
    CHECK(digit->x == doctest::Approx(64.0F - 18.0F));
    CHECK(digit->scale == doctest::Approx(0.9F));
}

TEST_CASE("a split HUD's countdown shrinks and sits under its half's rows") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud(), PlacementFor(1, 2));
    test::RingRace race(1, 3);
    race.clock.Advance(Milliseconds{2001}, false);
    race.clock.Advance(Milliseconds{2500}, false);
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 0, texts);

    const HudText one = LastNumbers(texts);
    const float scale = 0.9F * 0.8F;
    CHECK(one.text == u"1");
    CHECK(one.scale == doctest::Approx(scale));
    CHECK(one.x == doctest::Approx(320.0F - (static_cast<float>(kNumberWidth) * scale / 2.0F)));
    const float gapY = 240.0F + 7.0F + (21.0F / 8.0F * 16.0F * 0.9F);
    CHECK(one.y == doctest::Approx(gapY - (static_cast<float>(kNumberHeight) * scale / 4.0F)));
}

TEST_CASE("the results table lists each racer with its gap to the winner") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
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
    const auto names = Names();
    std::vector<HudText> texts;

    hud.Layout(race.clock, race.progress, 1, texts);
    hud.LayoutResults(race.clock, race.progress, 1, names, {}, texts);

    CHECK(YOf(texts, u"ANN DROID") == doctest::Approx(132.0F));
    CHECK(XOf(texts, u"ANN DROID") == doctest::Approx(50.0F));
    CHECK(YOf(texts, u"FINISHED") == doctest::Approx(132.0F));
    CHECK(YOf(texts, u"ROCKET RACER") == doctest::Approx(164.0F));
    CHECK(XOf(texts, u"0:02:00") == doctest::Approx(520.0F));
    CHECK(YOf(texts, u"STILL RACING") == doctest::Approx(196.0F));
    CHECK_FALSE(Find(texts, u"FINISH").has_value());
}

test::RingRace FinishedCircuitRace() {
    test::RingRace race(3, 1);
    race.StartRacing();
    race.DriveLap(1);
    race.DriveLap(1);
    race.DriveLap(0);
    race.DriveLap(0);
    race.clock.Advance(Milliseconds{12}, true);
    race.clock.Advance(Milliseconds{5001}, false);
    const std::array<Vector3, 3> positions{};
    race.progress.Rank(race.graph, positions);
    return race;
}

TEST_CASE("a circuit race's results add each racer's points for its place") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
    const test::RingRace race = FinishedCircuitRace();
    const auto names = Names();
    const std::array<std::int32_t, 3> before{40, 0, 5};
    std::vector<HudText> texts;

    hud.LayoutResults(race.clock, race.progress, 0, names, before, texts);

    CHECK(YOf(texts, u"ANN DROID") == doctest::Approx(132.0F));
    CHECK(XOf(texts, u"30") == doctest::Approx(450.0F));
    CHECK(YOf(texts, u"20") == doctest::Approx(164.0F));
    CHECK(YOf(texts, u"ISLANDER") == doctest::Approx(196.0F));
    CHECK_FALSE(Find(texts, u"CIRCUIT").has_value());
}

TEST_CASE("the player's results row blinks off for 150 ms of every 400") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
    test::RingRace race = FinishedCircuitRace();
    const auto names = Names();
    const std::array<std::int32_t, 3> before{};
    std::vector<HudText> texts;

    race.clock.Advance(Milliseconds{100}, false);
    hud.LayoutResults(race.clock, race.progress, 0, names, before, texts);

    CHECK_FALSE(Find(texts, u"ROCKET RACER").has_value());
    CHECK(Find(texts, u"ANN DROID").has_value());
}

TEST_CASE("after the race view, the circuit view ranks racers on their new totals") {
    const Fonts fonts;
    RaceHud hud(EnglishStrings(), fonts.Hud());
    test::RingRace race = FinishedCircuitRace();
    race.clock.Advance(Milliseconds{5000}, false);
    const auto names = Names();
    const std::array<std::int32_t, 3> before{40, 0, 5};
    std::vector<HudText> texts;

    hud.LayoutResults(race.clock, race.progress, 0, names, before, texts);

    CHECK(YOf(texts, u"CIRCUIT") == doctest::Approx(60.0F));
    CHECK(YOf(texts, u"ROCKET RACER") == doctest::Approx(132.0F));
    CHECK(YOf(texts, u"60") == doctest::Approx(132.0F));
    CHECK(YOf(texts, u"ANN DROID") == doctest::Approx(164.0F));
    CHECK(YOf(texts, u"ISLANDER") == doctest::Approx(196.0F));
    CHECK_FALSE(Find(texts, u"FINISHED").has_value());
}

}
}
