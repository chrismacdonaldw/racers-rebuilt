#include "app/sprite_test_scene.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

#include "assets/image_asset.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr sim::EntityId kPhaseId{1};
constexpr float kRadiansPerTick = 0.05F;
constexpr std::size_t kColumns = 40;
constexpr std::size_t kRows = 25;
constexpr float kCellWidth = render::kUiWidth / kColumns;
constexpr float kCellHeight = render::kUiHeight / kRows;
constexpr float kTileSize = 12.0F;
constexpr float kBobHeight = 2.0F;
constexpr float kBobRadiansPerTile = 0.15F;
constexpr std::uint32_t kTileSizePx = 16;
constexpr std::uint8_t kEdgeShade = 96;
constexpr render::Color kBackdrop{.r = 0.12F, .g = 0.16F, .b = 0.24F};

assets::ImageAsset BuildTile() {
    assets::ImageAsset image{.widthPx = kTileSizePx, .heightPx = kTileSizePx};
    for (std::uint32_t y = 0; y < kTileSizePx; ++y) {
        for (std::uint32_t x = 0; x < kTileSizePx; ++x) {
            const bool isEdge = x == 0 || y == 0 || x == kTileSizePx - 1 || y == kTileSizePx - 1;
            const std::byte shade{isEdge ? kEdgeShade : std::uint8_t{255}};
            const std::array pixel{shade, shade, shade, std::byte{255}};
            image.rgba.insert(image.rgba.end(), pixel.begin(), pixel.end());
        }
    }
    return image;
}

render::Color TileColor(std::size_t column, std::size_t row) {
    return {.r = static_cast<float>(column) / kColumns,
            .g = static_cast<float>(row) / kRows,
            .b = 1.0F - (static_cast<float>(column + row) / (kColumns + kRows)),
            .a = 0.85F};
}

}

Result<std::unique_ptr<SpriteTestScene>> SpriteTestScene::Create(render::Renderer& renderer) {
    return renderer.UploadTexture(BuildTile()).transform([&](render::TextureId tile) {
        return std::unique_ptr<SpriteTestScene>(new SpriteTestScene(renderer, tile));
    });
}

void SpriteTestScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    ++tick_;
}

void SpriteTestScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.entities.push_back(
        {.id = kPhaseId,
         .transform = {.position = {.x = static_cast<float>(tick_) * kRadiansPerTick}}});
    out.camera = {};
}

void SpriteTestScene::Render(const sim::RenderSnapshot& interpolated) {
    const float phase =
        interpolated.entities.empty() ? 0.0F : interpolated.entities.front().transform.position.x;
    sprites_.clear();
    sprites_.push_back({.bounds = {.width = render::kUiWidth, .height = render::kUiHeight},
                        .tint = kBackdrop,
                        .blend = render::BlendMode::Opaque});
    for (std::size_t row = 0; row < kRows; ++row) {
        for (std::size_t column = 0; column < kColumns; ++column) {
            const auto index = static_cast<float>((row * kColumns) + column);
            const float bob = kBobHeight * std::sin(phase + (index * kBobRadiansPerTile));
            sprites_.push_back({.texture = tile_,
                                .bounds = {.x = (static_cast<float>(column) * kCellWidth) +
                                                ((kCellWidth - kTileSize) / 2.0F),
                                           .y = (static_cast<float>(row) * kCellHeight) +
                                                ((kCellHeight - kTileSize) / 2.0F) + bob,
                                           .width = kTileSize,
                                           .height = kTileSize},
                                .tint = TileColor(column, row)});
        }
    }
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

SpriteTestScene::SpriteTestScene(render::Renderer& renderer, render::TextureId tile)
    : renderer_(&renderer), tile_(tile) {
    sprites_.reserve((kColumns * kRows) + 1);
}

}
