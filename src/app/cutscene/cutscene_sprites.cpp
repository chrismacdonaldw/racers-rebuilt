#include "app/cutscene/cutscene_sprites.hpp"

#include <format>

#include "app/track_space.hpp"
#include "assets/game_path.hpp"
#include "core/vector3.hpp"

namespace rr::app {

Result<CutsceneSprites> CutsceneSprites::Create(
    std::span<const ModelBinding> models, const assets::NameMap<render::Material>& materials) {
    CutsceneSprites created;
    for (const ModelBinding& model : models) {
        std::optional<Sprite>& sprite = created.sprites_.emplace_back();
        if (!model.sprite) {
            continue;
        }
        const auto material = materials.find(assets::ToUpperAscii(model.sprite->materialName));
        if (material == materials.end()) {
            return Fail(ErrorCode::NotFound,
                        std::format("{} sprite material not found", model.sprite->materialName));
        }
        sprite = Sprite{.look = LookOf(material->second),
                        .width = model.sprite->width,
                        .height = model.sprite->height,
                        .maxDistance = model.sprite->maxDistance};
    }
    return created;
}

void CutsceneSprites::Append(std::size_t model, const Transform& placement, const Transform& camera,
                             std::vector<render::QuadDraw>& out) const {
    const std::optional<Sprite>& sprite = sprites_[model];
    if (!sprite) {
        return;
    }
    const Vector3 centre = ToEngine(placement.position);
    if (Length(centre - camera.position) > sprite->maxDistance) {
        return;
    }
    AppendBillboard(sprite->look, centre, sprite->width, sprite->height, camera, out);
}

}
