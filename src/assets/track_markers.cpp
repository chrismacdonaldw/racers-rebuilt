#include "assets/track_markers.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <map>
#include <ranges>
#include <system_error>
#include <tuple>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/gltf_data.hpp"
#include "assets/hazard_settings.hpp"
#include "assets/json_value.hpp"
#include "assets/mirror.hpp"
#include "assets/route_markers.hpp"
#include "assets/track_loader.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace rr::assets {
namespace {

constexpr std::string_view kPaces = "FMS";
constexpr std::string_view kStartGridFile = "STARTPOS.SPB";
constexpr std::string_view kCheckpointsFile = "RACE.CPB";
constexpr std::size_t kMaxGates = kNoLink;
constexpr std::size_t kVec3 = 3;
constexpr std::size_t kVec4 = 4;
constexpr std::size_t kMatrixFloats = 16;

struct Pose {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
};

struct NumberedGate {
    std::size_t number = 0;
    Pose pose;
    std::vector<std::string> links;
    std::optional<float> distance;
};

std::vector<std::string_view> PartsOf(std::string_view name) {
    std::vector<std::string_view> parts;
    for (const auto part : std::views::split(name, '.')) {
        parts.emplace_back(part.begin(), part.end());
    }
    return parts;
}

std::optional<std::size_t> NumberOf(std::string_view text) {
    std::size_t number = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return number;
}

glm::mat4 WorldOf(const cgltf_node& node) {
    std::array<float, kMatrixFloats> values{};
    cgltf_node_transform_world(&node, values.data());
    glm::mat4 world{1.0F};
    for (std::size_t column = 0; column < kVec4; ++column) {
        for (std::size_t row = 0; row < kVec4; ++row) {
            world[static_cast<glm::length_t>(column)][static_cast<glm::length_t>(row)] =
                values.at((column * kVec4) + row);
        }
    }
    return world;
}

Pose PoseOf(const cgltf_node& node) {
    const glm::mat4 world = WorldOf(node);
    return {.position = ToGame(glm::vec3(world[3])),
            .forward = Normalized(ToGame(glm::vec3(world[0])), {.x = 1.0F}),
            .up = Normalized(ToGame(glm::vec3(world[1])), {.z = 1.0F})};
}

JsonValue ExtrasOf(const cgltf_node& node) {
    if (node.extras.data == nullptr) {
        return {};
    }
    return ParseJson(node.extras.data).value_or(JsonValue{});
}

std::vector<Vector3> LinePoints(const cgltf_node& node) {
    std::vector<Vector3> points;
    if (node.mesh == nullptr || node.mesh->primitives_count == 0) {
        return points;
    }
    const cgltf_primitive& line = node.mesh->primitives[0];
    if (line.type != cgltf_primitive_type_line_strip) {
        return points;
    }
    const glm::mat4 world = WorldOf(node);
    for (const cgltf_attribute& attribute : std::span(line.attributes, line.attributes_count)) {
        if (attribute.type != cgltf_attribute_type_position) {
            continue;
        }
        for (cgltf_size i = 0; i < attribute.data->count; ++i) {
            glm::vec3 local{};
            cgltf_accessor_read_float(attribute.data, i, &local.x, kVec3);
            points.push_back(ToGame(glm::vec3(world * glm::vec4(local, 1.0F))));
        }
    }
    return points;
}

using OrderedRoute = std::pair<std::tuple<std::size_t, std::size_t, std::size_t>, SlotRoute>;

template <typename Value>
Value& Ensure(std::optional<Value>& value) {
    if (!value) {
        value.emplace();
    }
    return *value;
}

class MarkerReader {
public:
    void Read(const cgltf_node& node) {
        if (node.name == nullptr) {
            return;
        }
        const std::string_view name = node.name;
        const std::vector<std::string_view> parts = PartsOf(name);
        if (parts.size() < 2) {
            return;
        }
        const std::string_view kind = parts.front();
        if (kind == "start") {
            ReadStart(node, parts);
        } else if (kind == "gate") {
            ReadGate(node, parts);
        } else if (kind == "brick" || kind == "trial_brick") {
            ReadBrick(node, parts, kind == "brick" ? markers_.bricks : markers_.trialBricks);
        } else if (kind == "hazard") {
            ReadHazard(node, parts);
        } else if (kind == "route") {
            ReadRoute(node, parts);
        }
    }

    TrackMarkers Finish() && {
        FinishStarts();
        FinishGates();
        std::ranges::sort(routes_, {}, &OrderedRoute::first);
        for (auto& [order, route] : routes_) {
            markers_.routes.push_back(std::move(route));
        }
        return std::move(markers_);
    }

private:
    void Complain(std::string_view node, std::string_view problem) {
        markers_.problems.push_back(std::format("{}: {}", node, problem));
    }

