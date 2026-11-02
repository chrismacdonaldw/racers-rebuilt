#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

struct DriverPart {
    std::string name;
    std::int32_t variant = 0;
    std::int32_t unlock = 0;
};

struct DriverPartShelf {
    std::string folder;
    std::string hookFolder;
    std::vector<DriverPart> parts;
};

/// Face part names are texture prefixes such as `kk_`. `headParts` holds one GCB part per hat.
struct DriverPartCatalog {
    std::vector<std::string> modelFolders;
    std::vector<std::string> modelFiles;
    std::vector<DriverPart> hats;
    DriverPartShelf faces;
    DriverPartShelf torsos;
    DriverPartShelf legs;
    std::vector<std::string> headParts;
    std::vector<std::string> bodies;
    std::vector<std::string> slots;
};

[[nodiscard]] Result<DriverPartCatalog> ParseDriverParts(std::span<const std::byte> bytes);

[[nodiscard]] bool IsPartUnlocked(std::int32_t unlock, const std::set<std::int32_t>& partSets,
                                  bool allRacesUnlocked);

/// Expressions outside 0 to 5 get the default face.
[[nodiscard]] std::string FaceMaterial(std::string_view prefix, std::int32_t expression);

}
