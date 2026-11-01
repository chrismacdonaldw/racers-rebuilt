#include "app/powerup/powerup_visuals.hpp"

#include <array>
#include <span>
#include <string_view>
#include <utility>

#include "app/powerup/warp_start.hpp"
#include "app/track_space.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/powerup/curse_effect.hpp"
#include "sim/powerup/magnet_effect.hpp"
#include "sim/powerup/warp_effect.hpp"

namespace rr::app {
namespace {

using sim::PowerupKind;

constexpr std::size_t kMostModels = 3;
using ModelNames = std::array<std::string_view, kMostModels>;

constexpr std::array<ModelNames, 4> kShieldModels{
    ModelNames{"SHIELD0", "SHLDIN0"}, ModelNames{"SHIELD1", "SHLDIN1"},
    ModelNames{"SHIELD2", "SHLDIN2"}, ModelNames{"SHIELD3", "SHLDIN3"}};
constexpr std::array<ModelNames, 3> kTurboModels{ModelNames{"TURBOL0", "TURB0F1", "TURB0F2"},
                                                 ModelNames{"TURBOL1", "TURB1F1", "TURB1F2"},
                                                 ModelNames{"TURBOL2", "TURB2F1", "TURB2F2"}};
constexpr std::array<std::string_view, 3> kTurboParts{"fireup", "sustain", "dieout"};
constexpr std::array<std::string_view, 3> kMissileParts{"right", "middle", "left"};
constexpr std::string_view kBlastModel = "EXPLSN";
constexpr std::string_view kSpikeModel = "SPIKEXP";
constexpr float kMagnetFadeMs = 1000.0F;
constexpr float kMagnetRingFadeAlpha = 127.0F / 255.0F;

ModelNames NamesFor(PowerupKind kind, std::uint8_t level) {
    switch (kind) {
        case PowerupKind::Shield:
            return kShieldModels.at(level);
        case PowerupKind::Turbo:
            return kTurboModels.at(level);
        case PowerupKind::Warp:
            return {"WARPPRT"};
        case PowerupKind::Missile:
            return {"DMISSIL"};
        case PowerupKind::GrapplingHook:
            return {"GRAPPLE"};
        case PowerupKind::Dynamite:
            return {"BARREL"};
        case PowerupKind::Magnet:
            return {"MAGNET", "MAGRING"};
        case PowerupKind::Curse:
            return {"CURSE", "CGREEN", "CGREEN2"};
        default:
            return {};
    }
}

struct PartChoice {
    std::string_view name;
    bool loops = true;
};

PartChoice PartFor(const sim::EffectSnapshot& effect) {
    if (effect.kind == PowerupKind::Turbo) {
        const auto phase = static_cast<sim::TurboPhase>(effect.phase);
        return {.name = kTurboParts.at(effect.phase), .loops = phase == sim::TurboPhase::Boost};
    }
    if (effect.kind == PowerupKind::Missile) {
        return {.name = kMissileParts.at(effect.use.shot), .loops = false};
    }
    return {};
}

bool IsShown(const sim::EffectSnapshot& effect, std::span<const sim::RacerSnapshot> racers) {
    if (effect.kind == PowerupKind::Turbo) {
        return effect.use.racer >= racers.size() || !racers[effect.use.racer].isGhost;
    }
    return effect.kind != PowerupKind::Warp ||
           effect.phase == static_cast<std::uint8_t>(sim::WarpPhase::Starting);
}

struct Look {
    float scale = 1.0F;
    float alpha = 1.0F;
};

Look LookOf(const sim::EffectSnapshot& effect, std::size_t part) {
    if (effect.kind == PowerupKind::Warp) {
        return {.scale = WarpPortalScale(effect.phaseLeftMs)};
    }
    if (effect.kind == PowerupKind::Magnet &&
        effect.phase == static_cast<std::uint8_t>(sim::MagnetPhase::Fading)) {
        const float left = effect.phaseLeftMs / kMagnetFadeMs;
        return part == 0 ? Look{.scale = left} : Look{.alpha = left * kMagnetRingFadeAlpha};
    }
    return {};
}

void Fade(std::span<render::MeshDraw> draws, float alpha) {
    for (render::MeshDraw& draw : draws) {
        draw.material.tint.a *= alpha;
        if (draw.material.blend == render::BlendMode::Opaque) {
            draw.material.blend = render::BlendMode::Alpha;
        }
    }
}

Transform OffsetOf(PowerupKind kind, std::size_t model) {
    return kind == PowerupKind::Curse && model > 0
               ? Transform{.position = {.z = -sim::kCurseModelHeight}}
               : Transform{};
}

}

PowerupVisuals::PowerupVisuals(const EffectModels& models) : models_(&models) {}

void PowerupVisuals::Update(const sim::RenderSnapshot& snapshot, float elapsedMs) {
    draws_.clear();
    for (Instance& instance : instances_) {
        instance.isLive = false;
    }
    for (const sim::EffectSnapshot& effect : snapshot.effects) {
        Instance& instance = InstanceFor(effect);
        Animate(instance, effect.phase, elapsedMs);
        if (IsShown(effect, snapshot.racers)) {
            for (Posed& posed : instance.models) {
                const Look look = LookOf(effect, posed.part);
                const std::size_t first = draws_.size();
                Place(posed, effect.transform, look.scale);
                if (look.alpha < 1.0F) {
                    Fade(std::span(draws_).subspan(first), look.alpha);
                }
            }
        }
    }
    for (const sim::ExplosionSnapshot& explosion : snapshot.explosions) {
        PlaceExplosion(explosion);
    }
}

PowerupVisuals::Instance& PowerupVisuals::InstanceFor(const sim::EffectSnapshot& effect) {
    if (effect.slot >= instances_.size()) {
        instances_.resize(effect.slot + std::size_t{1});
    }
    Instance& instance = instances_[effect.slot];
    const bool wasLive = std::exchange(instance.isLive, true);
    if (!wasLive || instance.kind != effect.kind || !(instance.use == effect.use)) {
        Start(instance, effect);
    }
    return instance;
}

void PowerupVisuals::Start(Instance& instance, const sim::EffectSnapshot& effect) {
    instance.kind = effect.kind;
    instance.use = effect.use;
    instance.phase.reset();
    instance.models.clear();
    const ModelNames names = NamesFor(effect.kind, effect.use.level);
    for (std::size_t part = 0; part < names.size(); ++part) {
        const auto found = names[part].empty() ? models_->end() : models_->find(names[part]);
        if (found == models_->end()) {
            continue;
        }
        Posed& posed = instance.models.emplace_back(
            Posed{.model = &found->second, .part = part, .offset = OffsetOf(effect.kind, part)});
        if (const JointedAssets* jointed = found->second.jointed) {
            Result<AnimationPlayer> player =
                AnimationPlayer::Create(jointed->animations, jointed->skeleton.nodes.size());
            if (player) {
                posed.pose.emplace(jointed->skeleton, found->second.draws.scale);
                posed.player.emplace(std::move(*player));
            }
        }
    }
}

void PowerupVisuals::Animate(Instance& instance, std::uint8_t phase, float elapsedMs) {
    const bool isNewPhase = instance.phase != phase;
    instance.phase = phase;
    const PartChoice choice = PartFor({.kind = instance.kind, .use = instance.use, .phase = phase});
    for (Posed& posed : instance.models) {
        if (!posed.player || !posed.pose) {
            continue;
        }
        if (isNewPhase) {
            const std::optional<std::size_t> part = choice.name.empty()
                                                        ? std::optional<std::size_t>{0}
                                                        : posed.player->FindPart(choice.name);
            posed.player->Play(part.value_or(0), choice.loops);
        }
        posed.player->Advance(elapsedMs);
        posed.player->PoseNodes(posed.pose->Locals());
    }
}

void PowerupVisuals::Place(Posed& posed, const Transform& engine, float scale) {
    const ModelDraws& model = posed.model->draws;
    const Transform game = Compose(TransformToGame(engine), posed.offset);
    const std::span<const Transform> nodes =
        posed.pose ? posed.pose->Solve() : std::span<const Transform>{};
    for (std::size_t i = 0; i < model.draws.size(); ++i) {
        render::MeshDraw draw = model.draws[i];
        const std::uint16_t joint = i < model.joints.size() ? model.joints[i] : 0;
        if (joint < nodes.size()) {
            Transform node = nodes[joint];
            node.position = node.position * scale;
            draw.transform = TransformToEngine(Compose(game, node));
        } else {
            draw.transform = TransformToEngine(game);
        }
        draw.scale *= scale;
        draws_.push_back(draw);
    }
}

void PowerupVisuals::PlaceExplosion(const sim::ExplosionSnapshot& explosion) {
    const std::string_view name =
        explosion.kind == sim::ExplosionKind::Spike ? kSpikeModel : kBlastModel;
    const auto found = models_->find(name);
    if (found == models_->end() || explosion.growth <= 0.0F) {
        return;
    }
    Posed posed{.model = &found->second};
    Place(posed, explosion.transform, explosion.growth);
}

}
