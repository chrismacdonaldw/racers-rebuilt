#include "app/cutscene/cutscene_overlay.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "app/screen_fill.hpp"
#include "app/ui_text/text_layout.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr float kChannelMax = 255.0F;

render::Color ToColor(const assets::CutsceneColor& color, float alpha = 1.0F) {
    return {.r = static_cast<float>(color[0]) / kChannelMax,
            .g = static_cast<float>(color[1]) / kChannelMax,
            .b = static_cast<float>(color[2]) / kChannelMax,
            .a = alpha};
}

render::Color TintOf(const assets::VisualLayout& layout) {
    return layout.color ? ToColor(*layout.color) : render::Color{};
}

float Place(std::optional<float> edge, float target, float viewportStart, float viewportSize,
            float size) {
    return edge ? *edge * target : viewportStart + ((viewportSize - size) * 0.5F);
}

std::optional<render::Rect> Layout(const assets::VisualLayout& layout, float contentWidth,
                                   float contentHeight, const render::Rect& viewport) {
    const float width = layout.width ? *layout.width * render::kUiWidth : contentWidth;
    const float height = layout.height ? *layout.height * render::kUiHeight : contentHeight;
    const render::Rect bounds{
        .x = Place(layout.left, render::kUiWidth, viewport.x, viewport.width, width),
        .y = Place(layout.top, render::kUiHeight, viewport.y, viewport.height, height),
        .width = width,
        .height = height};
    if (bounds.x + bounds.width > render::kUiWidth ||
        bounds.y + bounds.height > render::kUiHeight) {
        return std::nullopt;
    }
    return bounds;
}

}

CutsceneOverlay::CutsceneOverlay(const assets::CutsceneEffectsAsset& effects,
                                 CutsceneOverlayContent content)
    : effects_(&effects),
      content_(std::move(content)),
      imagesShown_(effects.images.size()),
      textsShown_(effects.texts.size()),
      fades_(effects.fades.size()) {}

void CutsceneOverlay::Start(assets::EffectKind kind, std::size_t effect) {
    switch (kind) {
        case assets::EffectKind::Image:
            imagesShown_[effect] = true;
            break;
        case assets::EffectKind::Text:
            textsShown_[effect] = true;
            break;
        case assets::EffectKind::Fade:
            fades_[effect] = Fade{.isShown = true};
            break;
        default:
            break;
    }
}

void CutsceneOverlay::Stop(assets::EffectKind kind, std::size_t effect) {
    switch (kind) {
        case assets::EffectKind::Image:
            imagesShown_[effect] = false;
            break;
        case assets::EffectKind::Text:
            textsShown_[effect] = false;
            break;
        case assets::EffectKind::Fade:
            fades_[effect] = Fade{};
            break;
        default:
            break;
    }
}

void CutsceneOverlay::Advance(Milliseconds elapsed) {
    for (std::size_t fade = 0; fade < fades_.size(); ++fade) {
        Fade& state = fades_[fade];
        if (!state.isShown) {
            continue;
        }
        if (state.hasEnded) {
            state = Fade{};
            continue;
        }
        const Milliseconds duration = effects_->fades[fade].duration;
        state.age = std::min(state.age + elapsed, duration);
        state.hasEnded = state.age >= duration;
    }
}

void CutsceneOverlay::Draw(const render::Rect& viewport,
                           std::vector<render::SpriteDraw>& out) const {
    DrawImages(viewport, out);
    DrawTexts(viewport, out);
    DrawFades(out);
}

void CutsceneOverlay::DrawImages(const render::Rect& viewport,
                                 std::vector<render::SpriteDraw>& out) const {
    for (std::size_t image = 0; image < imagesShown_.size(); ++image) {
        if (!imagesShown_[image]) {
            continue;
        }
        const UiImage& content = content_.images[image];
        const assets::VisualLayout& layout = effects_->images[image].layout;
        if (const std::optional<render::Rect> bounds =
                Layout(layout, static_cast<float>(content.widthPx),
                       static_cast<float>(content.heightPx), viewport)) {
            out.push_back({.texture = content.texture, .bounds = *bounds, .tint = TintOf(layout)});
        }
    }
}

void CutsceneOverlay::DrawTexts(const render::Rect& viewport,
                                std::vector<render::SpriteDraw>& out) const {
    for (std::size_t text = 0; text < textsShown_.size(); ++text) {
        const OverlayText& content = content_.texts[text];
        if (!textsShown_[text] || content.font == nullptr) {
            continue;
        }
        const assets::VisualLayout& layout = effects_->texts[text].layout;
        const auto fontHeight = static_cast<float>(content.font->heightPx);
        const float scale = layout.height && fontHeight > 0.0F
                                ? *layout.height * render::kUiHeight / fontHeight
                                : 1.0F;
        if (const std::optional<render::Rect> bounds = Layout(
                {.left = layout.left, .top = layout.top},
                TextWidth(*content.font, content.text) * scale, fontHeight * scale, viewport)) {
            LayoutText(*content.font, content.text,
                       {.x = bounds->x, .y = bounds->y, .scale = scale, .tint = TintOf(layout)},
                       out);
        }
    }
}

void CutsceneOverlay::DrawFades(std::vector<render::SpriteDraw>& out) const {
    for (std::size_t fade = 0; fade < fades_.size(); ++fade) {
        const assets::CutsceneFade& effect = effects_->fades[fade];
        if (!fades_[fade].isShown || !effect.direction) {
            continue;
        }
        const float done = effect.duration.count() > 0
                               ? static_cast<float>(fades_[fade].age.count()) /
                                     static_cast<float>(effect.duration.count())
                               : 1.0F;
        const float alpha =
            *effect.direction == assets::FadeDirection::FromColor ? 1.0F - done : done;
        out.push_back(ScreenFill(ToColor(effect.color, alpha)));
    }
}

}
