#include "render/sprite_batch.hpp"

#include <array>
#include <cstddef>

namespace rr::render {
namespace {

[[nodiscard]] float Anchored(float x, SpriteAnchor anchor, float shift, bool isRightEdge) {
    switch (anchor) {
        case SpriteAnchor::Center:
        case SpriteAnchor::Beside:
            return x;
        case SpriteAnchor::Left:
            return x - shift;
        case SpriteAnchor::Right:
            return x + shift;
        case SpriteAnchor::Fill:
            return isRightEdge ? x + shift : x - shift;
    }
    return x;
}

}

void AddToBatches(std::vector<SpriteBatch>& batches, TextureId texture, BlendMode blend,
                  TextureSampling sampling) {
    if (!batches.empty() && batches.back().texture == texture && batches.back().blend == blend &&
        batches.back().sampling == sampling) {
        ++batches.back().spriteCount;
        return;
    }
    const std::uint32_t first =
        batches.empty() ? 0 : batches.back().firstSprite + batches.back().spriteCount;
    batches.push_back({.texture = texture,
                       .blend = blend,
                       .sampling = sampling,
                       .firstSprite = first,
                       .spriteCount = 1});
}

void BatchSprites(std::span<const SpriteDraw> sprites, float edgeShift,
                  std::vector<SpriteVertex>& vertices, std::vector<SpriteBatch>& batches) {
    vertices.clear();
    batches.clear();
    for (const SpriteDraw& sprite : sprites) {
        if (sprite.anchor == SpriteAnchor::Beside && edgeShift <= 0.0F) {
            continue;
        }
        AddToBatches(batches, sprite.texture, sprite.blend);
        if (sprite.corners) {
            const std::array<SpriteCorner, kVerticesPerSprite>& corners = *sprite.corners;
            for (std::size_t i = 0; i < corners.size(); ++i) {
                const SpriteCorner& corner = corners[i];
                vertices.push_back({.x = Anchored(corner.x, sprite.anchor, edgeShift, i % 2 == 1),
                                    .y = corner.y,
                                    .u = corner.u,
                                    .v = corner.v,
                                    .color = corner.tint});
            }
            continue;
        }
        const Rect& at = sprite.bounds;
        const Rect& uv = sprite.uv;
        const float left = Anchored(at.x, sprite.anchor, edgeShift, false);
        const float right = Anchored(at.x + at.width, sprite.anchor, edgeShift, true);
        const float bottom = at.y + at.height;
        const float uvRight = uv.x + uv.width;
        const float uvBottom = uv.y + uv.height;
        vertices.insert(
            vertices.end(),
            {{.x = left, .y = at.y, .u = uv.x, .v = uv.y, .color = sprite.tint},
             {.x = right, .y = at.y, .u = uvRight, .v = uv.y, .color = sprite.tint},
             {.x = left, .y = bottom, .u = uv.x, .v = uvBottom, .color = sprite.tint},
             {.x = right, .y = bottom, .u = uvRight, .v = uvBottom, .color = sprite.tint}});
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
