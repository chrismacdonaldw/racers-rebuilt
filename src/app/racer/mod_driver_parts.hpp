#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "assets/driver_parts.hpp"
#include "assets/part_library.hpp"
#include "assets/vfs.hpp"
#include "core/color.hpp"
#include "core/result.hpp"

namespace rr::app {

inline constexpr std::string_view kModPartsFile = "MENUDATA/PARTDB/PARTS.TOML";
inline constexpr std::size_t kFaceExpressions = 6;

struct ModHat {
    std::string name;
    std::string model;
};

/// Images by expression in `FaceMaterial` order. An empty one shows the default.
struct ModFace {
    std::string name;
    std::array<std::string, kFaceExpressions> images;
};

struct ModDress {
    std::string name;
    std::string texture;
    std::int32_t variant = 0;
};

struct ModColour {
    std::string name;
    Rgba8 rgba = kOpaqueWhite;
};

/// File paths are relative to `MENUDATA/PARTDB`.
struct ModDriverParts {
    std::vector<ModHat> hats;
    std::vector<ModFace> faces;
    std::vector<ModDress> torsos;
    std::vector<ModDress> legs;
    std::vector<ModColour> colours;
};

/// An entry that's broken is logged and skipped.
[[nodiscard]] Result<ModDriverParts> ParseModDriverParts(std::string_view text,
                                                         std::string_view source);

/// Every layer's `PARTS.TOML`, lowest first, merged by name. A file that doesn't parse is skipped.
[[nodiscard]] ModDriverParts LoadModDriverParts(const assets::Vfs& vfs);

/// The parts as catalogue entries, each hat with a head part of its own name.
[[nodiscard]] assets::DriverPartCatalog ModPartCatalog(const ModDriverParts& parts);

/// Each hat's model as a head part. A model that doesn't load is logged and skipped.
[[nodiscard]] assets::PartLibrary ModHeadParts(const assets::Vfs& vfs, const ModDriverParts& parts);

/// `LoadModDriverParts`, read once for each `Vfs::Generation`.
[[nodiscard]] std::shared_ptr<const ModDriverParts> CachedModDriverParts(const assets::Vfs& vfs);

/// `ModHeadParts` of the cached parts, imported once for each `Vfs::Generation`.
[[nodiscard]] std::shared_ptr<const assets::PartLibrary> CachedModHeadParts(const assets::Vfs& vfs);

}
