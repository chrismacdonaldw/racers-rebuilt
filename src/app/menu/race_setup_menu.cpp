#include "app/menu/race_setup_menu.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <optional>
#include <utility>

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

CourseChoice CircuitChoice(const Catalogs& catalogs, std::size_t circuit) {
    const assets::CircuitEntry& entry = catalogs.races.Circuits()[circuit];
    CourseChoice choice{.circuit = entry.name, .circuitIndex = circuit};
    choice.racers.reserve(entry.grid.size());
    for (const std::string& code : entry.grid) {
        choice.racers.push_back(ChoiceOf(catalogs.racers, code));
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
            choice.nameLine = race->nameLine.value_or(-1);
        }
    }
    return choices;
}

bool IsRacedUnmirrored(const assets::RaceCatalog& races, std::size_t circuit) {
    const assets::CircuitRaces& slots = races.RacesOf(circuit);
    const auto isRaced = [](const std::optional<std::size_t>& slot) { return slot.has_value(); };
    const auto isMirrored = [&](const std::optional<std::size_t>& slot) {
        return slot && races.Races()[*slot].isMirrored;
    };
    return std::ranges::any_of(slots, isRaced) && std::ranges::none_of(slots, isMirrored);
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

Result<std::vector<CourseChoice>> LoadCircuitChoices(const assets::Vfs& vfs,
                                                     const std::set<std::string>& unlocked) {
    Result<Catalogs> catalogs = LoadCatalogs(vfs);
    if (!catalogs) {
        return std::unexpected(std::move(catalogs.error()));
    }
    std::vector<CourseChoice> choices;
    for (std::size_t circuit = 0; circuit < catalogs->races.Circuits().size(); ++circuit) {
        if (IsRacedUnmirrored(catalogs->races, circuit)) {
            CourseChoice& choice = choices.emplace_back(CircuitChoice(*catalogs, circuit));
            choice.isLocked = circuit != 0 && !unlocked.contains(choice.circuit);
        }
    }
    if (choices.empty()) {
        return Fail(ErrorCode::NotFound, "no circuit has only unmirrored races");
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

RaceSetupMenu::RaceSetupMenu(std::vector<CourseChoice> courses) : courses_(std::move(courses)) {
    RR_VERIFY(!courses_.empty());
}

std::optional<std::string> RaceSetupMenu::Tick(const sim::InputFrame& input,
                                               const RaceSceneSetup& rules) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        return std::string(kMainMenuScene);
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        MoveFocus(-1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        MoveFocus(1);
    }
    if (input.WasPressed(sim::MenuAction::Left)) {
        Change(-1);
    }
    if (input.WasPressed(sim::MenuAction::Right)) {
        Change(1);
    }
    return input.WasPressed(sim::MenuAction::Confirm) ? Confirm(rules) : std::nullopt;
}

std::uint64_t RaceSetupMenu::StateHash() const {
    return (((course_ * kHashBase) + racer_) * kHashBase) + static_cast<std::uint64_t>(focus_);
}

void RaceSetupMenu::MoveFocus(int step) {
    focus_ = static_cast<RaceSetupRow>(
        Wrap(static_cast<std::size_t>(focus_), step, static_cast<std::size_t>(kRowCount)));
}

void RaceSetupMenu::Change(int step) {
    switch (focus_) {
        case RaceSetupRow::Course:
            course_ = Wrap(course_, step, courses_.size());
            racer_ = std::min(racer_, Course().racers.size() - 1);
            return;
        case RaceSetupRow::Racer:
            racer_ = Wrap(racer_, step, Course().racers.size());
            return;
        default:
            return;
    }
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
            race.racer = racer_;
            return race.track.empty() ? CircuitSceneName({.race = race}) : RaceSceneName(race);
        case RaceSetupRow::Back:
            return std::string(kMainMenuScene);
        default:
            MoveFocus(1);
            return std::nullopt;
    }
}

}
