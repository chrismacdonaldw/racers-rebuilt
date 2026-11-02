#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "app/powerup/effect_trail.hpp"
#include "app/quad_shapes.hpp"
#include "assets/track_loader.hpp"
#include "core/transform.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

[[nodiscard]] std::span<const std::string_view> PowerupQuadMaterials();

// NOLINTNEXTLINE(bugprone-exception-escape)
class PowerupQuads {
public:
    explicit PowerupQuads(const assets::NameMap<render::Material>& materials);

    /// `camera` is in engine space.
    void Update(const sim::RenderSnapshot& snapshot, const Transform& camera, float elapsedMs);

    [[nodiscard]] std::span<const render::QuadDraw> Quads() const { return quads_; }

private:
    struct SlotTrail {
        sim::PowerupKind kind = sim::PowerupKind::Cannonball;
        EffectTrail trail;
        bool isLive = false;
    };

    [[nodiscard]] const QuadLook* Find(std::string_view material) const;
    void AppendEffect(const sim::EffectSnapshot& effect, const sim::RenderSnapshot& snapshot,
                      const Transform& camera, float elapsedMs);
    void AppendTrail(const sim::EffectSnapshot& effect, std::string_view material,
                     const TrailStyle& style, float elapsedMs);
    void AppendRope(const sim::EffectSnapshot& effect, const Vector3& owner,
                    const Transform& camera);
    void AppendBeam(const sim::EffectSnapshot& effect, const Vector3& owner, std::uint64_t tick,
                    const Transform& camera);
    void AppendExplosion(const sim::ExplosionSnapshot& explosion, const Transform& camera);

    assets::NameMap<QuadLook> looks_;
    std::vector<SlotTrail> trails_;
    std::vector<render::QuadDraw> quads_;
    std::vector<RibbonPoint> path_;
};

}
