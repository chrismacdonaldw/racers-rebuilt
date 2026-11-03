#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "app/menu/carousel_layout.hpp"
#include "app/menu/driver_kit.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/part_carousel.hpp"
#include "assets/menu_layout.hpp"
#include "assets/menu_styles.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Ambient and light pairs: side plain, side highlighted, centre plain, centre highlighted.
using CarouselColors = std::array<assets::MenuColor, assets::kSelectorItemStates>;

class CarouselView {
public:
    CarouselView(CarouselLayout layout, const CarouselColors& colors);

    [[nodiscard]] const render::Rect& Bounds() const { return layout_.bounds; }

    void Render(const PartCarousel& carousel, const DriverKit& kit, DriverSlot slot,
                bool isHighlighted, std::span<const MenuPicture> under,
                std::vector<render::RenderView>& out);

private:
    struct Lit {
        std::array<render::DirectionalLight, 1> light{};
        render::Color ambient;
    };

    static void Place(const PartPreview& preview, const Vector3& position,
                      std::vector<render::MeshDraw>& out);
    [[nodiscard]] render::RenderView ViewOf(const std::vector<render::MeshDraw>& meshes,
                                            const Lit& lit) const;

    CarouselLayout layout_;
    render::Camera camera_;
    std::array<Lit, 4> lits_;
    std::vector<render::MeshDraw> sides_;
    std::vector<render::MeshDraw> centre_;
    std::vector<render::QuadDraw> quads_;
};

}
