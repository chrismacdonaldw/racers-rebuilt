#include "mods/mod_version.hpp"

#include <array>
#include <charconv>
#include <format>
#include <system_error>

namespace rr::mods {
namespace {

struct RangePrefix {
    std::string_view text;
    RangeKind kind;
};

constexpr std::array kPrefixes{RangePrefix{.text = ">=", .kind = RangeKind::AtLeast},
                               RangePrefix{.text = "=", .kind = RangeKind::Exact},
                               RangePrefix{.text = "^", .kind = RangeKind::SameMajor}};

std::optional<int> ParsePart(std::string_view text) {
    if (text.empty() || text.front() < '0' || text.front() > '9') {
        return std::nullopt;
    }
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::string VersionText(const ModVersion& version) {
    return std::format("{}.{}.{}", version.major, version.minor, version.patch);
}

}

std::optional<ModVersion> ParseModVersion(std::string_view text) {
    const std::size_t first = text.find('.');
    if (first == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t second = text.find('.', first + 1);
    if (second == std::string_view::npos) {
        return std::nullopt;
    }
    const std::optional<int> major = ParsePart(text.substr(0, first));
    const std::optional<int> minor = ParsePart(text.substr(first + 1, second - first - 1));
    const std::optional<int> patch = ParsePart(text.substr(second + 1));
    if (!major || !minor || !patch) {
        return std::nullopt;
    }
    return ModVersion{.major = *major, .minor = *minor, .patch = *patch};
}

Result<VersionRange> ParseVersionRange(std::string_view text) {
    if (text == "*") {
        return VersionRange{};
    }
    for (const RangePrefix& prefix : kPrefixes) {
        if (!text.starts_with(prefix.text)) {
            continue;
        }
        if (const std::optional<ModVersion> version =
                ParseModVersion(text.substr(prefix.text.size()))) {
            return VersionRange{.kind = prefix.kind, .version = *version};
        }
        break;
    }
    return Fail(
        ErrorCode::InvalidData,
        std::format("\"{}\" isn't a version range like *, =1.2.0, >=1.2.0 or ^1.2.0", text));
}

bool Matches(const VersionRange& range, std::string_view version) {
    if (range.kind == RangeKind::Any) {
        return true;
    }
    const std::optional<ModVersion> parsed = ParseModVersion(version);
    if (!parsed) {
        return false;
    }
    switch (range.kind) {
        case RangeKind::Exact:
            return *parsed == range.version;
        case RangeKind::AtLeast:
            return *parsed >= range.version;
        case RangeKind::SameMajor:
            return parsed->major == range.version.major && *parsed >= range.version;
        case RangeKind::Any:
            break;
    }
    return true;
}

std::string DescribeRange(const VersionRange& range) {
    switch (range.kind) {
        case RangeKind::Exact:
            return VersionText(range.version);
        case RangeKind::AtLeast:
            return std::format("{} or newer", VersionText(range.version));
        case RangeKind::SameMajor:
            return std::format("{} or a newer {}.x", VersionText(range.version),
                               range.version.major);
        case RangeKind::Any:
            break;
    }
    return {};
}

}
