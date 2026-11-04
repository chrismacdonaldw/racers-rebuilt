#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "assets/piece_catalog.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct PaletteEntry {
    std::size_t set = 0;
    std::size_t choice = 0;

    friend bool operator==(const PaletteEntry&, const PaletteEntry&) = default;
};

struct PaletteGrid {
    render::Rect area;
    std::size_t columns = 1;
    std::size_t rows = 1;
    float gap = 0.0F;
};

class PiecePalette {
public:
    explicit PiecePalette(PaletteGrid grid) : grid_(grid) {}

    void Show(const assets::PieceCatalog& catalog, std::size_t set);

    [[nodiscard]] const std::vector<PaletteEntry>& Entries() const { return entries_; }
    [[nodiscard]] const render::Rect& Area() const { return grid_.area; }

    void Scroll(int rows);
    void MoveCursor(int across, int down);

    [[nodiscard]] std::size_t Cursor() const { return cursor_; }
    [[nodiscard]] std::size_t FirstRow() const { return firstRow_; }

    /// Indexes `Entries`.
    [[nodiscard]] std::optional<std::size_t> EntryAt(float x, float y) const;

    /// Unset for an entry scrolled out of view.
    [[nodiscard]] std::optional<render::Rect> TileOf(std::size_t entry) const;

private:
    [[nodiscard]] std::size_t RowCount() const;
    [[nodiscard]] float TileWidth() const;
    [[nodiscard]] float TileHeight() const;

    PaletteGrid grid_;
    std::vector<PaletteEntry> entries_;
    std::size_t firstRow_ = 0;
    std::size_t cursor_ = 0;
};

}
