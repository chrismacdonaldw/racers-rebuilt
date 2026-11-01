#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/hud/hud_text.hpp"
#include "app/hud/text_pool.hpp"
#include "core/duration.hpp"
#include "sim/race/race_progress.hpp"

namespace rr::app {

enum class ResultStatus : std::uint8_t { Winner, Behind, Racing, None };

struct ResultRow {
    std::size_t racer = 0;
    ResultStatus status = ResultStatus::Racing;
    Milliseconds behind{0};
    std::optional<std::int32_t> points;
};

/// Replaces `out` with one row per racer in standings order, timed against the winner's total.
void ReadResults(const sim::RaceProgress& progress, std::vector<ResultRow>& out);

/// `ReadResults` with each place's points, most first and ties to the later racer.
void ReadCircuitRace(const sim::RaceProgress& progress, std::vector<ResultRow>& out);

/// `before` plus this race's points per racer, most first and ties to the earlier racer.
void ReadCircuitTotals(const sim::RaceProgress& progress, std::span<const std::int32_t> before,
                       std::vector<ResultRow>& out);

struct ResultLabels {
    std::u16string_view finished;
    std::u16string_view stillRacing;
};

/// Appends up to six rows, leaving out `hidden`'s. `names` holds one name per racer.
void LayoutResults(std::span<const ResultRow> rows, std::span<const std::u16string> names,
                   const ResultLabels& labels, std::optional<std::size_t> hidden, TextPool& pool,
                   std::vector<HudText>& out);

}
