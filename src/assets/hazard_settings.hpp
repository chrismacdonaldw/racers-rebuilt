#pragma once

#include "assets/hazard_list.hpp"
#include "assets/json_value.hpp"

namespace rr::assets {

/// The payload `kind` carries, filled from the settings `--export-track` writes. Missing settings
/// keep their defaults.
[[nodiscard]] HazardPayload HazardPayloadFrom(HazardKind kind, const JsonValue& settings);

}
