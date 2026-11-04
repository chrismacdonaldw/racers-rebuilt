#include "render/fx_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <ranges>

#include "core/quaternion.hpp"
#include "render/fx_textures.hpp"

namespace rr::render {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kRandomScale = 1.0F / 16777216.0F;
constexpr Vector3 kCameraUp{.y = 1.0F};
constexpr Vector3 kCameraRight{.x = 1.0F};
constexpr Vector3 kTowardLight = -kDefaultLight.direction;
constexpr float kBounceFriction = 0.7F;
constexpr Rect kWholeTexture{.width = 1.0F, .height = 1.0F};
constexpr Vector3 kGroundRight{.x = 1.0F};
constexpr Vector3 kGroundAhead{.z = 1.0F};
constexpr float kBrickAmbientShare = 0.5F;

float SmoothStep(float x) {
    const float t = std::clamp(x, 0.0F, 1.0F);
    return t * t * (3.0F - (2.0F * t));
}

float FadeOf(const FxEffect& effect, float share) {
    const float in = effect.fadeInShare > 0.0F ? SmoothStep(share / effect.fadeInShare) : 1.0F;
    const float out =
        effect.fadeOutShare > 0.0F ? SmoothStep((1.0F - share) / effect.fadeOutShare) : 1.0F;
    return in * out;
}

float GrowthOf(float share) {
    const float left = 1.0F - share;
    return 1.0F - (left * left);
}

Vector3 Between(const Vector3& from, const Vector3& to, float share) {
    return from + ((to - from) * share);
}

Color Mix(const Color& from, const Color& to, float share) {
    return {.r = std::lerp(from.r, to.r, share),
            .g = std::lerp(from.g, to.g, share),
            .b = std::lerp(from.b, to.b, share),
            .a = std::lerp(from.a, to.a, share)};
}

Color Lit(const Color& color, float shade, const Vector3& offset) {
    const float facing = Dot(Normalized(offset, kCameraUp), kTowardLight);
    const float light = std::lerp(shade, 1.0F, 0.5F + (0.5F * facing));
    return {.r = color.r * light, .g = color.g * light, .b = color.b * light};
}

std::array<Vector3, 4> Corners(const Vector3& at, const Vector3& along, const Vector3& across) {
    return {at + along - across, at + along + across, at - along - across, at - along + across};
}

void Bounce(Vector3& position, Vector3& velocity, float floor, float bounce) {
    if (bounce <= 0.0F || position.y >= floor || velocity.y >= 0.0F) {
        return;
    }
    position.y = floor;
    velocity = {.x = velocity.x * kBounceFriction,
                .y = -velocity.y * bounce,
                .z = velocity.z * kBounceFriction};
}

}

FxSystem::FxSystem(std::vector<FxEffect> effects, FxAssets assets, std::size_t capacity)
    : effects_(std::move(effects)), assets_(assets), capacity_(capacity) {
    particles_.reserve(capacity_);
    order_.reserve(capacity_);
}

std::optional<FxEffectId> FxSystem::EffectFor(std::string_view emitter) const {
    const auto found = std::ranges::find(effects_, emitter, &FxEffect::emitter);
    if (found == effects_.end()) {
        return std::nullopt;
    }
    return static_cast<FxEffectId>(found - effects_.begin());
}

void FxSystem::Emit(FxEffectId effect, const EmitterPlacement& placement, float elapsedMs,
                    FxEmission& emission) {
    const auto first = static_cast<std::size_t>(effect);
    const std::string_view emitter = effects_.at(first).emitter;
    Vector3 from = emission.lastPosition.value_or(placement.position);
    if (Length(placement.position - from) > kFxMaxEmitStep) {
        from = placement.position;
    }
    emission.lastPosition = placement.position;
    const bool bursts = !emission.hasBurst;
    emission.hasBurst = true;
    for (std::size_t layer = 0; layer < kMaxFxLayers && first + layer < effects_.size() &&
                                effects_[first + layer].emitter == emitter;
         ++layer) {
        const auto index = static_cast<std::uint16_t>(first + layer);
        for (int spawned = 0; bursts && spawned < effects_[index].burst; ++spawned) {
            Spawn(index, placement.position, placement, 0.0F);
        }
        EmitLayer(index, emission.carry.at(layer), from, placement, elapsedMs);
    }
}

