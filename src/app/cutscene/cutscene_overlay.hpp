#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "app/ui_text/ui_font.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/cutscene_effects.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct OverlayText {
    std::u16string text;
    const UiFont* font = nullptr;
};

/// One entry per `CEB` text and image, in their order.
struct CutsceneOverlayContent {
    std::vector<OverlayText> texts;
    std::vector<UiImage> images;
};

/// Draws a cutscene's `CEB` images, text and fades. `effects` and the fonts must outlive it.
class CutsceneOverlay {
public:
    CutsceneOverlay(const assets::CutsceneEffectsAsset& effects, CutsceneOverlayContent content);

    /// Ignores tints, sounds and particles.
    void Start(assets::EffectKind kind, std::size_t effect);
    void Stop(assets::EffectKind kind, std::size_t effect);

    /// A fade that reached its end holds it until the next call, then ends.
    void Advance(Milliseconds elapsed);

    /// In UI units. Visuals without an edge centre in `viewport`, the cutscene's rectangle.
    void Draw(const render::Rect& viewport, std::vector<render::SpriteDraw>& out) const;

private:
    struct Fade {
        bool isShown = false;
        bool hasEnded = false;
        Milliseconds age{0};
    };

    void DrawImages(const render::Rect& viewport, std::vector<render::SpriteDraw>& out) const;
    void DrawTexts(const render::Rect& viewport, std::vector<render::SpriteDraw>& out) const;
    void DrawFades(std::vector<render::SpriteDraw>& out) const;

    const assets::CutsceneEffectsAsset* effects_;
    CutsceneOverlayContent content_;
    std::vector<bool> imagesShown_;
    std::vector<bool> textsShown_;
    std::vector<Fade> fades_;
};

}
