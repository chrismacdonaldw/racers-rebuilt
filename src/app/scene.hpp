#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "audio/music_player.hpp"
#include "overlay/racer_tools.hpp"
#include "settings/registry.hpp"
#include "sim/input_frame.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// `track` is a `MusicCatalog` id. Empty means silence.
struct SceneMusic {
    std::string_view track;
    audio::Repeat repeat = audio::Repeat::Loop;
};

class Scene {
public:
    virtual ~Scene() = default;

    virtual void Enter() {}
    virtual void Exit() {}

    /// Runs when the scene opens and after each setting change, so `Tick` never reads settings.
    virtual void ApplySettings([[maybe_unused]] const settings::Registry& settings) {}

    /// Read every frame, so it can change as the scene runs.
    [[nodiscard]] virtual SceneMusic Music() const { return {}; }

    /// A scene name for the application to open after this frame. Each request is taken once.
    [[nodiscard]] virtual std::optional<std::string> TakeNextScene() { return std::nullopt; }

    [[nodiscard]] virtual std::string DeveloperStatus() const { return {}; }

    /// Only built while the overlay shows. Its actions take effect on the next `Tick`.
    [[nodiscard]] virtual overlay::RacerTools OverlayRacerTools() { return {}; }

    /// Advances exactly one `kSimTick`. Must not read clocks, settings or files.
    virtual void Tick(const sim::InputFrame& input) = 0;

    /// Overwrites `out`, keeping its storage.
    virtual void Snapshot(sim::RenderSnapshot& out) const = 0;

    virtual void Render(const sim::RenderSnapshot& interpolated) = 0;
};

}
