#pragma once

#include <cstddef>

#include "render/render_view.hpp"

namespace rr::render {

inline constexpr std::size_t kSamplerCount = 6;

/// Point filtering ignores isAnisotropic, since anisotropy would make it linear.
[[nodiscard]] constexpr std::size_t SamplerSlot(TextureFilter filter, TextureAddress address,
                                                bool isAnisotropic) {
    std::size_t set = 0;
    if (filter == TextureFilter::Point) {
        set = 1;
    } else if (isAnisotropic) {
        set = 2;
    }
    return (set * 2) + static_cast<std::size_t>(address);
}

}
