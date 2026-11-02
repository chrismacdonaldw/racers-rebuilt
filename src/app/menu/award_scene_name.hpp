#pragma once

#include <string>
#include <string_view>

#include "core/result.hpp"
#include "sim/race/circuit_run.hpp"

namespace rr::app {

/// Such as `menu:award:gold`.
[[nodiscard]] std::string AwardSceneName(sim::CircuitAward award);

[[nodiscard]] bool IsAwardScene(std::string_view scene);
[[nodiscard]] Result<sim::CircuitAward> ParseAwardScene(std::string_view scene);

}
