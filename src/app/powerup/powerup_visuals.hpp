#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/powerup/powerup_models.hpp"
#include "app/racer/animation_player.hpp"
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

private:
    struct Posed {
        const EffectModel* model = nullptr;
        std::size_t part = 0;
        std::optional<sim::SkeletonPose> pose;
        std::optional<AnimationPlayer> player;
    };

    struct Instance {
        sim::PowerupKind kind = sim::PowerupKind::Cannonball;
        sim::PowerupUse use;
        std::optional<std::uint8_t> phase;
        std::vector<Posed> models;
        bool isLive = false;
    };

    [[nodiscard]] Instance& InstanceFor(const sim::EffectSnapshot& effect);
    void Start(Instance& instance, const sim::EffectSnapshot& effect);
    static void Animate(Instance& instance, std::uint8_t phase, float elapsedMs);
    void Place(Posed& posed, const Transform& engine, float scale);
    void PlaceExplosion(const sim::ExplosionSnapshot& explosion);

    const EffectModels* models_;
    std::vector<Instance> instances_;
    std::vector<render::MeshDraw> draws_;
};

}
