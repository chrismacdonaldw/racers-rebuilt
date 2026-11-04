#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/result.hpp"

namespace rr::mods {

struct ModVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;

    auto operator<=>(const ModVersion&) const = default;
};

enum class RangeKind : std::uint8_t { Any, Exact, AtLeast, SameMajor };

struct VersionRange {
    RangeKind kind = RangeKind::Any;
    ModVersion version;
};

/// Only `x.y.z` with whole numbers.
[[nodiscard]] std::optional<ModVersion> ParseModVersion(std::string_view text);

/// `*`, `=x.y.z`, `>=x.y.z` or `^x.y.z`.
[[nodiscard]] Result<VersionRange> ParseVersionRange(std::string_view text);

/// A version that isn't `x.y.z` only matches `*`.
[[nodiscard]] bool Matches(const VersionRange& range, std::string_view version);

/// Empty for `*`, otherwise like "1.2.0 or newer".
[[nodiscard]] std::string DescribeRange(const VersionRange& range);

}
