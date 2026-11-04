#pragma once

#include <SDL3/SDL_keycode.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "app/menu/build_camera.hpp"
#include "app/menu/build_drag.hpp"
#include "app/menu/build_help_panel.hpp"
#include "app/menu/build_pointer.hpp"
#include "app/menu/builder_screen.hpp"
#include "app/menu/car_build_look.hpp"
#include "app/menu/car_build_menu.hpp"
#include "app/menu/car_builder.hpp"
#include "app/menu/car_stand.hpp"
#include "app/menu/carousel_view.hpp"
#include "app/menu/held_piece_motion.hpp"
#include "app/menu/icon_strip.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/part_carousel.hpp"
#include "app/menu/piece_models.hpp"
#include "assets/piece_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// Draws over the edit car stand. What `services` points to must outlive it.
class CarBuildScreen final : public BuilderScreen {
public:
    [[nodiscard]] static Result<std::unique_ptr<CarBuildScreen>> Load(
        render::Renderer& renderer, const assets::Vfs& vfs, BuildServices services,
        sim::BuiltCar car, std::vector<std::size_t> sets);

    void HandleKey(SDL_Keycode key, SDL_Keymod mod) override;
    [[nodiscard]] bool UseControls(BuildMode mode) override;
    [[nodiscard]] BuildOutcome Tick(const sim::InputFrame& input) override;
    [[nodiscard]] bool HoldsPointer() const override { return menu_.Held().has_value(); }

    [[nodiscard]] const sim::BuiltCar& Car() const override { return builder_.Car(); }

    void Render(std::vector<render::RenderView>& views,
                std::vector<render::SpriteDraw>& sprites) override;

private:
    struct Fit {
        Vector3 offset;
        BoundingSphere bounds;
    };

    CarBuildScreen(BuildServices services, CarBuildLook look, PieceModels models,
                   CarBuilder builder, std::vector<std::size_t> sets);

    [[nodiscard]] BuildOutcome ApplyInput(const sim::InputFrame& input);
    [[nodiscard]] BuildOutcome Apply(const BuildRequest& request);
    void ApplyPad(const BuildRequest& request);
    void StepSet(int direction);
    void StepPiece(int direction);
    void ShowSet();
    void Move(std::size_t direction);
    void View(bool moved) const;
    [[nodiscard]] BuildOutcome Place();
    [[nodiscard]] BuildOutcome Land();
    bool Undo();
    [[nodiscard]] bool Refit();
    [[nodiscard]] float RestHeight() const;
    [[nodiscard]] BuildOutcome Animate();
    void ChangePad(std::optional<BuildPad> before);
    [[nodiscard]] render::Rect RowBounds(std::size_t row) const;
    [[nodiscard]] BuildRegions Regions();
    [[nodiscard]] Vector3 PieceCorner(float height) const;
    [[nodiscard]] Transform CarTransform() const;
    void AddPiece(float height, std::optional<float> alpha);
    [[nodiscard]] render::Camera ViewCamera() const;
    void DrawControls(std::vector<render::SpriteDraw>& sprites) const;
    void RenderScene(std::vector<render::RenderView>& views);

    BuildServices services_;
    CarBuildLook look_;
    PieceModels models_;
    CarBuilder builder_;
    std::vector<std::size_t> sets_;
    std::vector<std::size_t> setIcons_;
    std::size_t set_ = 0;
    PartCarousel carousel_;
    CarouselView carouselView_;
    float spin_ = 0.0F;
    IconStrip setStrip_;
    CarBuildMenu menu_;
    BuildPointer pointer_;
    BuildDrag drag_;
    BuildCamera camera_;
    HeldPieceMotion motion_;
    BuildHelpPanel help_;
    Fit fit_;
    std::vector<BuildRequest> keys_;
    std::vector<MenuPicture> under_;
    std::array<render::Rect, 2> holes_{};
    std::vector<render::MeshDraw> extras_;
};

}
