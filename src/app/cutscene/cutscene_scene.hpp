#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_play.hpp"
#include "app/cutscene/cutscene_view.hpp"
#include "app/race_scene_parts.hpp"
#include "app/scene.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

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

    [[nodiscard]] SceneMusic Music() const override { return view_->Play().music; }
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void HandleEvent(const SDL_Event& event) override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    CutsceneScene(render::Renderer& renderer, std::unique_ptr<CutsceneView> view)
        : renderer_(&renderer), view_(std::move(view)) {}

    void End();

    render::Renderer* renderer_;
    std::unique_ptr<CutsceneView> view_;
    Milliseconds played_{0};
    bool isSkipAsked_ = false;
    bool hasEnded_ = false;
    std::optional<std::string> next_;
    std::vector<render::SpriteDraw> sprites_;
};

}
