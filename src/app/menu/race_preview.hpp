#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "render/render_view.hpp"
#include "sim/physics/version_rules.hpp"

namespace rr::app {

/// A race's frame of the track preview cutscene, and the `DRIVERS.DDB` driver it shows.
struct RacePreview {
    std::string frame;
    std::string driver;
    std::size_t circuit = 0;
    std::int32_t position = 0;

    friend bool operator==(const RacePreview&, const RacePreview&) = default;
};

/// Unset past the original seven circuits, where the frame keeps its own colour.
[[nodiscard]] std::optional<render::Color> FrameTintOf(const RacePreview& preview,
                                                       const sim::VersionRules& rules);

}
