#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_view.hpp"
#include "app/menu/preview_cycle.hpp"
#include "app/race_scene_parts.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::app {

/// The race pick screens' looping `SINGRACE/PST` preview, showing the chosen races' frames.
class TrackPreview {
public:
    /// `layout` is the screen's `MIB`, whose `singrace` element places the preview.
    [[nodiscard]] static Result<std::unique_ptr<TrackPreview>> Load(const RaceOutputs& outputs,
                                                                    const assets::Vfs& vfs,
                                                                    assets::TrackLoader& loader,
                                                                    std::string_view language,
                                                                    std::string_view layout);

    /// `previews` names a frame per race of the course, unset where a circuit has no race.
    void Show(std::span<const std::optional<std::string>> previews, PreviewStyle style,
              bool isLocked);

    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const { return view_->Clocks(); }
    [[nodiscard]] const render::Rect& Bounds() const { return bounds_; }

    /// Appends the preview's overlay, dimmer and fade to `sprites`, as `CutsceneView` does.
    [[nodiscard]] render::RenderView Render(std::span<const sim::EntityClock> clocks,
                                            std::vector<render::SpriteDraw>& sprites);

private:
    TrackPreview(std::unique_ptr<CutsceneView> view, render::Rect bounds)
        : view_(std::move(view)), bounds_(bounds) {}

    void ShowRace(std::size_t race);

    std::unique_ptr<CutsceneView> view_;
    render::Rect bounds_;
    std::vector<std::optional<std::string>> previews_;
    bool isLocked_ = false;
    PreviewCycle cycle_;
};

}
