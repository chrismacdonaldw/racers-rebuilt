#include "assets/world_database.hpp"

#include <algorithm>
#include <format>
#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

template <typename T, std::size_t... Sizes>
constexpr auto Concat(const std::array<T, Sizes>&... parts) {
    std::array<T, (Sizes + ...)> joined{};
    auto next = joined.begin();
    ((next = std::ranges::copy(parts, next).out), ...);
    return joined;
}

template <auto Member, auto... Parts>
constexpr auto OptionalField(FieldId id) {
    using Target = MemberPointer<decltype(Member)>::Target;
    return FieldRule<Target>{
        .id = id,
        .read =
            [](TokenReader& reader, Target& target) {
                return ReadMembers<Parts...>(reader, (target.*Member).emplace());
            },
    };
}

template <typename Target>
Result<void> ReadNode(TokenReader& reader, Target& target) {
    return reader.Next().and_then([&](const Token& token) -> Result<void> {
        if (token.kind == TokenKind::String) {
            target.node = NodeReference{.name = std::string(token.text)};
        } else if (token.kind == TokenKind::Int) {
            target.node = NodeReference{.index = token.integer};
        } else {
            return Fail(ErrorCode::InvalidData,
                        std::format("expected a node index or name at offset {}", token.offset));
        }
        return {};
    });
}

template <auto... Indices>
Result<void> ReadLod(TokenReader& reader, WorldEntity& entity) {
    if (entity.lods.size() == kMaxWorldLods) {
        return Fail(ErrorCode::InvalidData,
                    std::format("an entity has more than {} levels of detail", kMaxWorldLods));
    }
    WorldLod& lod = entity.lods.emplace_back();
    return ReadMembers<Indices..., &WorldLod::maxDistance>(reader, lod).transform([&lod] {
        if (lod.maxDistance < 0.0F) {
            lod.maxDistance = std::numeric_limits<float>::infinity();
        }
    });
}

Result<void> ReadMaterialSlot(TokenReader& reader, WorldEntity& entity) {
    std::int32_t table = kNoIndex;
    std::int32_t slot = 0;
    Result<void> read = ReadValue(reader, table).and_then([&] { return ReadValue(reader, slot); });
    if (!read) {
        return read;
    }
    if (!std::in_range<std::size_t>(slot) ||
        static_cast<std::size_t>(slot) >= entity.materialTables.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("material table slot {} is past {}", slot, kMaxWorldLods - 1));
    }
    entity.materialTables.at(static_cast<std::size_t>(slot)) = table;
    return {};
}

Result<void> ReadBoundedMaterial(TokenReader& reader, WorldEntity& entity) {
    return ReadValue(reader, entity.materialTables.front());
}

Result<void> ReadAnimationBindings(TokenReader& reader, WorldEntity& entity) {
    entity.materialAnimations.clear();
    return ReadList(reader, [&] {
        return ReadMembers<&MaterialAnimationBinding::animation, &MaterialAnimationBinding::track,
                           &MaterialAnimationBinding::material,
                           &MaterialAnimationBinding::materialTable>(
            reader, entity.materialAnimations.emplace_back());
    });
}

Result<void> ReadLightColor(TokenReader& reader, WorldLight& light) {
    return ReadValue(reader, light.color[0])
        .and_then([&] { return ReadValue(reader, light.color[1]); })
        .and_then([&] { return ReadValue(reader, light.color[2]); });
}

constexpr std::array kPlacementFields{
    Field<&WorldEntity::position>(0x31),
    Field<&WorldEntity::forward, &WorldEntity::up>(0x32),
};

constexpr std::array kDecorationFields{
    FieldRule<WorldEntity>{.id = 0x2B, .read = ReadMaterialSlot},
    FieldRule<WorldEntity>{.id = 0x3E, .read = ReadAnimationBindings},
    OptionalField<&WorldEntity::textureScroll, &TextureScroll::uPerSecond,
                  &TextureScroll::vPerSecond>(0x3F),
    Field<&WorldEntity::hasFlag42>(0x42),
};

constexpr auto kModelEntityFields =
    Concat(kPlacementFields, kDecorationFields,
           std::array{
               FieldRule<WorldEntity>{.id = 0x2A, .read = ReadLod<&WorldLod::model>},
               Field<&WorldEntity::scale>(0x36),
               Field<&WorldEntity::hasFlag4C>(0x4C),
           });

