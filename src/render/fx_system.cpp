#include "render/fx_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <ranges>

#include "render/fx_textures.hpp"

namespace rr::render {
namespace {

constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kRandomScale = 1.0F / 16777216.0F;
constexpr Vector3 kCameraUp{.y = 1.0F};
constexpr Vector3 kCameraRight{.x = 1.0F};
constexpr Vector3 kTowardLight = -kDefaultLight.direction;

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

Color Lit(const Color& color, float shade, const Vector3& offset) {
    const float facing = Dot(Normalized(offset, kCameraUp), kTowardLight);
    const float light = std::lerp(shade, 1.0F, 0.5F + (0.5F * facing));
    return {.r = color.r * light, .g = color.g * light, .b = color.b * light};
}

}

FxSystem::FxSystem(std::vector<FxEffect> effects, TextureId smoke, std::size_t capacity)
    : effects_(std::move(effects)), smoke_(smoke), capacity_(capacity) {
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
    const auto index = static_cast<std::uint16_t>(effect);
    const FxEffect& definition = effects_.at(index);
    const Vector3 from = emission.lastPosition.value_or(placement.position);
    emission.lastPosition = placement.position;
    if (!emission.hasBurst) {
        emission.hasBurst = true;
        for (int spawned = 0; spawned < definition.burst; ++spawned) {
            Spawn(index, placement.position, placement, 0.0F);
        }
    }
    emission.carry += definition.ratePerSecond * elapsedMs / kMillisecondsPerSecond;
    const int steady = static_cast<int>(emission.carry);
    emission.carry -= static_cast<float>(steady);
    for (int spawned = 0; spawned < steady; ++spawned) {
        const float share = (static_cast<float>(spawned) + Unit()) / static_cast<float>(steady);
        Spawn(index, Between(from, placement.position, share), placement,
              (1.0F - share) * elapsedMs);
    }
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
    const Vector3 up = Rotate(camera.rotation, kCameraUp);
    const Vector3 right = Rotate(camera.rotation, kCameraRight);
    order_.clear();
    for (std::uint32_t i = 0; i < particles_.size(); ++i) {
        const Vector3 toCamera = particles_[i].position - camera.position;
        order_.emplace_back(Dot(toCamera, toCamera), i);
    }
    std::ranges::sort(order_, std::ranges::greater{});
    for (const std::uint32_t index : order_ | std::views::values) {
        out.push_back(QuadOf(particles_[index], up, right));
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
    const Vector3 velocity = (placement.velocity * definition.inherit) + jitter;
    const float spin = Roll(definition.spinPerSecond) * (Unit() < 0.5F ? -1.0F : 1.0F);
    particles_.push_back(
        {.position = at + (velocity * (ageMs / kMillisecondsPerSecond)),
         .velocity = velocity,
         .ageMs = ageMs,
         .lifeMs = Roll(definition.lifeMs),
         .startSize = Roll(definition.startSize),
         .endSize = Roll(definition.endSize),
         .angle = Unit() * std::numbers::pi_v<float> * 2.0F,
         .spin = spin,
         .effect = effect,
         .variant = static_cast<std::uint8_t>(Unit() * static_cast<float>(kSmokeVariants))});
}

QuadDraw FxSystem::QuadOf(const Particle& particle, const Vector3& up, const Vector3& right) const {
    const FxEffect& effect = effects_[particle.effect];
    const float share = particle.lifeMs > 0.0F ? particle.ageMs / particle.lifeMs : 1.0F;
    const float half = 0.5F * std::lerp(particle.startSize, particle.endSize, GrowthOf(share));
    const float cos = std::cos(particle.angle) * half;
    const float sin = std::sin(particle.angle) * half;
    const Vector3 across = (right * cos) + (up * sin);
    const Vector3 along = (up * cos) - (right * sin);
    const Vector3& at = particle.position;
    const std::array<Vector3, 4> offsets{along - across, along + across, -along - across,
                                         -along + across};
    QuadDraw quad{
        .texture = smoke_,
        .uv = SmokeCell(particle.variant),
        .tint = {.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = effect.color.a * FadeOf(effect, share)},
        .blend = effect.blend};
    for (std::size_t corner = 0; corner < offsets.size(); ++corner) {
        quad.corners.at(corner) = at + offsets.at(corner);
        quad.cornerTints.at(corner) = Lit(effect.color, effect.shade, offsets.at(corner));
    }
    return quad;
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
