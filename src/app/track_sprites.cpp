#include "app/track_sprites.hpp"

#include <chrono>
#include <string>

#include "app/track_space.hpp"
#include "assets/game_path.hpp"
#include "core/duration.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

std::optional<std::string> TableMaterialName(const assets::WorldAsset& world,
                                             const assets::SpriteMaterial& material,
                                             const assets::TrackAssets& track) {
    const auto table = static_cast<std::size_t>(material.materialTable);
    if (table >= world.materialTables.size()) {
        return std::nullopt;
    }
    const auto model = track.models.find(assets::ToUpperAscii(world.materialTables[table]));
    const auto slot = static_cast<std::size_t>(material.material);
    if (model == track.models.end() || slot >= model->second->materials.size()) {
        return std::nullopt;
    }
    return model->second->materials[slot];
}

std::optional<PlayedTrack> AnimationOf(const assets::WorldAsset& world,
                                       const assets::WorldSprite& sprite,
                                       const assets::TrackAssets& track,
                                       const assets::NameMap<render::Material>& materials) {
    if (!sprite.animation) {
        return std::nullopt;
    }
    return ResolveTrack(world, static_cast<std::size_t>(sprite.animation->animation),
                        static_cast<std::size_t>(sprite.animation->track),
                        {.animations = &track.materialAnimations, .materials = &materials});
}

QuadLook ShownLook(const QuadLook& look, const std::optional<PlayedTrack>& animation,
                   Milliseconds age) {
    if (!animation) {
        return look;
    }
    return LookOf(animation->materials[assets::SampleTrack(animation->frames, 0, age)]);
}

}

void TrackSprites::Add(const assets::WorldAsset& world, const assets::TrackAssets& track,
                       const assets::NameMap<render::Material>& materials) {
    for (const assets::WorldSprite& sprite : world.sprites) {
        const std::optional<std::string> name =
            sprite.material ? TableMaterialName(world, *sprite.material, track)
                            : std::optional(sprite.materialName);
        const auto material = name ? materials.find(assets::ToUpperAscii(*name)) : materials.end();
        if (material == materials.end()) {
            log::Warn("{} sprite material not found", name.value_or("table"));
            continue;
        }
        sprites_.push_back({.look = LookOf(material->second),
                            .animation = AnimationOf(world, sprite, track, materials),
                            .centre = ToEngine(sprite.position),
                            .top = sprite.axis.transform([&](const Vector3& axis) {
                                return Normalized(ToEngine(axis), {}) * (sprite.height * 0.5F);
                            }),
                            .width = sprite.width,
                            .height = sprite.height,
                            .rangeSquared = sprite.maxDistance * sprite.maxDistance});
    }
}

void TrackSprites::Append(const Transform& camera, bool hidesFar, float shownMs,
                          std::vector<render::QuadDraw>& out) const {
    const Milliseconds age{static_cast<Milliseconds::rep>(shownMs)};
    for (const Sprite& sprite : sprites_) {
        const Vector3 offset = sprite.centre - camera.position;
        if (hidesFar && Dot(offset, offset) > sprite.rangeSquared) {
            continue;
        }
        const QuadLook look = ShownLook(sprite.look, sprite.animation, age);
        if (sprite.top) {
            AppendUprightBillboard(look, sprite.centre, *sprite.top, sprite.width, camera, out);
        } else {
            AppendBillboard(look, sprite.centre, sprite.width, sprite.height, camera, out);
        }
    }
}

}