constexpr auto kAnimatedEntityFields =
    Concat(kPlacementFields, kDecorationFields,
           std::array{
               FieldRule<WorldEntity>{
                   .id = 0x33,
                   .read = ReadLod<&WorldLod::model, &WorldLod::sceneNode, &WorldLod::modelPart>},
               FieldRule<WorldEntity>{.id = 0x2C,
                                      .read = ReadLod<&WorldLod::sceneNode, &WorldLod::modelPart>},
               FieldRule<WorldEntity>{.id = 0x35, .read = ReadNode<WorldEntity>},
               Field<&WorldEntity::scale>(0x36),
               Field<&WorldEntity::hasFlag4C>(0x4C),
           });

constexpr auto kCollidableEntityFields =
    Concat(kPlacementFields, kDecorationFields,
           std::array{
               FieldRule<WorldEntity>{.id = 0x34,
                                      .read = ReadLod<&WorldLod::model, &WorldLod::boundingShape>},
           });

constexpr auto kBoundedEntityFields =
    Concat(kPlacementFields, std::array{
                                 Field<&WorldEntity::boundingVolume>(0x40),
                                 FieldRule<WorldEntity>{.id = 0x2B, .read = ReadBoundedMaterial},
                             });

constexpr std::array kSpriteFields{
    OptionalField<&WorldSprite::material, &SpriteMaterial::materialTable,
                  &SpriteMaterial::material>(0x2B),
    Field<&WorldSprite::position>(0x31),
    Field<&WorldSprite::axis>(0x38),
    Field<&WorldSprite::materialName>(0x39),
    Field<&WorldSprite::width>(0x3A),
    Field<&WorldSprite::height>(0x3B),
    Field<&WorldSprite::maxDistance>(0x3C),
    OptionalField<&WorldSprite::animation, &SpriteAnimation::animation, &SpriteAnimation::track>(
        0x3E),
};

constexpr std::array kCameraFields{
    Field<&WorldCamera::trackedEntity, &WorldCamera::trackedNode>(0x2F),
    Field<&WorldCamera::position>(0x31),
    Field<&WorldCamera::forward, &WorldCamera::up>(0x32),
    FieldRule<WorldCamera>{.id = 0x35, .read = ReadNode<WorldCamera>},
    Field<&WorldCamera::target>(0x44),
    Field<&WorldCamera::nearClip>(0x45),
    Field<&WorldCamera::farClip>(0x46),
    Field<&WorldCamera::fovDegrees>(0x47),
};

constexpr std::array kAmbientLightFields{
    FieldRule<WorldLight>{.id = 0x4A, .read = ReadLightColor},
};

constexpr std::array kLightFields{
    FieldRule<WorldLight>{.id = 0x4A, .read = ReadLightColor},
    Field<&WorldLight::direction>(0x4B),
};

template <typename Record>
Result<void> ReadOptionalName([[maybe_unused]] TokenReader& reader,
                              [[maybe_unused]] Record& record) {
    if constexpr (requires { record.name; }) {
        Result<Token> next = reader.Peek();
        if (!next) {
            return std::unexpected(std::move(next.error()));
        }
        if (next->kind == TokenKind::String) {
            return ReadValue(reader, record.name);
        }
    }
    return {};
}

template <typename Record>
Result<void> ReadRecords(TokenReader& reader, FieldId field,
                         std::span<const FieldRule<Record>> rules, std::vector<Record>& records) {
    records.clear();
    return ReadList(reader, [&] {
        Record& record = records.emplace_back();
        return ExpectField(reader, field)
            .and_then([&] { return ReadOptionalName(reader, record); })
            .and_then([&] { return ReadBlock<Record>(reader, rules, record); });
    });
}

template <auto Member, FieldId Id, const auto& Rules>
constexpr FieldRule<WorldAsset> RecordList() {
    return {
        .id = Id,
        .read =
            [](TokenReader& reader, WorldAsset& world) {
                using Records = std::remove_reference_t<decltype(world.*Member)>;
                return ReadRecords<typename Records::value_type>(reader, Id, Rules, world.*Member);
            },
    };
}

