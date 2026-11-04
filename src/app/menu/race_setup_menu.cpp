#include "app/menu/race_setup_menu.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <utility>

#include "app/circuit_awards.hpp"
#include "app/cutscene/cutscene_screen.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/race_scene_name.hpp"
#include "app/track_info.hpp"
#include "assets/game_path.hpp"
#include "assets/race_catalog.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/string_table.hpp"
#include "assets/track_loader.hpp"
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
constexpr std::string_view kMirroredSuffix = "MIRRORED";

std::string CircuitStart(const RaceSceneSetup& race, const CourseChoice& course) {
    std::string circuit = CircuitSceneName({.race = race});
    if (race.players != 1 || course.intro.empty()) {
        return circuit;
    }
    const std::span<const RacerChoice> racers = course.racers;
    const bool showsRacer = FindCutsceneScreen(course.intro).value_or(CutsceneScreen{}).showsRacer;
    return CutsceneScreenName(
        {.menu = course.intro,
         .driver = showsRacer && race.racer < racers.size() ? racers[race.racer].code : "",
         .next = circuit});
}

std::size_t Wrap(std::size_t value, int step, std::size_t count) {
    const auto signedCount = static_cast<std::ptrdiff_t>(count);
    const auto moved = static_cast<std::ptrdiff_t>(value) + step;
    return static_cast<std::size_t>(((moved % signedCount) + signedCount) % signedCount);
}

RacerChoice ChoiceOf(const assets::RacerCatalog& catalog, const std::string& code) {
    const Result<assets::DriverEntry> driver = catalog.FindDriver(code);
    return {.code = code,
            .nameIndex = driver ? driver->nameIndex : -1,
            .title = driver ? driver->title : std::string()};
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

RacePreview PreviewOf(const assets::RaceEntry& race, std::size_t circuit) {
    return {.frame = race.previewScene.value_or(std::string()),
            .driver = race.previewDriver.value_or(std::string()),
            .circuit = circuit,
            .position = race.position};
}

CourseChoice CircuitChoice(const Catalogs& catalogs, std::size_t circuit) {
    const assets::CircuitEntry& entry = catalogs.races.Circuits()[circuit];
    CourseChoice choice{.circuit = entry.name, .circuitIndex = circuit};
    choice.racers.reserve(entry.grid.size());
    for (const std::string& code : entry.grid) {
        choice.racers.push_back(ChoiceOf(catalogs.racers, code));
    }
    for (const std::optional<std::size_t>& slot : catalogs.races.RacesOf(circuit)) {
        choice.previews.push_back(
            slot ? std::optional(PreviewOf(catalogs.races.Races()[*slot], circuit)) : std::nullopt);
    }
    return choice;
}

CourseChoice CircuitCourse(const Catalogs& catalogs, std::size_t circuit) {
    const assets::CircuitEntry& entry = catalogs.races.Circuits()[circuit];
    CourseChoice choice = CircuitChoice(catalogs, circuit);
    choice.title = entry.title;
    const RacerChoice boss = ChoiceOf(
        catalogs.racers, entry.boss.empty() ? std::string(RetailBoss(circuit)) : entry.boss);
    choice.boss = entry.boss.empty() && boss.title.empty() ? RacerChoice{} : boss;
    choice.intro = CircuitIntro(entry).value_or(std::string());
    return choice;
}

std::vector<std::size_t> CircuitOrder(const assets::RaceCatalog& races) {
    std::vector<std::size_t> order = races.Story();
    for (std::size_t circuit = 0; circuit < races.Circuits().size(); ++circuit) {
        if (!std::ranges::contains(order, circuit)) {
            order.push_back(circuit);
        }
    }
    return order;
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
            choice.previews = {PreviewOf(*race, circuit)};
        }
    }
    return choices;
}

bool IsCircuitTrack(const assets::RaceCatalog& races, std::string_view track) {
    return std::ranges::any_of(races.Races(), [&](const assets::RaceEntry& race) {
        const std::optional<std::size_t> circuit =
            race.circuit ? races.FindCircuit(*race.circuit) : std::nullopt;
        return assets::ToUpperAscii(race.folder) == track &&
               !(circuit && races.Circuits()[*circuit].isFromMod);
    });
}

CourseChoice ModTrackChoice(const Catalogs& catalogs, std::string track, const TrackInfo& info) {
    const std::size_t grid = info.grid
                                 .and_then([&](const std::string& circuit) {
                                     return catalogs.races.FindCircuit(circuit);
                                 })
                                 .value_or(0);
    std::optional<RacePreview> preview;
    if (info.preview) {
        preview = RacePreview{.image = *info.preview};
    }
    return {.track = std::move(track),
            .racers = CircuitChoice(catalogs, grid).racers,
            .previews = {std::move(preview)},
            .title = info.name,
            .lapCount = info.lapCount};
}

std::vector<CourseChoice> ModTrackChoices(const assets::Vfs& vfs, const Catalogs& catalogs,
                                          bool isTrial) {
    std::vector<CourseChoice> choices;
    if (catalogs.races.Circuits().empty()) {
        return choices;
    }
    for (const std::string& track : assets::ListRaceTracks(vfs)) {
        if (IsCircuitTrack(catalogs.races, track)) {
            continue;
        }
        const TrackInfo info = FindTrackInfo(vfs, track).value_or(TrackInfo{});
        if (isTrial && !info.hasTimeTrial) {
            continue;
        }
        choices.push_back(ModTrackChoice(catalogs, track, info));
        if (!isTrial && info.hasMirrored) {
            CourseChoice mirrored = choices.back();
            mirrored.isMirrored = true;
            mirrored.title =
                std::format("{} {}", info.name.empty() ? track : info.name, kMirroredSuffix);
            choices.push_back(std::move(mirrored));
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
        if (!catalogs->races.Circuits()[circuit].isFromMod) {
            std::ranges::move(TrackChoicesOf(*catalogs, circuit), std::back_inserter(choices));
        }
    }
    std::ranges::move(ModTrackChoices(vfs, *catalogs, false), std::back_inserter(choices));
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
    const std::set<std::string> open = OpenCircuits(catalogs->races, unlocked);
    const std::size_t circuits = std::min(kTrialCircuits, catalogs->races.Circuits().size());
    for (std::size_t circuit = 0; circuit < circuits; ++circuit) {
        for (CourseChoice& choice : TrackChoicesOf(*catalogs, circuit)) {
            choice.isLocked = IsLocked(choice, open);
            for (std::optional<RacePreview>& preview : choice.previews) {
                if (preview) {
                    preview->driver = kTrialDriver;
                }
            }
            choices.push_back(std::move(choice));
        }
    }
    std::ranges::move(ModTrackChoices(vfs, *catalogs, true), std::back_inserter(choices));
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
    const std::set<std::string> open = OpenCircuits(catalogs->races, unlocked);
    for (const std::size_t circuit : CircuitOrder(catalogs->races)) {
        if (HasRaces(catalogs->races, circuit)) {
            CourseChoice& choice = choices.emplace_back(CircuitCourse(*catalogs, circuit));
            choice.isLocked = IsLocked(choice, open);
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
            race.isMirrored = Course().isMirrored;
            if (Course().lapCount && players_ == 1 && !race.isTimeTrial) {
                race.lapCount = *Course().lapCount;
            }
            return PickingRacers(race.track.empty() ? CircuitStart(race, Course())
                                                    : RaceSceneName(race));
        case RaceSetupRow::Back:
            return std::string(kMainMenuScene);
        default:
            focus_ = Stepped(focus_, 1);
            return std::nullopt;
    }
}

}
