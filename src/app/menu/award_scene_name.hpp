#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/result.hpp"
#include "sim/race/circuit_run.hpp"

namespace rr::app {

/// `racer` is the player's grid slot, as in the circuit's race scenes.
struct AwardSceneSetup {
    std::string circuit;
    sim::CircuitAward award = sim::CircuitAward::None;
    std::size_t racer = 0;

    friend bool operator==(const AwardSceneSetup&, const AwardSceneSetup&) = default;
};

/// Such as `menu:award:c0:gold:racer=2`.
[[nodiscard]] std::string AwardSceneName(const AwardSceneSetup& setup);

[[nodiscard]] bool IsAwardScene(std::string_view scene);
[[nodiscard]] Result<AwardSceneSetup> ParseAwardScene(std::string_view scene);

/// Driver codes, such as `CR`. The champion is the circuit's boss, in grid slot 1.
struct AwardDrivers {
    std::string player;
    std::string champion;
};

/// The award's cutscene screen, followed by a car win screen when it unlocked `newPartSet`.
[[nodiscard]] std::string AwardScreenName(sim::CircuitAward award,
                                          std::optional<std::int32_t> newPartSet,
                                          const AwardDrivers& drivers);

}
