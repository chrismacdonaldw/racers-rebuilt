#include "app/powerup/powerup_visuals.hpp"

#include <array>
#include <string_view>
#include <utility>

#include "app/track_space.hpp"
#include "sim/physics/turbo.hpp"
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
            return {"CURSE"};
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

bool IsShown(const sim::EffectSnapshot& effect) {
    return effect.kind != PowerupKind::Warp ||
           effect.phase == static_cast<std::uint8_t>(sim::WarpPhase::Starting);
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
        if (IsShown(effect)) {
            for (Posed& posed : instance.models) {
                Place(posed, effect.transform, 1.0F);
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
    for (const std::string_view name : NamesFor(effect.kind, effect.use.level)) {
        const auto found = name.empty() ? models_->end() : models_->find(name);
        if (found == models_->end()) {
            continue;
        }
        Posed& posed = instance.models.emplace_back(Posed{.model = &found->second});
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
    const Transform game = TransformToGame(engine);
    const std::span<const Transform> nodes =
        posed.pose ? posed.pose->Solve() : std::span<const Transform>{};
    for (std::size_t i = 0; i < model.draws.size(); ++i) {
        render::MeshDraw draw = model.draws[i];
        const std::uint16_t joint = i < model.joints.size() ? model.joints[i] : 0;
        draw.transform =
            joint < nodes.size() ? TransformToEngine(Compose(game, nodes[joint])) : engine;
        draw.scale = scale;
        draws_.push_back(draw);
    }
}

void PowerupVisuals::PlaceExplosion(const sim::ExplosionSnapshot& explosion) {
    const std::string_view name =
        explosion.kind == sim::ExplosionKind::Spike ? kSpikeModel : kBlastModel;
    const auto found = models_->find(name);
    if (found == models_->end() || explosion.scale <= 0.0F) {
        return;
    }
    Posed posed{.model = &found->second};
    Place(posed, explosion.transform, explosion.scale);
}

}
