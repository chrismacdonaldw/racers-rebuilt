#pragma once

#include "assets/hazard_list.hpp"
#include "assets/json_value.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Reads the settings `--export-track` writes; missing ones keep their defaults.
[[nodiscard]] Result<HazardPayload> HazardPayloadFrom(HazardKind kind, const JsonValue& settings);

}
