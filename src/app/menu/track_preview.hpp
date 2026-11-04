#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_view.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/preview_cycle.hpp"
#include "app/menu/race_preview.hpp"
#include "app/race_scene_parts.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/physics/version_rules.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::app {

/// The race pick screens' looping `SINGRACE/PST` preview, showing the chosen races' frames
/// inside a border tinted per race.
class TrackPreview {
public:
    /// `layout` is the screen's `MIB`, whose `singrace` element places the preview.
    [[nodiscard]] static Result<std::unique_ptr<TrackPreview>> Load(const RaceOutputs& outputs,
                                                                    const assets::Vfs& vfs,
                                                                    std::string_view language,
                                                                    std::string_view layout);

    /// `previews` has one per race of the course, unset where a circuit has no race.
    void Show(std::span<const std::optional<RacePreview>> previews, PreviewStyle style,
              bool isLocked);

    void Tick(Milliseconds elapsed);

    void DrawFarScenery(bool isDrawn) { view_->DrawFarScenery(isDrawn); }
    void UseRules(const sim::VersionRules& rules) { rules_ = rules; }

    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const { return view_->Clocks(); }
    [[nodiscard]] const render::Rect& Bounds() const { return bounds_; }

    /// Appends the preview's border, overlay, dimmer and fade to `sprites`.
    [[nodiscard]] render::RenderView Render(std::span<const sim::EntityClock> clocks,
                                            std::vector<render::SpriteDraw>& sprites);

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Loading {
        std::unique_ptr<assets::LoaderPool> pool;
        std::unique_ptr<assets::TrackLoader> loader;
    };

    struct Border {
        MenuFrameLook look;
        render::Rect rect;
    };

    TrackPreview(render::Renderer& renderer, const assets::Vfs& vfs, Loading loading,
                 std::unique_ptr<CutsceneView> view, render::Rect bounds,
                 std::optional<Border> border)
        : renderer_(&renderer),
          vfs_(&vfs),
          loading_(std::move(loading)),
          view_(std::move(view)),
          bounds_(bounds),
          border_(border) {}

    [[nodiscard]] static std::optional<Border> LoadBorder(render::Renderer& renderer,
                                                          const assets::Vfs& vfs,
                                                          std::string_view layout);
    void DrawBorder(std::vector<render::SpriteDraw>& sprites) const;

    void ShowRace(std::size_t race);
    [[nodiscard]] std::optional<render::TextureId> ImageOf(const std::string& path);

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    Loading loading_;
    std::unique_ptr<CutsceneView> view_;
    render::Rect bounds_;
    std::optional<Border> border_;
    sim::VersionRules rules_;
    std::optional<RacePreview> shown_;
    std::optional<render::TextureId> shownImage_;
    std::map<std::string, std::optional<render::TextureId>, std::less<>> images_;
    std::vector<std::optional<RacePreview>> previews_;
    bool isLocked_ = false;
    PreviewCycle cycle_;
};

}
