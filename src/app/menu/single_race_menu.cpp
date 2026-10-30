#include "app/menu/single_race_menu.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "app/menu/menu_scenes.hpp"
#include "app/race_scene.hpp"
#include "assets/circuit_database.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/string_table.hpp"
#include "assets/track_loader.hpp"
#include "core/assert.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr auto kRowCount = static_cast<int>(SingleRaceRow::Count);
constexpr std::uint64_t kHashBase = 16;
constexpr std::string_view kDriverFolder = "GAMEDATA/COMMON";
constexpr std::string_view kDriverNames = "DRIVERS.SRF";

std::size_t Wrap(std::size_t value, int step, std::size_t count) {
    const auto signedCount = static_cast<std::ptrdiff_t>(count);
    const auto moved = static_cast<std::ptrdiff_t>(value) + step;
    return static_cast<std::size_t>(((moved % signedCount) + signedCount) % signedCount);
}

RacerChoice ChoiceOf(const assets::RacerCatalog& catalog, const std::string& code) {
    const Result<assets::RacerFiles> files = catalog.Find(code);
    return {.code = code, .nameIndex = files ? files->driver.nameIndex : -1};
}

}

Result<std::vector<TrackChoice>> LoadTrackChoices(const assets::Vfs& vfs) {
    Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(vfs);
    if (!catalog) {
        return std::unexpected(std::move(catalog.error()));
    }
    std::vector<TrackChoice> choices;
    for (const std::string& track : assets::ListRaceTracks(vfs)) {
        const Result<std::vector<std::string>> codes = assets::LoadGrid(vfs, track);
        if (!codes || codes->empty()) {
            log::Warn("single race leaves out {}: it has no grid", track);
            continue;
        }
        TrackChoice& choice = choices.emplace_back(TrackChoice{.track = track});
        for (const std::string& code :
             std::span(*codes).first(std::min(codes->size(), assets::kGridSlots))) {
            choice.racers.push_back(ChoiceOf(*catalog, code));
        }
    }
    if (choices.empty()) {
        return Fail(ErrorCode::NotFound, "no race track has a grid");
    }
    return choices;
}

Result<std::vector<std::u16string>> ReadDriverNames(const assets::Vfs& vfs,
                                                    std::string_view language) {
    return vfs.Read(std::format("{}/{}/{}", kDriverFolder, language, kDriverNames))
        .and_then(
            [](const assets::FileData& file) { return assets::ParseStringTable(file.Bytes()); });
}

SingleRaceMenu::SingleRaceMenu(std::vector<TrackChoice> tracks) : tracks_(std::move(tracks)) {
    RR_VERIFY(!tracks_.empty());
}

std::optional<std::string> SingleRaceMenu::Tick(const sim::InputFrame& input,
                                                const sim::RespawnRule& respawn) {
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
    return input.WasPressed(sim::MenuAction::Confirm) ? Confirm(respawn) : std::nullopt;
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

std::optional<std::string> SingleRaceMenu::Confirm(const sim::RespawnRule& respawn) {
    switch (focus_) {
        case SingleRaceRow::Race:
            return RaceSceneName({.track = Track().track, .respawn = respawn, .racer = racer_});
        case SingleRaceRow::Back:
            return std::string(kMainMenuScene);
        default:
            MoveFocus(1);
            return std::nullopt;
    }
}

}