    std::optional<std::size_t> Number(const cgltf_node& node,
                                      std::span<const std::string_view> parts, std::size_t index) {
        const std::optional<std::size_t> number =
            index < parts.size() ? NumberOf(parts[index]) : std::nullopt;
        if (!number || *number == 0) {
            Complain(node.name, "its name needs a number from 1");
            return std::nullopt;
        }
        return number;
    }

    void ReadStart(const cgltf_node& node, std::span<const std::string_view> parts) {
        const std::optional<std::size_t> number = Number(node, parts, 1);
        if (number && !starts_.emplace(*number, PoseOf(node)).second) {
            Complain(node.name, "there's another start with this number");
        }
    }

    void ReadGate(const cgltf_node& node, std::span<const std::string_view> parts) {
        const std::optional<std::size_t> number = Number(node, parts, 1);
        if (!number) {
            return;
        }
        const JsonValue extras = ExtrasOf(node);
        NumberedGate gate{.number = *number, .pose = PoseOf(node)};
        for (const JsonValue& link : extras.Items("links")) {
            gate.links.emplace_back(link.Text().value_or(""));
        }
        if (const JsonValue* distance = extras.Find("distance")) {
            gate.distance = distance->Float();
        }
        gates_.push_back(std::move(gate));
    }

    void ReadBrick(const cgltf_node& node, std::span<const std::string_view> parts,
                   std::optional<BrickLayout>& layout) {
        const std::string_view colour = parts.size() > 1 ? parts[1] : std::string_view();
        const Vector3 position = PoseOf(node).position;
        if (colour == "white") {
            Ensure(layout).white.push_back(position);
            return;
        }
        const auto found = std::ranges::find(kBrickColourNames, colour);
        if (found == kBrickColourNames.end()) {
            Complain(node.name, std::format("there's no brick colour {}", colour));
            return;
        }
        const JsonValue* respawn = ExtrasOf(node).Find("respawn_ms");
        const std::optional<Milliseconds::rep> respawnMs =
            respawn != nullptr ? respawn->Integer<Milliseconds::rep>()
                               : kDefaultBrickRespawn.count();
        if (!respawnMs) {
            Complain(node.name, "respawn_ms needs a whole number in range");
            return;
        }
        Ensure(layout).coloured.push_back(
            {.position = position,
             .colour = static_cast<BrickColour>(found - kBrickColourNames.begin()),
             .respawn = Milliseconds{*respawnMs}});
    }

    void ReadHazard(const cgltf_node& node, std::span<const std::string_view> parts) {
        const std::string_view name = parts.size() > 1 ? parts[1] : std::string_view();
        const std::optional<HazardKind> kind = HazardKindNamed(name);
        if (!kind) {
            Complain(node.name, std::format("there's no hazard kind {}", name));
            return;
        }
        Result<HazardPayload> payload = HazardPayloadFrom(*kind, ExtrasOf(node));
        if (!payload) {
            Complain(node.name, payload.error().message);
            return;
        }
        Hazard hazard{.kind = *kind, .payload = std::move(*payload)};
        const Vector3 position = PoseOf(node).position;
        if (auto* shot = std::get_if<CannonShot>(&hazard.payload)) {
            shot->launch = position;
        } else if (auto* ship = std::get_if<ShipCannons>(&hazard.payload)) {
            ship->shot.launch = position;
        } else if (auto* cannon = std::get_if<LightningCannon>(&hazard.payload);
                   cannon != nullptr && cannon->source) {
            cannon->source = position;
        }
        Ensure(markers_.hazards).push_back(std::move(hazard));
    }

    void ReadRoute(const cgltf_node& node, std::span<const std::string_view> parts) {
        const std::optional<std::size_t> slot =
            parts.size() > 3 ? NumberOf(parts[1]) : std::nullopt;
        const std::size_t pace = parts.size() > 3 && parts[2].size() == 1 ? kPaces.find(parts[2])
                                                                          : std::string_view::npos;
        const std::optional<std::size_t> number =
            parts.size() > 3 ? NumberOf(parts[3]) : std::nullopt;
        if (!slot || pace == std::string_view::npos || !number) {
            Complain(node.name, "a route is named route.<slot>.<F, M or S>.<number>");
            return;
        }
        Result<RaceRoute> route = RouteFromLine(LinePoints(node), ExtrasOf(node));
        if (!route) {
            Complain(node.name, route.error().message);
            return;
        }
        route->pace = static_cast<RoutePace>(pace);
        routes_.emplace_back(std::tuple(*slot, pace, *number),
                             SlotRoute{.slot = *slot, .route = std::move(*route)});
    }

    void FinishStarts() {
        if (starts_.empty()) {
            return;
        }
        StartGridAsset& grid = markers_.grid.emplace();
        for (const auto& [number, pose] : starts_) {
            if (number != grid.slots.size() + 1) {
                Complain(std::format("start.{}", grid.slots.size() + 1), "it's missing");
                break;
            }
            grid.slots.push_back(
                {.position = pose.position, .forward = pose.forward, .up = pose.up});
        }
        if (grid.slots.size() < kFullGrid) {
            Complain("start",
                     std::format("the grid has {} of {} starts", grid.slots.size(), kFullGrid));
        }
    }

