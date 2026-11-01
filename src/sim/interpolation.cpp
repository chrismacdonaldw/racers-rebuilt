#include "sim/interpolation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace rr::sim {
namespace {

const EntitySnapshot* FindEntity(const RenderSnapshot& snapshot, EntityId id) {
    auto found = std::ranges::lower_bound(snapshot.entities, id, {}, &EntitySnapshot::id);
    if (found == snapshot.entities.end() || found->id != id) {
        return nullptr;
    }
    return &*found;
}

const EffectSnapshot* FindEffect(const RenderSnapshot& snapshot, std::uint16_t slot) {
    auto found = std::ranges::find(snapshot.effects, slot, &EffectSnapshot::slot);
    return found == snapshot.effects.end() ? nullptr : &*found;
}

Transform Blend(const Transform& previous, const Transform& current, SnapshotFlags flags,
                float alpha) {
    if (HasFlag(flags, SnapshotFlags::Teleported)) {
        return current;
    }
    return Interpolate(previous, current, alpha);
}

}

void Interpolate(const RenderSnapshot& previous, const RenderSnapshot& current, float alpha,
                 RenderSnapshot& out) {
    out.tick = current.tick;
    out.entities.resize(current.entities.size());
    for (std::size_t i = 0; i < current.entities.size(); ++i) {
        const EntitySnapshot& entity = current.entities[i];
        const EntitySnapshot* before = FindEntity(previous, entity.id);
        out.entities[i] = entity;
        if (before != nullptr) {
            out.entities[i].transform =
                Blend(before->transform, entity.transform, entity.flags, alpha);
        }
    }
    out.racers.assign(current.racers.begin(), current.racers.end());
    out.bricks.assign(current.bricks.begin(), current.bricks.end());
    out.effects.assign(current.effects.begin(), current.effects.end());
    for (EffectSnapshot& effect : out.effects) {
        if (const EffectSnapshot* before = FindEffect(previous, effect.slot)) {
            effect.transform = Blend(before->transform, effect.transform, effect.flags, alpha);
            if (before->phase == effect.phase) {
                effect.phaseLeftMs = std::lerp(before->phaseLeftMs, effect.phaseLeftMs, alpha);
            }
        }
    }
    out.explosions.assign(current.explosions.begin(), current.explosions.end());
    out.entityClocks.assign(current.entityClocks.begin(), current.entityClocks.end());
    out.hiddenModels = current.hiddenModels;
    if (previous.entityClocks.size() == current.entityClocks.size()) {
        for (std::size_t i = 0; i < out.entityClocks.size(); ++i) {
            if (previous.entityClocks[i] && current.entityClocks[i]) {
                out.entityClocks[i] =
                    InterpolateClock(*previous.entityClocks[i], *current.entityClocks[i], alpha);
            }
        }
    }
    out.camera = current.camera;
    out.simStateHash = current.simStateHash;
    out.camera.transform =
        Blend(previous.camera.transform, current.camera.transform, current.camera.flags, alpha);
}

}
