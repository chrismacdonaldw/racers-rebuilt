#include "app/menu/single_race_menu.hpp"

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

constexpr auto kRowCount = static_cast<int>(SingleRaceRow::Count);
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

std::vector<TrackChoice> CircuitChoices(const assets::RaceCatalog& races,
                                        const assets::RacerCatalog& racers, std::size_t circuit) {
    const assets::CircuitEntry& entry = races.Circuits()[circuit];
    std::vector<RacerChoice> grid;
    grid.reserve(entry.grid.size());
    for (const std::string& code : entry.grid) {
        grid.push_back(ChoiceOf(racers, code));
    }
    std::vector<TrackChoice> choices;
    for (const std::optional<std::size_t>& slot : races.RacesOf(circuit)) {
        const assets::RaceEntry* race = slot ? &races.Races()[*slot] : nullptr;
        if (race != nullptr && !race->isMirrored) {
            choices.push_back({.track = assets::ToUpperAscii(race->folder),
                               .circuit = entry.name,
                               .nameLine = race->nameLine.value_or(-1),
                               .racers = grid});
        }
    }
    return choices;
}

}

Result<std::vector<TrackChoice>> LoadTrackChoices(const assets::Vfs& vfs) {
    Result<assets::RacerCatalog> racers = assets::RacerCatalog::Load(vfs);
    Result<assets::RaceCatalog> races = racers.and_then(
        [&](const assets::RacerCatalog&) { return assets::RaceCatalog::Load(vfs); });
    if (!races) {
        return std::unexpected(std::move(races.error()));
    }
    std::vector<TrackChoice> choices;
    for (std::size_t circuit = 0; circuit < races->Circuits().size(); ++circuit) {
        std::ranges::move(CircuitChoices(*races, *racers, circuit), std::back_inserter(choices));
    }
    if (choices.empty()) {
        return Fail(ErrorCode::NotFound, "no circuit has an unmirrored race");
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

SingleRaceMenu::SingleRaceMenu(std::vector<TrackChoice> tracks) : tracks_(std::move(tracks)) {
    RR_VERIFY(!tracks_.empty());
}

std::optional<std::string> SingleRaceMenu::Tick(const sim::InputFrame& input,
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

std::uint64_t SingleRaceMenu::StateHash() const {
    return (((track_ * kHashBase) + racer_) * kHashBase) + static_cast<std::uint64_t>(focus_);
}

void SingleRaceMenu::MoveFocus(int step) {
    focus_ = static_cast<SingleRaceRow>(
        Wrap(static_cast<std::size_t>(focus_), step, static_cast<std::size_t>(kRowCount)));
}

void SingleRaceMenu::Change(int step) {
    switch (focus_) {
        case SingleRaceRow::Track:
            track_ = Wrap(track_, step, tracks_.size());
            racer_ = std::min(racer_, Track().racers.size() - 1);
            return;
        case SingleRaceRow::Racer:
            racer_ = Wrap(racer_, step, Track().racers.size());
            return;
        default:
            return;
    }
}

std::optional<std::string> SingleRaceMenu::Confirm(const RaceSceneSetup& rules) {
    RaceSceneSetup race = rules;
    switch (focus_) {
        case SingleRaceRow::Race:
            race.track = Track().track;
            race.circuit = Track().circuit;
            race.racer = racer_;
            return RaceSceneName(race);
        case SingleRaceRow::Back:
            return std::string(kMainMenuScene);
        default:
            MoveFocus(1);
            return std::nullopt;
    }
}

}
