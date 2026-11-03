#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/color.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

inline constexpr std::int32_t kNoIndex = -1;
inline constexpr std::size_t kMaxWorldLods = 3;

struct WorldLod {
    std::int32_t model = kNoIndex;
    std::int32_t sceneNode = kNoIndex;
    std::int32_t modelPart = kNoIndex;
    std::int32_t boundingShape = kNoIndex;
    float maxDistance = std::numeric_limits<float>::infinity();

    friend constexpr bool operator==(const WorldLod&, const WorldLod&) = default;
};

struct NameOrIndex {
    std::int32_t index = kNoIndex;
    std::string name;

    friend bool operator==(const NameOrIndex&, const NameOrIndex&) = default;
};

struct MaterialAnimationBinding {
    std::int32_t animation = 0;
    std::int32_t track = 0;
    std::int32_t material = 0;
    std::int32_t materialTable = 0;

    friend constexpr bool operator==(const MaterialAnimationBinding&,
                                     const MaterialAnimationBinding&) = default;
};

struct TextureScroll {
    float uPerSecond = 0.0F;
    float vPerSecond = 0.0F;

    friend constexpr bool operator==(const TextureScroll&, const TextureScroll&) = default;
};

struct WorldEntity {
    std::string name;
    std::vector<WorldLod> lods;
    Vector3 position;
    Vector3 forward{.x = 1.0F};
    Vector3 up{.z = 1.0F};
    float scale = 1.0F;
    std::array<std::int32_t, kMaxWorldLods> materialTables{kNoIndex, kNoIndex, kNoIndex};
    std::vector<MaterialAnimationBinding> materialAnimations;
    std::optional<TextureScroll> textureScroll;
    std::optional<NameOrIndex> startPart;
    std::int32_t boundingVolume = kNoIndex;
    bool hasFlag42 = false;
    bool isEnvironmentMapped = false;
};

struct SpriteMaterial {
    std::int32_t materialTable = 0;
    std::int32_t material = 0;

    friend constexpr bool operator==(const SpriteMaterial&, const SpriteMaterial&) = default;
};

struct SpriteAnimation {
    std::int32_t animation = 0;
    std::int32_t track = 0;

    friend constexpr bool operator==(const SpriteAnimation&, const SpriteAnimation&) = default;
};

struct WorldSprite {
    std::string materialName;
    std::optional<SpriteMaterial> material;
    std::optional<SpriteAnimation> animation;
    Vector3 position;
    std::optional<Vector3> axis;
    float width = 0.0F;
    float height = 0.0F;
    float maxDistance = std::numeric_limits<float>::infinity();
};

struct WorldCamera {
    std::string name;
    std::int32_t trackedEntity = kNoIndex;
    std::int32_t trackedNode = 0;
    std::optional<NameOrIndex> node;
    Vector3 position;
    Vector3 forward{.x = 1.0F};
    Vector3 up{.z = 1.0F};
    std::optional<Vector3> target;
    float nearClip = 1.0F;
    float farClip = 100.0F;
    float fovDegrees = 65.0F;
};

struct WorldLight {
    std::string name;
    Rgba8 color{0, 0, 0, 255};
    Vector3 direction;
};

struct WorldAsset {
    std::vector<std::string> textureLists;
    std::vector<std::string> materialLibraries;
    std::vector<std::string> modelParts;
    std::vector<std::string> models;
    std::vector<std::string> materialTables;
    std::vector<std::string> sceneNodes;
    std::vector<std::string> boundingShapes;
    std::vector<std::string> boundingVolumes;
    std::vector<std::string> materialAnimations;
    std::vector<WorldEntity> modelEntities;
    std::vector<WorldEntity> animatedEntities;
    std::vector<WorldEntity> collidableEntities;
    std::vector<WorldEntity> boundedEntities;
    std::vector<WorldSprite> sprites;
    std::vector<WorldCamera> cameras;
    std::vector<WorldLight> ambientLights;
    std::vector<WorldLight> lights;
};

/// Fails on any index past its list. Start parts and camera nodes are not checked.
[[nodiscard]] Result<WorldAsset> ParseWorld(std::span<const std::byte> bytes);

/// From the camera, as the original squares it at load. Past it no level is drawn.
[[nodiscard]] float DrawRangeSquared(const WorldEntity& entity);

}