void FxSystem::Burst(FxEffectId effect, const EmitterPlacement& placement) {
    FxEmission once;
    Emit(effect, placement, 0.0F, once);
}

void FxSystem::Update(FloatMilliseconds elapsed) {
    const float ms = elapsed.count();
    const float seconds = ms / kMillisecondsPerSecond;
    std::erase_if(particles_, [&](Particle& particle) {
        particle.ageMs += ms;
        if (particle.ageMs >= particle.lifeMs) {
            return true;
        }
        const FxEffect& effect = effects_[particle.effect];
        particle.velocity = particle.velocity * std::max(0.0F, 1.0F - (effect.drag * seconds));
        particle.velocity += effect.acceleration * seconds;
        particle.position += particle.velocity * seconds;
        particle.angle += particle.spin * seconds;
        Bounce(particle.position, particle.velocity, particle.floor, effect.bounce);
        return false;
    });
}

void FxSystem::Clear() {
    particles_.clear();
}

std::size_t FxSystem::Count() const {
    return particles_.size();
}

void FxSystem::AppendQuads(const Transform& camera, std::vector<QuadDraw>& out) const {
    const View view{.position = camera.position,
                    .up = Rotate(camera.rotation, kCameraUp),
                    .right = Rotate(camera.rotation, kCameraRight)};
    order_.clear();
    for (std::uint32_t i = 0; i < particles_.size(); ++i) {
        if (effects_[particles_[i].effect].shape == FxShape::Brick) {
            continue;
        }
        const Vector3 toCamera = particles_[i].position - camera.position;
        const float distance = Dot(toCamera, toCamera);
        if (distance < kFxDrawDistance * kFxDrawDistance) {
            order_.emplace_back(distance, i);
        }
    }
    std::ranges::sort(order_, std::ranges::greater{});
    for (const std::uint32_t index : order_ | std::views::values) {
        out.push_back(QuadOf(particles_[index], view));
    }
}

void FxSystem::AppendMeshes(std::vector<MeshDraw>& out) const {
    for (const Particle& particle : particles_) {
        const FxEffect& effect = effects_[particle.effect];
        if (effect.shape != FxShape::Brick) {
            continue;
        }
        const float share = particle.lifeMs > 0.0F ? particle.ageMs / particle.lifeMs : 1.0F;
        const Color paint = ColorAt(particle, share);
        const Color ambient{.r = paint.r * kBrickAmbientShare,
                            .g = paint.g * kBrickAmbientShare,
                            .b = paint.b * kBrickAmbientShare};
        out.push_back({.mesh = assets_.brick,
                       .material = {.tint = paint, .ambient = ambient},
                       .transform = {.position = particle.position,
                                     .rotation = FromAxisAngle(particle.axis, particle.angle)},
                       .scale = particle.startSize * FadeOf(effect, share)});
    }
}

void FxSystem::EmitLayer(std::uint16_t layer, float& carry, const Vector3& from,
                         const EmitterPlacement& placement, float elapsedMs) {
    carry += effects_[layer].ratePerSecond * elapsedMs / kMillisecondsPerSecond;
    const int steady = static_cast<int>(carry);
    carry -= static_cast<float>(steady);
    for (int spawned = 0; spawned < steady; ++spawned) {
        const float share = (static_cast<float>(spawned) + Unit()) / static_cast<float>(steady);
        Spawn(layer, Between(from, placement.position, share), placement,
              (1.0F - share) * elapsedMs);
    }
}

