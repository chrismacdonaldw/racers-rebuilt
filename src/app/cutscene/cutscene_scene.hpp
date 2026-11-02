#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_effect_player.hpp"
#include "app/cutscene/cutscene_player.hpp"
#include "app/race_scene_parts.hpp"
#include "app/scene.hpp"
#include "app/track_entity_draws.hpp"
#include "app/ui_text/ui_font.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

inline constexpr std::string_view kCutsceneScenePrefix = "cutscene:";

[[nodiscard]] bool IsCutsceneScene(std::string_view scene);

/// Plays one cutscene's first frame full screen, looping, to check it against the original.
class CutsceneScene final : public Scene {
public:
    /// `scene` is like `cutscene:CIRCUIT1/CIR1-01`.
    [[nodiscard]] static Result<std::unique_ptr<CutsceneScene>> Create(const RaceOutputs& outputs,
                                                                       const assets::Vfs& vfs,
                                                                       assets::TrackLoader& loader,
                                                                       std::string_view language,
                                                                       std::string_view scene);

    CutsceneScene(const CutsceneScene&) = delete;
    CutsceneScene& operator=(const CutsceneScene&) = delete;
    CutsceneScene(CutsceneScene&&) = delete;
    CutsceneScene& operator=(CutsceneScene&&) = delete;
    ~CutsceneScene() override;

    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Parts {
        render::Renderer* renderer = nullptr;
        std::string name;
        std::unique_ptr<assets::CutsceneTimelineAsset> timeline;
        std::unique_ptr<sim::TrackEntitySources> sources;
        std::vector<render::MeshDraw> statics;
        std::vector<std::uint32_t> slots;
        TrackEntityDraws jointed;
        CutscenePlayer player;
        std::unique_ptr<assets::CutsceneEffectsAsset> effects;
        UiFonts fonts;
        CutsceneEffectPlayer effectPlayer;
    };

    explicit CutsceneScene(Parts parts) : parts_(std::move(parts)) {}

    Parts parts_;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::QuadDraw> quads_;
    std::vector<render::SpriteDraw> sprites_;
    std::vector<render::DirectionalLight> lights_;
};

}
