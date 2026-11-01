#include "render/particle_bank.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

namespace rr::render {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kChanceScale = 256.0F;
constexpr std::uint32_t kByteMask = 0xFFU;
constexpr float kInt64Limit = 9.2233720368547758e18F;
constexpr Rect kParticleUv{.x = 0.01F, .y = 0.01F, .width = 0.97F, .height = 0.97F};
constexpr Vector3 kCameraUp{.y = 1.0F};
constexpr Vector3 kCameraRight{.x = 1.0F};

float SpawnIntervalMs(const assets::EmitterAsset& emitter) {
    if (emitter.emitRate <= 0.0F) {
        return 0.0F;
    }
    return std::trunc(kMillisecondsPerSecond / emitter.emitRate);
}

std::uint32_t ChanceByte(const assets::EmitterAsset& emitter) {
    const float scaled = emitter.emitChance * kChanceScale;
    if (!(std::abs(scaled) < kInt64Limit)) {
        return 0;
    }
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled)) & kByteMask;
}

float LifeShare(const assets::EmitterAsset& emitter, float ageMs) {
    const auto lifeMs = static_cast<float>(emitter.particleLife.count());
    return lifeMs > 0.0F ? ageMs / lifeMs : 0.0F;
}

}

ParticleBank::ParticleBank(ParticleBankSetup setup)
    : setup_(std::move(setup)), slots_(setup_.slotCount) {}

bool ParticleBank::HasEmitter(std::string_view name) const {
    return std::ranges::contains(setup_.emitters, name, &assets::EmitterAsset::name);
}

std::optional<EmitterHandle> ParticleBank::Start(std::string_view name,
                                                 const EmitterPlacement& placement) {
    const auto found = std::ranges::find(setup_.emitters, name, &assets::EmitterAsset::name);
    if (found == setup_.emitters.end()) {
        return std::nullopt;
    }
    const std::optional<std::size_t> chosen = ChooseSlot(found->priority);
    if (!chosen) {
        return std::nullopt;
    }
    Slot& slot = slots_[*chosen];
    slot = {.emitter = static_cast<std::size_t>(found - setup_.emitters.begin()),
            .generation = slot.generation + 1,
            .isSpawning = true,
            .placement = placement,
            .emitTimerMs = SpawnIntervalMs(*found)};
    if (found->duration) {
        return std::nullopt;
    }
    return EmitterHandle{.slot = static_cast<std::uint32_t>(*chosen),
                         .generation = slot.generation};
}

void ParticleBank::Place(EmitterHandle handle, const EmitterPlacement& placement) {
    if (Slot* slot = Find(handle)) {
        slot->placement = placement;
    }
}

std::size_t ParticleBank::SpawnCount(EmitterHandle handle) const {
    return IsLive(handle) ? slots_[handle.slot].spawnCount : 0;
}

void ParticleBank::Finish(EmitterHandle handle) {
    if (Slot* slot = Find(handle)) {
        slot->isSpawning = false;
        ++slot->generation;
    }
}

void ParticleBank::Release(EmitterHandle handle) {
    if (Slot* slot = Find(handle)) {
        *slot = {.generation = slot->generation + 1};
    }
}

void ParticleBank::StopAll() {
    for (Slot& slot : slots_) {
        slot = {.generation = slot.generation + 1};
    }
}

void ParticleBank::Update(FloatMilliseconds elapsed) {
    for (Slot& slot : slots_) {
        UpdateSlot(slot, elapsed.count());
    }
}

void ParticleBank::AppendQuads(const Transform& camera, std::vector<QuadDraw>& out) const {
    for (const Slot& slot : slots_) {
        AppendSlot(slot, camera, out);
    }
}

bool ParticleBank::IsLive(EmitterHandle handle) const {
    return handle.slot < slots_.size() && slots_[handle.slot].emitter &&
           slots_[handle.slot].generation == handle.generation;
}

ParticleBank::Slot* ParticleBank::Find(EmitterHandle handle) {
    return IsLive(handle) ? &slots_[handle.slot] : nullptr;
}

std::optional<std::size_t> ParticleBank::ChooseSlot(std::uint8_t priority) const {
    std::optional<std::size_t> weakest;
    std::uint8_t weakestPriority = 0;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        const Slot& slot = slots_[i];
        if (!slot.emitter) {
            return i;
        }
        const std::uint8_t held = slot.isSpawning ? setup_.emitters[*slot.emitter].priority : 0;
        if (!weakest || held < weakestPriority) {
            weakest = i;
            weakestPriority = held;
        }
    }
    return weakest && priority > weakestPriority ? weakest : std::nullopt;
}

