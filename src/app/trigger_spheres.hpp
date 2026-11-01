#pragma once

#include <span>
#include <vector>

#include "render/render_view.hpp"
#include "settings/setting_spec.hpp"
#include "sim/track/track_events.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kShowTriggerSpheres{.key = "dev.show_triggers",
                                                                 .defaultValue = false,
                                                                 .label = "Show trigger spheres",
                                                                 .category = "Developer"};

/// Engine-space rings for every trigger sphere, built once and relit each frame.
class TriggerRings {
public:
    explicit TriggerRings(const sim::TrackEvents& triggers);

    void Light(const sim::TrackEvents& triggers);

    [[nodiscard]] std::span<const render::DebugLine> Lines() const { return lines_; }

private:
    std::vector<render::DebugLine> lines_;
};

}
