#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>

#include "app/menu/menu_backdrop.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/stand_scene.hpp"
#include "app/menu/tile_drawing.hpp"
#include "app/menu/workshop/workshop_panels.hpp"
#include "app/track_space.hpp"
#include "app/ui_text/text_layout.hpp"
#include "render/ui_layout.hpp"
#include "sim/car_build/built_car.hpp"

namespace rr::app {
namespace {

constexpr render::Color kDrawerColour{.r = 0.10F, .g = 0.11F, .b = 0.13F};
constexpr render::Color kTileColour{.r = 0.17F, .g = 0.18F, .b = 0.21F};
constexpr render::Color kPanelColour{.r = 0.08F, .g = 0.09F, .b = 0.11F, .a = 0.85F};
constexpr render::Color kTabColour{.r = 0.2F, .g = 0.21F, .b = 0.25F};
constexpr render::Color kHoverColour{.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 0.6F};
constexpr render::Color kChosenColour{.r = 1.0F, .g = 0.87F, .b = 0.0F};
constexpr render::Color kHintColour{.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 0.75F};
constexpr float kPanelPadding = 4.0F;
constexpr float kTextPadding = 6.0F;
constexpr std::array<std::u16string_view, kWorkshopTools> kToolLabels{u"UNDO", u"REDO", u"TURN",
                                                                      u"DELETE", u"FRAME"};
constexpr std::u16string_view kDoneLabel = u"DONE";
constexpr std::u16string_view kExtraLabel = u"LDRAW";
constexpr std::u16string_view kHint =
    u"RIGHT DRAG TURNS   MIDDLE DRAG PANS   WHEEL ZOOMS   F FRAMES THE CAR";

render::Rect Grown(const render::Rect& area, float by) {
    return {.x = area.x - by,
            .y = area.y - by,
            .width = area.width + (2.0F * by),
            .height = area.height + (2.0F * by)};
}

std::u16string Widened(std::string_view text) {
    return {text.begin(), text.end()};
}

void DrawCentred(const UiFont* font, std::u16string_view text, const render::Rect& area,
                 const render::Color& tint, std::vector<render::SpriteDraw>& out) {
    const float width = font != nullptr ? TextWidth(*font, text) : 0.0F;
    if (width <= 0.0F) {
        return;
    }
    const auto fullHeight = static_cast<float>(font->heightPx);
    const float scale = std::min({1.0F, (area.width - (2.0F * kTextPadding)) / width,
                                  area.height / std::max(fullHeight, 1.0F)});
    const float height = fullHeight * scale;
    LayoutText(*font, text,
               {.x = area.x + ((area.width - (width * scale)) / 2.0F),
                .y = area.y + ((area.height - height) / 2.0F),
                .scale = scale,
                .tint = tint},
               out);
}

render::Rect Fitted(const UiImage& image, const render::Rect& area) {
    const float width = std::max(static_cast<float>(image.widthPx), 1.0F);
    const float height = std::max(static_cast<float>(image.heightPx), 1.0F);
    const float scale = std::min(area.width / width, area.height / height);
    return {.x = area.x + ((area.width - (width * scale)) / 2.0F),
            .y = area.y + ((area.height - (height * scale)) / 2.0F),
            .width = width * scale,
            .height = height * scale};
}

}

void WorkshopPanels::Render(PieceModels& models, const PanelLook& look,
                            std::vector<render::RenderView>& views,
                            std::vector<render::SpriteDraw>& sprites) {
    DrawDrawer(models, views);
    DrawTabs(look, sprites);
    DrawButtons(look.font, sprites);
    DrawColours(models, sprites);
    DrawLabels(look, sprites);
    if (hovered_) {
        DrawOutline(*hovered_, kHoverColour, sprites);
    }
}

void WorkshopPanels::DrawDrawer(PieceModels& models, std::vector<render::RenderView>& views) {
    drawerBacks_.assign(1, {.bounds = layout_.drawer, .tint = kDrawerColour});
    for (std::size_t entry = 0; entry < drawer_.Entries().size(); ++entry) {
        if (const std::optional<render::Rect> tile = drawer_.TileOf(entry)) {
            drawerBacks_.push_back({.bounds = *tile, .tint = kTileColour});
        }
    }
    const render::Camera flat = PieceTileCamera({.radius = 1.0F});
    drawerQuads_.clear();
    AppendBackdropQuads(drawerBacks_, layout_.drawer, flat, drawerQuads_);
    views.push_back(
        {.camera = flat, .quads = drawerQuads_, .viewport = render::UiViewport(layout_.drawer)});

    std::size_t shown = 0;
    for (std::size_t entry = 0; entry < drawer_.Entries().size() && shown < tiles_.size();
         ++entry) {
        const std::optional<render::Rect> area = drawer_.TileOf(entry);
        if (!area) {
            continue;
        }
        const PaletteEntry& at = drawer_.Entries()[entry];
        const assets::PartChoice& choice = catalog_->Choices(at.set)[at.choice];
        const PartPreview& model =
            models.Model(catalog_->library.pieces.at(choice.piece), ColourOf(at), 0);
        Tile& tile = tiles_.at(shown++);
        tile.meshes.clear();
        for (render::MeshDraw draw : model.draws) {
            draw.transform = TransformToEngine({});
            tile.meshes.push_back(draw);
        }
        views.push_back({.camera = PieceTileCamera(model.bounds),
                         .lighting = {.lights = light_, .ambient = kMenuSceneAmbient},
                         .meshes = tile.meshes,
                         .viewport = render::UiViewport(*area)});
    }
}

void WorkshopPanels::DrawTabs(const PanelLook& look,
                              std::vector<render::SpriteDraw>& sprites) const {
    for (std::size_t tab = 0; tab < std::min(sets_.size(), kTabColumns * kTabRows); ++tab) {
        const render::Rect area = TabOf(layout_, tab);
        sprites.push_back({.bounds = area, .tint = kTabColour});
        if (sets_[tab] < setIcons_.size()) {
            const UiImage& icon = setIcons_[sets_[tab]];
            sprites.push_back({.texture = icon.texture, .bounds = Fitted(icon, area)});
        } else {
            DrawCentred(look.font, kExtraLabel, area, {}, sprites);
        }
        if (tab == tab_) {
            DrawOutline(area, kChosenColour, sprites);
        }
    }
}

void WorkshopPanels::DrawButtons(const UiFont* font,
                                 std::vector<render::SpriteDraw>& sprites) const {
    sprites.push_back({.bounds = layout_.done, .tint = kPanelColour});
    DrawCentred(font, kDoneLabel, layout_.done, {}, sprites);
    for (std::size_t tool = 0; tool < kWorkshopTools; ++tool) {
        sprites.push_back({.bounds = layout_.tools.at(tool), .tint = kPanelColour});
        DrawCentred(font, kToolLabels.at(tool), layout_.tools.at(tool), {}, sprites);
    }
}

void WorkshopPanels::DrawColours(const PieceModels& models,
                                 std::vector<render::SpriteDraw>& sprites) const {
    sprites.push_back({.bounds = Grown(layout_.colours, kPanelPadding), .tint = kPanelColour});
    for (std::size_t colour = 0; colour < swatches_.Count(); ++colour) {
        const render::Rect swatch = swatches_.SwatchOf(colour);
        const std::optional<render::Material> material =
            models.ColourMaterial(static_cast<std::uint8_t>(colour));
        sprites.push_back({.texture = material ? material->texture : render::kWhiteTexture,
                           .bounds = swatch,
                           .tint = material ? material->tint : render::Color{}});
        if (swatches_.Chosen() == colour) {
            DrawOutline(swatch, kChosenColour, sprites);
        }
    }
}

void WorkshopPanels::DrawLabels(const PanelLook& look, std::vector<render::SpriteDraw>& sprites) {
    if (look.pieces != counted_.pieces || look.plates != counted_.plates || counts_.empty()) {
        counted_ = {.pieces = look.pieces, .plates = look.plates};
        counts_ = Widened(std::format("{} OF {} PIECES   {} OF {} HIGH", look.pieces,
                                      sim::kMaxCarPieces, look.plates, sim::kMaxCarHeight));
    }
    DrawCentred(look.font, counts_, layout_.counts, {}, sprites);
    if (look.showsHint) {
        DrawCentred(look.font, kHint, layout_.hint, kHintColour, sprites);
    }
}

}
