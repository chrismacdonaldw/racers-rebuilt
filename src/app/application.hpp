#pragma once

#include "app/frame_loop.hpp"
#include "core/result.hpp"
#include "platform/event_pump.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/sdl_context.hpp"
#include "platform/window.hpp"

namespace rr::app {

class Application {
public:
    [[nodiscard]] static Result<Application> Create();

    void Run();

private:
    Application(platform::SdlContext sdl, platform::Window window, platform::FramePacer pacer);

    platform::SdlContext sdl_;
    platform::Window window_;
    platform::EventPump events_;
    platform::FramePacer pacer_;
    FrameLoop frameLoop_;
};

}
