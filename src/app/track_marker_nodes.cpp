#include "app/track_marker_nodes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "app/json_fields.hpp"
#include "app/parsed_file.hpp"
#include "assets/brick_layout.hpp"
#include "assets/checkpoints.hpp"
#include "assets/game_path.hpp"
#include "assets/hazard_list.hpp"
#include "assets/race_route.hpp"
#include "assets/start_grid.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

using assets::GltfNode;
using assets::GltfScene;

constexpr std::string_view kRouteExtension = ".RRB";

template <typename Asset>
Result<std::optional<Asset>> ReadOptional(const assets::Vfs& vfs, const std::string& path,
                                          Parser<Asset> parse) {
    if (!vfs.Contains(path)) {
        return std::nullopt;
    }
    return ReadParsed(vfs, path, parse).transform([](Asset asset) {
        return std::optional(std::move(asset));
    });
}

Quaternion Facing(const Vector3& forward, const Vector3& up) {
    return sim::ToQuaternion(sim::OrthonormalBasis(forward, up));
}

std::size_t AddGroup(GltfScene& scene, std::string_view name, std::vector<GltfNode> children) {
    const std::size_t group = scene.AddNode({.name = std::string(name)});
    for (GltfNode& child : children) {
        const std::size_t index = scene.AddNode(std::move(child));
        scene.Node(group).children.push_back(index);
    }
    return group;
}

std::string GateName(std::size_t index) {
    return std::format("gate.{:02}", index + 1);
}

std::vector<GltfNode> StartNodes(const assets::StartGridAsset& grid) {
    std::vector<GltfNode> nodes;
    nodes.reserve(grid.slots.size());
    for (const assets::StartSlot& slot : grid.slots) {
        nodes.push_back({.name = std::format("start.{}", nodes.size() + 1),
                         .position = slot.position,
                         .rotation = Facing(slot.forward, slot.up)});
    }
    return nodes;
}

std::vector<GltfNode> GateNodes(const assets::CheckpointsAsset& gates) {
    std::vector<GltfNode> nodes;
    nodes.reserve(gates.checkpoints.size());
    for (const assets::Checkpoint& gate : gates.checkpoints) {
        std::vector<std::string> links;
        for (const std::uint8_t link : gate.links) {
            if (link != assets::kNoLink) {
                links.push_back(GateName(link));
            }
        }
        nodes.push_back(
            {.name = GateName(nodes.size()),
             .position = gate.position,
             .rotation = Facing(-gate.normal, {.z = 1.0F}),
             .extras = JsonFields().Add("links", links).Add("distance", gate.distance).Text()});
    }
    return nodes;
}

std::vector<GltfNode> BrickNodes(const assets::BrickLayout& layout, std::string_view prefix) {
    std::vector<GltfNode> nodes;
    std::array<std::size_t, assets::kBrickColourNames.size()> counts{};
    for (const assets::ColourBrickPlacement& brick : layout.coloured) {
        const auto colour = static_cast<std::size_t>(brick.colour);
        JsonFields extras;
        if (brick.respawn != assets::kDefaultBrickRespawn) {
            extras.Add("respawn_ms", brick.respawn.count());
        }
        nodes.push_back(
            {.name = std::format("{}.{}.{}", prefix, assets::kBrickColourNames.at(colour),
                                 ++counts.at(colour)),
             .position = brick.position,
             .extras = extras.IsEmpty() ? std::string() : extras.Text()});
    }
    for (std::size_t index = 0; index < layout.white.size(); ++index) {
        nodes.push_back({.name = std::format("{}.white.{}", prefix, index + 1),
                         .position = layout.white[index]});
    }
    return nodes;
}

JsonFields AimJson(const assets::AimPoint& aim) {
    return JsonFields().Add("position", aim.position).Add("event", aim.event);
}

JsonFields ShotJson(const assets::CannonShot& shot) {
    return JsonFields()
        .Add("launch", shot.launch)
        .Add("target", shot.target)
        .Add("trigger_centre", shot.triggerCentre)
        .Add("trigger_radius", shot.triggerRadius)
        .Add("event", shot.event);
}

std::string AimList(std::span<const assets::AimPoint> aims) {
    std::string joined;
    for (const assets::AimPoint& aim : aims) {
        joined += (joined.empty() ? "" : ",") + AimJson(aim).Text();
    }
    return "[" + joined + "]";
}

struct PayloadJson {
    JsonFields operator()(std::monostate /*none*/) const { return {}; }

    JsonFields operator()(const assets::CannonShot& shot) const { return ShotJson(shot); }

