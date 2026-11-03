#include "app/racer/car_particles.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kDust = "dust";
constexpr std::string_view kExhaust = "carsmke";
constexpr std::string_view kTireSmoke = "tiresmk";
constexpr std::string_view kLanding = "carland";
constexpr std::string_view kSparks = "carsprk";
constexpr std::size_t kDustPuffs = 10;
constexpr std::size_t kExhaustPuffs = 4;
constexpr std::size_t kRearRight = 2;
constexpr std::size_t kRearLeft = 3;
constexpr int kFallMs = 400;
constexpr Vector3 kExhaustRise{.z = 2.0F};

Vector3 WheelOf(const sim::Racer& racer, std::size_t wheel) {
    return racer.Body().motion.wheels.at(wheel).position;
}

render::EmitterPlacement At(render::EmitterPlacement placement, const Vector3& position) {
    placement.position = ToEngine(position);
    return placement;
}

Vector3 ExhaustOf(const sim::Racer& racer) {
    return ((WheelOf(racer, kRearRight) + WheelOf(racer, kRearLeft)) * 0.5F) + kExhaustRise;
}

std::optional<std::size_t> SmokingWheel(const SkidWheels& wheels, bool isFirst) {
    const auto first = std::ranges::find(wheels, true);
    if (first == wheels.end()) {
        return std::nullopt;
    }
    if (isFirst) {
        return static_cast<std::size_t>(std::distance(wheels.begin(), first));
    }
    const auto last = std::ranges::find(wheels.rbegin(), wheels.rend(), true);
    return wheels.size() - 1 - static_cast<std::size_t>(std::distance(wheels.rbegin(), last));
}

}

void CarParticles::Tick(std::span<const sim::Racer> racers, render::ParticleBank& bank) {
    landings_.resize(racers.size());
    for (std::size_t i = 0; i < racers.size(); ++i) {
        const sim::Racer& racer = racers[i];
        if (const std::optional<Vector3>& point = racer.Events().sparkPoint) {
            bank.Start(kSparks, {.position = ToEngine(*point)});
        }
        Landing& landing = landings_[i];
        const sim::BodyState& body = racer.Body();
        landing.isFalling = landing.isFalling || body.airborneMs > kFallMs;
        if (landing.isFalling && body.motion.contactCount > landing.contacts) {
            landing.isFalling = false;
            bank.Start(kLanding, {.position = TransformToEngine(racer.Pose()).position});
        }
        landing.contacts = body.motion.contactCount;
    }
}

void CarParticles::Update(const sim::Racer& player, Puffs& puffs,
                          const std::optional<SkidWheels>& skidding, bool isRearKicking,
                          const Vector3& velocity, render::ParticleBank& bank) {
    const render::EmitterPlacement moving{.rotation = TransformToEngine(player.Pose()).rotation,
                                          .velocity = velocity};
    StartPuffs(player, puffs, isRearKicking, moving, bank);
    const std::size_t dustWheel = random_.Next() < 0.5F ? kRearLeft : kRearRight;
    Puff(puffs.dust, kDustPuffs, At(moving, WheelOf(player, dustWheel)), bank);
    Puff(puffs.exhaust, kExhaustPuffs, At(moving, ExhaustOf(player)), bank);
    UpdateTireSmoke(player, puffs, skidding, moving, bank);
}

void CarParticles::StartPuffs(const sim::Racer& player, Puffs& puffs, bool isRearKicking,
                              const render::EmitterPlacement& moving, render::ParticleBank& bank) {
    const bool hasTurboStarted = player.TurboCount() != puffs.turboCount;
    const bool hasDriftStarted = player.Driver().isDrifting && !puffs.wasDrifting;
    puffs.turboCount = player.TurboCount();
    puffs.wasDrifting = player.Driver().isDrifting;
    if ((hasTurboStarted || hasDriftStarted) && !puffs.dust && !isRearKicking) {
        puffs.dust = bank.Start(kDust, At(moving, WheelOf(player, kRearLeft)));
    }
    if (hasTurboStarted && !puffs.exhaust) {
        puffs.exhaust = bank.Start(kExhaust, At(moving, ExhaustOf(player)));
    }
}

void CarParticles::UpdateTireSmoke(const sim::Racer& player, Puffs& puffs,
                                   const std::optional<SkidWheels>& skidding,
                                   const render::EmitterPlacement& moving,
                                   render::ParticleBank& bank) {
    if (!skidding) {
        if (puffs.tireSmoke) {
            bank.Finish(*puffs.tireSmoke);
            puffs.tireSmoke.reset();
        }
        return;
    }
    const std::optional<std::size_t> wheel = SmokingWheel(*skidding, random_.Next() < 0.5F);
    if (!wheel) {
        return;
    }
    const render::EmitterPlacement placement = At(moving, WheelOf(player, *wheel));
    if (puffs.tireSmoke) {
        bank.Place(*puffs.tireSmoke, placement);
    } else {
        puffs.tireSmoke = bank.Start(kTireSmoke, placement);
    }
}

void CarParticles::Puff(std::optional<render::EmitterHandle>& handle, std::size_t puffs,
                        const render::EmitterPlacement& placement, render::ParticleBank& bank) {
    if (!handle) {
        return;
    }
    if (!bank.IsLive(*handle) || bank.SpawnCount(*handle) >= puffs) {
        bank.Finish(*handle);
        handle.reset();
        return;
    }
    bank.Place(*handle, placement);
}

}
