#include "app/circuit_leg.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include "app/menu/award_scene_name.hpp"
#include "app/menu/menu_scenes.hpp"
#include "assets/game_path.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kPlayerSlot = 0;

}

Result<CircuitLeg> CircuitLeg::Create(const assets::RaceCatalog& catalog,
                                      const CircuitSceneSetup& setup) {
    const std::optional<std::size_t> circuit = catalog.FindCircuit(setup.race.circuit);
    if (!circuit) {
        return Fail(ErrorCode::NotFound, std::format("there's no circuit {}", setup.race.circuit));
    }
    const assets::CircuitRaces& races = catalog.RacesOf(*circuit);
    std::array<bool, assets::kCircuitRaces> hasRace{};
    std::ranges::transform(races, hasRace.begin(),
                           [](const std::optional<std::size_t>& race) { return race.has_value(); });
    if (!std::ranges::contains(hasRace, true)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("circuit {} has no races", setup.race.circuit));
    }
    const sim::CircuitRun run(hasRace, kPlayerSlot, true, setup.standing);
    if (!run.Race()) {
        return Fail(ErrorCode::InvalidData, std::format("circuit {} has no race left from race {}",
                                                        setup.race.circuit, setup.standing.race));
    }
    RaceSceneSetup race = setup.race;
    race.track = assets::ToUpperAscii(catalog.Races()[*races[*run.Race()]].folder);
    return CircuitLeg(run, setup.race, std::move(race));
}

std::string CircuitLeg::AfterFinish(std::span<const std::optional<int>> places) const {
    sim::CircuitRun run = run_;
    run.FinishRace(places);
    return Next(run);
}

std::string CircuitLeg::AfterRestart() const {
    sim::CircuitRun run = run_;
    run.Restart();
    return Next(run);
}

std::string CircuitLeg::AfterQuit() const {
    sim::CircuitRun run = run_;
    run.Quit();
    return Next(run);
}

CircuitLeg::CircuitLeg(const sim::CircuitRun& run, RaceSceneSetup rules, RaceSceneSetup race)
    : run_(run), rules_(std::move(rules)), race_(std::move(race)) {}

std::string CircuitLeg::Next(const sim::CircuitRun& run) const {
    if (run.Race()) {
        return CircuitSceneName({.race = rules_, .standing = run.Standing()});
    }
    if (run.Award()) {
        return AwardSceneName({.circuit = rules_.circuit, .award = *run.Award()});
    }
    return std::string(kMainMenuScene);
}

}
