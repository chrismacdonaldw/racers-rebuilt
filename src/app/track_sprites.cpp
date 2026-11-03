#include "app/track_sprites.hpp"

#include "app/track_space.hpp"
#include "assets/game_path.hpp"
#include "core/log.hpp"

namespace rr::app {

void TrackSprites::Add(std::span<const assets::WorldSprite> sprites,
                       const assets::NameMap<render::Material>& materials) {
    for (const assets::WorldSprite& sprite : sprites) {
        if (sprite.material) {
            continue;
        }
        const auto material = materials.find(assets::ToUpperAscii(sprite.materialName));
        if (material == materials.end()) {
            log::Warn("{} sprite material not found", sprite.materialName);
            continue;
        }
        sprites_.push_back({.look = LookOf(material->second),
                            .centre = ToEngine(sprite.position),
                            .top = sprite.axis.transform([&](const Vector3& axis) {
                                return Normalized(ToEngine(axis), {}) * (sprite.height * 0.5F);
                            }),
                            .width = sprite.width,
                            .height = sprite.height,
                            .rangeSquared = sprite.maxDistance * sprite.maxDistance});
    }
}

void TrackSprites::Append(const Transform& camera, bool hidesFar,
                          std::vector<render::QuadDraw>& out) const {
    for (const Sprite& sprite : sprites_) {
        const Vector3 offset = sprite.centre - camera.position;
        if (hidesFar && Dot(offset, offset) > sprite.rangeSquared) {
            continue;
        }
        if (sprite.top) {
            AppendUprightBillboard(sprite.look, sprite.centre, *sprite.top, sprite.width, camera,
                                   out);
        } else {
            AppendBillboard(sprite.look, sprite.centre, sprite.width, sprite.height, camera, out);
        }
    }
}

}
