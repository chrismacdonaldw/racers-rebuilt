#pragma once

#include "core/result.hpp"
#include "platform/event_pump.hpp"
#include "platform/sdl_context.hpp"
#include "platform/window.hpp"

namespace rr::app {

/// Composition root. Builds every system in dependency order and runs until the user quits.
class Application {
public:
    /// Resolves the data folder, opens the log, starts SDL and opens the main window.
    [[nodiscard]] static Result<Application> Create();

    /// Handles events until the user closes the window.
    void Run();

private:
    Application(platform::SdlContext sdl, platform::Window window);

    platform::SdlContext sdl_;
    platform::Window window_;
    platform::EventPump events_;
};

}
