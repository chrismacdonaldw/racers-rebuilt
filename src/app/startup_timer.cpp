#include "app/startup_timer.hpp"

#include <chrono>
#include <format>
#include <iterator>

namespace rr::app {
namespace {

double ToMilliseconds(Nanoseconds duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

}

StartupTimer::StartupTimer(Nanoseconds start) : start_(start), phaseStart_(start) {}

void StartupTimer::EndPhase(std::string_view name, Nanoseconds now) {
    phases_.emplace_back(name, now - phaseStart_);
    phaseStart_ = now;
}

std::string StartupTimer::Summary() const {
    std::string summary =
        std::format("startup took {:.1f} ms", ToMilliseconds(phaseStart_ - start_));
    const char* separator = ": ";
    for (const auto& [name, duration] : phases_) {
        std::format_to(std::back_inserter(summary), "{}{} {:.1f} ms", separator, name,
                       ToMilliseconds(duration));
        separator = ", ";
    }
    return summary;
}

}
