#pragma once

#include <cstdint>
#include <span>

namespace rr::render::shaders {

extern const std::span<const std::uint8_t> kMeshVertex;
extern const std::span<const std::uint8_t> kMeshFragment;
extern const std::span<const std::uint8_t> kLineVertex;
extern const std::span<const std::uint8_t> kLineFragment;
extern const std::span<const std::uint8_t> kSpriteVertex;
extern const std::span<const std::uint8_t> kSpriteFragment;

}
