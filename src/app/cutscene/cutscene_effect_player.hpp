#pragma once

#include <span>
#include <vector>

#include "app/cutscene/cutscene_links.hpp"
#include "app/cutscene/cutscene_overlay.hpp"
#include "app/cutscene/cutscene_particles.hpp"
#include "app/cutscene/cutscene_sounds.hpp"
#include "app/cutscene/cutscene_timeline.hpp"
#include "app/cutscene/cutscene_tints.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct CutsceneEffects {
    CutsceneSounds sounds;
    CutsceneOverlay overlay;
    CutsceneTints tints;
    CutsceneParticles particles;
};

/// Runs the `CEB` effects the frame's transforms link to. The frame and `effects` must outlive it.
class CutsceneEffectPlayer {
public:
    CutsceneEffectPlayer(const assets::CutsceneFrame& frame,
                         const assets::CutsceneEffectsAsset& effects, CutsceneEffects parts);

    /// `changes` are the frame's for the same `elapsed`.
    void Advance(std::span<const CutsceneEventChange> changes, Milliseconds elapsed);

    /// Stops tints, sounds, text and images. Fades and particles run on, as in the original.
    void StopAll();

    [[nodiscard]] const CutsceneTints& Tints() const { return parts_.tints; }

    void AppendQuads(const Transform& camera, std::vector<render::QuadDraw>& out) const {
        parts_.particles.AppendQuads(camera, out);
    }

    void Draw(const render::Rect& viewport, std::vector<render::SpriteDraw>& out) const {
        parts_.overlay.Draw(viewport, out);
    }

private:
    void Run(const EffectCommand& command);

    const assets::CutsceneFrame* frame_;
    const assets::CutsceneEffectsAsset* effects_;
    CutsceneEffects parts_;
    std::vector<EffectCommand> commands_;
};

}
