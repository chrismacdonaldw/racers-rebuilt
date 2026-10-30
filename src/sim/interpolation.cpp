#include "sim/interpolation.hpp"

#include <algorithm>
#include <cstddef>

namespace rr::sim {
namespace {

const EntitySnapshot* FindEntity(const RenderSnapshot& snapshot, EntityId id) {
    auto found = std::ranges::lower_bound(snapshot.entities, id, {}, &EntitySnapshot::id);
    if (found == snapshot.entities.end() || found->id != id) {
        return nullptr;
    }
    return &*found;
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
    out.camera = current.camera;
    out.simStateHash = current.simStateHash;
    out.camera.transform =
        Blend(previous.camera.transform, current.camera.transform, current.camera.flags, alpha);
}

}
