#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "settings/registry.hpp"

namespace rr::mods {

struct SceneEntered {};

struct SceneExited {};

struct TickStarted {
    std::uint64_t tick = 0;
};

struct TickEnded {
    std::uint64_t tick = 0;
};

/// Handlers may move `camera`. It only affects this frame's view, never the sim.
struct FrameRendering {
    Transform& camera;
    FloatSeconds elapsed;
};

struct SettingChanged {
    std::string_view key;
    const settings::Registry& settings;
};

/// Handlers run on loader threads at once, and must answer a path the same way all session.
struct AssetResolving {
    assets::AssetRequest& request;
};

/// Handlers run in the order they subscribed, on the main thread unless the event says otherwise.
class HookRegistry {
public:
    template <typename Event>
    using Handler = std::function<void(Event&)>;

    template <typename Event>
    void Subscribe(Handler<Event> handler) {
        std::get<std::vector<Handler<Event>>>(handlers_).push_back(std::move(handler));
    }

    template <typename Event>
    void Emit(Event event) const {
        for (const Handler<Event>& handler : std::get<std::vector<Handler<Event>>>(handlers_)) {
            handler(event);
        }
    }

private:
    std::tuple<std::vector<Handler<SceneEntered>>, std::vector<Handler<SceneExited>>,
               std::vector<Handler<TickStarted>>, std::vector<Handler<TickEnded>>,
               std::vector<Handler<FrameRendering>>, std::vector<Handler<SettingChanged>>,
               std::vector<Handler<AssetResolving>>>
        handlers_;
};

/// `hooks` must outlive every later read through `vfs`.
void ResolveThroughHooks(assets::Vfs& vfs, const HookRegistry& hooks);

}
