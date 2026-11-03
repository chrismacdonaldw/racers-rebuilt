#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "settings/racers.hpp"

namespace rr::app {

/// Covers the look, chassis and car, but not the name or trophies.
[[nodiscard]] std::uint64_t RacerHash(const settings::Racer& racer);

/// A racer by name in any case, and by `RacerHash` too when `hash` is set, telling copies apart.
struct RacerRef {
    std::string name;
    std::optional<std::uint64_t> hash;

    friend bool operator==(const RacerRef&, const RacerRef&) = default;
};

[[nodiscard]] RacerRef RefOf(const settings::Racer& racer);

[[nodiscard]] bool IsRef(const settings::Racer& racer, const RacerRef& ref);

}
