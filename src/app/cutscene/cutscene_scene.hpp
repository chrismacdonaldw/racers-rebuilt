#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_effect_player.hpp"
#include "app/cutscene/cutscene_play.hpp"
#include "app/cutscene/cutscene_player.hpp"
#include "app/race_scene_parts.hpp"
#include "app/scene.hpp"
#include "app/track_entity_draws.hpp"
#include "app/ui_text/ui_font.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

inline constexpr std::string_view kCutsceneScenePrefix = "cutscene:";

/// True for viewer names like `cutscene:CIRCUIT1/CIR1-01` and screens like `screen:legal`.
[[nodiscard]] bool IsCutsceneScene(std::string_view scene);

/// `scene` is like `cutscene:CIRCUIT1/CIR1-01`, which loops full screen for checking by eye.
[[nodiscard]] Result<CutscenePlay> ViewerPlay(std::string_view scene);

/// Plays one cutscene's first frame, then shows `play.next` unless it loops.
class CutsceneScene final : public Scene {
public:
    [[nodiscard]] static Result<std::unique_ptr<CutsceneScene>> Create(const RaceOutputs& outputs,
                                                                       const assets::Vfs& vfs,
                                                                       assets::TrackLoader& loader,
                                                                       std::string_view language,
                                                                       std::string_view scene);

    [[nodiscard]] static Result<std::unique_ptr<CutsceneScene>> Create(const RaceOutputs& outputs,
                                                                       const assets::Vfs& vfs,
                                                                       assets::TrackLoader& loader,
                                                                       std::string_view language,
                                                                       CutscenePlay play);

    CutsceneScene(const CutsceneScene&) = delete;
    CutsceneScene& operator=(const CutsceneScene&) = delete;
    CutsceneScene(CutsceneScene&&) = delete;
    CutsceneScene& operator=(CutsceneScene&&) = delete;
    ~CutsceneScene() override;

    [[nodiscard]] SceneMusic Music() const override { return parts_.play.music; }
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void HandleEvent(const SDL_Event& event) override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Parts {
        render::Renderer* renderer = nullptr;
        CutscenePlay play;
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

    void End();

    Parts parts_;
    Milliseconds played_{0};
    bool isSkipAsked_ = false;
    bool hasEnded_ = false;
    std::optional<std::string> next_;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::QuadDraw> quads_;
    std::vector<render::SpriteDraw> sprites_;
    std::vector<render::DirectionalLight> lights_;
};

}
