#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/fx_effect.hpp"
#include "render/particle_bank.hpp"
#include "render/render_view.hpp"

namespace rr::render {

inline constexpr std::size_t kFxCapacity = 4096;
inline constexpr float kFxDrawDistance = 600.0F;
/// An emitter that moves further than this in a frame jumped, so nothing spawns along the gap.
inline constexpr float kFxMaxEmitStep = 30.0F;

/// Effects drawn in place of original emitters, from textures made in code, on one shared pool.
/// Spawns past `capacity` are dropped. Draws randomness from its own stream.
class FxSystem {
public:
    FxSystem(std::vector<FxEffect> effects, FxAssets assets, std::size_t capacity = kFxCapacity);

    /// The first of the layers that name `emitter`.
    [[nodiscard]] std::optional<FxEffectId> EffectFor(std::string_view emitter) const;

    /// Spawns this frame's share along the path from the emitter's last position to `placement`.
    void Emit(FxEffectId effect, const EmitterPlacement& placement, float elapsedMs,
              FxEmission& emission);
    /// Spawns only the effect's bursts.
    void Burst(FxEffectId effect, const EmitterPlacement& placement);
    void Update(FloatMilliseconds elapsed);
    void Clear();

    [[nodiscard]] std::size_t Count() const;

    /// Back to front, leaving out particles past `kFxDrawDistance`.
    void AppendQuads(const Transform& camera, std::vector<QuadDraw>& out) const;
    void AppendMeshes(std::vector<MeshDraw>& out) const;

private:
    struct Particle {
        Vector3 position;
        Vector3 velocity;
        float ageMs = 0.0F;
        float lifeMs = 0.0F;
        float startSize = 0.0F;
        float endSize = 0.0F;
        float angle = 0.0F;
        float spin = 0.0F;
        float floor = 0.0F;
        Vector3 axis;
        std::uint16_t effect = 0;
        std::uint8_t variant = 0;
        std::uint8_t paint = 0;
    };

    struct View {
        Vector3 position;
        Vector3 up;
        Vector3 right;
    };

    void EmitLayer(std::uint16_t layer, float& carry, const Vector3& from,
                   const EmitterPlacement& placement, float elapsedMs);
    void Spawn(std::uint16_t effect, const Vector3& at, const EmitterPlacement& placement,
               float ageMs);
    [[nodiscard]] QuadDraw QuadOf(const Particle& particle, const View& view) const;
    [[nodiscard]] Color ColorAt(const Particle& particle, float share) const;
    [[nodiscard]] TextureId TextureOf(FxShape shape) const;
    [[nodiscard]] Vector3 RandomAxis();
    [[nodiscard]] float Roll(FxRange range);
    [[nodiscard]] float Unit();

    std::vector<FxEffect> effects_;
    FxAssets assets_;
    std::size_t capacity_;
    std::vector<Particle> particles_;
    mutable std::vector<std::pair<float, std::uint32_t>> order_;
    std::uint32_t random_ = 0x2545F491U;
};

}
