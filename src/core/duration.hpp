#pragma once

#include <chrono>
#include <cstdint>

namespace rr {

/// Integer nanoseconds. The unit for clocks and frame timing.
using Nanoseconds = std::chrono::duration<std::int64_t, std::nano>;

/// Integer microseconds.
using Microseconds = std::chrono::duration<std::int64_t, std::micro>;

/// Integer milliseconds. The unit for simulation ticks.
using Milliseconds = std::chrono::duration<std::int64_t, std::milli>;

/// Fractional seconds for display and rate calculations. Never enters the simulation.
using FloatSeconds = std::chrono::duration<double>;

}
