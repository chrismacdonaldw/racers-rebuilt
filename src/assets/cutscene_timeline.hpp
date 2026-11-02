#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// Frames on the owning frame's clock.
struct CutsceneSpan {
    std::int32_t start = 0;
    std::int32_t duration = 0;

    [[nodiscard]] std::int32_t End() const { return start + duration; }

    friend bool operator==(const CutsceneSpan&, const CutsceneSpan&) = default;
};

struct CutsceneCamera {
    std::string name;
    CutsceneSpan span;
    std::string camera;
    std::optional<std::int32_t> part;
};

struct EntityByName {
    std::string name;
    bool isJointed = false;

    friend bool operator==(const EntityByName&, const EntityByName&) = default;
};

struct EntityByIndex {
    std::size_t world = 0;
    std::size_t entity = 0;

    friend bool operator==(const EntityByIndex&, const EntityByIndex&) = default;
};

/// `slot` indexes the materials of the entity's model, in its `GDB` order.
struct CutsceneMaterialBinding {
    std::size_t world = 0;
    std::size_t animation = 0;
    std::size_t track = 0;
    std::size_t slot = 0;
    std::size_t materialTable = 0;

    friend bool operator==(const CutsceneMaterialBinding&,
                           const CutsceneMaterialBinding&) = default;
};

struct CutsceneModel {
    std::string name;
    CutsceneSpan span;
    std::variant<EntityByName, EntityByIndex> model;
    std::optional<std::int32_t> part;
    std::vector<CutsceneMaterialBinding> materials;
    Vector3 position;
    Vector3 direction{.x = 1.0F};
    Vector3 up{.z = 1.0F};
};

struct CutsceneTransform {
    std::string name;
    CutsceneSpan span;
    Vector3 position;
    Vector3 direction{.x = 1.0F};
    Vector3 up{.z = 1.0F};
};

/// Counted in 30 FPS frames, whatever the cutscene's own rate.
struct LightBlink {
    std::int32_t framesOn = 0;
    std::int32_t framesOff = 0;

    friend bool operator==(const LightBlink&, const LightBlink&) = default;
};

using CutsceneColor = std::array<std::uint8_t, 3>;

struct CutsceneAmbientLight {
    std::string name;
    CutsceneSpan span;
    CutsceneColor color{};
    std::optional<LightBlink> blink;
};

struct CutsceneDirectionalLight {
    std::string name;
    CutsceneSpan span;
    CutsceneColor color{};
    Vector3 direction;
    std::optional<LightBlink> blink;
};

struct CutsceneFrame {
    std::string name;
    std::int32_t framesPerSecond = 30;
    std::int32_t length = 0;
    std::vector<CutsceneCamera> cameras;
    std::vector<CutsceneModel> models;
    std::vector<CutsceneTransform> transforms;
    std::vector<CutsceneAmbientLight> ambientLights;
    std::vector<CutsceneDirectionalLight> directionalLights;
};

struct CutsceneTimelineAsset {
    std::vector<std::string> worlds;
    std::vector<CutsceneFrame> frames;
};

/// Reads a `CDB`. A frame without a length runs to its last camera's end.
[[nodiscard]] Result<CutsceneTimelineAsset> ParseCutsceneTimeline(std::span<const std::byte> bytes);

}
