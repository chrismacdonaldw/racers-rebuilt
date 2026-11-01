#pragma once

#include <span>
#include <vector>

#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "render/sprite_batch.hpp"

namespace rr::render {

struct QuadVertex {
    Vector3 position;
    float u = 0.0F;
    float v = 0.0F;
    Color color;
};

/// Overwrites both outputs, batching neighbours as `BatchSprites` does.
void BatchQuads(std::span<const QuadDraw> quads, std::vector<QuadVertex>& vertices,
                std::vector<SpriteBatch>& batches);

}