    JsonFields operator()(const assets::BreakableProp& prop) const {
        return JsonFields()
            .Add("surface", prop.surface)
            .Add("entities", prop.entities)
            .Add("trigger", prop.trigger);
    }

    JsonFields operator()(const assets::WaterScroll& water) const {
        return JsonFields()
            .Add("entity", water.entity)
            .Add("u_amplitude", water.uAmplitude)
            .Add("v_amplitude", water.vAmplitude);
    }

    JsonFields operator()(const assets::ShipCannons& ship) const {
        return JsonFields()
            .AddRaw("launches", AimList(ship.launches))
            .AddRaw("targets", AimList(ship.targets))
            .Add("shot", ShotJson(ship.shot));
    }

    JsonFields operator()(const assets::SwingingBox& box) const {
        return JsonFields()
            .Add("entity", box.entity)
            .Add("trigger", box.trigger)
            .Add("start_frame", box.startFrame)
            .Add("size", box.size);
    }

    JsonFields operator()(const assets::LightningCannon& cannon) const {
        return JsonFields()
            .Add("trigger", cannon.trigger)
            .Add("entity", cannon.entity)
            .Add("source", cannon.source)
            .Add("period_ms", cannon.period.count())
            .Add("sweep_radians", cannon.sweepRadians)
            .Add("vertical_range", cannon.verticalRange)
            .Add("vertical_base", cannon.verticalBase);
    }

    JsonFields operator()(const assets::DragonCannonballs& dragon) const {
        return JsonFields()
            .Add("material", dragon.material)
            .Add("entity", dragon.entity)
            .Add("trigger", dragon.trigger)
            .Add("interval_ms", dragon.interval.count());
    }

    JsonFields operator()(const assets::UfoGrabber& ufo) const {
        return JsonFields()
            .Add("trigger", ufo.trigger)
            .Add("entity", ufo.entity)
            .Add("pull_strength", ufo.pullStrength)
            .Add("grab_frame_low", ufo.grabFrameLow)
            .Add("grab_frame_high", ufo.grabFrameHigh);
    }
};

Vector3 HazardPosition(const assets::HazardPayload& payload) {
    if (const auto* shot = std::get_if<assets::CannonShot>(&payload)) {
        return shot->launch;
    }
    if (const auto* ship = std::get_if<assets::ShipCannons>(&payload)) {
        return ship->shot.launch;
    }
    const auto* cannon = std::get_if<assets::LightningCannon>(&payload);
    return cannon != nullptr ? cannon->source.value_or(Vector3{}) : Vector3{};
}

std::vector<GltfNode> HazardNodes(std::span<const assets::Hazard> hazards) {
    std::vector<GltfNode> nodes;
    std::map<std::string_view, std::size_t> counts;
    for (const assets::Hazard& hazard : hazards) {
        const std::string_view name = assets::HazardKindName(hazard.kind);
        const JsonFields settings = std::visit(PayloadJson{}, hazard.payload);
        nodes.push_back({.name = std::format("hazard.{}.{}", name, ++counts[name]),
                         .position = HazardPosition(hazard.payload),
                         .extras = settings.IsEmpty() ? std::string() : settings.Text()});
    }
    return nodes;
}

std::string RouteName(std::string_view file) {
    std::string name(file.substr(1, file.size() - 1 - kRouteExtension.size()));
    std::ranges::replace(name, '_', '.');
    return "route." + name;
}

GltfNode RouteNode(GltfScene& scene, std::string_view file, const assets::RaceRoute& route) {
    std::vector<Vector3> points{route.startPosition};
    std::vector<float> rotations;
    std::vector<float> widths;
    std::vector<float> kinds;
    std::vector<float> durations;
    points.reserve(route.points.size() + 1);
    for (const assets::RoutePoint& point : route.points) {
        points.push_back(points.back() + point.offset);
        const Quaternion& turn = point.rotation;
        rotations.insert(rotations.end(), {turn.w, turn.x, turn.y, turn.z});
        widths.insert(widths.end(), {point.widthLeft, point.widthRight});
        kinds.push_back(static_cast<float>(point.kind));
        durations.push_back(static_cast<float>(point.durationMs));
    }
    const std::string extras = JsonFields()
                                   .Add("start_rotation", route.startRotation)
                                   .Add("loop_point", route.loopPoint)
                                   .Add("loop_time_ms", route.loopTimeMs)
                                   .Add("loop_position", route.loopPosition)
                                   .Add("loop_rotation", route.loopRotation)
                                   .Add("rotations", rotations)
                                   .Add("widths", widths)
                                   .Add("kinds", kinds)
                                   .Add("durations_ms", durations)
                                   .Text();
    return {.name = RouteName(file), .mesh = scene.AddLine(points), .extras = extras};
}