void FxSystem::Spawn(std::uint16_t effect, const Vector3& at, const EmitterPlacement& placement,
                     float ageMs) {
    if (particles_.size() >= capacity_) {
        return;
    }
    const FxEffect& definition = effects_[effect];
    const Vector3 jitter{.x = definition.spread.x * ((Unit() * 2.0F) - 1.0F),
                         .y = definition.spread.y * ((Unit() * 2.0F) - 1.0F),
                         .z = definition.spread.z * ((Unit() * 2.0F) - 1.0F)};
    const Vector3 velocity = (placement.velocity * definition.inherit) + definition.launch + jitter;
    const float spin = Roll(definition.spinPerSecond) * (Unit() < 0.5F ? -1.0F : 1.0F);
    const Vector3 start = at + definition.offset;
    particles_.push_back(
        {.position = start + (velocity * (ageMs / kMillisecondsPerSecond)),
         .velocity = velocity,
         .ageMs = ageMs,
         .lifeMs = Roll(definition.lifeMs),
         .startSize = Roll(definition.startSize),
         .endSize = Roll(definition.endSize),
         .angle = Unit() * std::numbers::pi_v<float> * 2.0F,
         .spin = spin,
         .floor = start.y - definition.floorDrop,
         .axis = RandomAxis(),
         .effect = effect,
         .variant = static_cast<std::uint8_t>(Unit() * static_cast<float>(kSmokeVariants)),
         .paint =
             static_cast<std::uint8_t>(Unit() * static_cast<float>(definition.palette.size()))});
}

QuadDraw FxSystem::QuadOf(const Particle& particle, const View& view) const {
    const FxEffect& effect = effects_[particle.effect];
    const float share = particle.lifeMs > 0.0F ? particle.ageMs / particle.lifeMs : 1.0F;
    const float half = 0.5F * std::lerp(particle.startSize, particle.endSize, GrowthOf(share));
    const Color color = ColorAt(particle, share);
    QuadDraw quad{.tint = {.a = color.a * FadeOf(effect, share)}, .blend = effect.blend};
    const Color plain{.r = color.r, .g = color.g, .b = color.b};
    quad.cornerTints = {plain, plain, plain, plain};
    if (effect.shape == FxShape::Streak) {
        const float speed = Length(particle.velocity);
        const Vector3 along = Normalized(particle.velocity, view.up);
        const Vector3 across =
            Normalized(Cross(along, view.position - particle.position), view.right);
        const Vector3 head = particle.position;
        const float length = std::max(half * 2.0F, speed * effect.streakSeconds);
        quad.texture = assets_.spark;
        quad.uv = kWholeTexture;
        quad.corners =
            Corners(head - (along * (0.5F * length)), along * (0.5F * length), across * half);
        return quad;
    }
    const bool isFlat = effect.shape == FxShape::Ring;
    const Vector3 right = isFlat ? kGroundRight : view.right;
    const Vector3 up = isFlat ? kGroundAhead : view.up;
    const float cos = std::cos(particle.angle) * half;
    const float sin = std::sin(particle.angle) * half;
    const Vector3 across = (right * cos) + (up * sin);
    const Vector3 along = (up * cos) - (right * sin);
    quad.corners = Corners(particle.position, along, across);
    if (effect.shape != FxShape::Puff) {
        quad.texture = isFlat ? assets_.ring : assets_.glow;
        quad.uv = kWholeTexture;
        return quad;
    }
    quad.texture = assets_.smoke;
    quad.uv = SmokeCell(particle.variant);
    const std::array<Vector3, 4> offsets{along - across, along + across, -along - across,
                                         -along + across};
    for (std::size_t corner = 0; corner < offsets.size(); ++corner) {
        quad.cornerTints.at(corner) = Lit(plain, effect.shade, offsets.at(corner));
    }
    return quad;
}

Color FxSystem::ColorAt(const Particle& particle, float share) const {
    const FxEffect& effect = effects_[particle.effect];
    Color start = effect.color;
    if (particle.paint < effect.palette.size()) {
        const Color& paint = effect.palette[particle.paint];
        start = {.r = paint.r, .g = paint.g, .b = paint.b, .a = start.a};
    }
    return effect.endColor ? Mix(start, *effect.endColor, share) : start;
}

Vector3 FxSystem::RandomAxis() {
    const Vector3 axis{
        .x = (Unit() * 2.0F) - 1.0F, .y = (Unit() * 2.0F) - 1.0F, .z = (Unit() * 2.0F) - 1.0F};
    return Normalized(axis, kCameraUp);
}

float FxSystem::Roll(FxRange range) {
    return std::lerp(range.min, range.max, Unit());
}

float FxSystem::Unit() {
    random_ ^= random_ << 13U;
    random_ ^= random_ >> 17U;
    random_ ^= random_ << 5U;
    return static_cast<float>(random_ >> 8U) * kRandomScale;
}

}
