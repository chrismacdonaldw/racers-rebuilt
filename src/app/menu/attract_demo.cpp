#include "app/menu/attract_demo.hpp"

#include <cstddef>
#include <format>
#include <optional>
#include <string_view>

#include "app/race_scene_name.hpp"
#include "assets/game_path.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kDemoCircuit = "c0";

}

Result<std::string> AttractDemoScene(const assets::RaceCatalog& races, std::uint32_t roll) {
    const std::optional<std::size_t> circuit = races.FindCircuit(kDemoCircuit);
    const std::optional<std::size_t> race =
        circuit ? races.RacesOf(*circuit).at(roll % assets::kCircuitRaces) : std::nullopt;
    if (!race) {
        return Fail(ErrorCode::NotFound,
                    std::format("{} has no race {}", kDemoCircuit, roll % assets::kCircuitRaces));
    }
    const assets::RaceEntry& entry = races.Races()[*race];
    return RaceSceneName({.track = assets::ToUpperAscii(entry.folder),
                          .circuit = std::string(kDemoCircuit),
                          .isMirrored = entry.isMirrored,
                          .isDemo = true});
}

}
