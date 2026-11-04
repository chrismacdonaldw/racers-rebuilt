#include "assets/minifig_skin.hpp"

#include <cstddef>
#include <tuple>
#include <utility>

namespace rr::assets {
namespace {

constexpr int kBoundarySteps = 12;
constexpr int kSplitDepth = 4;

SkinCorner Mix(const SkinCorner& from, const SkinCorner& to, float amount) {
    return {.position = from.position + ((to.position - from.position) * amount),
            .normal = Normalized(from.normal + ((to.normal - from.normal) * amount), from.normal)};
}

bool IsBefore(const Vector3& a, const Vector3& b) {
    return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
}

SkinCorner Boundary(const JointLocator& locator, const SkinCorner& first,
                    const SkinCorner& second) {
    const bool swapped = IsBefore(second.position, first.position);
    const SkinCorner& from = swapped ? second : first;
    const SkinCorner& to = swapped ? first : second;
    const std::uint16_t side = locator(from.position);
    float near = 0.0F;
    float far = 1.0F;
    for (int step = 0; step < kBoundarySteps; ++step) {
        const float middle = (near + far) * 0.5F;
        (locator(Mix(from, to, middle).position) == side ? near : far) = middle;
    }
    return Mix(from, to, (near + far) * 0.5F);
}

std::size_t LoneCorner(const std::array<std::uint16_t, 3>& joints) {
    for (std::size_t corner = 0; corner < 3; ++corner) {
        if (joints.at(corner) != joints.at((corner + 1) % 3) &&
            joints.at(corner) != joints.at((corner + 2) % 3)) {
            return corner;
        }
    }
    return 0;
}

}

std::vector<SkinnedTriangle> SkinTriangle(const JointLocator& locator,
                                          const std::array<SkinCorner, 3>& corners) {
    using Corners = std::array<SkinCorner, 3>;
    std::vector<SkinnedTriangle> skinned;
    std::vector<std::pair<Corners, int>> pending{{corners, kSplitDepth}};
    while (!pending.empty()) {
        const auto [part, depth] = pending.back();
        pending.pop_back();
        std::array<std::uint16_t, 3> joints{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            joints.at(corner) = locator(part.at(corner).position);
        }
        if (depth == 0 || (joints[0] == joints[1] && joints[1] == joints[2])) {
            skinned.push_back({.corners = part, .joints = joints});
            continue;
        }
        const std::size_t lone = LoneCorner(joints);
        const SkinCorner& a = part.at(lone);
        const SkinCorner& b = part.at((lone + 1) % 3);
        const SkinCorner& c = part.at((lone + 2) % 3);
        const SkinCorner ab = Boundary(locator, a, b);
        const SkinCorner ac = Boundary(locator, a, c);
        for (const Corners& piece : {Corners{a, ab, ac}, Corners{ab, b, c}, Corners{ab, c, ac}}) {
            pending.emplace_back(piece, depth - 1);
        }
    }
    return skinned;
}

}
