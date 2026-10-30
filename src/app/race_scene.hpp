#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/chase_camera.hpp"
#include "app/scene.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"
#include "sim/race/respawn.hpp"

namespace rr::app {

/// What a race fixes when it starts. `track` is a folder under `GAMEDATA`, such as `RACEC0R0`.
struct RaceSceneSetup {
    std::string track;
    sim::RespawnRule respawn;

    friend bool operator==(const RaceSceneSetup&, const RaceSceneSetup&) = default;
};

/// The name replays record, such as `race:RACEC0R0:respawn=3000` or `...:respawn=off`.
[[nodiscard]] std::string RaceSceneName(const RaceSceneSetup& setup);

[[nodiscard]] bool IsRaceScene(std::string_view scene);
[[nodiscard]] Result<RaceSceneSetup> ParseRaceScene(std::string_view scene);

/// A race on a full grid with the player in slot 0. Other racers idle until AI drivers arrive.
class RaceScene final : public Scene {
public:
    /// `renderer` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<RaceScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, assets::TrackLoader& loader,
        const RaceSceneSetup& setup, std::uint64_t seed, Nanoseconds started);

    void ApplySettings(const settings::Registry& settings) override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;
    [[nodiscard]] std::string_view DeveloperStatus() const override { return status_; }

private:
    struct Parts {
        render::Renderer* renderer = nullptr;
        std::string track;
        std::vector<render::MeshDraw> trackDraws;
        std::vector<render::MeshDraw> carDraws;
        render::Camera camera;
        std::unique_ptr<sim::RaceTrack> raceTrack;
        std::unique_ptr<sim::WorldList> world;
        sim::RaceSession session;
        Nanoseconds started{};
    };

    explicit RaceScene(Parts parts);

    [[nodiscard]] Transform PlayerCar() const;
    void DrawDebugLines();

    Parts parts_;
    ChaseCamera chase_;
    std::vector<sim::RacerControls> controls_;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::DebugLine> collisionLines_;
    std::vector<render::DebugLine> debugLines_;
    std::string status_;
    bool showsCheckpoints_ = false;
    bool showsCollision_ = false;
    bool hasDrawn_ = false;
};

}
