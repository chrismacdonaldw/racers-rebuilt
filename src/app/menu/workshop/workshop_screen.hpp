#pragma once

#include <SDL3/SDL_keycode.h>

#include <array>
#include <memory>
#include <vector>

#include "app/menu/builder_screen.hpp"
#include "app/menu/piece_models.hpp"
#include "app/menu/workshop/orbit_camera.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/car_build/bench.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// The full-window builder, with a free camera over a plain background. What `services` points
/// to must outlive it.
class WorkshopScreen final : public BuilderScreen {
public:
    [[nodiscard]] static Result<std::unique_ptr<WorkshopScreen>> Load(render::Renderer& renderer,
                                                                      const BuildServices& services,
                                                                      sim::BuiltCar car);

    void HandleKey(SDL_Keycode key, SDL_Keymod mod) override;
    [[nodiscard]] bool UseControls(BuildMode mode) override;
    [[nodiscard]] BuildOutcome Tick(const sim::InputFrame& input) override;
    [[nodiscard]] bool HoldsPointer() const override { return isDragging_; }
    [[nodiscard]] const sim::BuiltCar& Car() const override { return bench_.car; }

    void Render(std::vector<render::RenderView>& views,
                std::vector<render::SpriteDraw>& sprites) override;

private:
    WorkshopScreen(render::Renderer& renderer, const BuildServices& services, PieceModels models,
                   render::TextureId groundCell, sim::BuiltCar car);

    void Steer(const sim::MenuPointer& pointer);
    void FrameCar();
    [[nodiscard]] Vector3 GroundCorner() const;
    [[nodiscard]] render::Camera ViewCamera() const;
    void AddLooseBricks();

    render::Renderer* renderer_;
    BuildServices services_;
    PieceModels models_;
    render::TextureId groundCell_;
    sim::Bench bench_;
    Vector3 carOffset_;
    OrbitCamera camera_;
    bool isDragging_ = false;
    bool isDone_ = false;
    std::vector<render::MeshDraw> draws_;
    std::array<render::QuadDraw, 2> quads_{};
    std::array<render::DirectionalLight, 2> lights_{};
};

}
