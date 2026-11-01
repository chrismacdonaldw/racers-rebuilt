#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/brick_models.hpp"
#include "app/chase_camera.hpp"
#include "app/hazard/ghost_trail.hpp"
#include "app/hazard/water_scroll.hpp"
#include "app/hidden_models.hpp"
#include "app/hud/race_hud_layer.hpp"
#include "app/menu/pause_menu.hpp"
#include "app/powerup/powerup_models.hpp"
#include "app/powerup/powerup_quads.hpp"
#include "app/powerup/powerup_visuals.hpp"
#include "app/powerup/warp_start.hpp"
#include "app/powerup/warp_tube.hpp"
#include "app/race_audio.hpp"
#include "app/race_particles.hpp"
#include "app/race_scene_name.hpp"
#include "app/racer/car_shadows.hpp"
#include "app/racer/racer_grid.hpp"
#include "app/racer_effect_tools.hpp"
#include "app/scene.hpp"
#include "app/sound_banks.hpp"
#include "app/track_entity_draws.hpp"
#include "app/trigger_spheres.hpp"
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

/// Where a race draws and plays. Each must outlive the scene.
struct RaceOutputs {
    render::Renderer* renderer = nullptr;
    audio::Mixer* mixer = nullptr;
    SoundBanks* banks = nullptr;
};

/// A race on a full grid with the player in slot 0. Other racers idle until AI drivers arrive.
class RaceScene final : public Scene {
public:
    [[nodiscard]] static Result<std::unique_ptr<RaceScene>> Create(
        const RaceOutputs& outputs, const assets::Vfs& vfs, assets::TrackLoader& loader,
        const RaceSceneSetup& setup, std::uint64_t seed, Nanoseconds started);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    [[nodiscard]] overlay::RacerTools OverlayRacerTools() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;
    [[nodiscard]] SceneMusic Music() const override;

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Parts {
        render::Renderer* renderer = nullptr;
        const assets::Vfs* vfs = nullptr;
        RaceSceneSetup setup;
        std::vector<render::MeshDraw> trackDraws;
        RacerGrid racers;
        assets::NameMap<render::Material> racerFx;
        BrickModels bricks;
        PowerupModelSet powerups;
        std::optional<WarpTube> warpTube;
        render::Camera camera;
        std::unique_ptr<sim::RaceTrack> raceTrack;
        TrackEntityDraws entityDraws;
        HiddenModels hiddenModels;
        GhostTrail ghostTrail;
        WaterScroll waterScroll;
        std::unique_ptr<sim::WorldList> world;
        sim::RaceSession session;
        std::unique_ptr<RaceAudio> audio;
        std::optional<RaceParticles> particles;
        Nanoseconds started{};
    };

    explicit RaceScene(Parts parts);

    [[nodiscard]] Transform PlayerCar() const;
    void TickPause(const sim::InputFrame& input);
    [[nodiscard]] overlay::RacerActionGroup EffectTools();
    [[nodiscard]] overlay::RacerActionGroup PowerupTools();
    void ApplyChosenTools();
    [[nodiscard]] std::optional<std::size_t> PauseFocus() const;
    void GatherMeshes(const sim::RenderSnapshot& interpolated, float elapsedMs);
    void DrawDebugLines();

    Parts parts_;
    CarShadows shadows_;
    PowerupVisuals powerupVisuals_;
    PowerupQuads powerupQuads_;
    ChaseCamera chase_;
    RaceHudLayer hud_;
    PauseMenu pause_;
    std::optional<std::string> next_;
    std::vector<sim::RacerControls> controls_;
    std::size_t chosenRacer_ = 0;
    std::vector<std::function<void(sim::RaceSession&)>> chosenTools_;
    std::vector<CarLook> carLooks_;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::DebugLine> collisionLines_;
    TriggerRings triggerRings_;
    std::vector<render::DebugLine> debugLines_;
    std::vector<render::QuadDraw> quads_;
    bool showsCheckpoints_ = false;
    bool showsCollision_ = false;
    bool showsTriggers_ = false;
    std::optional<Nanoseconds> lastRender_;
    bool hasDrawn_ = false;
};

}
