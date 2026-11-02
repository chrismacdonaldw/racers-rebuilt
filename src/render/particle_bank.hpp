#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/emitter_library.hpp"
#include "assets/material_animation.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::render {

using FloatMilliseconds = std::chrono::duration<float, std::milli>;

inline constexpr std::size_t kParticlesPerSlot = 16;

struct ParticleLook {
    TextureId texture = kWhiteTexture;
    Color tint;
    BlendMode blend = BlendMode::Alpha;
};

/// Emitters and `animation` are in world space. A look that is absent hides its particles.
struct ParticleBankSetup {
    std::vector<assets::EmitterAsset> emitters;
    std::vector<std::optional<ParticleLook>> emitterLooks;
    assets::MaterialAnimationAsset animation;
    std::vector<std::optional<ParticleLook>> keyLooks;
    std::size_t slotCount = 0;
};

/// `rotation` turns spawn velocities, and `velocity` in units per second adds to every spawn.
struct EmitterPlacement {
    Vector3 position;
    Quaternion rotation;
    Vector3 velocity;
};

/// Goes stale when its slot ends or is taken, and calls with it then do nothing.
struct EmitterHandle {
    std::uint32_t slot = 0;
    std::uint32_t generation = 0;

    friend constexpr bool operator==(const EmitterHandle&, const EmitterHandle&) = default;
};

/// One loaded `EMB` on a fixed budget, drawing randomness from its own stream and never the sim's.
class ParticleBank {
public:
    explicit ParticleBank(ParticleBankSetup setup);

    [[nodiscard]] bool HasEmitter(std::string_view name) const;

    /// Only an emitter that runs until stopped returns a handle.
    std::optional<EmitterHandle> Start(std::string_view name, const EmitterPlacement& placement);

    void Place(EmitterHandle handle, const EmitterPlacement& placement);
    [[nodiscard]] bool IsLive(EmitterHandle handle) const;
    [[nodiscard]] std::size_t SpawnCount(EmitterHandle handle) const;

    /// Stops spawning and lets the particles live out their lives.
    void Finish(EmitterHandle handle);

    /// Removes the particles at once.
    void Release(EmitterHandle handle);
    void StopAll();

    void Update(FloatMilliseconds elapsed);

    /// Camera-facing, sized along the camera's up and right axes.
    void AppendQuads(const Transform& camera, std::vector<QuadDraw>& out) const;

private:
    struct Particle {
        Vector3 position;
        Vector3 velocity;
        float ageMs = 0.0F;
        bool isAlive = false;
    };

    struct Slot {
        std::optional<std::size_t> emitter;
        std::uint32_t generation = 0;
        bool isSpawning = false;
        EmitterPlacement placement;
        float emitTimerMs = 0.0F;
        float ageMs = 0.0F;
        std::size_t spawnCount = 0;
        std::optional<Vector3> anchor;
        std::array<Particle, kParticlesPerSlot> particles{};
    };

    [[nodiscard]] Slot* Find(EmitterHandle handle);
    [[nodiscard]] std::optional<std::size_t> ChooseSlot(std::uint8_t priority) const;
    void UpdateSlot(Slot& slot, float elapsedMs);
    void Spawn(Slot& slot);
    void AppendSlot(const Slot& slot, const Transform& camera, std::vector<QuadDraw>& out) const;
    [[nodiscard]] const ParticleLook* LookAt(std::size_t emitter, float ageMs) const;
    std::uint32_t NextRandom();

    ParticleBankSetup setup_;
    std::vector<Slot> slots_;
    std::uint32_t random_ = 0x9E3779B9U;
};

}
