#pragma once

#include <filesystem>

#include "app/frame_loop.hpp"
#include "app/game_boot.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/result.hpp"
#include "platform/event_pump.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/sdl_context.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "settings/registry.hpp"

namespace rr::app {

// NOLINTNEXTLINE(bugprone-exception-escape)
class Application {
public:
    [[nodiscard]] static Result<Application> Create(GameBoot boot);

    [[nodiscard]] Result<void> Run();

private:
    struct Parts {
        platform::SdlContext sdl;
        settings::Registry settings;
        std::filesystem::path settingsFile;
        assets::Vfs vfs;
        audio::Mixer mixer;
        platform::Window window;
        render::Renderer renderer;
        platform::FramePacer pacer;
        platform::PacingConfig pacing;
    };

    explicit Application(Parts parts);

    void CyclePacing();
    void ApplyPacing();

    platform::SdlContext sdl_;
    settings::Registry settings_;
    std::filesystem::path settingsFile_;
    assets::Vfs vfs_;
    audio::Mixer mixer_;
    platform::Window window_;
    render::Renderer renderer_;
    platform::EventPump events_;
    platform::FramePacer pacer_;
    platform::PacingConfig pacing_;
    FrameLoop frameLoop_;
};

}
