#include "app/checkpoint_gates.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

#include "app/track_space.hpp"
#include "assets/checkpoints.hpp"

namespace rr::app {
namespace {

constexpr float kGateHeight = 12.0F;
constexpr render::Color kGateColor{.r = 0.3F, .g = 0.5F, .b = 0.9F};
constexpr render::Color kLastGateColor{.r = 0.2F, .g = 0.9F, .b = 0.3F};
constexpr render::Color kNextGateColor{.r = 0.95F, .g = 0.8F, .b = 0.2F};

bool IsNext(const sim::LapTracker& laps, const sim::CheckpointGraph& graph, std::size_t gate) {
    const std::optional<std::size_t> last = laps.LastCheckpoint();
    return last && std::ranges::contains(graph.Checkpoints()[*last].links, gate);
}

render::Color GateColor(const sim::LapTracker& laps, const sim::CheckpointGraph& graph,
                        std::size_t gate) {
    if (laps.LastCheckpoint() == gate) {
        return kLastGateColor;
    }
    return IsNext(laps, graph, gate) ? kNextGateColor : kGateColor;
}

void AddGate(const assets::Checkpoint& gate, render::Color color,
             std::vector<render::DebugLine>& out) {
    const float length = std::hypot(gate.normal.x, gate.normal.y);
    const float scale = length > 0.0F ? sim::kGateRadius / length : 0.0F;
    const Vector3 across{.x = -gate.normal.y * scale, .y = gate.normal.x * scale};
    const Vector3& at = gate.position;
    const Vector3 left{.x = at.x - across.x, .y = at.y - across.y, .z = at.z};
    const Vector3 right{.x = at.x + across.x, .y = at.y + across.y, .z = at.z};
    const Vector3 leftTop{.x = left.x, .y = left.y, .z = left.z + kGateHeight};
    const Vector3 rightTop{.x = right.x, .y = right.y, .z = right.z + kGateHeight};
    for (const auto& [from, to] : {std::pair{left, right}, std::pair{leftTop, rightTop},
                                   std::pair{left, leftTop}, std::pair{right, rightTop}}) {
        out.push_back({.from = ToEngine(from), .to = ToEngine(to), .color = color});
    }
}

}

void DrawGates(const sim::CheckpointGraph& graph, const sim::LapTracker& laps,
               std::vector<render::DebugLine>& out) {
    out.clear();
    for (std::size_t i = 0; i < graph.Checkpoints().size(); ++i) {
        AddGate(graph.Checkpoints()[i], GateColor(laps, graph, i), out);
    }
}

}
