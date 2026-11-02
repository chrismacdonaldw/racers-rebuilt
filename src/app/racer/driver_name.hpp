#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace rr::app {

/// A prebuilt driver's line in `DRIVERS.SRF`, or a built racer's own name.
using DriverName = std::variant<std::int32_t, std::u16string>;

}
