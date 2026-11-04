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

/// Effects drawn in place of original emitters, from textures made in code, on one shared pool.
/// Spawns past `capacity` are dropped. Draws randomness from its own stream.
class FxSystem {
public:
    FxSystem(std::vector<FxEffect> effects, TextureId smoke, std::size_t capacity = kFxCapacity);

    [[nodiscard]] std::optional<FxEffectId> EffectFor(std::string_view emitter) const;

    /// Spawns this frame's share along the path from the emitter's last position to `placement`.
    void Emit(FxEffectId effect, const EmitterPlacement& placement, float elapsedMs,
              FxEmission& emission);
    void Update(FloatMilliseconds elapsed);
    void Clear();

    [[nodiscard]] std::size_t Count() const;

    /// Camera-facing, turned by each particle's spin, back to front.
    void AppendQuads(const Transform& camera, std::vector<QuadDraw>& out) const;

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
        std::uint16_t effect = 0;
        std::uint8_t variant = 0;
    };

    void Spawn(std::uint16_t effect, const Vector3& at, const EmitterPlacement& placement,
               float ageMs);
    [[nodiscard]] QuadDraw QuadOf(const Particle& particle, const Vector3& up,
                                  const Vector3& right) const;
    [[nodiscard]] float Roll(FxRange range);
    [[nodiscard]] float Unit();

    std::vector<FxEffect> effects_;
    TextureId smoke_;
    std::size_t capacity_;
    std::vector<Particle> particles_;
    mutable std::vector<std::pair<float, std::uint32_t>> order_;
    std::uint32_t random_ = 0x2545F491U;
};

}
