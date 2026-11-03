#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/powerup/cursed_cars.hpp"
#include "app/powerup/powerup_models.hpp"
#include "app/racer/animation_player.hpp"
#include "app/track_model.hpp"
#include "core/transform.hpp"
#include "render/render_view.hpp"
#include "sim/animation/skeleton_pose.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// `models` must outlive the visuals.
class PowerupVisuals {
public:
    explicit PowerupVisuals(const EffectModels& models);

    void Update(const sim::RenderSnapshot& snapshot, float elapsedMs);

    /// In engine space.
    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return draws_; }
    /// Their draws count from the start of Draws.
    [[nodiscard]] std::span<const FarModel> PlacedModels() const { return placed_; }

private:
    struct Posed {
        const EffectModel* model = nullptr;
        std::size_t part = 0;
        Transform offset;
        std::optional<sim::SkeletonPose> pose;
        std::optional<AnimationPlayer> player;
    };

    struct Instance {
        sim::PowerupKind kind = sim::PowerupKind::Cannonball;
        sim::PowerupUse use;
        std::optional<std::uint8_t> phase;
        std::vector<Posed> models;
        float ageMs = 0.0F;
        bool isLive = false;
        bool wasLive = false;
    };

    [[nodiscard]] Instance& InstanceFor(const sim::EffectSnapshot& effect);
    void Start(Instance& instance, const sim::EffectSnapshot& effect);
    [[nodiscard]] std::optional<Posed> PoseModel(std::string_view name, std::size_t part) const;
    static void Animate(Instance& instance, std::uint8_t phase, float elapsedMs);
    static void Advance(Posed& posed, float elapsedMs);
    void Place(Posed& posed, const Transform& engine, float scale, float ageMs);
    void PlaceExplosion(const sim::ExplosionSnapshot& explosion);
    void PlaceCursedCars(const sim::RenderSnapshot& snapshot, float elapsedMs);

    const EffectModels* models_;
    std::vector<Instance> instances_;
    CursedCars cursedCars_;
    std::vector<std::optional<Posed>> carCurses_;
    std::vector<render::MeshDraw> draws_;
    std::vector<FarModel> placed_;
};

}