bool IsRouteFile(const assets::VfsEntry& entry) {
    const std::string name = assets::ToUpperAscii(entry.name);
    return entry.kind == assets::EntryKind::File && name.starts_with('R') &&
           name.ends_with(kRouteExtension);
}

Result<std::vector<GltfNode>> RouteNodes(const assets::Vfs& vfs, std::string_view track,
                                         GltfScene& scene) {
    const std::string folder = std::format("GAMEDATA/{}", track);
    Result<std::vector<assets::VfsEntry>> entries = vfs.List(folder);
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    std::vector<GltfNode> nodes;
    for (const assets::VfsEntry& entry : *entries) {
        if (!IsRouteFile(entry)) {
            continue;
        }
        Result<assets::RaceRoute> route =
            ReadParsed(vfs, assets::JoinGamePath(folder, entry.name), assets::ParseRaceRoute);
        if (!route) {
            return std::unexpected(std::move(route.error()));
        }
        nodes.push_back(RouteNode(scene, assets::ToUpperAscii(entry.name), *route));
    }
    return nodes;
}

class MarkerBuilder {
public:
    MarkerBuilder(const assets::Vfs& vfs, std::string_view track, GltfScene& scene)
        : vfs_(&vfs), folder_(std::format("GAMEDATA/{}", track)), scene_(&scene) {}

    Result<void> AddRequired() {
        Result<assets::StartGridAsset> grid =
            ReadParsed(*vfs_, Path("STARTPOS.SPB"), assets::ParseSpb);
        Result<assets::CheckpointsAsset> gates = grid.and_then([&](const assets::StartGridAsset&) {
            return ReadParsed(*vfs_, Path("RACE.CPB"), assets::ParseCpb);
        });
        if (!gates) {
            return std::unexpected(std::move(gates.error()));
        }
        Add("start", StartNodes(*grid));
        Add("gates", GateNodes(*gates));
        return {};
    }

    Result<void> AddOptional(std::string_view track) {
        Result<std::optional<assets::BrickLayout>> bricks =
            ReadOptional(*vfs_, Path("POWERUP.PWB"), assets::ParsePwb);
        Result<std::optional<assets::BrickLayout>> trialBricks = bricks.and_then([&](const auto&) {
            return ReadOptional(*vfs_, Path("POWERUP2.PWB"), assets::ParsePwb);
        });
        Result<std::optional<std::vector<assets::Hazard>>> hazards =
            trialBricks.and_then([&](const auto&) {
                return ReadOptional(*vfs_, Path("HAZARDS.HZB"), assets::ParseHazards);
            });
        Result<std::vector<GltfNode>> routes =
            hazards.and_then([&](const auto&) { return RouteNodes(*vfs_, track, *scene_); });
        if (!routes) {
            return std::unexpected(std::move(routes.error()));
        }
        AddIf(bricks->has_value(), "bricks", [&] { return BrickNodes(**bricks, "brick"); });
        AddIf(trialBricks->has_value(), "trial_bricks",
              [&] { return BrickNodes(**trialBricks, "trial_brick"); });
        AddIf(hazards->has_value(), "hazards", [&] { return HazardNodes(**hazards); });
        AddIf(!routes->empty(), "routes", [&] { return std::move(*routes); });
        return {};
    }

    [[nodiscard]] std::vector<std::size_t> Roots() && { return std::move(roots_); }

private:
    [[nodiscard]] std::string Path(std::string_view file) const {
        return assets::JoinGamePath(folder_, file);
    }

    void Add(std::string_view name, std::vector<GltfNode> nodes) {
        roots_.push_back(AddGroup(*scene_, name, std::move(nodes)));
    }

    template <typename Make>
    void AddIf(bool has, std::string_view name, Make make) {
        if (has) {
            Add(name, make());
        }
    }

    const assets::Vfs* vfs_;
    std::string folder_;
    GltfScene* scene_;
    std::vector<std::size_t> roots_;
};

}

Result<std::vector<std::size_t>> AddTrackMarkers(const assets::Vfs& vfs, std::string_view track,
                                                 GltfScene& scene) {
    MarkerBuilder builder(vfs, track, scene);
    return builder.AddRequired()
        .and_then([&] { return builder.AddOptional(track); })
        .transform([&] { return std::move(builder).Roots(); });
}

}
