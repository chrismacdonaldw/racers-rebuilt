#include "app/hazard/ghost_trail.hpp"

#include <array>
#include <cstddef>
#include <variant>

#include "app/track_space.hpp"
#include "assets/game_path.hpp"
#include "core/duration.hpp"

namespace rr::app {
namespace {

struct TrailCopy {
    Milliseconds lag{0};
    float alpha = 1.0F;
};

constexpr std::string_view kTrailModel = "GTRAIL";
constexpr float kAlphaScale = 1.0F / 255.0F;
constexpr std::array kCopies{TrailCopy{.lag = Milliseconds{100}, .alpha = 126.0F * kAlphaScale},
                             TrailCopy{.lag = Milliseconds{200}, .alpha = 84.0F * kAlphaScale},
                             TrailCopy{.lag = Milliseconds{300}, .alpha = 42.0F * kAlphaScale}};

}

GhostTrail::GhostTrail(std::span<const render::MeshDraw> trackDraws,
                       std::span<const NamedDraws> named, const sim::TrackEntitySources& sources)
    : sources_(&sources) {
    for (const NamedDraws& draws : named) {
        if (assets::ToUpperAscii(draws.entity) == kTrailModel) {
            const auto first = trackDraws.begin() + static_cast<std::ptrdiff_t>(draws.span.first);
            model_.assign(first, first + static_cast<std::ptrdiff_t>(draws.span.count));
            return;
        }
    }
}

void GhostTrail::Append(const sim::Hazards& hazards, std::span<const sim::EntityClock> clocks,
                        std::vector<render::MeshDraw>& draws) const {
    for (const sim::HazardSlot& slot : hazards.Slots()) {
        const auto* ghost = std::get_if<sim::Ghost>(&slot.state);
        if (ghost != nullptr && ghost->entity && *ghost->entity < clocks.size() &&
            slot.phase == sim::HazardPhase::Active) {
            AppendCopies(*ghost->entity, clocks[*ghost->entity], draws);
        }
    }
}

void GhostTrail::AppendCopies(std::size_t ghost, const sim::EntityClock& clock,
                              std::vector<render::MeshDraw>& draws) const {
    for (const TrailCopy& copy : kCopies) {
        const Transform placed = TransformToEngine(
            sim::NodeWorldBefore(*sources_, ghost, clock, sim::kGhostBodyNode, copy.lag));
        for (render::MeshDraw draw : model_) {
            draw.transform = placed;
            draw.material.tint.a *= copy.alpha;
            draw.material.blend = render::BlendMode::Alpha;
            draws.push_back(draw);
        }
    }
}

}
