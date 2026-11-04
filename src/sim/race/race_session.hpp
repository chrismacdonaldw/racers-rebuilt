#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "assets/brick_layout.hpp"
#include "assets/hazard_list.hpp"
#include "assets/target_points.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "sim/ai/ai_powerups.hpp"
#include "sim/ai/ai_skill.hpp"
#include "sim/ai/ai_taunts.hpp"
#include "sim/ai/route_choice.hpp"
#include "sim/ai/route_driver.hpp"
#include "sim/ai/rubber_band.hpp"
#include "sim/collision/racer_worlds.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/collision/track_surfaces.hpp"
#include "sim/hazard/hazards.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/physics/version_rules.hpp"
#include "sim/powerup/aim_targets.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/powerup/powerup_inventory.hpp"
#include "sim/powerup/powerup_system.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/race_clock.hpp"
#include "sim/race/race_progress.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/race_sounds.hpp"
#include "sim/race/random.hpp"
#include "sim/race/respawn.hpp"
#include "sim/track/event_resources.hpp"
#include "sim/track/racer_tints.hpp"
#include "sim/track/surface_events.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_events.hpp"
#include "sim/track/trigger_lists.hpp"

namespace rr::sim {

/// What a session reads from its track. Everything it points to must outlive the session.
struct RaceCourse {
    const CheckpointGraph* checkpoints = nullptr;
    std::span<const RacerStart> grid;
    const CollisionQuery* world = nullptr;
    float lowestTrackZ = 0.0F;
    const GridRoutes* routes = nullptr;
    const assets::BrickLayout* bricks = nullptr;
    std::span<const assets::TargetPoint> targets;
    const TrackEventSources* events = nullptr;
    const TrackCollision* collision = nullptr;
    const TrackEntitySources* entities = nullptr;
    std::span<const assets::Hazard> hazards;
    bool isMirrored = false;
};

class RaceSession {
public:
    /// Fails when the racers don't fit on the grid from `setup.firstSlot`.
    [[nodiscard]] static Result<RaceSession> Create(const RaceSetup& setup,
                                                    const RaceCourse& course);

    /// `controls` has one entry per racer. The intro ignores them and the countdown only steers.
    void Tick(Milliseconds elapsed, std::span<const RacerControls> controls);

    [[nodiscard]] const RaceClock& Clock() const { return clock_; }
    [[nodiscard]] std::span<const Racer> Racers() const { return racers_; }
    [[nodiscard]] std::span<Racer> Racers() { return racers_; }
    [[nodiscard]] const RaceProgress& Progress() const { return progress_; }
    [[nodiscard]] Random& Rng() { return random_; }
    [[nodiscard]] const BrickField& Bricks() const { return bricks_; }
    [[nodiscard]] std::span<const PowerupInventory> Inventories() const { return inventories_; }
    [[nodiscard]] const PowerupSystem& Powerups() const { return powerups_; }
    [[nodiscard]] const PowerupTuning& PowerupTunings() const { return powerupTuning_; }
    [[nodiscard]] const Explosions& Blasts() const { return explosions_; }
    [[nodiscard]] const Explosions& Spikes() const { return spikes_; }
    [[nodiscard]] const TrackEvents& Triggers() const { return triggers_; }
    [[nodiscard]] const TrackSurfaces& Surfaces() const { return surfaces_; }
    [[nodiscard]] std::span<const TriggerWorlds> SwitchedWorlds() const { return switchedWorlds_; }
    [[nodiscard]] const SurfaceEvents& SurfaceContacts() const { return surfaceEvents_; }
    [[nodiscard]] const TrackEntities& Entities() const { return entities_; }
    [[nodiscard]] const Hazards& TrackHazards() const { return hazards_; }
    [[nodiscard]] const EventResources& Resources() const { return resources_; }
    [[nodiscard]] const RacerTints& Tints() const { return tints_; }
    [[nodiscard]] bool IsMirrored() const { return course_.isMirrored; }
    [[nodiscard]] std::span<const AiTaunter> Taunters() const { return taunts_; }
    [[nodiscard]] const AimTargets& Targets() const { return targets_; }
    [[nodiscard]] const std::optional<RouteDriver>& DriverOf(std::size_t racer) const {
        return drivers_.at(racer);
    }

    /// Replaces what `racer` holds, sending its white bricks home.
    void GivePowerup(std::size_t racer, assets::BrickColour colour, std::uint8_t level);

    /// The sounds the last `Tick` set off.
    [[nodiscard]] std::span<const SoundEvent> Sounds() const { return sounds_.Events(); }

