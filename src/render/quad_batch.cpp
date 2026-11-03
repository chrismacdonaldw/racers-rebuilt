#include "render/quad_batch.hpp"

#include <array>
#include <cstddef>

namespace rr::render {
namespace {

std::array<TexturePoint, 4> CornerUvs(const QuadDraw& quad) {
    if (quad.cornerUvs) {
        return *quad.cornerUvs;
    }
    const Rect& uv = quad.uv;
    const float right = uv.x + uv.width;
    const float bottom = uv.y + uv.height;
    return {{{.u = uv.x, .v = uv.y},
             {.u = right, .v = uv.y},
             {.u = uv.x, .v = bottom},
             {.u = right, .v = bottom}}};
}

}

void BatchQuads(std::span<const QuadDraw> quads, std::vector<QuadVertex>& vertices,
                std::vector<SpriteBatch>& batches) {
    vertices.clear();
    batches.clear();
    for (const QuadDraw& quad : quads) {
        const std::array<TexturePoint, 4> uvs = CornerUvs(quad);
        for (std::size_t corner = 0; corner < uvs.size(); ++corner) {
            vertices.push_back({.position = quad.corners.at(corner),
                                .u = uvs.at(corner).u,
                                .v = uvs.at(corner).v,
                                .color = quad.tint * quad.cornerTints.at(corner)});
        }
        AddToBatches(batches, quad.texture, quad.blend, quad.sampling);
    }
}

}
