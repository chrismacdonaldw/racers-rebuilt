#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "render/render_view.hpp"

namespace rr::render {

struct SpriteVertex {
    float x = 0.0F;
    float y = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
    Color color;
};

inline constexpr std::uint32_t kVerticesPerSprite = 4;
inline constexpr std::uint32_t kIndicesPerSprite = 6;

struct SpriteBatch {
    TextureId texture{};
    BlendMode blend = BlendMode::Alpha;
    TextureSampling sampling;
    std::uint32_t firstSprite = 0;
    std::uint32_t spriteCount = 0;
};

void AddToBatches(std::vector<SpriteBatch>& batches, TextureId texture, BlendMode blend,
                  TextureSampling sampling = {});

/// In UI units: `bar` is the side bar's width, `hud` how far sprites anchored to a side move.
struct EdgeShifts {
    float bar = 0.0F;
    float hud = 0.0F;
};

/// Overwrites both outputs.
void BatchSprites(std::span<const SpriteDraw> sprites, EdgeShifts shifts,
                  std::vector<SpriteVertex>& vertices, std::vector<SpriteBatch>& batches);

[[nodiscard]] std::vector<std::uint16_t> SpriteIndices(std::uint32_t spriteCount);

}
