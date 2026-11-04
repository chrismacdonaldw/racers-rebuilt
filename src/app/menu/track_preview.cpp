#include "app/menu/track_preview.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include "app/cutscene/cutscene_screen.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_widgets.hpp"
#include "assets/png_image.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPreviewElement = "singrace";
constexpr std::string_view kBorderElement = "brickbox";
constexpr float kDimmerAlpha = 200.0F / 255.0F;
constexpr std::size_t kLoaderWorkers = 2;

render::SpriteDraw Shade(const render::Rect& bounds, float alpha) {
    return {.bounds = bounds, .tint = {.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = alpha}};
}

}

Result<std::unique_ptr<TrackPreview>> TrackPreview::Load(const RaceOutputs& outputs,
                                                         const assets::Vfs& vfs,
                                                         std::string_view language,
                                                         std::string_view layout) {
    auto pool = std::make_unique<assets::LoaderPool>(kLoaderWorkers);
    auto loader = std::make_unique<assets::TrackLoader>(vfs, *pool);
    Loading loading{.pool = std::move(pool), .loader = std::move(loader)};
    Result<CutsceneScreenPlacement> placed =
        ReadCutsceneScreen(vfs, {.menu = kPreviewElement, .layout = layout});
    Result<std::unique_ptr<CutsceneView>> view =
        placed.and_then([&](const CutsceneScreenPlacement& placement) {
            return CutsceneView::Create(
                outputs, vfs, *loading.loader, language,
                {.folder = placement.folder, .file = placement.cutscene, .rect = placement.rect});
        });
    if (!view) {
        return std::unexpected(std::move(view.error()));
    }
    return std::unique_ptr<TrackPreview>(
        new TrackPreview(*outputs.renderer, vfs, std::move(loading), std::move(*view), placed->rect,
                         LoadBorder(*outputs.renderer, vfs, layout)));
}

std::optional<TrackPreview::Border> TrackPreview::LoadBorder(render::Renderer& renderer,
                                                             const assets::Vfs& vfs,
                                                             std::string_view layout) {
    const Result<MenuFiles> files = ReadMenuFiles(vfs, layout);
    const Result<const assets::MenuElement*> element = files.and_then(
        [&](const MenuFiles& read) { return FindMenuElement(read, layout, kBorderElement); });
    if (!element) {
        log::Warn("the race preview has no border: {}", element.error().message);
        return std::nullopt;
    }
    const auto* frame = std::get_if<assets::FrameElement>(&(*element)->body);
    if (frame == nullptr) {
        log::Warn("the race preview has no border: {} isn't a frame", kBorderElement);
        return std::nullopt;
    }
    MenuImageCache images(renderer, vfs, *files);
    Result<MenuFrameLook> look = LoadMenuFrame(images, *frame);
    if (!look) {
        log::Warn("the race preview has no border: {}", look.error().message);
        return std::nullopt;
    }
    return Border{.look = *look,
                  .rect = SpanOf(OriginOf(files->layout, **element), (*element)->widget.rect)};
}

void TrackPreview::Show(std::span<const std::optional<RacePreview>> previews, PreviewStyle style,
                        bool isLocked) {
    previews_.assign(previews.begin(), previews.end());
    isLocked_ = isLocked;
    cycle_.Restart(style);
    ShowRace(cycle_.Race());
}

void TrackPreview::Tick(Milliseconds elapsed) {
    if (const std::optional<std::size_t> race = cycle_.Advance(elapsed)) {
        ShowRace(*race);
    }
    view_->Advance(elapsed);
}

render::RenderView TrackPreview::Render(std::span<const sim::EntityClock> clocks,
                                        std::vector<render::SpriteDraw>& sprites) {
    DrawBorder(sprites);
    const render::RenderView view = view_->Render(clocks, sprites);
    if (shownImage_) {
        sprites.push_back({.texture = *shownImage_, .bounds = bounds_});
    }
    if (isLocked_) {
        sprites.push_back(Shade(bounds_, kDimmerAlpha));
    }
    if (const float fade = cycle_.FadeAlpha(); fade > 0.0F) {
        sprites.push_back(Shade(bounds_, fade));
    }
    return view;
}

void TrackPreview::DrawBorder(std::vector<render::SpriteDraw>& sprites) const {
    if (!border_) {
        return;
    }
    MenuFrameLook look = border_->look;
    if (shown_ && shown_->image.empty()) {
        look.borderTint = FrameTintOf(*shown_, rules_).value_or(look.borderTint);
    }
    DrawMenuFrameBorder(look, border_->rect, sprites);
}

std::optional<render::TextureId> TrackPreview::ImageOf(const std::string& path) {
    const auto cached = images_.find(path);
    if (cached != images_.end()) {
        return cached->second;
    }
    Result<render::TextureId> texture =
        vfs_->Read(path)
            .and_then([](const assets::FileData& file) { return assets::DecodePng(file.Bytes()); })
            .and_then(
                [&](const assets::ImageAsset& image) { return renderer_->UploadTexture(image); });
    if (!texture) {
        log::Warn("the race preview shows no image: {}", texture.error().message);
    }
    return images_.emplace(path, texture ? std::optional(*texture) : std::nullopt).first->second;
}

void TrackPreview::ShowRace(std::size_t race) {
    shownImage_.reset();
    if (race >= previews_.size() || !previews_[race]) {
        cycle_.Shown(true);
        return;
    }
    const RacePreview& preview = *previews_[race];
    shown_ = preview;
    if (!preview.image.empty()) {
        shownImage_ = ImageOf(preview.image);
        cycle_.Shown(true);
        return;
    }
    const Result<void> dressed = preview.driver.empty()
                                     ? Result<void>{}
                                     : view_->ShowDriver(preview.driver, *loading_.loader);
    if (!dressed) {
        log::Warn("the race preview keeps its last driver: {}", dressed.error().message);
    }
    const Result<bool> switched = view_->SelectFrame(preview.frame);
    if (!switched) {
        log::Warn("the race preview stays put: {}", switched.error().message);
    }
    cycle_.Shown(switched.value_or(false));
}

}
