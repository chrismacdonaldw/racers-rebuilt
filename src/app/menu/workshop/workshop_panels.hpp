#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/colour_swatches.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/piece_models.hpp"
#include "app/menu/piece_palette.hpp"
#include "app/menu/workshop/background_menu.hpp"
#include "app/menu/workshop/track_backdrop.hpp"
#include "app/menu/workshop/workshop_layout.hpp"
#include "app/ui_text/ui_font.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/piece_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class PanelAction : std::uint8_t { Done, Undo, Redo, Turn, Delete, Frame, Background };

struct PanelTick {
    std::optional<PanelAction> action;
    std::optional<PaletteEntry> grabbed;
    std::optional<std::uint8_t> swatch;
    std::optional<BackgroundChoice> background;
    bool isOver = false;
};

struct PanelLook {
    const UiFont* font = nullptr;
    std::size_t pieces = 0;
    int plates = 0;
    bool showsHint = true;
    std::string_view background;
    render::Color backdrop;
};

/// The builder's part set images, indexed by catalog part set.
[[nodiscard]] Result<std::vector<UiImage>> LoadSetIcons(render::Renderer& renderer,
                                                        const assets::Vfs& vfs);

/// The drawer of set tabs and piece tiles, the toolbar, DONE and the colour bar. `catalog` must
/// outlive it.
class WorkshopPanels {
public:
    /// `setIcons` are indexed by catalog part set.
    WorkshopPanels(const assets::PieceCatalog& catalog, std::vector<std::size_t> sets,
                   std::vector<UiImage> setIcons, std::vector<BackdropTrack> tracks);

    void Fit(const render::Rect& window);

    void OfferExtras(bool offers);

    PanelTick Tick(const sim::MenuPointer& pointer);

    [[nodiscard]] bool Covers(float x, float y) const;
    [[nodiscard]] const WorkshopLayout& Layout() const { return layout_; }
    [[nodiscard]] std::span<const std::size_t> Sets() const { return sets_; }
    [[nodiscard]] std::size_t Tab() const { return tab_; }
    [[nodiscard]] const PiecePalette& Drawer() const { return drawer_; }
    [[nodiscard]] const BackgroundMenu& Backgrounds() const { return backgrounds_; }

    /// Next pieces from the drawer come in `colour`, or in their own colour when unset.
    void ChooseColour(std::optional<std::uint8_t> colour);
    [[nodiscard]] std::uint8_t ColourOf(const PaletteEntry& entry) const;

    void Render(PieceModels& models, const PanelLook& look, std::vector<render::RenderView>& views,
                std::vector<render::SpriteDraw>& sprites);

private:
    struct Tile {
        std::vector<render::MeshDraw> meshes;
    };

    struct Counts {
        std::size_t pieces = 0;
        int plates = 0;
    };

    void LayOut();
    void ShowTab(std::size_t tab);
    [[nodiscard]] render::Rect AreaOf(PanelAction action) const;
    [[nodiscard]] std::optional<PanelAction> ActionAt(float x, float y) const;
    [[nodiscard]] std::optional<std::size_t> TabAt(float x, float y) const;
    [[nodiscard]] PanelTick TickBackgrounds(const sim::MenuPointer& pointer);

    void DrawDrawer(PieceModels& models, std::vector<render::RenderView>& views);
    void DrawTabs(const PanelLook& look, std::vector<render::SpriteDraw>& sprites) const;
    void DrawButtons(const UiFont* font, std::vector<render::SpriteDraw>& sprites) const;
    void DrawColours(const PieceModels& models, std::vector<render::SpriteDraw>& sprites) const;
    void DrawLabels(const PanelLook& look, std::vector<render::SpriteDraw>& sprites);

    const assets::PieceCatalog* catalog_;
    std::vector<std::size_t> sets_;
    std::vector<UiImage> setIcons_;
    std::size_t tab_ = 0;
    render::Rect window_{};
    WorkshopLayout layout_;
    PiecePalette drawer_;
    ColourSwatches swatches_;
    BackgroundMenu backgrounds_;
    std::optional<render::Rect> hovered_;
    std::vector<MenuPicture> drawerBacks_;
    std::vector<render::QuadDraw> drawerQuads_;
    Counts counted_;
    std::u16string counts_;
    std::vector<Tile> tiles_;
    std::array<render::DirectionalLight, 1> light_{};
};

}
