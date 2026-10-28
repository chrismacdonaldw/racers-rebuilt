#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/duration.hpp"

namespace rr::app {

class StartupTimer {
public:
    explicit StartupTimer(Nanoseconds start);

    /// `name` must outlive the timer.
    void EndPhase(std::string_view name, Nanoseconds now);

    [[nodiscard]] std::string Summary() const;

private:
    Nanoseconds start_;
    Nanoseconds phaseStart_;
    std::vector<std::pair<std::string_view, Nanoseconds>> phases_;
};

}
