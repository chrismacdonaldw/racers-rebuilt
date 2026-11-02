#include "app/menu/race_setup_scene.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/race_settings.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::string_view kTitleFont = "fontmenu";
constexpr std::size_t kCircuitTitleLine = 13;
constexpr std::size_t kSingleRaceTitleLine = 14;
constexpr std::size_t kVersusTitleLine = 0x19;
constexpr std::size_t kPlayerOneLine = 0x56;
constexpr std::size_t kPlayerTwoLine = 0x57;
constexpr std::size_t kVersusPlayers = 2;
constexpr std::size_t kBackLine = 26;
constexpr std::size_t kRaceLine = 27;
constexpr std::size_t kHostingLine = 0x4A;
constexpr std::size_t kCircuitNumberLine = 0x4B;
constexpr std::size_t kFirstChampionLine = 0x4C;
constexpr std::size_t kFirstNumberLine = 0x61;
constexpr std::size_t kDisabledState = 0;
constexpr std::size_t kDisabledFocusedState = 1;
constexpr float kLeft = 3.0F;
constexpr float kValueLeft = 230.0F;
constexpr float kTitleTop = 118.0F;
constexpr float kChampionTop = 278.0F;
constexpr std::array kRowTops{178.0F, 218.0F, 258.0F, 358.0F, 418.0F};

std::u16string Widen(std::string_view text) {
    std::u16string wide;
    for (const char letter : text) {
        wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(letter)));
    }
    return wide;
}

std::u16string OneLine(std::u16string_view text) {
    std::u16string line(text);
    std::ranges::replace(line, u'\n', u' ');
    return line;
}

std::u16string Lookup(const std::vector<std::u16string>& names, std::int32_t index,
                      std::string_view fallback) {
    const auto at = static_cast<std::size_t>(index);
    return index >= 0 && at < names.size() ? names[at] : Widen(fallback);
}

std::vector<std::vector<std::u16string>> RacerNames(const std::vector<CourseChoice>& courses,
                                                    const std::vector<std::u16string>& names) {
    std::vector<std::vector<std::u16string>> racerNames;
    for (const CourseChoice& course : courses) {
        std::vector<std::u16string>& row = racerNames.emplace_back();
        for (const RacerChoice& racer : course.racers) {
            row.push_back(Lookup(names, racer.nameIndex, racer.code));
        }
    }
    return racerNames;
}

std::size_t TitleLine(RaceSetupMode mode) {
    switch (mode) {
        case RaceSetupMode::Circuit:
            return kCircuitTitleLine;
        case RaceSetupMode::Versus:
            return kVersusTitleLine;
        case RaceSetupMode::SingleRace:
            return kSingleRaceTitleLine;
    }
    return kSingleRaceTitleLine;
}

Result<std::vector<CourseChoice>> LoadCourses(const assets::Vfs& vfs, RaceSetupMode mode,
                                              const std::set<std::string>& unlockedCircuits) {
    return mode == RaceSetupMode::Circuit ? LoadCircuitChoices(vfs, unlockedCircuits)
                                          : LoadTrackChoices(vfs);
}

}

std::optional<RaceSetupMode> RaceSetupModeOf(std::string_view scene) {
    if (scene == kSingleRaceScene) {
        return RaceSetupMode::SingleRace;
    }
    if (scene == kCircuitRaceScene) {
        return RaceSetupMode::Circuit;
    }
    if (scene == kVersusRaceScene) {
        return RaceSetupMode::Versus;
    }
    return std::nullopt;
}

Result<std::unique_ptr<RaceSetupScene>> RaceSetupScene::Create(
    render::Renderer& renderer, const assets::Vfs& vfs, RaceSetupMode mode,
    const std::set<std::string>& unlockedCircuits) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kScreen);
    Result<MenuPage> page = files.and_then([&](const MenuFiles& read) {
        return BuildMenuPage(renderer, vfs, read, std::span<const ButtonLine>{});
    });
    Result<std::vector<CourseChoice>> courses =
        page.and_then([&](const MenuPage&) { return LoadCourses(vfs, mode, unlockedCircuits); });
    if (!courses) {
        return std::unexpected(std::move(courses.error()));
    }
    return std::unique_ptr<RaceSetupScene>(new RaceSetupScene(
        renderer, vfs, mode, std::move(page->pictures), TextButtonStates(*files),
        RaceSetupMenu(std::move(*courses), mode == RaceSetupMode::Versus ? kVersusPlayers : 1)));
}

