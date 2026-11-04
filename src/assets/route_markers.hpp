#pragma once

#include <span>

#include "assets/json_value.hpp"
#include "assets/race_route.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// `line` is the start then each point, in game space, beside extras `--export-track` writes.
[[nodiscard]] Result<RaceRoute> RouteFromLine(std::span<const Vector3> line,
                                              const JsonValue& extras);

}
