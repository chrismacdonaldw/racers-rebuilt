#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/menu/colour_swatches.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/palette_drag.hpp"
#include "app/menu/piece_models.hpp"
#include "app/menu/piece_palette.hpp"
#include "assets/piece_catalog.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

struct PaletteTick {
    std::optional<PaletteEntry> picked;
    bool isDragged = false;
    DragTick drag;
    std::optional<std::uint8_t> swatch;
};

/// `catalog` must outlive it.
class PaletteControls {
public:
    explicit PaletteControls(const assets::PieceCatalog& catalog);

    void ShowSet(std::size_t set);

    /// `usesKeys` lets the menu directions move the cursor and confirm pick its piece.
    PaletteTick Tick(const sim::InputFrame& input, bool usesKeys, bool cancels);

    [[nodiscard]] bool IsDragging() const { return drag_.Entry().has_value(); }

    void ChooseColour(std::uint8_t colour) { swatches_.Choose(colour); }

    /// The chosen swatch, else the colour `entry` comes in.
    [[nodiscard]] std::uint8_t ColourOf(const PaletteEntry& entry) const;

    [[nodiscard]] const render::Rect& Area() const { return palette_.Area(); }

    void Render(PieceModels& models, std::span<const MenuPicture> under,
                std::vector<render::RenderView>& views, std::vector<render::SpriteDraw>& sprites);

private:
    struct Tile {
        std::vector<render::MeshDraw> meshes;
        std::vector<render::QuadDraw> quads;
    };

    [[nodiscard]] render::RenderView DrawTile(PieceModels& models,
                                              std::span<const MenuPicture> under,
                                              const PaletteEntry& entry, const render::Rect& area,
                                              Tile& tile) const;
    void DrawSwatches(const PieceModels& models, std::vector<render::SpriteDraw>& sprites) const;

    const assets::PieceCatalog* catalog_;
    PiecePalette palette_;
    ColourSwatches swatches_;
    PaletteDrag drag_;
    std::optional<std::size_t> hovered_;
    bool showsCursor_ = false;
    std::array<render::DirectionalLight, 1> light_{};
    Tile backdrop_;
    std::vector<Tile> tiles_;
};

}
