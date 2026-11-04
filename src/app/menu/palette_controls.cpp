#include "app/menu/palette_controls.hpp"

#include "app/menu/menu_backdrop.hpp"
#include "app/menu/stand_scene.hpp"
#include "app/menu/tile_drawing.hpp"
#include "app/track_space.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr PaletteGrid kGrid{.area = {.x = 24.0F, .y = 44.0F, .width = 172.0F, .height = 352.0F},
                            .columns = 3,
                            .rows = 6,
                            .gap = 8.0F};
constexpr render::Rect kSwatchArea{.x = 211.0F, .y = 68.0F, .width = 412.0F, .height = 44.0F};
constexpr render::Color kCursorOutline{.r = 1.0F, .g = 0.87F, .b = 0.0F};
constexpr render::Color kHoverOutline{.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 0.6F};

}

PaletteControls::PaletteControls(const assets::PieceCatalog& catalog)
    : catalog_(&catalog),
      palette_(kGrid),
      swatches_(catalog.colours.size(), kSwatchArea),
      light_{MenuSceneLight()},
      tiles_(kGrid.columns * kGrid.rows) {}

void PaletteControls::ShowSet(std::size_t set) {
    palette_.Show(*catalog_, set);
}

PaletteTick PaletteControls::Tick(const sim::InputFrame& input, bool usesKeys, bool cancels) {
    const sim::MenuPointer& pointer = input.pointer;
    if (pointer.moved) {
        hovered_ = pointer.isOver ? palette_.EntryAt(pointer.x, pointer.y) : std::nullopt;
        showsCursor_ = false;
    }
    PaletteTick tick{.drag = drag_.Tick(pointer, hovered_, cancels)};
    if (tick.drag.step == DragStep::Picked && hovered_) {
        tick.picked = palette_.Entries().at(*hovered_);
        tick.isDragged = true;
        return tick;
    }
    if (IsDragging()) {
        return tick;
    }
    if (pointer.pressed && !pointer.rightPressed) {
        tick.swatch = swatches_.ColourAt(pointer.x, pointer.y);
    }
    if (!hovered_ && pointer.wheel != 0.0F) {
        palette_.Scroll(pointer.wheel > 0.0F ? -1 : 1);
    }
    if (!usesKeys) {
        return tick;
    }
    const auto pressed = [&](sim::MenuAction action) { return input.WasPressed(action); };
    const int across =
        (pressed(sim::MenuAction::Right) ? 1 : 0) - (pressed(sim::MenuAction::Left) ? 1 : 0);
    const int down =
        (pressed(sim::MenuAction::Down) ? 1 : 0) - (pressed(sim::MenuAction::Up) ? 1 : 0);
    if (across != 0 || down != 0) {
        palette_.MoveCursor(across, down);
        showsCursor_ = true;
    }
    if (pressed(sim::MenuAction::Confirm) && palette_.Cursor() < palette_.Entries().size()) {
        tick.picked = palette_.Entries()[palette_.Cursor()];
    }
    return tick;
}

std::uint8_t PaletteControls::ColourOf(const PaletteEntry& entry) const {
    return swatches_.Chosen().value_or(catalog_->Choices(entry.set)[entry.choice].colour);
}

void PaletteControls::Render(PieceModels& models, std::span<const MenuPicture> under,
                             std::vector<render::RenderView>& views,
                             std::vector<render::SpriteDraw>& sprites) {
    const render::Camera camera = PieceTileCamera({.radius = 1.0F});
    backdrop_.quads.clear();
    AppendBackdropQuads(under, palette_.Area(), camera, backdrop_.quads);
    views.push_back({.camera = camera,
                     .quads = backdrop_.quads,
                     .viewport = render::UiViewport(palette_.Area())});
    std::size_t shown = 0;
    for (std::size_t entry = 0; entry < palette_.Entries().size(); ++entry) {
        const std::optional<render::Rect> area = palette_.TileOf(entry);
        if (!area || shown >= tiles_.size()) {
            continue;
        }
        views.push_back(
            DrawTile(models, under, palette_.Entries()[entry], *area, tiles_.at(shown++)));
        if (showsCursor_ && entry == palette_.Cursor()) {
            DrawOutline(*area, kCursorOutline, sprites);
        } else if (entry == hovered_ || entry == drag_.Entry()) {
            DrawOutline(*area, kHoverOutline, sprites);
        }
    }
    DrawSwatches(models, sprites);
}

render::RenderView PaletteControls::DrawTile(PieceModels& models,
                                             std::span<const MenuPicture> under,
                                             const PaletteEntry& entry, const render::Rect& area,
                                             Tile& tile) const {
    const assets::PartChoice& choice = catalog_->Choices(entry.set)[entry.choice];
    const PartPreview& model =
        models.Model(catalog_->library.pieces.at(choice.piece), ColourOf(entry), 0);
    const render::Camera camera = PieceTileCamera(model.bounds);
    tile.meshes.clear();
    for (render::MeshDraw draw : model.draws) {
        draw.transform = TransformToEngine({});
        tile.meshes.push_back(draw);
    }
    tile.quads.clear();
    AppendBackdropQuads(under, area, camera, tile.quads);
    return {.camera = camera,
            .lighting = {.lights = light_, .ambient = kMenuSceneAmbient},
            .meshes = tile.meshes,
            .quads = tile.quads,
            .viewport = render::UiViewport(area)};
}

void PaletteControls::DrawSwatches(const PieceModels& models,
                                   std::vector<render::SpriteDraw>& sprites) const {
    for (std::size_t colour = 0; colour < swatches_.Count(); ++colour) {
        const render::Rect swatch = swatches_.SwatchOf(colour);
        const std::optional<render::Material> material =
            models.ColourMaterial(static_cast<std::uint8_t>(colour));
        sprites.push_back({.texture = material ? material->texture : render::kWhiteTexture,
                           .bounds = swatch,
                           .tint = material ? material->tint : render::Color{}});
        if (swatches_.Chosen() == colour) {
            DrawOutline(swatch, kCursorOutline, sprites);
        }
    }
}

}