void RaceSetupScene::ApplySettings(const settings::Registry& settings) {
    rules_ = {.respawn = RespawnRuleFrom(settings), .aiDriving = AiDrivingFrom(settings)};
    if (mode_ == RaceSetupMode::Versus) {
        rules_.lapCount = settings.Get(kVersusLaps);
    }
    if (!text_.Apply(*renderer_, *vfs_, settings)) {
        return;
    }
    const Result<std::vector<std::u16string>> names = ReadDriverNames(*vfs_, text_.Language());
    if (!names) {
        log::Warn("racers show their codes: {}", names.error().message);
    }
    racerNames_ = RacerNames(menu_.Courses(), names.value_or(std::vector<std::u16string>{}));
    if (mode_ != RaceSetupMode::Circuit) {
        const Result<std::vector<std::u16string>> races = ReadRaceNames(*vfs_, text_.Language());
        if (!races) {
            log::Warn("races show their folders: {}", races.error().message);
        }
        raceNames_ = races.value_or(std::vector<std::u16string>{});
    }
    courseNames_ = CourseNames();
}

std::optional<std::string> RaceSetupScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void RaceSetupScene::Tick(const sim::InputFrame& input) {
    if (std::optional<std::string> next = menu_.Tick(input, rules_)) {
        next_ = std::move(next);
    }
}

void RaceSetupScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
}

void RaceSetupScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(pictures_, sprites_);
    const bool isCircuit = mode_ == RaceSetupMode::Circuit;
    const bool isVersus = mode_ == RaceSetupMode::Versus;
    DrawLabel(text_, {.font = std::string(kTitleFont)}, text_.Line(TitleLine(mode_)), kLeft,
              kTitleTop, sprites_);
    DrawRow(RaceSetupRow::Course, isCircuit ? OneLine(text_.Line(kCircuitNumberLine)) : u"TRACK",
            courseNames_.at(menu_.CourseIndex()));
    DrawRow(RaceSetupRow::Racer, isVersus ? OneLine(text_.Line(kPlayerOneLine)) : u"RACER",
            RacerName(menu_.Racer()));
    if (isVersus) {
        DrawRow(RaceSetupRow::SecondRacer, OneLine(text_.Line(kPlayerTwoLine)),
                RacerName(menu_.SecondRacer()));
    }
    if (isCircuit) {
        DrawChampion();
    }
    DrawRow(RaceSetupRow::Race, text_.Line(kRaceLine), {});
    DrawRow(RaceSetupRow::Back, text_.Line(kBackLine), {});
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

RaceSetupScene::RaceSetupScene(render::Renderer& renderer, const assets::Vfs& vfs,
                               RaceSetupMode mode, std::vector<MenuPicture> pictures,
                               ButtonStates states, RaceSetupMenu menu)
    : renderer_(&renderer),
      vfs_(&vfs),
      mode_(mode),
      pictures_(std::move(pictures)),
      states_(std::move(states)),
      menu_(std::move(menu)),
      courseNames_(CourseNames()) {}

std::vector<std::u16string> RaceSetupScene::CourseNames() const {
    std::vector<std::u16string> names;
    for (const CourseChoice& course : menu_.Courses()) {
        names.push_back(mode_ == RaceSetupMode::Circuit
                            ? std::u16string(text_.Line(kFirstNumberLine + course.circuitIndex))
                            : Lookup(raceNames_, course.nameLine, course.track));
    }
    return names;
}

void RaceSetupScene::DrawRow(RaceSetupRow row, std::u16string_view label,
                             std::u16string_view value) {
    const bool isFocused = menu_.Focus() == row;
    std::size_t state = isFocused ? kFocusedState : kNormalState;
    if (row == RaceSetupRow::Race && menu_.Course().isLocked) {
        state = isFocused ? kDisabledFocusedState : kDisabledState;
    }
    const float top = kRowTops.at(static_cast<std::size_t>(row));
    DrawLabel(text_, states_.at(state), label, kLeft, top, sprites_);
    DrawLabel(text_, states_.at(state), value, kValueLeft, top, sprites_);
}

std::u16string_view RaceSetupScene::RacerName(std::size_t racer) const {
    return racerNames_.empty() ? std::u16string_view{}
                               : racerNames_.at(menu_.CourseIndex()).at(racer);
}

void RaceSetupScene::DrawChampion() {
    const MenuButtonState& state = states_.at(kNormalState);
    DrawLabel(text_, state, OneLine(text_.Line(kHostingLine)), kLeft, kChampionTop, sprites_);
    DrawLabel(text_, state, OneLine(text_.Line(kFirstChampionLine + menu_.Course().circuitIndex)),
              kValueLeft, kChampionTop, sprites_);
}

}