    void FinishGates() {
        if (gates_.empty()) {
            return;
        }
        std::ranges::sort(gates_, {}, &NumberedGate::number);
        std::map<std::string, std::uint8_t, std::less<>> indices;
        CheckpointsAsset& gates = markers_.gates.emplace();
        for (const NumberedGate& gate : gates_) {
            if (gate.number != gates.checkpoints.size() + 1 ||
                gates.checkpoints.size() >= kMaxGates) {
                Complain(std::format("gate.{:02}", gates.checkpoints.size() + 1),
                         "gates must be numbered in order from 01, up to 254");
                break;
            }
            const Vector3 normal = Normalized(-gate.pose.forward, {.x = -1.0F});
            indices.emplace(std::format("gate.{:02}", gate.number),
                            static_cast<std::uint8_t>(gates.checkpoints.size()));
            gates.checkpoints.push_back(
                {.normal = normal,
                 .distance = gate.distance.value_or(-Dot(normal, gate.pose.position)),
                 .position = gate.pose.position});
        }
        for (std::size_t index = 0; index < gates.checkpoints.size(); ++index) {
            LinkGate(gates, index, gates_[index].links, indices);
        }
    }

    void LinkGate(CheckpointsAsset& gates, std::size_t index, std::span<const std::string> links,
                  const std::map<std::string, std::uint8_t, std::less<>>& indices) {
        std::array<std::uint8_t, 4>& linked = gates.checkpoints[index].links;
        linked.fill(kNoLink);
        if (links.empty()) {
            linked[0] = static_cast<std::uint8_t>((index + 1) % gates.checkpoints.size());
            return;
        }
        for (std::size_t i = 0; i < links.size() && i < linked.size(); ++i) {
            const auto found = indices.find(links[i]);
            if (found == indices.end()) {
                Complain(std::format("gate.{:02}", index + 1),
                         std::format("there's no gate {}", links[i]));
                continue;
            }
            linked.at(i) = found->second;
        }
    }

    TrackMarkers markers_;
    std::map<std::size_t, Pose> starts_;
    std::vector<NumberedGate> gates_;
    std::vector<OrderedRoute> routes_;
};

}

Result<TrackMarkers> ParseTrackMarkers(std::span<const std::byte> glb) {
    Result<GltfData> gltf = ParseGlb(glb);
    if (!gltf) {
        return std::unexpected(std::move(gltf.error()));
    }
    MarkerReader reader;
    for (const cgltf_node& node : std::span((*gltf)->nodes, (*gltf)->nodes_count)) {
        reader.Read(node);
    }
    return std::move(reader).Finish();
}

Result<std::optional<TrackMarkers>> ReadTrackMarkers(const Vfs& vfs, std::string_view track) {
    const std::string path = JoinGamePath(std::format("GAMEDATA/{}", track), kTrackModelFile);
    if (!vfs.Contains(path)) {
        return std::nullopt;
    }
    return vfs.Read(path)
        .and_then([](const FileData& file) { return ParseTrackMarkers(file.Bytes()); })
        .transform([](TrackMarkers markers) { return std::optional(std::move(markers)); })
        .transform_error([&](Error error) {
            error.message = std::format("{}: {}", path, error.message);
            return error;
        });
}

std::vector<std::string> TrackMarkerProblems(const Vfs& vfs) {
    std::vector<std::string> problems;
    for (const std::string& track : ListRaceTracks(vfs)) {
        const Result<std::optional<TrackMarkers>> markers = ReadTrackMarkers(vfs, track);
        if (!markers) {
            problems.push_back(markers.error().message);
            continue;
        }
        if (!markers->has_value()) {
            continue;
        }
        const TrackMarkers& found = markers->value();
        const std::string folder = std::format("GAMEDATA/{}", track);
        const std::string path = JoinGamePath(folder, kTrackModelFile);
        for (const std::string& problem : found.problems) {
            problems.push_back(std::format("{}: {}", path, problem));
        }
        if (!found.grid && !vfs.Contains(JoinGamePath(folder, kStartGridFile))) {
            problems.push_back(std::format("{}: it has no start.1 to start.6", path));
        }
        if (!found.gates && !vfs.Contains(JoinGamePath(folder, kCheckpointsFile))) {
            problems.push_back(std::format("{}: it has no gate.01 and on", path));
        }
    }
    return problems;
}

void MirrorY(TrackMarkers& markers) {
    if (markers.grid) {
        MirrorY(*markers.grid);
    }
    if (markers.gates) {
        MirrorY(*markers.gates);
    }
    for (std::optional<BrickLayout>* bricks : {&markers.bricks, &markers.trialBricks}) {
        if (*bricks) {
            MirrorY(**bricks);
        }
    }
    if (markers.hazards) {
        MirrorY(*markers.hazards);
    }
    for (SlotRoute& route : markers.routes) {
        route.route = Mirrored(std::move(route.route));
    }
}

}
