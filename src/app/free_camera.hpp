#pragma once

#include <functional>
#include <string_view>
#include <utility>

#include "core/transform.hpp"
#include "mods/enhancement.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kFreeCamera{.key = "enhancements.free_camera",
                                                         .defaultValue = false,
                                                         .label = "Free camera",
                                                         .category = "Enhancements"};

inline constexpr settings::SettingSpec<double> kFreeCameraSpeed{
    .key = "enhancements.free_camera_speed",
    .defaultValue = 40.0,
    .range = settings::SettingRange<double>{.min = 1.0, .max = 400.0},
    .label = "Free camera speed",
    .category = "Enhancements"};

struct FlyKeys {
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool down = false;
    bool up = false;
    bool lookLeft = false;
    bool lookRight = false;
    bool lookUp = false;
    bool lookDown = false;
    bool fast = false;
};

/// WASD, Q and E, the arrow keys and Shift.
[[nodiscard]] FlyKeys ReadFlyKeys();

/// Starts from the scene camera each time it is turned on or the scene changes.
class FreeCamera final : public mods::Enhancement {
public:
    explicit FreeCamera(std::function<FlyKeys()> readKeys = ReadFlyKeys)
        : readKeys_(std::move(readKeys)) {}

    [[nodiscard]] std::string_view Name() const override { return "free camera"; }
    [[nodiscard]] bool ChangesGameplay() const override { return false; }

    void DeclareSettings(settings::Registry& registry) const override;
    void Attach(mods::HookRegistry& hooks, const settings::Registry& settings) override;

private:
    void Fly(mods::FrameRendering& frame);

    std::function<FlyKeys()> readKeys_;
    bool isEnabled_ = false;
    float unitsPerSecond_ = 0.0F;
    bool hasPose_ = false;
    Vector3 position_;
    float yawRadians_ = 0.0F;
    float pitchRadians_ = 0.0F;
};

}