    /// Whether the last `Tick` respawned `racer`.
    [[nodiscard]] bool WasRespawned(std::size_t racer) const { return wasRespawned_.at(racer); }

private:
    RaceSession(const RaceSetup& setup, const RaceCourse& course);

    void TickRacer(std::size_t racer, Milliseconds elapsed, const RacerControls& controls);
    [[nodiscard]] RacerControls ControlsOf(std::size_t racer,
                                           std::span<const RacerControls> controls);
    [[nodiscard]] bool WantsPowerup(std::size_t racer, Milliseconds elapsed,
                                    const RacerControls& controls);
    void UsePowerup(std::size_t racer);
    [[nodiscard]] bool UseButtonCheats(std::size_t racer, const RacerControls& controls);
    [[nodiscard]] bool SkipAhead(std::size_t racer);
    void Taunt(std::size_t racer);
    void StartRacing();
    void ReleaseComputerRacers(Milliseconds elapsed);
    void AdvanceClock(Milliseconds elapsed);
    void StartBoosts();
    void ResolveContacts();
    void PaceRoutes(Milliseconds elapsed);
    void TuneSkills();
    [[nodiscard]] std::shared_ptr<const RacingLine> LineFor(std::size_t slot,
                                                            std::size_t variant) const;
    [[nodiscard]] std::span<const std::shared_ptr<const RacingLine>> LinesFor(
        std::size_t slot) const;
    void MeasureReaches();
    void TickBricks(Milliseconds elapsed);
    void RollChaosBrick(PowerupInventory& inventory);
    void KeepWhitesFull();
    [[nodiscard]] PlayerCheats CheatsOf(std::size_t racer) const;
    void ReactToPlaces();
    void TickTriggers(Milliseconds elapsed);
    void SwitchOnTriggerWorlds();
    void TickHazards(Milliseconds elapsed, HazardContext& hazards);
    [[nodiscard]] RacerWorldView WorldsOf(std::size_t racer) const;
    [[nodiscard]] PowerupContext EffectContext();
    [[nodiscard]] HazardContext HazardsContext(Milliseconds elapsed, PowerupContext& effects);
    [[nodiscard]] bool HasPlayerFinished() const;

    RaceCourse course_;
    RespawnRule respawn_;
    Cheats cheats_;
    Modifiers modifiers_;
    PlayerCheats playerCheats_;
    Milliseconds headStartLeft_{0};
    std::vector<bool> isPlayer_;
    bool isDemo_ = false;
    const VersionRules* rules_ = nullptr;
    bool lagsInput_ = false;
    std::vector<RacerControls> lastControls_;
    std::vector<Racer> racers_;
    std::vector<std::optional<RouteDriver>> drivers_;
    std::vector<RespawnWatch> respawns_;
    std::vector<bool> wasRespawned_;
    std::vector<Vector3> positions_;
    std::vector<Vector3> rivals_;
    std::vector<std::size_t> places_;
    RaceProgress progress_;
    RaceClock clock_;
    RaceSounds sounds_;
    Random random_;
    TrackSurfaces surfaces_;
    TrackEvents triggers_;
    TrackEntities entities_;
    Hazards hazards_;
    EventResources resources_;
    RacerTints tints_;
    AimTargets targets_;
    std::vector<TriggerRacer> triggerRacers_;
    std::vector<std::optional<std::size_t>> sphereWorlds_;
    std::vector<TriggerWorlds> switchedWorlds_;
    SurfaceEvents surfaceEvents_;
    RubberBand rubberBand_;
    std::vector<int> skillLevels_;
    bool catchesUp_ = false;
    std::vector<PacedRacer> paced_;
    std::vector<float> routeSpeeds_;
    BrickField bricks_;
    std::vector<PowerupInventory> inventories_;
    PowerupSystem powerups_;
    PowerupTuning powerupTuning_;
    std::vector<AiPowerupUser> aiPowerups_;
    std::vector<AiTaunter> taunts_;
    WeaponEvents weapons_;
    Explosions explosions_;
    Explosions spikes_{kSpikeExplosion, kSpikeExplosionCount};
    std::vector<BoundingSphere> localBounds_;
    std::vector<BoundingSphere> reaches_;
    std::vector<BoundingSphere> pickupReaches_;
    std::vector<ContactBody> contactBodies_;
    std::vector<std::vector<std::shared_ptr<const RacingLine>>> lines_;
    std::vector<RacerWorldView> contactWorlds_;
    RacerContacts contacts_;
};

}
