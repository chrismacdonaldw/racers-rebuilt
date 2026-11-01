#include "render/quad_batch.hpp"

namespace rr::render {

void BatchQuads(std::span<const QuadDraw> quads, std::vector<QuadVertex>& vertices,
                std::vector<SpriteBatch>& batches) {
    vertices.clear();
    batches.clear();
    for (const QuadDraw& quad : quads) {
        const Rect& uv = quad.uv;
        const float uvRight = uv.x + uv.width;
        const float uvBottom = uv.y + uv.height;
        vertices.insert(vertices.end(), {{.position = quad.corners[0],
                                          .u = uv.x,
                                          .v = uv.y,
                                          .color = quad.tint * quad.cornerTints[0]},
                                         {.position = quad.corners[1],
                                          .u = uvRight,
                                          .v = uv.y,
                                          .color = quad.tint * quad.cornerTints[1]},
                                         {.position = quad.corners[2],
                                          .u = uv.x,
                                          .v = uvBottom,
                                          .color = quad.tint * quad.cornerTints[2]},
                                         {.position = quad.corners[3],
                                          .u = uvRight,
                                          .v = uvBottom,
                                          .color = quad.tint * quad.cornerTints[3]}});
        AddToBatches(batches, quad.texture, quad.blend);
    }
}

}
