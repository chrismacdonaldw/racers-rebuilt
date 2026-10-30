#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/duration.hpp"
#include "sim/race/race_progress.hpp"

namespace rr::app {

enum class ResultStatus : std::uint8_t { Winner, Behind, Racing };

struct ResultRow {
    std::size_t racer = 0;
    ResultStatus status = ResultStatus::Racing;
    Milliseconds behind{0};
};

/// Replaces `out` with one row per racer in standings order, timed against the winner's total.
void ReadResults(const sim::RaceProgress& progress, std::vector<ResultRow>& out);

}
