#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/far_models.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/stand_scene.hpp"
#include "app/track_entity_draws.hpp"
#include "app/track_model.hpp"
#include "assets/car_data.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::app {

/// A view other than the stand's own, with draws added to the car's in game space.
struct StandShot {
    render::Camera camera;
    render::Rect interior;
    render::Color light;
    std::span<const render::MeshDraw> extras;
    render::Color carAmbient;
};

class CarStand {
public:
    [[nodiscard]] static Result<std::unique_ptr<CarStand>> Load(render::Renderer& renderer,
                                                                const assets::Vfs& vfs,
                                                                const MenuFiles& files,
                                                                std::string_view screen,
                                                                const settings::Racer& racer);

    CarStand(const CarStand&) = delete;
    CarStand& operator=(const CarStand&) = delete;
    CarStand(CarStand&&) = delete;
    CarStand& operator=(CarStand&&) = delete;
    ~CarStand() = default;

    /// Rebuilds the car. One that fails to build leaves the stand empty, with a warning.
    void ShowCar(const settings::Racer& racer);

    void Tick(Milliseconds elapsed);

    void DrawFarScenery(bool isDrawn) { farModels_.SetHidden(false, !isDrawn); }

    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const { return entities_.Clocks(); }
    [[nodiscard]] const StandFraming& Framing() const { return framing_; }

    /// `car` is in game space, turned about the spot.
    [[nodiscard]] render::RenderView Render(std::span<const sim::EntityClock> clocks,
                                            const Transform& car);

    /// The shown car placed at `car` in game space, as engine space draws.
    void AddCarDraws(const Transform& car, const render::Color& ambient,
                     std::vector<render::MeshDraw>& draws) const;

    /// Leaves the driver out, as the car builder does.
    [[nodiscard]] render::RenderView Render(const Transform& car, const StandShot& shot);

    [[nodiscard]] const render::Camera& SceneCamera() const { return scene_->camera; }
    [[nodiscard]] assets::TrackLoader& Loader() const { return *loading_.loader; }

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Loading {
        std::unique_ptr<assets::LoaderPool> pool;
        std::unique_ptr<assets::TrackLoader> loader;
    };

    struct CarModel {
        std::string chassis;
        assets::CarData car{};
        JointedModel model;
        Vector3 seat;
    };

    CarStand(render::Renderer& renderer, const assets::Vfs& vfs, Loading loading,
             std::unique_ptr<StandScene> scene, TrackModel model, const StandFraming& framing);

    [[nodiscard]] Result<void> ShowDriver(const settings::Racer& racer);
    void DrawCar(const Transform& car, const render::Color& ambient = {});

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    Loading loading_;
    std::unique_ptr<StandScene> scene_;
    TrackModel model_;
    FarModels farModels_;
    StandFraming framing_;
    sim::TrackEntities entities_;
    TrackEntityDraws jointed_;
    std::vector<CarModel> cars_;
    std::optional<std::size_t> shown_;
    bool isDriverShown_ = false;
    std::vector<render::MeshDraw> draws_;
    std::array<render::DirectionalLight, 1> light_{};
};

}
