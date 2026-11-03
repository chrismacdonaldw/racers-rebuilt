#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_effect_player.hpp"
#include "app/cutscene/cutscene_materials.hpp"
#include "app/cutscene/cutscene_play.hpp"
#include "app/cutscene/cutscene_player.hpp"
#include "app/race_scene_parts.hpp"
#include "app/track_entity_draws.hpp"
#include "app/track_model.hpp"
#include "app/ui_text/ui_font.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/driver_look.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// Plays one cutscene a frame at a time, starting with its first, in `play.rect`.
class CutsceneView {
public:
    [[nodiscard]] static Result<std::unique_ptr<CutsceneView>> Create(const RaceOutputs& outputs,
                                                                      const assets::Vfs& vfs,
                                                                      assets::TrackLoader& loader,
                                                                      std::string_view language,
                                                                      CutscenePlay play);

    CutsceneView(const CutsceneView&) = delete;
    CutsceneView& operator=(const CutsceneView&) = delete;
    CutsceneView(CutsceneView&&) = delete;
    CutsceneView& operator=(CutsceneView&&) = delete;
    ~CutsceneView();

    [[nodiscard]] const CutscenePlay& Play() const { return parts_.play; }

    /// Rewinds to frame `name`, in any case, and stops the effects. False if playing or missing.
    [[nodiscard]] Result<bool> SelectFrame(std::string_view name);

    /// Puts driver `code` in for `guy1` from the next SelectFrame, loading each driver only once.
    [[nodiscard]] Result<void> ShowDriver(std::string_view code, assets::TrackLoader& loader);

    void Advance(Milliseconds elapsed);

    [[nodiscard]] bool IsComplete() const { return parts_.player.IsComplete(); }
    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const {
        return parts_.player.Clocks();
    }

    /// Appends the overlay to `sprites`, which the view leaves for the caller to fill and point at.
    [[nodiscard]] render::RenderView Render(std::span<const sim::EntityClock> clocks,
                                            std::vector<render::SpriteDraw>& sprites);

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct DressedDriver {
        assets::DriverLook look;
        JointedModel model;
        assets::NameMap<render::Material> materials;
    };

    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Parts {
        const assets::Vfs* vfs = nullptr;
        render::Renderer* renderer = nullptr;
        CutscenePlay play;
        std::optional<assets::DriverLook> driver;
        std::optional<std::string> driverModel;
        std::string shownDriver;
        bool isDriverPending = false;
        assets::NameMap<DressedDriver> dressed;
        std::unique_ptr<assets::CutsceneTimelineAsset> timeline;
        std::size_t frame = 0;
        assets::TrackAssets assets;
        std::unique_ptr<TrackModel> model;
        WorldAnimations animations;
        std::unique_ptr<sim::TrackEntitySources> sources;
        TrackEntityDraws jointed;
        CutscenePlayer player;
        std::unique_ptr<assets::CutsceneEffectsAsset> effects;
        UiFonts fonts;
        CutsceneEffectPlayer effectPlayer;
    };

    explicit CutsceneView(Parts parts) : parts_(std::move(parts)) {}

    [[nodiscard]] Result<DressedDriver> Dress(const std::string& code,
                                              assets::TrackLoader& loader) const;
    [[nodiscard]] Result<void> Replay(std::size_t frame);

    Parts parts_;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::QuadDraw> quads_;
    std::vector<render::DirectionalLight> lights_;
};

}
