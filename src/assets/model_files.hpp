#pragma once

#include <string>

namespace rr::assets {

/// File names without their extensions.
struct ModelFiles {
    std::string materials;
    std::string textures;
    std::string geometry;

    friend bool operator==(const ModelFiles&, const ModelFiles&) = default;
};

}
