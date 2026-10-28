#pragma once

#include <chrono>
#include <cstdint>

namespace rr {

using Nanoseconds = std::chrono::duration<std::int64_t, std::nano>;

using Microseconds = std::chrono::duration<std::int64_t, std::micro>;

using Milliseconds = std::chrono::duration<std::int64_t, std::milli>;

/// Never enters the simulation.
using FloatSeconds = std::chrono::duration<double>;

}