void ParticleBank::UpdateSlot(Slot& slot, float elapsedMs) {
    if (!slot.emitter) {
        return;
    }
    const assets::EmitterAsset& emitter = setup_.emitters[*slot.emitter];
    const float seconds = elapsedMs / kMillisecondsPerSecond;
    const Vector3 drift = emitter.acceleration * (0.5F * seconds * seconds);
    bool isAnyAlive = false;
    for (Particle& particle : slot.particles | std::views::filter(&Particle::isAlive)) {
        particle.ageMs += elapsedMs;
        if (particle.ageMs > static_cast<float>(emitter.particleLife.count())) {
            particle.isAlive = false;
            continue;
        }
        particle.position += (particle.velocity * seconds) + drift;
        particle.velocity += emitter.acceleration * seconds;
        isAnyAlive = true;
    }
    if (slot.isSpawning) {
        slot.ageMs += elapsedMs;
        slot.emitTimerMs += elapsedMs;
        slot.isSpawning =
            !emitter.duration || slot.ageMs < static_cast<float>(emitter.duration->count());
    }
    if (!slot.isSpawning) {
        if (!isAnyAlive) {
            slot = {.generation = slot.generation + 1};
        }
        return;
    }
    const float intervalMs = SpawnIntervalMs(emitter);
    if (intervalMs <= 0.0F) {
        Spawn(slot);
        return;
    }
    for (std::size_t spawned = 0; spawned < kParticlesPerSlot && slot.emitTimerMs >= intervalMs;
         ++spawned) {
        slot.emitTimerMs -= intervalMs;
        Spawn(slot);
    }
    slot.emitTimerMs = std::min(slot.emitTimerMs, intervalMs);
}

void ParticleBank::Spawn(Slot& slot) {
    const assets::EmitterAsset& emitter = setup_.emitters[*slot.emitter];
    if ((NextRandom() & kByteMask) < ChanceByte(emitter)) {
        return;
    }
    const Vector3& point = emitter.points[NextRandom() % emitter.points.size()];
    const auto free = std::ranges::find(slot.particles, false, &Particle::isAlive);
    Particle& particle = free != slot.particles.end()
                             ? *free
                             : *std::ranges::max_element(slot.particles, {}, &Particle::ageMs);
    particle = {.position = slot.placement.position,
                .velocity = Rotate(slot.placement.rotation, point) + slot.placement.velocity,
                .isAlive = true};
    ++slot.spawnCount;
    if (!slot.anchor) {
        slot.anchor = particle.position;
    }
}

void ParticleBank::AppendSlot(const Slot& slot, const Transform& camera,
                              std::vector<QuadDraw>& out) const {
    if (!slot.emitter || !slot.anchor) {
        return;
    }
    const std::size_t index = *slot.emitter;
    const assets::EmitterAsset& emitter = setup_.emitters[index];
    const Vector3 toCamera = camera.position - *slot.anchor;
    if (Dot(toCamera, toCamera) > emitter.radius * emitter.radius) {
        return;
    }
    const Vector3 up = Rotate(camera.rotation, kCameraUp) * 0.5F;
    const Vector3 right = Rotate(camera.rotation, kCameraRight) * 0.5F;
    for (const Particle& particle : slot.particles | std::views::filter(&Particle::isAlive)) {
        const ParticleLook* look = LookAt(index, particle.ageMs);
        const float share = LifeShare(emitter, particle.ageMs);
        const float height = emitter.startSizeUp + (emitter.sizeGrowthUp * share);
        const float width = emitter.startSizeForward + (emitter.sizeGrowthForward * share);
        if (look == nullptr || height <= 0.0F || width <= 0.0F) {
            continue;
        }
        const Vector3 top = up * height;
        const Vector3 side = right * width;
        const Vector3& at = particle.position;
        out.push_back(
            {.texture = look->texture,
             .corners = {at + top - side, at + top + side, at - top - side, at - top + side},
             .uv = kParticleUv,
             .tint = look->tint,
             .blend = look->blend});
    }
}

const ParticleLook* ParticleBank::LookAt(std::size_t emitter, float ageMs) const {
    const std::optional<std::size_t> track = setup_.emitters[emitter].materialTrack;
    const std::optional<ParticleLook>* look = nullptr;
    if (track) {
        if (*track >= setup_.animation.tracks.size()) {
            return nullptr;
        }
        const std::size_t key = assets::SampleTrack(setup_.animation, *track,
                                                    Milliseconds{static_cast<std::int64_t>(ageMs)});
        look = key < setup_.keyLooks.size() ? &setup_.keyLooks[key] : nullptr;
    } else {
        look = emitter < setup_.emitterLooks.size() ? &setup_.emitterLooks[emitter] : nullptr;
    }
    return look != nullptr && look->has_value() ? &**look : nullptr;
}

std::uint32_t ParticleBank::NextRandom() {
    random_ ^= random_ << 13U;
    random_ ^= random_ >> 17U;
    random_ ^= random_ << 5U;
    return random_;
}

}
