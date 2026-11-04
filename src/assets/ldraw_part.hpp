#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "assets/geometry_asset.hpp"
#include "assets/ldraw.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr float kGameUnitsPerLdu = 0.05F;

struct LdrawLimits {
    int maxDepth = 16;
    std::size_t maxTriangles = 100'000;
};

/// Searches `parts/`, `parts/s/` and `p/` in that order.
class LdrawLibrary {
public:
    explicit LdrawLibrary(std::filesystem::path folder) : folder_(std::move(folder)) {}

    /// Each range's material is its resolved LDraw colour code, such as `16` for the main colour.
    [[nodiscard]] Result<GeometryAsset> LoadPart(std::string_view name,
                                                 const LdrawLimits& limits = {});

    /// Parsed once, then cached.
    [[nodiscard]] Result<const LdrawFile*> Find(std::string_view name);

private:
    std::filesystem::path folder_;
    std::unordered_map<std::string, LdrawFile> cache_;
};

}
