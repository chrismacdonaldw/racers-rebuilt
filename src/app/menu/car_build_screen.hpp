#pragma once

#include <SDL3/SDL_keycode.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "app/menu/car_build_look.hpp"
#include "app/menu/car_build_menu.hpp"
#include "app/menu/car_builder.hpp"
#include "app/menu/car_stand.hpp"
#include "app/menu/carousel_view.hpp"
#include "app/menu/menu_parts.hpp"
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

enum class BuildOutcome : std::uint8_t { None, CarChanged, TooManyPieces, Done };

/// Draws over the edit car stand. `stand` and `catalog` must outlive it.
class CarBuildScreen {
public:
    [[nodiscard]] static Result<std::unique_ptr<CarBuildScreen>> Load(
        render::Renderer& renderer, const assets::Vfs& vfs, CarStand& stand,
        const assets::PieceCatalog& catalog, sim::BuiltCar car, std::vector<std::size_t> sets);

    void HandleKey(SDL_Keycode key);
    [[nodiscard]] BuildOutcome Tick(const sim::InputFrame& input);

    [[nodiscard]] const sim::BuiltCar& Car() const { return builder_.Car(); }

    void Render(std::vector<render::RenderView>& views, std::vector<render::SpriteDraw>& sprites);

private:
    struct Fit {
        Vector3 offset;
        float radius = 0.0F;
    };

    CarBuildScreen(CarStand& stand, const assets::PieceCatalog& catalog, CarBuildLook look,
                   PieceModels models, CarBuilder builder, std::vector<std::size_t> sets);

    [[nodiscard]] BuildOutcome Apply(const BuildRequest& request);
    void StepSet(int direction);
    void ShowSet();
    [[nodiscard]] BuildOutcome Place();
    bool Undo();
    [[nodiscard]] bool Refit();
    [[nodiscard]] int RestHeight() const;
    void AddPiece(float height, std::optional<float> alpha);
    [[nodiscard]] render::Camera BuildCamera() const;
    void DrawControls(std::vector<render::SpriteDraw>& sprites) const;

    CarStand* stand_;
    const assets::PieceCatalog* catalog_;
    CarBuildLook look_;
    PieceModels models_;
    CarBuilder builder_;
    std::vector<std::size_t> sets_;
    std::size_t set_ = 0;
    PartCarousel carousel_;
    CarouselView carouselView_;
    CarBuildMenu menu_;
    Fit fit_;
    std::vector<BuildRequest> keys_;
    std::vector<MenuPicture> under_;
    std::array<render::Rect, 2> holes_{};
    std::vector<render::MeshDraw> extras_;
};

}
