#pragma once

#include <string_view>

#include "app/cutscene/cutscene_overlay.hpp"
#include "app/ui_text/ui_font.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// `overlay`'s texts point into `fonts`.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct CutsceneResources {
    UiFonts fonts;
    CutsceneOverlayContent overlay;
};

/// String table 0 is `<language>.SRF` when `folder` has one, and the `CEB`'s otherwise.
[[nodiscard]] Result<CutsceneResources> LoadCutsceneResources(
    render::Renderer& renderer, const assets::Vfs& vfs, std::string_view folder,
    std::string_view language, const assets::CutsceneEffectsAsset& effects);

}