constexpr std::array kWorldFields{
    Field<&WorldAsset::textureLists>(0x27),
    Field<&WorldAsset::materialLibraries>(0x28),
    Field<&WorldAsset::modelParts>(0x29),
    Field<&WorldAsset::models>(0x2A),
    Field<&WorldAsset::materialTables>(0x2B),
    Field<&WorldAsset::sceneNodes>(0x2C),
    Field<&WorldAsset::boundingShapes>(0x2D),
    Field<&WorldAsset::materialAnimations>(0x3D),
    Field<&WorldAsset::boundingVolumes>(0x40),
    RecordList<&WorldAsset::modelEntities, 0x2E, kModelEntityFields>(),
    RecordList<&WorldAsset::animatedEntities, 0x2F, kAnimatedEntityFields>(),
    RecordList<&WorldAsset::collidableEntities, 0x30, kCollidableEntityFields>(),
    RecordList<&WorldAsset::sprites, 0x37, kSpriteFields>(),
    RecordList<&WorldAsset::boundedEntities, 0x41, kBoundedEntityFields>(),
    RecordList<&WorldAsset::cameras, 0x43, kCameraFields>(),
    RecordList<&WorldAsset::ambientLights, 0x48, kAmbientLightFields>(),
    RecordList<&WorldAsset::lights, 0x49, kLightFields>(),
};

struct IndexUse {
    std::string_view list;
    std::int32_t index = kNoIndex;
    std::size_t count = 0;
};

void AddEntityUses(const WorldAsset& world, const WorldEntity& entity,
                   std::vector<IndexUse>& uses) {
    for (const WorldLod& lod : entity.lods) {
        uses.push_back({.list = "model", .index = lod.model, .count = world.models.size()});
        uses.push_back(
            {.list = "scene node", .index = lod.sceneNode, .count = world.sceneNodes.size()});
        uses.push_back(
            {.list = "model part", .index = lod.modelPart, .count = world.modelParts.size()});
        uses.push_back({.list = "bounding shape",
                        .index = lod.boundingShape,
                        .count = world.boundingShapes.size()});
    }
    for (const std::int32_t table : entity.materialTables) {
        uses.push_back(
            {.list = "material table", .index = table, .count = world.materialTables.size()});
    }
    for (const MaterialAnimationBinding& binding : entity.materialAnimations) {
        uses.push_back({.list = "material animation",
                        .index = binding.animation,
                        .count = world.materialAnimations.size()});
    }
    uses.push_back({.list = "bounding volume",
                    .index = entity.boundingVolume,
                    .count = world.boundingVolumes.size()});
}

std::vector<IndexUse> IndexUses(const WorldAsset& world) {
    std::vector<IndexUse> uses;
    for (const std::vector<WorldEntity>* entities :
         {&world.modelEntities, &world.animatedEntities, &world.collidableEntities,
          &world.boundedEntities}) {
        for (const WorldEntity& entity : *entities) {
            AddEntityUses(world, entity, uses);
        }
    }
    for (const WorldSprite& sprite : world.sprites) {
        if (sprite.material) {
            uses.push_back({.list = "material table",
                            .index = sprite.material->materialTable,
                            .count = world.materialTables.size()});
        }
        if (sprite.animation) {
            uses.push_back({.list = "material animation",
                            .index = sprite.animation->animation,
                            .count = world.materialAnimations.size()});
        }
    }
    for (const WorldCamera& camera : world.cameras) {
        uses.push_back({.list = "animated entity",
                        .index = camera.trackedEntity,
                        .count = world.animatedEntities.size()});
    }
    return uses;
}

Result<void> CheckIndices(const WorldAsset& world) {
    for (const IndexUse& use : IndexUses(world)) {
        const bool isInRange = use.index >= 0 && std::cmp_less(use.index, use.count);
        if (use.index != kNoIndex && !isInRange) {
            return Fail(ErrorCode::InvalidData, std::format("{} {} is past the {} in the list",
                                                            use.list, use.index, use.count));
        }
    }
    return {};
}

}

Result<WorldAsset> ParseWorld(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    WorldAsset world;
    return ReadFields<WorldAsset>(reader, kWorldFields, world)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return CheckIndices(world); })
        .transform([&] { return std::move(world); });
}

}
