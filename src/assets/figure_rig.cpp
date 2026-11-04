#include "assets/figure_rig.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <sstream>
#include <string_view>

namespace rr::assets {
namespace {

constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";
constexpr std::string_view kVersion = "rig 1";
constexpr std::string_view kPivot = "pivot";
constexpr std::string_view kLimb = "limb";
constexpr float kShortestSegment = 0.01F;

std::string_view LineAt(std::string_view text, std::size_t start, std::size_t end) {
    std::string_view line = text.substr(start, end - start);
    if (line.ends_with('\r')) {
        line.remove_suffix(1);
    }
    return line;
}

const RigPivot* PivotOf(const FigureRig& rig, std::size_t node) {
    const auto pivot = std::ranges::find(rig.pivots, node, &RigPivot::node);
    return pivot == rig.pivots.end() ? nullptr : &*pivot;
}

bool IsReachable(const FigureRig& rig, const RigLimb& limb) {
    const std::array pivots{PivotOf(rig, limb.nodes[0]), PivotOf(rig, limb.nodes[1]),
                            PivotOf(rig, limb.nodes[2])};
    if (std::ranges::contains(pivots, nullptr)) {
        return false;
    }
    return Length(pivots[1]->position - pivots[0]->position) >= kShortestSegment &&
           Length(pivots[2]->position - pivots[1]->position) >= kShortestSegment;
}

Result<void> ReadLine(std::string_view line, FigureRig& rig) {
    std::istringstream words{std::string(line)};
    std::string keyword;
    words >> keyword;
    if (keyword.empty()) {
        return {};
    }
    if (keyword == kPivot) {
        RigPivot pivot;
        if (words >> pivot.node >> pivot.position.x >> pivot.position.y >> pivot.position.z) {
            rig.pivots.push_back(pivot);
            return {};
        }
    } else if (keyword == kLimb) {
        RigLimb limb;
        if (words >> limb.nodes[0] >> limb.nodes[1] >> limb.nodes[2]) {
            rig.limbs.push_back(limb);
            return {};
        }
    }
    return Fail(ErrorCode::InvalidData, std::format("the rig line '{}' can't be read", line));
}

}

std::string WriteFigureRig(const FigureRig& rig) {
    std::string text = std::format("{}\n", kVersion);
    for (const RigPivot& pivot : rig.pivots) {
        text += std::format("{} {} {} {} {}\n", kPivot, pivot.node, pivot.position.x,
                            pivot.position.y, pivot.position.z);
    }
    for (const RigLimb& limb : rig.limbs) {
        text += std::format("{} {} {} {}\n", kLimb, limb.nodes[0], limb.nodes[1], limb.nodes[2]);
    }
    return text;
}

Result<FigureRig> ParseFigureRig(std::span<const std::byte> bytes) {
    std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (text.starts_with(kByteOrderMark)) {
        text.remove_prefix(kByteOrderMark.size());
    }
    FigureRig rig;
    const std::size_t versionEnd = std::min(text.find('\n'), text.size());
    if (LineAt(text, 0, versionEnd) != kVersion) {
        return Fail(ErrorCode::InvalidData, std::format("a rig starts with '{}'", kVersion));
    }
    std::size_t start = versionEnd + 1;
    while (start < text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        if (Result<void> read = ReadLine(LineAt(text, start, end), rig); !read) {
            return std::unexpected(std::move(read.error()));
        }
        start = end + 1;
    }
    if (!std::ranges::all_of(rig.limbs,
                             [&](const RigLimb& limb) { return IsReachable(rig, limb); })) {
        return Fail(ErrorCode::InvalidData,
                    "a rig limb has a node without a pivot or a segment too short to bend");
    }
    return rig;
}

}
