#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/hud/hud_text.hpp"
#include "app/hud/text_pool.hpp"
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

struct ResultLabels {
    std::u16string_view finished;
    std::u16string_view stillRacing;
};

/// Appends the original's results table, up to six rows. `names` holds one name per racer.
void LayoutResults(std::span<const ResultRow> rows, std::span<const std::u16string> names,
                   const ResultLabels& labels, TextPool& pool, std::vector<HudText>& out);

}
