#pragma once

#include <string>
#include <string_view>

#include "core/result.hpp"
#include "sim/race/circuit_run.hpp"

namespace rr::app {

struct AwardSceneSetup {
    std::string circuit;
    sim::CircuitAward award = sim::CircuitAward::None;

    friend bool operator==(const AwardSceneSetup&, const AwardSceneSetup&) = default;
};

/// Such as `menu:award:c0:gold`.
[[nodiscard]] std::string AwardSceneName(const AwardSceneSetup& setup);

[[nodiscard]] bool IsAwardScene(std::string_view scene);
[[nodiscard]] Result<AwardSceneSetup> ParseAwardScene(std::string_view scene);

}
