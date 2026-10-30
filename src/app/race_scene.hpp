#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/chase_camera.hpp"
#include "app/hud/race_hud_layer.hpp"
#include "app/race_audio.hpp"
#include "app/racer/racer_grid.hpp"
#include "app/scene.hpp"
#include "app/sound_banks.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
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

/// The player drives grid slot `racer`'s prebuilt racer, which swaps places with slot 0.
struct RaceSceneSetup {
    std::string track;
    sim::RespawnRule respawn;
    std::size_t racer = 0;

    friend bool operator==(const RaceSceneSetup&, const RaceSceneSetup&) = default;
};

/// Where a race draws and plays. Each must outlive the scene.
struct RaceOutputs {
    render::Renderer* renderer = nullptr;
    audio::Mixer* mixer = nullptr;
    SoundBanks* banks = nullptr;
};

/// The name replays record, such as `race:RACEC0R0:respawn=3000:racer=2`, with `respawn=off`
/// when respawning is off. `racer` may be left out.
[[nodiscard]] std::string RaceSceneName(const RaceSceneSetup& setup);

[[nodiscard]] bool IsRaceScene(std::string_view scene);
[[nodiscard]] Result<RaceSceneSetup> ParseRaceScene(std::string_view scene);

/// A race on a full grid with the player in slot 0. Other racers idle until AI drivers arrive.
class RaceScene final : public Scene {
public:
    [[nodiscard]] static Result<std::unique_ptr<RaceScene>> Create(
        const RaceOutputs& outputs, const assets::Vfs& vfs, assets::TrackLoader& loader,
        const RaceSceneSetup& setup, std::uint64_t seed, Nanoseconds started);

    void ApplySettings(const settings::Registry& settings) override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;
    [[nodiscard]] SceneMusic Music() const override;

private:
    struct Parts {
        render::Renderer* renderer = nullptr;
        const assets::Vfs* vfs = nullptr;
        std::string track;
        std::vector<render::MeshDraw> trackDraws;
        RacerGrid racers;
        render::Camera camera;
        std::unique_ptr<sim::RaceTrack> raceTrack;
        std::unique_ptr<sim::WorldList> world;
        sim::RaceSession session;
        std::unique_ptr<RaceAudio> audio;
        Nanoseconds started{};
    };

    explicit RaceScene(Parts parts);

    [[nodiscard]] Transform PlayerCar() const;
    void DrawDebugLines();

    Parts parts_;
    ChaseCamera chase_;
    RaceHudLayer hud_;
    std::vector<sim::RacerControls> controls_;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::DebugLine> collisionLines_;
    std::vector<render::DebugLine> debugLines_;
    bool showsCheckpoints_ = false;
    bool showsCollision_ = false;
    std::optional<Nanoseconds> lastRender_;
    bool hasDrawn_ = false;
};

}
