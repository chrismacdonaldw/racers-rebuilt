#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <vector>

#include "app/menu/car_build_screen.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/track_space.hpp"
#include "assets/world_database.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kSpot{.z = 1.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr Vector3 kLookAbove{.z = 4.0F};
constexpr render::Color kWhite{};
constexpr auto kFirstButtonRow = static_cast<std::size_t>(BuildRow::Move);
constexpr float kItemTilt = 2.0F * std::numbers::pi_v<float> * 114.0F / 1024.0F;
constexpr float kLitLevel = 180.0F / 120.0F;
constexpr render::Color kLitAmbient{.r = kLitLevel, .g = kLitLevel, .b = kLitLevel};

Quaternion ItemTurn(float spin) {
    const Vector3 axis{.x = std::sin(kItemTilt), .z = std::cos(kItemTilt)};
    return FromAxisAngle(axis, spin) * FromAxisAngle(Vector3{.y = 1.0F}, kItemTilt);
}

std::size_t ButtonState(const CarBuildMenu& menu, std::size_t button) {
    const auto row = static_cast<BuildRow>(button + kFirstButtonRow);
    return menu.Focus() == row ? kFocusedState : kNormalState;
}

}

render::Rect CarBuildScreen::RowBounds(std::size_t row) const {
    switch (static_cast<BuildRow>(row)) {
        case BuildRow::Sets:
            return look_.sets.bounds;
        case BuildRow::Pieces:
            return look_.pieces.bounds;
        default:
            return look_.buttons.at(row - kFirstButtonRow).bounds;
    }
}

Vector3 CarBuildScreen::PieceCorner(float height) const {
    const sim::PiecePlacement at = builder_.Cursor().Placement();
    return {.x = fit_.offset.x + static_cast<float>(at.x),
            .y = fit_.offset.y + static_cast<float>(at.y),
            .z = fit_.offset.z + height};
}

Transform CarBuildScreen::CarTransform() const {
    return {.position = kSpot, .rotation = FromAxisAngle(kUp, camera_.Angle())};
}

void CarBuildScreen::AddPiece(float height, std::optional<float> alpha) {
    const PartPreview& model = models_.Model(*builder_.Cursor().Piece(), builder_.Colour(),
                                             builder_.Cursor().Placement().rotation);
    const bool isLit = drag_.Lit() == BuildRegion::Piece;
    for (render::MeshDraw draw : model.draws) {
        draw.transform = {.position = PieceCorner(height)};
        if (isLit) {
            draw.material.ambient = draw.material.ambient * kLitAmbient;
        }
        if (alpha) {
            draw.material.blend = render::BlendMode::Alpha;
            draw.material.tint.a *= *alpha;
            draw.material.alphaCutoff *= *alpha;
        }
        extras_.push_back(draw);
    }
}

render::Camera CarBuildScreen::ViewCamera() const {
    const render::Camera& scene = services_.stand->SceneCamera();
    assets::WorldCamera camera;
    camera.position = camera_.Eye(fit_.bounds.radius);
    camera.target = kSpot + kLookAbove;
    camera.fovDegrees = scene.verticalFovDegrees;
    camera.nearClip = scene.nearPlane;
    camera.farClip = scene.farPlane;
    return MenuSceneCamera(camera);
}

void CarBuildScreen::Render(std::vector<render::RenderView>& views,
                            std::vector<render::SpriteDraw>& sprites) {
    DrawPicturesAround(under_, holes_, sprites);
    RenderScene(views);
    DrawMenuFrameBorder(look_.garage.look, look_.garage.frame, sprites);
    carouselView_.Render(carousel_, models_.SetPreviews(builder_.Set()),
                         menu_.Focus() == BuildRow::Pieces, under_, views, ItemTurn(spin_));
    DrawControls(sprites);
}

void CarBuildScreen::RenderScene(std::vector<render::RenderView>& views) {
    extras_.clear();
    if (builder_.Cursor().Piece() != nullptr) {
        const HeldPieceLayers layers = motion_.Layers(camera_.IsOverhead());
        if (layers.trail) {
            AddPiece(layers.trail->height, layers.trail->alpha);
        }
        if (layers.ghost) {
            AddPiece(layers.ghost->height, layers.ghost->alpha);
        }
        if (layers.solid) {
            AddPiece(*layers.solid, std::nullopt);
        }
    }
    const bool isCarLit = drag_.Lit() == BuildRegion::Car;
    views.push_back(services_.stand->Render(
        CarTransform(), {.camera = ViewCamera(),
                         .interior = look_.garage.interior,
                         .light = kWhite,
                         .extras = extras_,
                         .carAmbient = isCarLit ? kLitAmbient : render::Color{}}));
}

void CarBuildScreen::DrawControls(std::vector<render::SpriteDraw>& sprites) const {
    DrawSelectorArrows(look_.arrows, look_.sets, menu_.Focus() == BuildRow::Sets, sprites);
    DrawSelectorArrows(look_.arrows, look_.pieces, menu_.Focus() == BuildRow::Pieces, sprites);
    DrawStrip(setStrip_, look_.setImages, setIcons_, look_.setBox, sprites);
    for (std::size_t index = 0; index < kBuildButtons; ++index) {
        const StateButton& shown = look_.buttons.at(index);
        sprites.push_back(
            {.texture = shown.images.at(ButtonState(menu_, index)), .bounds = shown.bounds});
    }
    pointer_.DrawPressed(look_, sprites);
    help_.Draw(sprites);
}

}
