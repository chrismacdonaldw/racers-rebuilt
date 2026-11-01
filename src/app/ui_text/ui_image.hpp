#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "assets/image_list.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct UiImage {
    render::TextureId texture{};
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
};

[[nodiscard]] Result<UiImage> LoadUiImage(render::Renderer& renderer, const assets::Vfs& vfs,
                                          std::string_view folder,
                                          const assets::ImageListEntry& entry);

/// In list order.
[[nodiscard]] Result<std::vector<UiImage>> LoadUiImages(render::Renderer& renderer,
                                                        const assets::Vfs& vfs,
                                                        std::string_view folder,
                                                        std::string_view imageList);

}
