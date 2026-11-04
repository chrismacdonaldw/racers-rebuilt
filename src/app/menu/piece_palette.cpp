#include "app/menu/piece_palette.hpp"

#include <algorithm>

namespace rr::app {
namespace {

std::size_t Stepped(std::size_t value, int step, std::size_t count) {
    const auto moved = static_cast<long long>(value) + step;
    return static_cast<std::size_t>(std::clamp(moved, 0LL, static_cast<long long>(count) - 1));
}

}

void PiecePalette::Show(const assets::PieceCatalog& catalog, std::size_t set) {
    entries_.clear();
    if (set < catalog.partSets.size()) {
        for (std::size_t choice = 0; choice < catalog.partSets[set].choices.size(); ++choice) {
            entries_.push_back({.set = set, .choice = choice});
        }
    }
    firstRow_ = 0;
    cursor_ = 0;
}

void PiecePalette::Scroll(int rows) {
    const std::size_t lastFirst = RowCount() > grid_.rows ? RowCount() - grid_.rows : 0;
    firstRow_ = Stepped(firstRow_, rows, lastFirst + 1);
}

void PiecePalette::MoveCursor(int across, int down) {
    if (entries_.empty()) {
        return;
    }
    const std::size_t column = Stepped(cursor_ % grid_.columns, across, grid_.columns);
    const std::size_t row = Stepped(cursor_ / grid_.columns, down, RowCount());
    cursor_ = std::min((row * grid_.columns) + column, entries_.size() - 1);
    const std::size_t cursorRow = cursor_ / grid_.columns;
    if (cursorRow < firstRow_) {
        firstRow_ = cursorRow;
    } else if (cursorRow >= firstRow_ + grid_.rows) {
        firstRow_ = cursorRow + 1 - grid_.rows;
    }
}

std::optional<std::size_t> PiecePalette::EntryAt(float x, float y) const {
    for (std::size_t entry = 0; entry < entries_.size(); ++entry) {
        const std::optional<render::Rect> tile = TileOf(entry);
        if (tile && x >= tile->x && y >= tile->y && x < tile->x + tile->width &&
            y < tile->y + tile->height) {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<render::Rect> PiecePalette::TileOf(std::size_t entry) const {
    const std::size_t row = entry / grid_.columns;
    if (entry >= entries_.size() || row < firstRow_ || row >= firstRow_ + grid_.rows) {
        return std::nullopt;
    }
    const auto column = static_cast<float>(entry % grid_.columns);
    const auto shownRow = static_cast<float>(row - firstRow_);
    return render::Rect{.x = grid_.area.x + (column * (TileWidth() + grid_.gap)),
                        .y = grid_.area.y + (shownRow * (TileHeight() + grid_.gap)),
                        .width = TileWidth(),
                        .height = TileHeight()};
}

std::size_t PiecePalette::RowCount() const {
    return (entries_.size() + grid_.columns - 1) / grid_.columns;
}

float PiecePalette::TileWidth() const {
    const auto columns = static_cast<float>(grid_.columns);
    return (grid_.area.width - (grid_.gap * (columns - 1.0F))) / columns;
}

float PiecePalette::TileHeight() const {
    const auto rows = static_cast<float>(grid_.rows);
    return (grid_.area.height - (grid_.gap * (rows - 1.0F))) / rows;
}

}
