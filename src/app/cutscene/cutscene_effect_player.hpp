#pragma once

#include <span>
#include <vector>

#include "app/cutscene/cutscene_links.hpp"
#include "app/cutscene/cutscene_overlay.hpp"
#include "app/cutscene/cutscene_sounds.hpp"
#include "app/cutscene/cutscene_timeline.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Runs the `CEB` effects the frame's transforms link to. The frame and `effects` must outlive it.
class CutsceneEffectPlayer {
public:
    CutsceneEffectPlayer(const assets::CutsceneFrame& frame,
                         const assets::CutsceneEffectsAsset& effects, CutsceneSounds sounds,
                         CutsceneOverlay overlay);

    /// `changes` are the frame's for the same `elapsed`.
    void Advance(std::span<const CutsceneEventChange> changes, Milliseconds elapsed);

    /// Stops sounds, text and images, and leaves fades running, as the original's `StopAll` does.
    void StopAll();

    void Draw(const render::Rect& viewport, std::vector<render::SpriteDraw>& out) const {
        overlay_.Draw(viewport, out);
    }

private:
    const assets::CutsceneFrame* frame_;
    const assets::CutsceneEffectsAsset* effects_;
    CutsceneSounds sounds_;
    CutsceneOverlay overlay_;
    std::vector<EffectCommand> commands_;
};

}
