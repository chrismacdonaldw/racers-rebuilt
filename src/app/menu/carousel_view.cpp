#include "app/menu/carousel_view.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

#include "app/menu/menu_backdrop.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/stand_scene.hpp"
#include "app/track_space.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kCentreLits = 2;

}

CarouselView::CarouselView(CarouselLayout layout, const CarouselColors& colors)
    : layout_(std::move(layout)), camera_(CarouselCamera(layout_)) {
    for (std::size_t lit = 0; lit < lits_.size(); ++lit) {
        render::DirectionalLight light = MenuSceneLight();
        light.color = ToColor(colors.at((2 * lit) + 1));
        lits_.at(lit) = {.light = {light}, .ambient = ToColor(colors.at(2 * lit))};
    }
}

void CarouselView::Render(const PartCarousel& carousel, std::span<const PartPreview> previews,
                          bool isHighlighted, std::span<const MenuPicture> under,
                          std::vector<render::RenderView>& out, const Quaternion& itemTurn) {
    sides_.clear();
    centre_.clear();
    quads_.clear();
    const std::size_t slots = std::min(carousel.Shape().slots, layout_.slots.size());
    for (std::size_t place = 0; place < slots; ++place) {
        const std::optional<std::uint8_t> part = carousel.PartInSlot(place);
        if (!part || *part >= previews.size()) {
            continue;
        }
        const PartPreview& preview = previews[*part];
        Vector3 position = SlotPosition(layout_, place, preview.bounds);
        if (const std::optional<std::size_t> from = carousel.CameFrom(place);
            from && *from < layout_.slots.size()) {
            const Vector3 start = SlotPosition(layout_, *from, preview.bounds);
            position = position + ((start - position) * carousel.Lag());
        }
        const bool isCentre = place == carousel.Shape().focused && !carousel.IsScrolling();
        Place(preview, position, itemTurn, isCentre ? centre_ : sides_);
    }
    AppendBackdropQuads(under, layout_.bounds, camera_, quads_);
    const std::size_t lit = isHighlighted ? 1 : 0;
    render::RenderView sides = ViewOf(sides_, lits_.at(lit));
    sides.quads = quads_;
    out.push_back(sides);
    out.push_back(ViewOf(centre_, lits_.at(kCentreLits + lit)));
}

void CarouselView::Place(const PartPreview& preview, const Vector3& position,
                         const Quaternion& turn, std::vector<render::MeshDraw>& out) {
    const Vector3 pivot = preview.bounds.centre - Rotate(turn, preview.bounds.centre);
    const Transform placed = TransformToEngine({.position = position + pivot, .rotation = turn});
    for (render::MeshDraw draw : preview.draws) {
        draw.transform = placed;
        out.push_back(draw);
    }
}

render::RenderView CarouselView::ViewOf(const std::vector<render::MeshDraw>& meshes,
                                        const Lit& lit) const {
    return {.camera = camera_,
            .lighting = {.lights = lit.light, .ambient = lit.ambient},
            .meshes = meshes,
            .viewport = render::UiViewport(layout_.bounds)};
}

}
