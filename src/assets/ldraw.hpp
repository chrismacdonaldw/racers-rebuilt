#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

inline constexpr std::int32_t kLdrawMainColour = 16;
inline constexpr std::int32_t kLdrawEdgeColour = 24;

/// LDraw's `x y z a b c d e f g h i`: a point p moves to `position + rows · p`.
struct LdrawMatrix {
    Vector3 position;
    std::array<Vector3, 3> rows{Vector3{.x = 1.0F}, Vector3{.y = 1.0F}, Vector3{.z = 1.0F}};

    friend bool operator==(const LdrawMatrix&, const LdrawMatrix&) = default;
};

[[nodiscard]] Vector3 Apply(const LdrawMatrix& matrix, const Vector3& point);

/// `inner` first, then `outer`.
[[nodiscard]] LdrawMatrix Compose(const LdrawMatrix& outer, const LdrawMatrix& inner);

[[nodiscard]] float Determinant(const LdrawMatrix& matrix);

struct LdrawReference {
    std::int32_t colour = kLdrawMainColour;
    LdrawMatrix matrix;
    std::string file;
    bool isInverted = false;

    friend bool operator==(const LdrawReference&, const LdrawReference&) = default;
};

/// Corners are stored counter-clockwise, whatever winding the file declared.
struct LdrawFace {
    std::int32_t colour = kLdrawMainColour;
    std::array<Vector3, 4> corners{};
    std::uint8_t cornerCount = 3;

    friend bool operator==(const LdrawFace&, const LdrawFace&) = default;
};

struct LdrawFile {
    std::string licence;
    std::vector<LdrawReference> references;
    std::vector<LdrawFace> faces;
};

/// Fails on a malformed face or sub-file line. Edge lines and other meta lines are skipped.
[[nodiscard]] Result<LdrawFile> ParseLdraw(std::string_view text);

}
