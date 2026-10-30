#pragma once

#include <string>

#include "core/duration.hpp"

namespace rr::app {

/// `m:ss:cc`, or `mm:ss:cc` from ten minutes, wrapping every hour as the original does.
[[nodiscard]] std::u16string RaceTimeText(Milliseconds time);

}
