#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_player.hpp"
#include "app/scene.hpp"
#include "app/track_entity_draws.hpp"
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
    /// `scene` is like `cutscene:CIRCUIT1/CIR1-01`. `renderer` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<CutsceneScene>> Create(render::Renderer& renderer,
                                                                       const assets::Vfs& vfs,
                                                                       assets::TrackLoader& loader,
                                                                       std::string_view scene);

    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    struct Parts {
        render::Renderer* renderer = nullptr;
        std::string name;
        std::unique_ptr<assets::CutsceneTimelineAsset> timeline;
        std::unique_ptr<sim::TrackEntitySources> sources;
        std::vector<render::MeshDraw> statics;
        std::vector<std::uint32_t> slots;
        TrackEntityDraws jointed;
        CutscenePlayer player;
    };

    explicit CutsceneScene(Parts parts) : parts_(std::move(parts)) {}

    Parts parts_;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::DirectionalLight> lights_;
};

}
