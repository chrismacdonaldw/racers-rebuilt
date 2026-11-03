#include "app/menu/race_setup_menu.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <utility>

#include "app/cutscene/cutscene_screen.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/race_scene_name.hpp"
#include "assets/game_path.hpp"
#include "assets/race_catalog.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/string_table.hpp"
#include "core/assert.hpp"

namespace rr::app {
namespace {

constexpr auto kRowCount = static_cast<int>(RaceSetupRow::Count);
constexpr std::uint64_t kHashBase = 16;
constexpr std::string_view kDriverFolder = "GAMEDATA/COMMON";
constexpr std::string_view kDriverNames = "DRIVERS.SRF";
constexpr std::string_view kRaceNamesFolder = "MENUDATA";
constexpr std::string_view kRaceNames = "CIRCUIT.SRF";
constexpr std::size_t kTrialCircuits = 3;
constexpr std::string_view kTrialDriver = "VV";

std::string CircuitStart(const RaceSceneSetup& race, std::span<const RacerChoice> racers) {
    std::string circuit = CircuitSceneName({.race = race});
    const std::optional<std::string> intro = CircuitIntro(race.circuit);
    if (race.players != 1 || !intro) {
        return circuit;
    }
    const bool showsRacer = FindCutsceneScreen(*intro).value_or(CutsceneScreen{}).showsRacer;
    return CutsceneScreenName(
        {.menu = *intro,
         .driver = showsRacer && race.racer < racers.size() ? racers[race.racer].code : "",
         .next = circuit});
}

std::size_t Wrap(std::size_t value, int step, std::size_t count) {
    const auto signedCount = static_cast<std::ptrdiff_t>(count);
    const auto moved = static_cast<std::ptrdiff_t>(value) + step;
    return static_cast<std::size_t>(((moved % signedCount) + signedCount) % signedCount);
}

RacerChoice ChoiceOf(const assets::RacerCatalog& catalog, const std::string& code) {
    const Result<assets::RacerFiles> files = catalog.Find(code);
    return {.code = code, .nameIndex = files ? files->driver.nameIndex : -1};
}

Result<std::vector<std::u16string>> ReadStrings(const assets::Vfs& vfs, std::string_view path) {
    return vfs.Read(path).and_then(
        [](const assets::FileData& file) { return assets::ParseStringTable(file.Bytes()); });
}

struct Catalogs {
    assets::RaceCatalog races;
    assets::RacerCatalog racers;
};

Result<Catalogs> LoadCatalogs(const assets::Vfs& vfs) {
    Result<assets::RacerCatalog> racers = assets::RacerCatalog::Load(vfs);
    Result<assets::RaceCatalog> races = racers.and_then(
        [&](const assets::RacerCatalog&) { return assets::RaceCatalog::Load(vfs); });
    if (!races) {
        return std::unexpected(std::move(races.error()));
    }
    return Catalogs{.races = std::move(*races), .racers = std::move(*racers)};
}

RacePreview PreviewOf(const assets::RaceEntry& race) {
    return {.frame = race.previewScene.value_or(std::string()),
            .driver = race.previewDriver.value_or(std::string())};
}

CourseChoice CircuitChoice(const Catalogs& catalogs, std::size_t circuit) {
    const assets::CircuitEntry& entry = catalogs.races.Circuits()[circuit];
    CourseChoice choice{.circuit = entry.name, .circuitIndex = circuit};
    choice.racers.reserve(entry.grid.size());
    for (const std::string& code : entry.grid) {
        choice.racers.push_back(ChoiceOf(catalogs.racers, code));
    }
    for (const std::optional<std::size_t>& slot : catalogs.races.RacesOf(circuit)) {
        choice.previews.push_back(slot ? std::optional(PreviewOf(catalogs.races.Races()[*slot]))
                                       : std::nullopt);
    }
    return choice;
}

std::vector<CourseChoice> TrackChoicesOf(const Catalogs& catalogs, std::size_t circuit) {
    const CourseChoice base = CircuitChoice(catalogs, circuit);
    std::vector<CourseChoice> choices;
    for (const std::optional<std::size_t>& slot : catalogs.races.RacesOf(circuit)) {
        const assets::RaceEntry* race = slot ? &catalogs.races.Races()[*slot] : nullptr;
        if (race != nullptr && !race->isMirrored) {
            CourseChoice& choice = choices.emplace_back(base);
            choice.track = assets::ToUpperAscii(race->folder);
            choice.race = *slot;
            choice.nameLine = race->nameLine.value_or(-1);
            choice.previews = {PreviewOf(*race)};
        }
    }
    return choices;
}

RaceSetupRow Stepped(RaceSetupRow row, int step) {
    return static_cast<RaceSetupRow>(
        Wrap(static_cast<std::size_t>(row), step, static_cast<std::size_t>(kRowCount)));
}

bool IsLocked(const CourseChoice& choice, const std::set<std::string>& unlocked) {
    return choice.circuitIndex != 0 && !unlocked.contains(choice.circuit);
}

bool HasRaces(const assets::RaceCatalog& races, std::size_t circuit) {
    return std::ranges::any_of(races.RacesOf(circuit), [](const std::optional<std::size_t>& slot) {
        return slot.has_value();
    });
}

}

Result<std::vector<CourseChoice>> LoadTrackChoices(const assets::Vfs& vfs) {
    Result<Catalogs> catalogs = LoadCatalogs(vfs);
    if (!catalogs) {
        return std::unexpected(std::move(catalogs.error()));
    }
    std::vector<CourseChoice> choices;
    for (std::size_t circuit = 0; circuit < catalogs->races.Circuits().size(); ++circuit) {
        std::ranges::move(TrackChoicesOf(*catalogs, circuit), std::back_inserter(choices));
    }
    if (choices.empty()) {
        return Fail(ErrorCode::NotFound, "no circuit has an unmirrored race");
    }
    return choices;
}

Result<std::vector<CourseChoice>> LoadTrialChoices(const assets::Vfs& vfs,
                                                   const std::set<std::string>& unlocked) {
    Result<Catalogs> catalogs = LoadCatalogs(vfs);
    if (!catalogs) {
        return std::unexpected(std::move(catalogs.error()));
    }
    std::vector<CourseChoice> choices;
    const std::size_t circuits = std::min(kTrialCircuits, catalogs->races.Circuits().size());
    for (std::size_t circuit = 0; circuit < circuits; ++circuit) {
        for (CourseChoice& choice : TrackChoicesOf(*catalogs, circuit)) {
            choice.isLocked = IsLocked(choice, unlocked);
            for (std::optional<RacePreview>& preview : choice.previews) {
                if (preview) {
                    preview->driver = kTrialDriver;
                }
            }
            choices.push_back(std::move(choice));
        }
    }
    if (choices.empty()) {
        return Fail(ErrorCode::NotFound, "no time trial circuit has an unmirrored race");
    }
    return choices;
}

Result<std::vector<CourseChoice>> LoadCircuitChoices(const assets::Vfs& vfs,
                                                     const std::set<std::string>& unlocked) {
    Result<Catalogs> catalogs = LoadCatalogs(vfs);
    if (!catalogs) {
        return std::unexpected(std::move(catalogs.error()));
    }
    std::vector<CourseChoice> choices;
    for (std::size_t circuit = 0; circuit < catalogs->races.Circuits().size(); ++circuit) {
        if (HasRaces(catalogs->races, circuit)) {
            CourseChoice& choice = choices.emplace_back(CircuitChoice(*catalogs, circuit));
            choice.isLocked = IsLocked(choice, unlocked);
        }
    }
    if (choices.empty()) {
        return Fail(ErrorCode::NotFound, "no circuit has a race");
    }
    return choices;
}

Result<std::vector<std::u16string>> ReadDriverNames(const assets::Vfs& vfs,
                                                    std::string_view language) {
    return ReadStrings(vfs, std::format("{}/{}/{}", kDriverFolder, language, kDriverNames));
}

Result<std::vector<std::u16string>> ReadRaceNames(const assets::Vfs& vfs,
                                                  std::string_view language) {
    return ReadStrings(vfs, std::format("{}/{}/{}", kRaceNamesFolder, language, kRaceNames));
}

RaceSetupMenu::RaceSetupMenu(std::vector<CourseChoice> courses, std::size_t players)
    : courses_(std::move(courses)), players_(players) {
    RR_VERIFY(!courses_.empty());
}

std::optional<std::string> RaceSetupMenu::Tick(const sim::InputFrame& input,
                                               const RaceSceneSetup& rules) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        return std::string(kMainMenuScene);
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        focus_ = Stepped(focus_, -1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        focus_ = Stepped(focus_, 1);
    }
    if (focus_ == RaceSetupRow::Course && input.WasPressed(sim::MenuAction::Left)) {
        course_ = Wrap(course_, -1, courses_.size());
    }
    if (focus_ == RaceSetupRow::Course && input.WasPressed(sim::MenuAction::Right)) {
        course_ = Wrap(course_, 1, courses_.size());
    }
    return input.WasPressed(sim::MenuAction::Confirm) ? Confirm(rules) : std::nullopt;
}

std::uint64_t RaceSetupMenu::StateHash() const {
    return (course_ * kHashBase) + static_cast<std::uint64_t>(focus_);
}

std::string RaceSetupMenu::PickingRacers(std::string_view scenes) const {
    const std::string picks =
        players_ == 1 ? std::string(scenes) : ChainedScene(kPickSecondRacerScene, scenes);
    return ChainedScene(kPickRacerScene, picks);
}

std::optional<std::string> RaceSetupMenu::Confirm(const RaceSceneSetup& rules) {
    RaceSceneSetup race = rules;
    switch (focus_) {
        case RaceSetupRow::Race:
            if (Course().isLocked) {
                return std::nullopt;
            }
            race.track = Course().track;
            race.circuit = Course().circuit;
            race.players = players_;
            return PickingRacers(race.track.empty() ? CircuitStart(race, Course().racers)
                                                    : RaceSceneName(race));
        case RaceSetupRow::Back:
            return std::string(kMainMenuScene);
        default:
            focus_ = Stepped(focus_, 1);
            return std::nullopt;
    }
}

}
