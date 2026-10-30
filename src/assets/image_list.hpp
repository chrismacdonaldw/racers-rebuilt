#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/texture_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct ImageListEntry {
    std::string name;
    TextureFileKind fileKind = TextureFileKind::Tga;
    std::optional<ColorKey> colorKey;

    [[nodiscard]] std::string FileName() const { return ImageFileName(name, fileKind); }
};

[[nodiscard]] Result<std::vector<ImageListEntry>> ParseImageList(std::span<const std::byte> bytes);

}
