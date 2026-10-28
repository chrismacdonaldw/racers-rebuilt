#include "render/sprite_batch.hpp"

#include <array>
#include <cstddef>

namespace rr::render {

void BatchSprites(std::span<const SpriteDraw> sprites, std::vector<SpriteVertex>& vertices,
                  std::vector<SpriteBatch>& batches) {
    vertices.clear();
    batches.clear();
    for (const SpriteDraw& sprite : sprites) {
        const Rect& at = sprite.bounds;
        const Rect& uv = sprite.uv;
        const float right = at.x + at.width;
        const float bottom = at.y + at.height;
        const float uvRight = uv.x + uv.width;
        const float uvBottom = uv.y + uv.height;
        vertices.insert(
            vertices.end(),
            {{.x = at.x, .y = at.y, .u = uv.x, .v = uv.y, .color = sprite.tint},
             {.x = right, .y = at.y, .u = uvRight, .v = uv.y, .color = sprite.tint},
             {.x = at.x, .y = bottom, .u = uv.x, .v = uvBottom, .color = sprite.tint},
             {.x = right, .y = bottom, .u = uvRight, .v = uvBottom, .color = sprite.tint}});

        if (!batches.empty() && batches.back().texture == sprite.texture &&
            batches.back().blend == sprite.blend) {
            ++batches.back().spriteCount;
            continue;
        }
        const auto first = static_cast<std::uint32_t>(vertices.size() / kVerticesPerSprite) - 1;
        batches.push_back({.texture = sprite.texture,
                           .blend = sprite.blend,
                           .firstSprite = first,
                           .spriteCount = 1});
    }
}

std::vector<std::uint16_t> SpriteIndices(std::uint32_t spriteCount) {
    constexpr std::array<std::uint16_t, kIndicesPerSprite> kCorners{0, 1, 2, 2, 1, 3};
    std::vector<std::uint16_t> indices;
    indices.reserve(std::size_t{spriteCount} * kIndicesPerSprite);
    for (std::uint32_t sprite = 0; sprite < spriteCount; ++sprite) {
        for (const std::uint16_t corner : kCorners) {
            indices.push_back(static_cast<std::uint16_t>((sprite * kVerticesPerSprite) + corner));
        }
    }
    return indices;
}

}
