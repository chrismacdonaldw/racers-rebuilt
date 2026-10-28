#pragma once

#include <span>
#include <string_view>

#include "app/frame_loop.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "platform/event_pump.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/sdl_context.hpp"
#include "platform/window.hpp"
#include "settings/registry.hpp"

namespace rr::app {

// NOLINTNEXTLINE(bugprone-exception-escape)
class Application {
public:
    /// `args` excludes the program name.
    [[nodiscard]] static Result<Application> Create(std::span<const std::string_view> args);

    void Run();

private:
    struct Parts {
        platform::SdlContext sdl;
        settings::Registry settings;
        assets::Vfs vfs;
        platform::Window window;
        platform::FramePacer pacer;
    };

    explicit Application(Parts parts);

    platform::SdlContext sdl_;
    settings::Registry settings_;
    assets::Vfs vfs_;
    platform::Window window_;
    platform::EventPump events_;
    platform::FramePacer pacer_;
    FrameLoop frameLoop_;
};

}
