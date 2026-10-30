#pragma once

#include <string_view>

#include "settings/registry.hpp"
#include "sim/input_frame.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

class Scene {
public:
    virtual ~Scene() = default;

    virtual void Enter() {}
    virtual void Exit() {}

    /// Runs when the scene opens and after each setting change, so `Tick` never reads settings.
    virtual void ApplySettings([[maybe_unused]] const settings::Registry& settings) {}

    /// A `MusicCatalog` id. Empty means silence.
    [[nodiscard]] virtual std::string_view MusicTrack() const { return {}; }

    /// A line for the developer overlay.
    [[nodiscard]] virtual std::string_view DeveloperStatus() const { return {}; }

    /// Advances exactly one `kSimTick`. Must not read clocks, settings or files.
    virtual void Tick(const sim::InputFrame& input) = 0;

    /// Overwrites `out`, keeping its storage.
    virtual void Snapshot(sim::RenderSnapshot& out) const = 0;

    virtual void Render(const sim::RenderSnapshot& interpolated) = 0;
};

}
