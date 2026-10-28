#pragma once

#include "core/duration.hpp"

namespace rr::platform {

/// Monotonic time since an arbitrary start point.
[[nodiscard]] Nanoseconds Now();

}
