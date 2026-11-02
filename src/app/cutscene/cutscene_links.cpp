#include "app/cutscene/cutscene_links.hpp"

#include <span>
#include <vector>

namespace rr::app {

void DispatchLinks(const assets::CutsceneFrame& frame, std::span<const assets::CutsceneLink> links,
                   std::span<const CutsceneEventChange> changes, std::vector<EffectCommand>& out) {
    for (const CutsceneEventChange& change : changes) {
        if (change.event.kind != CutsceneEventKind::Transform) {
            continue;
        }
        const assets::CutsceneTransform& transform = frame.transforms[change.event.index];
        for (const assets::CutsceneLink& link : links) {
            if (link.transform == transform.name && link.onTransformEnd != change.begins) {
                out.push_back({.kind = link.kind,
                               .effect = link.effect,
                               .starts = link.starts,
                               .position = transform.position,
                               .axes = {.direction = transform.direction, .up = transform.up}});
            }
        }
    }
}

}
